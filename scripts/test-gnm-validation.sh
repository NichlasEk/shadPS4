#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p build-validation
${CXX:-clang++} -std=c++23 -g -O1 -fsanitize=address,undefined -Isrc tests/test_gnm_validation.cpp -o build-validation/test-gnm-validation
build-validation/test-gnm-validation
