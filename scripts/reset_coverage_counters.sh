#!/usr/bin/env bash
set -euo pipefail

BUILD_DIR_INPUT="${1:-$(pwd)}"
BUILD_DIR="$(cd "${BUILD_DIR_INPUT}" && pwd)"

find "${BUILD_DIR}" -name '*.gcda' -delete
