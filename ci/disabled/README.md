# Hosted CI disabled

GitHub Actions and CircleCI remain disabled indefinitely under the developer's explicit instruction. The files in this directory are archived historical definitions with non-executable filenames. They must not be copied back, dispatched or enabled without renewed developer approval.

Builds, tests, visual checks and packages are produced using the isolated VM workflow in `tools/vm`. Push source with `[skip ci]` in its commit message. Do not assume that a skip marker covers a release-triggered workflow on a different branch; inspect that branch before creating any tag or release.
