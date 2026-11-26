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

The `3.0.0-alpha.1` application checkpoint is `c2150885c82bb202df8b82f6da760cc41fcd4f99`: all 24 regression suites passed in isolated QEMU. Documentation/source synchronization at `f0fe320677e40c05e0c176a2fd3efdbe961d9dcd` did not change those application sources. Native archives and actual native/portable X11 main-window launches were checked. See [executed validation](SYSPULSE_VALIDATION.md) and [release status](RELEASE_STATUS.md).

Native artifacts from that VM target Debian 13 x86_64, libc6 >= 2.41 and Qt >= 5.15.15. They must not be advertised as Mint/Ubuntu binaries. Full Mint/XFCE sessions, PolicyKit prompts, clean package-manager installation/upgrades/removal, hardware and Wayland remain outstanding. Broader bundled-runtime redistribution also needs exact third-party source-version provenance; native archives do not bundle those libraries. Continue those workstreams and keep the limits explicit in testing releases.
