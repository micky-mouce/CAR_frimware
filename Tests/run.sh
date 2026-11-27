#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
build_dir=$(mktemp -d)
trap 'rm -rf "$build_dir"' EXIT HUP INT TERM
# CMSIS shim only supplies host compiler definitions; no MCU init is run.
cc=${CC:-gcc}
"$cc" -std=gnu99 -Wall -Wextra -Werror -Wno-int-to-pointer-cast -Wno-unknown-pragmas \
    -DSTM32F40_41xxx -DUSE_STDPERIPH_DRIVER \
    -ITests/support -IHardware -IUser -IVendor/CMSIS/Include \
    -IVendor/Device/Include -IVendor/Device/StdPeriph_Driver/inc \
    -ffunction-sections -fdata-sections \
    Tests/test_firmware.c Hardware/Motion.c Hardware/PID.c Hardware/Board.c \
    -Wl,--gc-sections -o "$build_dir/test_firmware"
"$build_dir/test_firmware"
