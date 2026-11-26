# Release and forward-continuity policy

SysPulse is the next major generation of the Tux Steward native C++ codebase. The maintained source line is the source of truth. Future versions inherit tested features and regression coverage through shared code and Git ancestry. Ancestry does not prove semantic compatibility: every change still needs appropriate verification. Historical tags and downloadable assets remain immutable snapshots; maintained old branches require separately verified backports. No automatic cross-repository or proprietary-code synchronization is configured.

## Hosted CI is disabled

GitHub Actions and CircleCI remain disabled indefinitely unless the developer explicitly approves their use again. Do not invoke manual workflows, restore archived YAML, or create a tag/release event that can trigger a workflow on another branch. Definitions under ci/disabled are historical references with non-executable filenames. Source synchronization uses `[skip ci]` commit messages. A source push is not a test run or a release certification.

## Required execution evidence

Application builds, tests, screenshots, installers, and system-action verification run in a real isolated VM. A container, chroot, or offscreen renderer alone does not meet that requirement. Record source identity, guest kernel/userspace, compiler and Qt versions, suite results, actual package checks, and limitations. Preserve failed-run evidence and distinguish environmental failures from fixed regressions.

Before publishing a testing artifact, verify its executable/version, required libraries, install layout or portable launcher, checksums, and private build-path exclusion. State the exact tested platform. Debian 13 binaries must not be advertised as Mint/Ubuntu compatible without evidence. Bundled runtimes retain dependency notices, source references, and provenance limitations. Application source must be available with the artifact version.

An alpha may be delivered for exploratory testing with explicit missing gates. Beta, release-candidate, and stable labels require progressively stronger target-environment evidence. Do not certify full XFCE sessions, PolicyKit accept/deny/cancel, sign-in recovery, package-manager upgrades, hardware integrations, or filesystem repair based only on fixtures and offscreen UI tests.

## Publication and feedback

Follow the developer's authorized task scope for publication; prepare reviewable, verified artifacts before any required final approval. Do not infer that CI restrictions prohibit source synchronization or manual packaging. Inspect existing assets before changing a release, do not silently replace previously downloaded files, and keep version/changelog/checksums aligned. Never report a release asset as uploaded without a verified upload result.

No telemetry, background self-update, automatic cleanup, or automatic feedback upload runs on user machines. Reports default to redacted summaries; raw output requires explicit fresh opt-in. Preserve developer attribution while excluding private computer information from source examples, screenshots and public release evidence.
