# SPDX-FileCopyrightText: 2026 Joshua Jun
# SPDX-License-Identifier: Apache-2.0
FROM ubuntu:24.04
RUN apt-get update -qq && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
    gcc clang cmake make python3 python3-venv ca-certificates \
    gcc-arm-none-eabi libnewlib-arm-none-eabi && rm -rf /var/lib/apt/lists/*
RUN apt-get update -qq && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
    libclang-rt-18-dev && rm -rf /var/lib/apt/lists/*
