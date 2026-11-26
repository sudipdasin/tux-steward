# SysPulse 3.1.0-rc.1 core maintenance candidate

SysPulse completes the inspect → review → act → supported recovery → saved outcome workflow in the approved Precision interface. It remains the XFCE-first C++17/Qt successor to Tux Steward, developed by Sudip Das and licensed under GPL-3.0-or-later.

## Completed core workflows

- Reviewed thumbnail-cache analysis with no initial category selection, bounded scans, one-hour plans and identity revalidation before moving files.
- Private staged recovery, no-overwrite restore and separately reviewed permanent removal. Moving to recovery does not free disk space and is not a backup.
- Persistent owner-only history of the latest 200 redacted summaries, timestamps, explicit refresh and preview-before-save exports. Raw details remain limited to session memory and fresh export consent.
- Reviewed per-user read-only diagnostic schedules with fixed presets, hourly/daily/weekly cadence, optional AC gating, explicit enable/disable and actual runtime/startup-state checks.
- The existing live CPU/per-core/memory/swap/disk/network monitoring, process identity safeguards, startup override recovery, service management, 35-tool catalog, manual release review, tool-availability audit and native XFCE settings launchers.
- Native packaging with monotonic Debian version `3.1.0~rc1`, plus the three retained compatibility applications and normal-user diagnostic helper.

Read [the candidate feature guide](SYSPULSE_RC1.md) for operation, storage scope and recovery limits. Settings-changing catalog tools remain in Experimental; the new timer cannot run them. Opening the app starts no maintenance or automatic update check. An explicitly enabled diagnostic timer runs independently of the GUI.

## Scope and validation

This candidate does not provide filesystem snapshots, universal undo, arbitrary file cleanup, antivirus scanning, automatic package installation or signed update distribution. Original photos and documents are outside thumbnail maintenance. Dependency availability does not prove administrator authorization or complete system health.

Application builds, tests, package installation and GUI validation must run in real isolated VMs. Read the versioned executed validation and release record for exact source identity, tested guest environments and remaining gates. Actual Mint hardware, Wayland and arbitrary privileged catalog operations are not certified by fixture or Ubuntu-base testing.

GitHub Actions and CircleCI remain disabled. Releases are already authorized by the owner under [the project blueprint](PROJECT_BLUEPRINT.md), subject to verification and truthful labels. Historical alpha artifacts and their versioned evidence remain immutable.
