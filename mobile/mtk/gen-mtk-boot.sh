#!/bin/bash
# gen-mtk-boot.sh — MediaTek boot.img generator
set -euo pipefail
die() { echo "[ERR] $*" >&2; exit 1; }
: "${MKBOOTFS:=mkbootfs}"
: "${MKBOOTIMG:=mkbootimg}"

usage() { echo "gen-mtk-boot.sh --kernel IMG --ramdisk DIR|CPIO --dtb DTB --out BOOT.IMG [--base ADDR]"; exit 0; }

KERNEL=""; RAMDISK=""; DTB=""; OUT="out/mtk-boot.img"; BASE="0x40000000"; PAGESIZE="4096"
while [ $# -gt 0 ]; do
    case "$1" in
        --kernel)  KERNEL="$2"; shift 2 ;;
        --ramdisk) RAMDISK="$2"; shift 2 ;;
        --dtb)     DTB="$2"; shift 2 ;;
        --out)     OUT="$2"; shift 2 ;;
        --base)    BASE="$2"; shift 2 ;;
        --help|-h) usage ;;
        *) die "Unknown: $1" ;;
    esac
done
test -n "$KERNEL" || die "--kernel required"
test -n "$RAMDISK" || die "--ramdisk required"
test -f "$KERNEL" || die "Kernel not found: $KERNEL"
mkdir -p "$(dirname "$OUT")"
if [ -d "$RAMDISK" ]; then
    RAMDISK_CPIO="$(mktemp /tmp/mtk-rd-XXXXXX.cpio.gz)"
    (cd "$RAMDISK" && find . | sort | "$MKBOOTFS" - | gzip > "$RAMDISK_CPIO")
    RAMDISK="$RAMDISK_CPIO"
fi
"$MKBOOTIMG" --kernel "$KERNEL" --ramdisk "$RAMDISK" --dtb "$DTB" \
    --cmdline "console=ttyS0,115200n8 root=/dev/ram0 rw init=/init" \
    --base "$BASE" --pagesize "$PAGESIZE" \
    --kernel_offset 0x00008000 --ramdisk_offset 0x02000000 \
    --tags_offset 0x00000100 --header_version 4 -o "$OUT"
echo "[ OK ] Wrote $OUT ($(du -h "$OUT" | cut -f1))"
