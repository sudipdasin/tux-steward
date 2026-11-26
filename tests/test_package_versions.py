#!/usr/bin/env python3
"""Real dpkg comparisons for historical prerelease/stable package schemes."""
import subprocess
import sys

candidate = sys.argv[1]
historical = (
    "3.0.0~alpha1", "3.0.0~alpha2", "3.0.0~alpha3",
    "3.0.0-alpha.1", "3.0.0-alpha3", "3.0.0", "3.0.0-1",
    "3.0.0+local1", "3.0.0+mint22.3", "2.4.0~beta1",
)
for installed in historical:
    subprocess.run(["dpkg", "--compare-versions", candidate, "gt", installed], check=True)
subprocess.run(["dpkg", "--compare-versions", candidate, "lt", "3.1.0"], check=True)
subprocess.run(["dpkg", "--compare-versions", candidate, "lt", "3.2.0"], check=True)
print(f"{candidate}: upgrades all {len(historical)} historical version schemes; stable/future versions remain newer")
