#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Run inside the isolated VM; deliberately corrupt independent release inputs."""
import copy
import hashlib
import importlib.util
from pathlib import Path
import tempfile
import unittest

spec = importlib.util.spec_from_file_location(
    "release_assets", Path(__file__).parents[1] / "tools/verify-release-assets.py")
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)


class ReleaseAssets(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.folder = Path(self.temp.name)
        self.name = "preview_3.0.0.alpha1_amd64.deb"
        (self.folder / self.name).write_bytes(b"hello")
        self.hello_hash = "2cf24dba5fb0a30e26e83b2ac5b9e29e1b161e5c1fa7425e73043362938b9824"
        (self.folder / "SHA256SUMS").write_text(self.hello_hash + "  " + self.name + "\n")
        self.commit = "1" * 40
        self.tag = {"ref": "refs/tags/v3.0.0-alpha.1",
                    "object": {"type": "commit", "sha": self.commit}}
        self.release = {"url": "https://api.github.com/repos/example/app/releases/1",
                        "tag_name": "v3.0.0-alpha.1", "draft": False, "prerelease": True,
                        "published_at": "2026-11-26T00:00:00Z", "assets": []}
        self.refresh_metadata()

    def refresh_metadata(self):
        self.release["assets"] = [
            {"name": path.name, "size": len(path.read_bytes()), "state": "uploaded",
             "digest": "sha256:" + hashlib.sha256(path.read_bytes()).hexdigest()}
            for path in self.folder.iterdir()]

    def check(self):
        return module.verify(self.folder, self.release, self.tag,
                             "example/app", "v3.0.0-alpha.1", self.commit)

    def test_valid_published_assets(self):
        self.assertEqual(self.check()["verified_assets"], 2)

    def test_wrong_tag_target(self):
        self.tag["object"]["sha"] = "2" * 40
        with self.assertRaisesRegex(ValueError, "wrong tag target"):
            self.check()

    def test_draft_is_not_publication(self):
        self.release["draft"] = True
        with self.assertRaisesRegex(ValueError, "not published"):
            self.check()

    def test_wrong_repository(self):
        self.release["url"] = "https://api.github.com/repos/other/app/releases/1"
        with self.assertRaisesRegex(ValueError, "wrong repository"):
            self.check()

    def test_maturity_mismatch(self):
        self.release["prerelease"] = False
        with self.assertRaisesRegex(ValueError, "wrong release maturity"):
            self.check()

    def test_same_size_tampering(self):
        (self.folder / self.name).write_bytes(b"jello")
        with self.assertRaisesRegex(ValueError, "digest differs"):
            self.check()

    def test_wrong_upload_size(self):
        self.release["assets"][0]["size"] += 1
        with self.assertRaisesRegex(ValueError, "size differs"):
            self.check()

    def test_duplicate_remote_filename(self):
        self.release["assets"].append(copy.deepcopy(self.release["assets"][0]))
        with self.assertRaisesRegex(ValueError, "duplicate remote"):
            self.check()

    def test_unexpected_local_file(self):
        (self.folder / "unexpected").write_bytes(b"extra")
        with self.assertRaisesRegex(ValueError, "names differ"):
            self.check()

    def test_incomplete_upload(self):
        self.release["assets"][0]["state"] = "new"
        with self.assertRaisesRegex(ValueError, "upload is incomplete"):
            self.check()

    def test_original_filename_after_github_normalization(self):
        (self.folder / "SHA256SUMS").write_text(
            self.hello_hash + "  preview_3.0.0~alpha1_amd64.deb\n")
        self.refresh_metadata()
        with self.assertRaisesRegex(ValueError, "missing public asset"):
            self.check()

    def test_path_escape_in_checksum(self):
        (self.folder / "SHA256SUMS").write_text(self.hello_hash + "  ../escape\n")
        self.refresh_metadata()
        with self.assertRaisesRegex(ValueError, "invalid asset filename"):
            self.check()

    def test_symlink_asset_refused(self):
        (self.folder / "linked").symlink_to(self.folder / self.name)
        with self.assertRaisesRegex(ValueError, "only regular files"):
            self.check()


if __name__ == "__main__":
    unittest.main()
