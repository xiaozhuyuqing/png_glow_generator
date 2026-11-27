#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")"

cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug \
      -DCMAKE_RUNTIME_OUTPUT_DIRECTORY="$PWD/output"
cmake --build build -j"$(nproc)"
