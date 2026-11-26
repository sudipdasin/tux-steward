# Release and forward-continuity policy

SysPulse is the next major generation of the Tux Steward native C++ codebase. The maintained source line is the source of truth. Future versions inherit tested features and regression coverage through shared code and Git ancestry. Ancestry does not prove semantic compatibility: every change still needs appropriate verification. Historical tags and downloadable assets remain immutable snapshots; maintained old branches require separately verified backports. No automatic cross-repository or proprietary-code synchronization is configured.

## Hosted CI is disabled

GitHub Actions and CircleCI remain disabled indefinitely unless the developer explicitly approves their use again. Do not invoke manual workflows, restore archived YAML, or create a tag/release event that can trigger a workflow on another branch. Definitions under ci/disabled are historical references with non-executable filenames. Source synchronization uses `[skip ci]` commit messages. A source push is not a test run or a release certification.

## Required execution evidence

Application builds, tests, screenshots, installers, and system-action verification run in a real isolated VM. A container, chroot, or offscreen renderer alone does not meet that requirement. Record source identity, guest kernel/userspace, compiler and Qt versions, suite results, actual package checks, and limitations. Preserve failed-run evidence and distinguish environmental failures from fixed regressions.

Before publishing a testing artifact, verify its executable/version, required libraries, install layout or portable launcher, checksums, and private build-path exclusion. State the exact tested platform. Debian 13 binaries must not be advertised as Mint/Ubuntu compatible without evidence. Bundled runtimes retain dependency notices, source references, and provenance limitations. Application source must be available with the artifact version.

An alpha may be delivered for exploratory testing with explicit missing gates. Beta, release-candidate, and stable labels require progressively stronger target-environment evidence. Do not certify full XFCE sessions, PolicyKit accept/deny/cancel, sign-in recovery, package-manager upgrades, hardware integrations, or filesystem repair based only on fixtures and offscreen UI tests.

## Publication and feedback

The owner's standing instruction of 26 November 2026 is: **"releases are always approved"**. Routine release publication for this project is already authorized. After the relevant verification gates pass, commit the changes and publish the appropriate alpha, beta, release candidate or stable version without requesting release approval again. This includes creating a version tag at the verified source revision, preparing/publishing the release, uploading native binaries, corresponding application source and checksums, and updating release notes and download links. See the [project blueprint](PROJECT_BLUEPRINT.md).

Standing approval does not waive execution evidence, accurate maturity/platform labels, source/runtime provenance, privacy, immutable historical artifacts or the prohibition on GitHub Actions and CircleCI. Do not create a tag or release event that could execute an old hosted workflow. If a higher-priority tool policy explicitly blocks an action, finish unaffected work, identify the exact blocked action and stated reason, and request only the authorization that policy requires; do not invent another project-level approval gate.

Do not infer that CI restrictions prohibit source synchronization or manual packaging. Inspect existing assets before changing a release, do not silently replace previously downloaded files, and keep version/changelog/checksums aligned. Never report a release asset as uploaded without a verified upload result. A change to repository visibility, publication of unrelated private/Pro code or disclosure of private diagnostics is outside routine release publication.

No telemetry, background self-update, automatic cleanup, or automatic feedback upload runs on user machines. Reports default to redacted summaries; raw output requires explicit fresh opt-in. Preserve developer attribution while excluding private computer information from source examples, screenshots and public release evidence.
