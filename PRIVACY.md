# Project privacy

Developer attribution is intentional: the package credits its developer. Repository ownership and public development activity are not confidential machine diagnostics.

Keep private computer information out of the repository:

- Do not commit personal home paths, external-drive layouts, hostnames, device serial numbers, UUIDs, private network addresses, credentials or tokens.
- Do not upload real diagnostic reports, environment files, local configuration, Wine registries, installation logs or debug traces. Use synthetic examples with generic paths.
- Review exports before sharing. Commands display real information about the computer where the app runs; reports are saved locally and are not uploaded automatically.
- Ignore rules reduce accidental staging; they do not remove files already tracked or guarantee that arbitrary filenames are protected.

Before public visibility, review the tracked files and complete Git history, branches, tags, issues, pull requests and CI artifacts. Review new contributions for private data. Publishing the source does not publish the original reference attachments or local files unless they are explicitly added or uploaded.

The initial source review used private reference scripts without copying their contents or diagnostic logs into this repository. No automatic telemetry or report-upload feature is implemented.

## Process report defaults

Task Manager's standard export omits usernames and command lines. A detailed report requires an explicit opt-in for each export and may contain local paths or secrets in arguments. Process names, PIDs, timestamps and activity metrics remain in standard reports; standard does not mean anonymous. No report is uploaded automatically.


## Startup undo data

Startup changes create owner-only undo records under the user's Tux Steward configuration directory. These records contain original desktop-entry contents and can include local paths or command arguments. Keep this directory private and do not add its contents to source control or support attachments. No startup data is uploaded automatically. Developer attribution remains intentional.

## Maintenance report defaults

Maintenance exports now require a preview before saving. Summary mode uses an allowlist of known tool names, exit codes, durations and truncation flags; it does not copy diagnostic text or command arguments. Unknown action labels become “Unrecognized task”. Full recorded details require explicit consent on each export. This is omission, not regular-expression anonymization: summaries can still reveal tool activity and outcomes. Detailed reports can include arbitrary sensitive output. Review before sharing; neither mode uploads anything. Live output and in-memory session records remain detailed so the local user can diagnose problems.
