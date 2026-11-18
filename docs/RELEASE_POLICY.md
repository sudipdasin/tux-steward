# Release and forward-continuity policy

The public main branch is the single source of truth. All future versions start from main and inherit existing features and their regression suites. CI verifies ancestry from the tested 0.9 foundation, compiles with GCC and Clang in separate Ubuntu hosted VMs, and runs every CTest suite plus an upgrade from the checksum-pinned 0.9 DEB, GUI startup and removal. PR review must preserve working behavior or explain intentional changes. Ancestry alone cannot prove semantic compatibility; regressions need tests and user feedback.

Published tags are historical checkpoints, not branches to update automatically. Fixes flow forward on main. Maintained older branches may receive reviewed backports with their own validation; automation does not rewrite old tags or copy untested code into them. No proprietary repository or cross-repository synchronization is configured by this policy.

Only an owner-created version tag on main authorizes public beta publication. The release workflow validates the tag, ancestry and full dual-VM matrix, then rebuilds/packages on Ubuntu 22.04 and checks dependencies, versions, build-path privacy, installation, GUI launch and removal. Publication refuses overwriting existing assets. If publication fails halfway through, inspect the existing release and repair deliberately; do not replace previously downloaded assets silently. Credentials have job-scoped permissions; no secrets are exposed to pull-request code. The GitHub VM checks are automatic checks, not server-enforced branch protection.

Public beta downloads are unsigned DEB and TGZ packages, with SHA256SUMS for integrity. TGZ requires compatible Qt/system libraries. No automatic update, telemetry or feedback upload runs on users' computers. Open issue templates from Help to send reviewed feedback. Developer identity remains; personal machine logs, local paths and configuration do not belong in commits.

1.0-beta is for evaluation. Real Mint/XFCE and Wayland sessions, PolicyKit accept/deny/cancel, sign-in recovery and Bluetooth/GPU hardware remain incomplete. Do not label a stable release until these gates have recorded results.
