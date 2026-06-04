#!/bin/bash
# gen-tensor-boot.sh — Google Tensor boot.img generator with AVB signing support
set -euo pipefail
die() { echo "[ERR] $*" >&2; exit 1; }
: "${MKBOOTIMG:=mkbootimg}"
: "${AVBTOOL:=avbtool}"

usage() { echo "gen-tensor-boot.sh --kernel IMG --ramdisk CPIO --dtb DTB --avbkey KEY --out BOOT.IMG [--avb-metadata META]"; exit 0; }

KERNEL=""; RAMDISK=""; DTB=""; AVBKEY=""; OUT="out/tensor-boot.img"; AVB_META=""
while [ $# -gt 0 ]; do
    case "$1" in
        --kernel)     KERNEL="$2"; shift 2 ;;
        --ramdisk)    RAMDISK="$2"; shift 2 ;;
        --dtb)        DTB="$2"; shift 2 ;;
        --avbkey)     AVBKEY="$2"; shift 2 ;;
        --out)        OUT="$2"; shift 2 ;;
        --avb-metadata) AVB_META="$2"; shift 2 ;;
        --help|-h)    usage ;;
        *) die "Unknown: $1" ;;
    esac
done
test -n "$KERNEL" || die "--kernel required"
test -n "$RAMDISK" || die "--ramdisk required"
test -f "$KERNEL" || die "Kernel not found: $KERNEL"
mkdir -p "$(dirname "$OUT")"
"$MKBOOTIMG" --kernel "$KERNEL" --ramdisk "$RAMDISK" --dtb "$DTB" \
    --cmdline "console=ttyMSM0,115200n8 quiet root=/dev/sda1 rw" \
    --pagesize 4096 --base 0x00000000 \
    --kernel_offset 0x00008000 --ramdisk_offset 0x02000000 \
    --dtb_offset 0x01F00000 --tags_offset 0x01E00000 \
    --header_version 4 -o "$OUT"
if [ -n "$AVBKEY" ] && [ -f "$AVBKEY" ]; then
    KEYDIR="$(dirname "$AVBKEY")"
    "$AVBTOOL" add_hash_image --image "$OUT" --partition_name boot \
        --hash_algorithm sha256 --key "$AVBKEY" --algorithm SHA256_RSA4096 \
        --chain_metadata "avb: tensor-boot" ${AVB_META:+--avb_metadata "$AVB_META"}
    echo "[ OK ] Wrote AVB-signed $OUT ($(du -h "$OUT" | cut -f1))"
else
    echo "[ OK ] Wrote unsigned $OUT ($(du -h "$OUT" | cut -f1))"
fi