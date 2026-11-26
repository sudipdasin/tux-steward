# Native idle benchmark

`syspulse-idle-probe` measures the real visible SysPulse Overview using C++17 and Qt. It creates `Window(true)`, allows two seconds of warmup, and then spends twelve seconds in the normal QApplication event loop. It never runs maintenance or service actions, busy-polls `processEvents`, or sleeps in the event loop. The optional paused mode pauses only the embedded resource sampler, so unrelated application wakeups remain measurable.

Build and run **inside the isolated validation VM**. The executable is available only with `BUILD_TESTING=ON` and is not registered as a normal CTest benchmark:

```sh
cmake -S . -B build-probe -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON
cmake --build build-probe --target syspulse-idle-probe -j2
QT_QPA_PLATFORM=offscreen build-probe/syspulse-idle-probe --source-label baseline-commit --output baseline-live.json
QT_QPA_PLATFORM=offscreen build-probe/syspulse-idle-probe --paused --source-label baseline-commit --output baseline-paused.json
```

Repeat with the candidate commit and different output names. Use a real desktop renderer such as `xcb` for desktop measurements; `offscreen` is a controlled headless renderer and must not be presented as an XFCE desktop performance result. Keep renderer, build type, VM resources, window size, sampling interval, and test conditions identical when comparing revisions. Use at least three alternating baseline/candidate runs per mode before claiming an improvement; keep raw JSON and compare medians and spread. QEMU TCG results describe that VM, not native laptop performance.

The default measured interval is 12000 ms after 2000 ms warmup. `--duration-ms` accepts integers from 1000 through 120000; use 10000–15000 ms or longer for comparisons. Short runs are useful for smoke checks but provide coarse CPU quantization. `--source-label` is caller-provided data, JSON-escaped rather than executed or interpolated into commands. Use a commit or comparison label, not a personal path or machine identifier. `--output` saves an owner-only JSON file; the same JSON is printed to stdout. Diagnostics go to stderr.

Fields distinguish measurements and limitations:

| Field | Meaning |
| --- | --- |
| `elapsed_seconds` | Actual duration from a monotonic clock, not the requested timer interval |
| `cpu_ticks_delta`, `cpu_tick_hz` | This process’s user+system tick delta from procfs and `sysconf(_SC_CLK_TCK)` |
| `process_cpu_seconds` | Tick delta divided by the reported frequency; includes all process threads, excludes child processes |
| `cpu_fraction_one_core` | Process CPU seconds divided by elapsed wall seconds; 1.0 means one full core and can be exceeded by multiple threads |
| `cpu_percent_one_core` | The same fraction multiplied by 100; not divided by the machine’s CPU count |
| `rss_start_kib`, `rss_end_kib` | Process VmRSS after warmup and at completion, in KiB (1024 bytes) |
| `rss_endpoint_max_kib` | The greater endpoint value; **not** a sampled high-water mark or peak RSS |
| `timer_events_total` | Delivered `QEvent::Timer` events on the QApplication thread during measurement |
| `probe_stop_timer_events` | Delivered events belonging to this probe’s single stop timer |
| `application_timer_events` | Total minus the probe’s stop events; warmup events are excluded |

CPU usage is tick-quantized. Two procfs endpoint reads and the stop callback add a small amount of probe work; no periodic benchmark sampling timer is introduced. Timer counts are delivery counts, not kernel wakeup counts, and do not include non-timer events or timers delivered on other threads. Endpoint RSS does not characterize transient peaks or leaks. No hostname, CPU model, username, local path, address, or machine identifier is collected in JSON.

The probe exits nonzero if required counters cannot be read, the duration is invalid, the visible Overview changes, or JSON cannot be saved. Run the same committed probe on both revisions when assessing removal of the earlier admission polling timer. This document records the method; numerical claims require actual VM results.

## Executed VM comparison — 26 November 2026

Release build, Qt 5.15.15, offscreen renderer, QEMU TCG with six virtual CPUs and 4 GiB assigned RAM. Each case used the same committed C++ probe, a two-second warmup and approximately twelve measured seconds. The baseline source label was `4e4e407+probe2e84796`; the optimized label added `idle0056153`.

| Mode | Application timer events before → after | CPU percent of one virtual core before → after | RSS endpoint before → after |
| --- | --- | --- | --- |
| Live monitoring | 132 → 12 | 3.750% → 1.250% | 46,480 → 46,424 KiB |
| Sampling paused | 120 → 0 | 2.166% → 0.083% | 46,396 → 46,076 KiB |

The change removes a repeating 100 ms admission timer and uses synchronous busy-state notifications. The result demonstrates fewer delivered Qt timer events. These events are not a measurement of kernel wakeups. CPU values are tick-quantized observations from one short run per condition inside an emulator, not statistical significance or a physical-hardware speed guarantee. RSS values are process endpoints, not peak memory, and include the probe instrumentation.

Raw evidence: `baseline-live.json`, `baseline-paused.json`, `after-live.json`, and `after-paused.json`, retained with the milestone verification files. The idle branch also passed its management and shell regression suites before merge.
