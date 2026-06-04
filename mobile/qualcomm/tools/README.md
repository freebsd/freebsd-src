# Qualcomm Snapdragon Tools

Helper utilities for building, packaging, and flashing Mobile OS on Qualcomm Snapdragon SoCs.

## Prerequisites

Install required tools:

```bash
pkg install dtc mkdtimg android-tools  # FreeBSD
# or
apt install device-tree-compiler android-sdk-platform-tools-common  # Debian/Ubuntu
```

## Building Individual Components

### Device Tree Compiler (dtc)

```bash
dtc -I dts -O dtb -o output.dtb input.dts
```

### DTBO (Device Tree Overlay) Packing

```bash
mkdtimg create dtbo.img overlay1.dtbo overlay2.dtbo
```

### Boot Image Creation

```bash
mkbootimg \
    --kernel zImage \
    --ramdisk ramdisk.cpio.gz \
    --dtb output.dtb \
    --cmdline "root=ufs rootdev=0 rootwait console=ttyMSM0,115200n8" \
    --header_version 4 \
    -o boot.img
```

### Fastboot Flashing

```bash
fastboot flash boot boot.img
fastboot flash dtbo dtbo.img
fastboot flash vendor_boot vendor_boot.img
fastboot flash super super.img
fastboot flash vbmeta vbmeta.img --disable-verity --disable-verification
fastboot reboot
```

## Directory Structure

```
mobile/qualcomm/tools/
├── README.md           # This file
└── (additional tools will be added here)
```

## Adding Custom Tools

Place executable scripts or binaries in this directory. Ensure they are marked executable:

```bash
chmod +x tools/your-tool.sh
```

## License

BSD 2-Clause
