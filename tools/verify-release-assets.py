#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Compare saved GitHub release/tag metadata with a verified local asset folder.

Offline development tooling only. No network, upload, application launch or CI
dispatch is performed. For annotated tags, supply dereferenced commit metadata.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import sys


def require(condition, message):
    if not condition:
        raise ValueError(message)


def safe_name(name):
    require(isinstance(name, str) and bool(name) and name not in (".", "..")
            and not any(c in name for c in "/\\\r\n\0"), "invalid asset filename")
    return name


def sha256(path):
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def verify(folder, release, tag_object, repository, tag, source_commit,
           maturity="prerelease"):
    require(re.fullmatch(r"[A-Za-z0-9_.-]+/[A-Za-z0-9_.-]+", repository),
            "invalid repository")
    require(re.fullmatch(r"[a-f0-9]{40}", source_commit), "invalid source commit")
    require(release.get("url", "").startswith(
        f"https://api.github.com/repos/{repository}/releases/"), "wrong repository")
    require(release.get("tag_name") == tag, "wrong release tag")
    require(release.get("draft") is False and bool(release.get("published_at")),
            "release is not published")
    require(release.get("prerelease") is (maturity == "prerelease"),
            "wrong release maturity")
    require(tag_object.get("ref") == "refs/tags/" + tag, "wrong tag ref")
    require(tag_object.get("object", {}).get("type") == "commit",
            "tag must be resolved to a commit")
    require(tag_object["object"].get("sha") == source_commit, "wrong tag target")

    folder = Path(folder)
    require(folder.is_dir(), "asset folder is missing")
    local = {}
    for path in folder.iterdir():
        safe_name(path.name)
        require(path.is_file() and not path.is_symlink(),
                "asset folder must contain only regular files")
        local[path.name] = path
    assets = release.get("assets")
    require(isinstance(assets, list) and bool(assets), "release has no assets")
    remote = {}
    for asset in assets:
        name = safe_name(asset.get("name"))
        require(name not in remote, "duplicate remote asset name")
        remote[name] = asset
    require(set(local) == set(remote), "local and remote asset names differ")
    hashes = {}
    for name, path in sorted(local.items()):
        asset = remote[name]
        require(asset.get("state") == "uploaded", "asset upload is incomplete: " + name)
        require(type(asset.get("size")) is int and asset["size"] == path.stat().st_size,
                "asset size differs: " + name)
        hashes[name] = sha256(path)
        require(asset.get("digest") == "sha256:" + hashes[name],
                "asset digest differs: " + name)

    require("SHA256SUMS" in local, "canonical SHA256SUMS is missing")
    manifest = {}
    for line in local["SHA256SUMS"].read_text(encoding="utf-8").splitlines():
        match = re.fullmatch(r"([a-fA-F0-9]{64}) [ *](.+)", line)
        require(match is not None, "invalid checksum record")
        name = safe_name(match[2])
        require(name not in manifest, "duplicate checksum filename")
        require(name in hashes, "checksum names a missing public asset: " + name)
        require(match[1].lower() == hashes[name], "checksum content differs: " + name)
        manifest[name] = match[1].lower()
    payloads = {name for name in local if not name.startswith("SHA256SUMS")}
    require(payloads and payloads.issubset(manifest), "checksum list omits payload assets")
    return {"verified_assets": len(assets), "verified_payloads": len(payloads),
            "repository": repository, "tag": tag, "source_commit": source_commit}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--assets", required=True, type=Path)
    parser.add_argument("--release-json", required=True, type=Path)
    parser.add_argument("--tag-json", required=True, type=Path)
    parser.add_argument("--repository", required=True)
    parser.add_argument("--tag", required=True)
    parser.add_argument("--source-commit", required=True)
    parser.add_argument("--maturity", choices=("prerelease", "stable"), default="prerelease")
    args = parser.parse_args()
    try:
        result = verify(args.assets, json.loads(args.release_json.read_text()),
                        json.loads(args.tag_json.read_text()), args.repository,
                        args.tag, args.source_commit, args.maturity)
    except (OSError, ValueError, TypeError, KeyError) as error:
        print("Release verification failed: " + str(error), file=sys.stderr)
        return 1
    print(json.dumps(result, sort_keys=True))
    return 0


if __name__ == "__main__":
    sys.exit(main())
