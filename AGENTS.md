# SysPulse project instructions

SysPulse is an XFCE-first native C++ Linux system/administration suite built on Tux Steward. The approved UI is Precision (white/crimson, concept 1), using Observatory monitoring depth (3) and Forge service table/inspector/activity organization (5). Read docs/SYSPULSE_UI_SPECIFICATION.md before UI changes.

- Releases are pre-approved by the owner as a standing instruction dated 26 November 2026. Follow docs/PROJECT_BLUEPRINT.md and docs/RELEASE_POLICY.md: complete verification, commit and publish the appropriate release without asking for routine release approval again. This authorization includes version tags, release notes, binary/source/checksum uploads and release-link updates for this project; it does not waive validation, privacy, provenance or CI restrictions.
- Do not run or trigger GitHub Actions or CircleCI unless the developer explicitly approves them again. Their definitions are archived under ci/disabled. Source commits use `[skip ci]`. Do not publish tags or release events that could invoke old workflows on other branches.
- Build and verify applications, installers, system mutations and regression tests in a REAL isolated VM. An offscreen display may run inside that VM; a container or chroot alone is not a VM. Host-side source inspection and editing are allowed. Use tools/vm for the documented local harness.
- Preserve every validated feature through shared source and regression tests. Reuse audited Tux Steward modules; do not silently remove inherited capabilities or import unrelated private/Pro code.
- Keep machine-identifying information, private paths, personal logs and local configuration out of source, public fixtures, screenshots and release artifacts. Developer credit remains Sudip Das.
- Opening the app, navigating, inspecting a result or exporting a summary must not start maintenance automatically. System changes require an explicit scoped in-app review. Use fixed argument vectors, never shell-interpolated commands.
- Keep settings-changing catalog workflows in Experimental until their supported environments are verified. State runtime capability limits honestly. Never treat test fixtures or generated UI mockups as proof of real functionality.
- No telemetry or automatic uploads. Reports default to redacted summaries; raw details require fresh opt-in. Never promise universal undo or retroactive updates to historical release artifacts.
- Separate UI, collectors, planning, execution and reports. Use C++17 and the existing Qt foundation. Development tooling is separate from application runtime.
