# Project privacy

Developer attribution is intentional: the package credits its developer. Repository ownership and public development activity are not confidential machine diagnostics.

Keep private computer information out of the repository:

- Do not commit personal home paths, external-drive layouts, hostnames, device serial numbers, UUIDs, private network addresses, credentials or tokens.
- Do not upload real diagnostic reports, environment files, local configuration, Wine registries, installation logs or debug traces. Use synthetic examples with generic paths.
- Review exports before sharing. Commands display real information about the computer where the app runs; reports are saved locally and are not uploaded automatically.
- Ignore rules reduce accidental staging; they do not remove files already tracked or guarantee that arbitrary filenames are protected.

Before public visibility, review the tracked files and complete Git history, branches, tags, issues, pull requests and CI artifacts. Review new contributions for private data. Publishing the source does not publish the original reference attachments or local files unless they are explicitly added or uploaded.

The initial source review used private reference scripts without copying their contents or diagnostic logs into this repository. No automatic telemetry or report-upload feature is implemented.
