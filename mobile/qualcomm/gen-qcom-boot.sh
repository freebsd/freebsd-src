#!/bin/bash
# gen-qcom-boot.sh — Qualcomm boot.img generator (mkbootimg with QCOM offsets)
set -euo pipefail
die() { echo "[ERR] $*" >&2; exit 1; }
: "${MKBOOTIMG:=mkbootimg}"

usage() { echo "gen-qcom-boot.sh --kernel IMG --ramdisk CPIO --dtb DTB --out BOOT.IMG"; exit 0; }

KERNEL=""; RAMDISK=""; DTB=""; OUT="out/qcom-boot.img"
while [ $# -gt 0 ]; do
    case "$1" in
        --kernel)  KERNEL="$2"; shift 2 ;;
        --ramdisk) RAMDISK="$2"; shift 2 ;;
        --dtb)     DTB="$2"; shift 2 ;;
        --out)     OUT="$2"; shift 2 ;;
        --help|-h) usage ;;
        *) die "Unknown: $1" ;;
    esac
done
test -n "$KERNEL" || die "--kernel required"
test -n "$RAMDISK" || die "--ramdisk required"
test -f "$KERNEL" || die "Kernel not found: $KERNEL"
mkdir -p "$(dirname "$OUT")"
"$MKBOOTIMG" --kernel "$KERNEL" --ramdisk "$RAMDISK" --dtb "$DTB" \
    --cmdline "console=ttyMSM0,115200n8 root=/dev/sda1 rw" \
    --pagesize 4096 --base 0x00000000 \
    --kernel_offset 0x00008000 --ramdisk_offset 0x02000000 \
    --dtb_offset 0x01F00000 --tags_offset 0x01E00000 \
    --header_version 4 -o "$OUT"
echo "[ OK ] Wrote $OUT ($(du -h "$OUT" | cut -f1))"
