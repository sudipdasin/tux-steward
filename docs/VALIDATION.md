# Validation — 0.7.0 Startup Apps upgrade

Executed locally on Linux with GCC 14.2 and Qt 5.15.15:

- C++ build succeeded for both applications and all native test targets.
- All six CTest suites passed: core, ui, task-manager-tabs, process-explorer, service-queries and autostart. Total local CTest duration: 7.46 seconds; this is test runtime, not a performance benchmark.
- The startup executable reported 104 backend/GUI checks when also rendering its fixture view. Coverage includes XDG priority, section-scoped Hidden parsing, user-only overrides, exact-content undo including CRLF/BOM, desktop restrictions, TryExec, duplicate keys, bounded reads, corrupt journals, external edits, backup failure, symbolic paths, confirmation cancellation, source filters and search.
- The startup fixture view was rendered offscreen and visually inspected. All mutation tests used temporary directories and synthetic desktop entries, without changing real sign-in configuration or executing desktop-entry commands.

Reproduce:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j2
ctest --test-dir build --output-on-failure
```

CI policy is preserved: CircleCI is primary on main; GitHub Actions is a manual fallback. A local pass does not imply a remote pass. The pull request records subsequent primary-pipeline status.

Limitations: no real desktop sign-in was exercised; eligibility is not proof of successful launch. Real PolicyKit, service mutations, Bluetooth/GPU hardware, disk-full/power-loss interruption and full Linux Mint/XFCE sessions remain unverified. The one-step undo journal is not a complete backup system and refuses changed targets rather than overwriting them. Keep undo contents private.
