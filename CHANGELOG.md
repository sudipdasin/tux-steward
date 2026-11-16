# Changelog

## 0.3.0 — 2026-11-16

Added **Tux Task Manager**, a separate lightweight process-monitor application alongside Tux Steward's existing system tools. It reads `/proc`, shows CPU, resident memory, PID, user and process state, supports search and sorting, and refreshes only while visible. End Task and Force stop are limited to the current user's processes, with PID-reuse checks and separate confirmation. Added local JSON export with a privacy warning and atomic save. Existing Tux Steward tools and the bounded, shell-free command runner remain available.

## 0.2.0 — 2026-11-15

Replaced the Python/Tk implementation, tests and packaging with C++17, Qt Widgets and CMake. Retained all 25 tools, seven grouped tabs, search, live bounded output, PolicyKit authentication, default-cancel command review and JSON reports. Added scrollable tool pages, resizable output, atomic report export, desktop installation and C++ subprocess/GUI tests. No Python runtime remains.

## Privacy cleanup — 2026-11-10

Removed account-specific references from validation documentation. Developer attribution is retained. Added exclusions and guidance to keep private machine data, local configuration and diagnostic exports out of source control.

## 0.1.1 — 2026-11-10

Live command output, case-insensitive tool search, bounded output/history memory, partial-output retention on timeout, and deadline handling for child-held output pipes. Added real subprocess regression tests and Python 3.10/3.12 CI matrix. Desktop visual testing remains pending.

## 0.1.0 — 2026-11-10

Initial development alpha: native dark GUI, seven grouped tabs, 25 explicit actions, PolicyKit authentication per admin action, command review, worker queue, dependency checks, JSON report export, source audit, tests and CI configuration.
