# Canonical reusable image: ci-foundry/cpp-qt5-ubuntu2204:qt5-ubuntu22.04-v1
# Recipe only: image must be built, scanned, and digest-pinned before use.
FROM ubuntu:22.04
ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update \
 && apt-get install -y --no-install-recommends ca-certificates cmake g++ qtbase5-dev dpkg-dev file desktop-file-utils pkg-config \
 && rm -rf /var/lib/apt/lists/*
RUN cmake --version && g++ --version && pkg-config --modversion Qt5Core
