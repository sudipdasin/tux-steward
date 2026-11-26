# SysPulse 3.1.0-rc.1

This candidate completes the core maintenance loop: inspect, review, act, recover supported data, and retain a redacted outcome. It keeps the existing monitoring, process, startup, service, 35-tool catalog, manual release review and XFCE settings features.

## Reviewed thumbnail cleanup

Open **Storage & cleanup → Reviewed cleanup**. Select thumbnail categories and a minimum age, then analyze. Categories start unselected. Analysis is read-only and bounded to 2,000 eligible files, 20,000 entries and approximately five seconds. A bounded result explicitly says when its limit was reached. Only the listed files enter the reviewed plan; plans expire after one hour and file identities are checked again before moving.

The scope is the current user's standard thumbnail cache: `normal`, `large`, `x-large`, `xx-large` and thumbnail failure records in supported vendor folders. Original photos, documents, browser profiles, package caches and arbitrary directories are outside this workflow. Symlinks, multiply linked files, unsafe ownership/permissions, changed files and cross-filesystem moves are refused or skipped. Symbolic-link storage layouts are unsupported and reported rather than followed.

**Move to recovery does not free disk space.** It moves matching files into owner-only recovery storage within the user's cache, with a write-ahead manifest. Review the outcome and any skipped files. External cache cleaners can remove this storage, so it is a recovery convenience rather than a backup.

## Recovery and permanent removal

Open **Recovery → Thumbnail recovery → Refresh recovery**, select a saved run, and review restore or permanent removal. Restore never overwrites a regenerated cache file; conflicting or changed files remain staged. Permanent removal has its own default-Cancel confirmation and cannot be undone. Logical file sizes are estimates, not promised physical free-space gains.

**Startup recovery & limits** retains the guarded startup-override recovery workflow. There is no universal undo, disk snapshot or recovery of terminated processes and unrelated system changes.

## Persistent local reports

The latest 200 summaries persist in the user's application-data directory with owner-only permissions. They contain allowlisted action names, exit codes, elapsed times, cancellation/truncation flags and completion timestamps. Raw diagnostic output, command arguments, device paths and unknown task labels are excluded. This records activity and is not anonymous.

Reports refresh on entry or with Refresh, including results produced by scheduled diagnostics. **Export saved summaries** previews the exact JSON before a save dialog. The latest 20 detailed results remain only in session memory and use the existing per-export detail consent. Nothing is uploaded automatically. Removing the package preserves personal reports, plans and recovery data.

## Read-only diagnostic scheduling

Open **Schedules → Diagnostic plan**. Choose a fixed Health Check preset, hourly/daily/weekly cadence, local time, and optional AC-power condition. Preview writes nothing. **Review save & enable** creates the fixed per-user `syspulse-diagnostics.service` and `.timer`, reloads the user manager, enables/restarts the timer and checks its actual runtime/startup states. External edits to managed unit files are retained and block replacement. Errors do not manufacture an enabled/disabled result.

The installed helper `/usr/bin/syspulse-diagnostic --preset …` refuses root execution. It runs fixed read-only checks and saves only redacted summaries. It cannot run cleanup, package installation or arbitrary commands. A persistent timer may catch up a missed run when the user's manager starts. An active desktop systemd user manager is required; signing out generally stops a user's manager unless the system administrator configured lingering. Optional AC gating follows system-reported power state.

**Review disable** stops/disables only this timer and retains reports and configuration. Disable an enabled timer before uninstalling: personal user-unit files are preserved on package removal, so a retained timer would otherwise point to a removed helper. Opening SysPulse does not enable a timer or perform maintenance. Once explicitly enabled, the user timer runs independently of the GUI. Automation status is shown as unchecked/unknown until queried, never assumed off.

## Upgrade ordering and installation

The internal Debian version is **`3.1.0~rc1`**, which sorts above the historical `3.0.0` alpha, hyphen prerelease, plain, revision and local-version forms covered by the regression. The previous alpha's `3.0.0~alpha3` could sort below an already installed `3.0.0` or a hyphen form. A release version must advance monotonically; do not force a downgrade or add an epoch to conceal the mismatch. A genuinely newer installed release must remain newer.

Use the amd64 Ubuntu-ABI DEB on the supported Ubuntu/Mint family. Verify its checksum, then install with APT to resolve dependencies. Start SysPulse as the normal desktop user. The native package uses system Qt/libc and includes the three compatibility applications plus the new diagnostic helper. The legacy setup wizard still manages its original compatibility applications.

## Verification boundaries

See the executed validation record for exact source identity, suite counts and guest integration results. A release candidate is not a claim of physical Mint hardware, Wayland, arbitrary filesystem support, signed update distribution, antivirus scanning or universal administrator-action certification. Settings-changing catalog workflows remain Experimental and retain their scoped review. No GitHub Actions or CircleCI is used.

Developed by Sudip Das. GNU GPL version 3 or later.
