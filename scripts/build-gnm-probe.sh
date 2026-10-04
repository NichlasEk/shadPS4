#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
sdk=${OO_PS4_TOOLCHAIN:-/opt/openorbis/OpenOrbis/PS4Toolchain}
export OO_PS4_TOOLCHAIN="$sdk"
mkdir -p build-validation/guest
clang --target=x86_64-pc-freebsd12-elf --sysroot="$sdk" -fPIC -D__ORBIS__ -I"$sdk/include" \
 -c tests/gnm_submission_guest.c -o build-validation/guest/probe.o
ld.lld -pie --script="$sdk/link.x" --eh-frame-hdr -z max-page-size=0x4000 \
 -L"$sdk/lib" "$sdk/lib/crt1.o" build-validation/guest/probe.o -lc -lkernel -lSceGnmDriver \
 -o build-validation/guest/probe.elf
"${OO_CREATE_FSELF:-$sdk/bin/linux/create-fself}" -in=build-validation/guest/probe.elf \
 -out=build-validation/guest/probe.oelf --eboot build-validation/guest/eboot.bin --paid 0x3800000000000011

test -s build-validation/guest/eboot.bin

# The guest C runtime is supplied by the local OpenOrbis SDK.
mkdir -p build-validation/guest/sce_module
cp "$sdk/samples/hello_world/sce_module/libc.prx" build-validation/guest/sce_module/
