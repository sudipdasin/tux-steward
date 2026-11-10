# Validation — 0.1.1

Executed in the development container:

- 20 unittest cases passed, including real `df -hT` and real Python subprocess integration tests.
- Live output arrives before command completion; stderr and nonzero exit codes are retained.
- A 1,000,000-character producer retains at most 250,000 characters and marks truncation.
- Timeouts preserve partial output and finish promptly when child processes hold stdout open or the producer closes stdout before exiting.
- Split UTF-8, invalid bytes, callback failures, literal argument handling, missing tools, mutation confirmation and Wayland gating are covered.
- Python byte compilation passed.

Not executed: rendered GUI smoke testing (no display server), real Linux Mint/XFCE desktop testing, privileged mutations or GitHub Actions for this revision. Remote CI is configured for Python 3.10 and 3.12, but configuration is not evidence of a passed remote run.

The project repository is private. No public release has been published. The live UI queue can discard bursts; exported reports retain the independent bounded output tail. Elevated processes may survive a timeout; this does not roll back system changes.
