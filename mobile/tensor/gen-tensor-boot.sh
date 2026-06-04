#!/bin/bash
# gen-tensor-boot.sh — Google Tensor / Pixel boot.img generator
set -euo pipefail
die() { echo "[ERR] $*" >&2; exit 1; }
: "${MKBOOTIMG:=mkbootimg}"
: "${AVBTOOL:=avbtool}"

usage() { echo "gen-tensor-boot.sh --kernel IMG --ramdisk CPIO --dtb DTB --out BOOT.IMG [--signing-key KEY]"; exit 0; }

KERNEL=""; RAMDISK=""; DTB=""; OUT="out/tensor-boot.img"; SIGNING_KEY=""
while [ $# -gt 0 ]; do
    case "$1" in
        --kernel)     KERNEL="$2"; shift 2 ;;
        --ramdisk)    RAMDISK="$2"; shift 2 ;;
        --dtb)        DTB="$2"; shift 2 ;;
        --out)        OUT="$2"; shift 2 ;;
        --signing-key) SIGNING_KEY="$2"; shift 2 ;;
        --help|-h)    usage ;;
        *) die "Unknown: $1" ;;
    esac
done
test -n "$KERNEL" || die "--kernel required"
test -n "$RAMDISK" || die "--ramdisk required"
test -n "$DTB" || die "--dtb required"
test -f "$KERNEL" || die "Kernel not found: $KERNEL"
mkdir -p "$(dirname "$OUT")"
"$MKBOOTIMG" --kernel "$KERNEL" --ramdisk "$RAMDISK" --dtb "$DTB" \
    --cmdline "console=ttyMSM0,115200n8 quiet root=/dev/sda1 rw" \
    --pagesize 4096 --base 0x00000000 \
    --kernel_offset 0x00008000 --ramdisk_offset 0x02000000 \
    --dtb_offset 0x01F00000 --tags_offset 0x01E00000 \
    --header_version 4 -o "$OUT"
if [ -n "$SIGNING_KEY" ] && [ -f "$SIGNING_KEY" ] && [ -x "$AVBTOOL" ]; then
    AVBOUT="${OUT%.img}-avb.img"
    "$AVBTOOL" add_hash_footer --image "$OUT" --partition_name boot \
        --key "$SIGNING_KEY" --algorithm SHA256_RSA4096 \
        --prop com.android.build.boot.os_version:13 \
        -o "$AVBOUT"
    mv "$AVBOUT" "$OUT"
    echo "[ OK ] AVB-signed $OUT"
else
    echo "[ OK ] Wrote $OUT ($(du -h "$OUT" | cut -f1))"
fi
