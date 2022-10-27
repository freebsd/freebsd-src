#!/bin/bash
source shiny_tools/source.sh
if [[ $(/usr/bin/id -u) -ne 0 ]]; then
    echo "Run as root"
    exit
fi
# We capture the loop device we want to map to avoid a TOCTOU issues with assigning a loop
LOOP_DEVICE=$(losetup -f) &&
losetup $LOOP_DEVICE ../qemu/sdcard.img &&
echo "Mounted loop back device at $LOOP_DEVICE"

