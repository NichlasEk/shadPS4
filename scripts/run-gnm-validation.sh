#!/usr/bin/env bash
set -euo pipefail
root=$(CDPATH='' cd -- "$(dirname -- "$0")/.." && pwd)
export SHADPS4_GNM_VALIDATION=1
exec "${SHADPS4_BINARY:-$root/build-validation/shadps4}" "$@"
