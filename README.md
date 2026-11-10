# ◉ Tux Steward

**A calmer way to care for Linux.** A native, lightweight Linux desktop toolkit, starting with Linux Mint / XFCE. Version 0.1.1 is a development alpha, intended for an initially private GitHub repository and eventual public development under GPL-3.0-or-later.

## Run on your desktop

Requires Python 3.10+ and Tk. On Mint/Ubuntu, install Tk if missing:

```sh
sudo apt install python3-tk
./run.sh
```

Run as your normal desktop user. Never start the GUI with sudo. Administrator actions invoke PolicyKit (`pkexec`) and require a running desktop authentication agent. No pip dependencies are needed to launch from source. Optional packaging: `python3 -m pip install --user .` then `tux-steward` (use a virtual environment on systems that require one).

## Available now

Seven tabs group 25 tools: Diagnostics, Storage, Maintenance, Performance, Input, Gaming, and Schedules. Tools include storage/mount inspection, journal diagnostics, failed services, APT removal simulation and cache cleaning, journal retention, memory/zRAM/TLP inspection, reversible XFCE compositor controls, X11 device inspection, Wine/Lutris/Vulkan checks, and timer inspection plus enabling the distribution SSD trim timer. File and Help menus provide report export and project information.

Every tool displays its exact command before execution. Missing dependencies are explained. Operations run in a background worker with live output, exit codes, and output retained for JSON report export. Search tools by name, category or description. No tools run on startup. No shell evaluation, password collection, root GUI, or execution of the legacy scripts.

## Boundaries

Input toggling, game installation/uninstallation, custom weekly schedules, and filesystem repair are **not implemented yet**. Their tabs provide current diagnostics and explain future work. Read [source review](docs/SOURCE_REVIEW.md) and [roadmap](docs/ROADMAP.md). No broad deletion, forced process termination, automatic third-party repositories, kernel removal, or security-update disabling is carried forward. Journal pruning permanently removes archived logs after explicit confirmation. SSD trimming requires supported filesystems. Command availability and journal permissions vary across distributions; the GUI is not proof of hardware compatibility.

Commands have a 120-second timeout. A timeout can leave an operation partially applied; inspect system state before retrying. Output streams as the tool flushes it. The live preview can skip bursts when its bounded queue fills. Reports retain the last 250,000 characters per task; session history retains the latest 20 tasks. Stderr is merged into stdout. A timeout attempts to stop only the task’s process group; elevated descendants may resist termination and remain running. Reports can contain identifying system details. Export is local and never uploaded automatically.

## Development

```sh
python3 -m unittest discover -s tests -v
python3 -m compileall -q tux_steward
```

Architecture: `core.py` owns the action catalog and execution boundary; `runner.py` handles bounded subprocess streaming and deadlines; `app.py` owns the native Tk interface and thread-to-UI queue. Add actions with descriptions, dependency checks, mutation classification, and tests. See CONTRIBUTING.md. Keep semantic versions and CHANGELOG.md current. Do not add personal logs or hard-coded home/media paths.
