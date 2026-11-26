# SysPulse project blueprint

Owner directive updated: 26 November 2026. Maintained repository: `sudipdasin/tux-steward`. SysPulse is the XFCE-first C++17/Qt successor to Tux Steward. Developer credit remains Sudip Das; the application is GPL-3.0-or-later.

## Standing release authorization

The owner's instruction is **"releases are always approved"**. This is persistent authorization for routine releases of this project and its maintained versions. Do not ask for the same release approval again after preparing and verifying an appropriate release.

The authorized release work includes source commits and merges, version tags at verified revisions, draft/public testing releases, native executable and package uploads, corresponding application-source/checksum uploads, release notes and download-link updates. Alpha, beta, release-candidate and stable labels must reflect the actual evidence. Standing approval permits publication; it does not supply missing verification or change an alpha into a stable release.

Publication of unrelated private/Pro code, repository visibility changes, private diagnostics disclosure and silent replacement of historical assets are outside routine release work. Honor any higher-priority tool-policy block and explain its concrete action/reason if it requires further authorization; do not add a new approval requirement based on this blueprint.

## Non-negotiable execution constraints

- Do not run or trigger GitHub Actions or CircleCI unless the owner explicitly reauthorizes those services. Release approval is not CI approval. Their configuration remains archived under `ci/disabled`; maintained release revisions must have no active configuration. Check event/target safety before creating a tag or publishing a release, including older branches where relevant.
- Use small batches. Develop independent milestones on separate branches. Run applicable checks before merging, preserve failed evidence and resolve failures before promotion. Source commits include `[skip ci]`.
- Build and execute the applications, installers, package smoke checks and regression suites inside a real isolated VM. A container, chroot or standalone offscreen renderer does not satisfy this requirement. Source inspection, documentation editing and Git archive operations may run on the host.
- Preserve tested functionality through the shared maintained C++ core and regressions. Do not import unrelated private/Pro implementations. Existing releases remain immutable snapshots.
- Keep private machine details, paths, logs and configuration out of public source and artifacts. Retain developer attribution and the required application/dependency notices and corresponding source.

## Product objectives

Use the approved Precision white/crimson interface, Observatory monitoring depth and Forge table/inspector/activity organization. Keep UI, collectors, planning, execution and reports separate. Preserve live CPU/per-core, memory/swap, disk/network history, process identity protections, startup overrides/recovery, bounded service queries and reviewed service actions, the inherited maintenance tools and redacted local reports.

Opening the app, navigating or inspecting a result must not start maintenance. System changes require explicit scoped review inside the app, fixed argument vectors and truthful actual outcomes. Settings-changing catalog tools remain Experimental until their supported environments are verified. There is no telemetry or automatic upload. Never promise universal undo or represent fixture success as hardware, desktop-session or authorization certification.

## Release procedure

1. Resolve the exact source revision, version, application scope and affected verification. Check the destination release for existing assets before writing anything.
2. Run the required real-VM checks and record source identity, guest/toolchain versions, results and limits. Fix failures on the relevant branches and merge verified changes.
3. Prepare the appropriately labeled packages, corresponding source, dependency/provenance information and checksums. Verify executable/version, dependencies, layout, privacy and the extracted launch path. Match the source archive to the release's application revision.
4. Confirm that publication cannot invoke prohibited CI. Create the version tag at the verified revision and publish/upload directly under standing approval. Do not silently rewrite historical downloads.
5. Verify the published release, its tag target and every asset's filename, byte size and digest against the prepared files. Only then report the live release URL and update current download/status documentation.

## Current verified checkpoint and remaining work

The published `3.0.0-alpha.2` tag points to `0105b57667f79129765c56e3f1654cd91c42cf3a`, tree `b56acee2f956beecedba0aaec70bb03ea1ca4d06`. Its Ubuntu 22.04 build passed 24/24 application suites, native package/archive verification and installed XFCE checks. The exact same native amd64 DEB passed installation, desktop-entry launch, removal and reinstallation in both Ubuntu 22.04 / XFCE 4.16 and Ubuntu 24.04 / XFCE 4.18 real VMs. All six published assets were checked against prepared byte sizes/digests; 13 publication regressions and the offline release/tag/asset verifier passed in a fresh isolated VM. See [Ubuntu validation](SYSPULSE_UBUNTU_VALIDATION.md), [download guidance](SYSPULSE_DOWNLOADS.md), [release status](RELEASE_STATUS.md) and [publication receipt](releases/3.0.0-alpha.2.json).

The owner confirmed Mint 22.3 XFCE 64-bit as the local installation target. Its Ubuntu 24.04 package base is covered by a runtime compatibility check, but actual Mint hardware, complete login/PolicyKit/systemd integration, Wayland and every historical upgrade path remain unverified. Native binaries use system libraries. Do not replace these limits with a stable/RC compatibility claim. Settings-changing catalog workflows remain Experimental.

Alpha.1 remains an immutable historical Debian 13 native build; its libc6 >= 2.41 / Qt >= 5.15.15 artifacts must not be presented as Mint/Ubuntu downloads. Its separate portable preview still has bundled-runtime source-provenance work outstanding. Continue target-desktop/hardware checks and secure update design without silent automatic system changes or prohibited hosted CI.
