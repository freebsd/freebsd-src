# BSD Mobile Build SoC System

Multi-SoC build orchestration and device flashing infrastructure for BSD-based Mobile OS.

## Supported SoCs

### MediaTek Dimensity
| SoC | Models | Supported Boards | Vendor Notes |
|-----|--------|----------------|--------------|
| MT6893 | Dimensity 1100 | X01BD (Redmi Note 11 Pro), mojito (Redmi Note 10 Pro) | clang + aarch64-elf- toolchain |
| MT6877 | Dimensity 1100-E | veux (POCO X4 Pro), Redmi Note 11S | clang + aarch64-elf- toolchain |
| MT6833 | Dimensity 810 | evergo (POCO M4 Pro), Redmi 10A | clang + aarch64-elf- toolchain |
| MT6895 | Dimensity 8100 | plato (Xiaomi 12T), Redmi K50 Ultra | clang + aarch64-elf- toolchain |
| MT6983 | Dimensity 9000 | cupid (Xiaomi 12 Pro), Vivo X80 | clang + aarch64-elf- toolchain |
| MT6985 | Dimensity 9000+ | n8q (Xiaomi 13 Pro) | clang + aarch64-elf- toolchain |
| MT6989 | Dimensity 9200 | houjiao (Xiaomi 14) | clang + aarch64-elf- toolchain |
| MT6991 | Dimensity 9200+ | v2309 (Vivo X100 Pro) | clang + aarch64-elf- toolchain |

### Qualcomm Snapdragon
| SoC | Models | Supported Boards | Vendor Notes |
|-----|--------|----------------|--------------|
| SM8450 | Snapdragon 8 Gen 1 | lavender (Galaxy S22), lemonade (OnePlus 10 Pro) | clang + aarch64-elf- required |
| SM8350 | Snapdragon 888 | lilac (Zenfone 8), ROG Phone 5 | clang + aarch64-elf- toolchain |
| SM7450 | Snapdragon 7 Gen 1 | zen22 (Zenfone 8 Flip), Surface Pro X | clang + aarch64-elf- toolchain |
| SM7350 | Snapdragon 732G | rhodium (Moto G200) | clang + aarch64-elf- toolchain |
| SM6375 | Snapdragon 695 | pong (Nothing Phone 2) | clang + aarch64-elf- toolchain |
| SM6450 | Snapdragon 695 | ocean (generic reference) | clang + aarch64-elf- toolchain |
| SM6625 | Snapdragon 695 variant | generic_lp5 | clang + aarch64-elf- toolchain |
| MSM8998 | Snapdragon 845 | dumpling (OnePlus 5T) | clang + aarch64-elf- toolchain |

### Samsung Exynos
| SoC | Models | Supported Boards | Vendor Notes |
|-----|--------|----------------|--------------|
| S5E9925 | Exynos 2200 | lavender (Galaxy S22), g0 (S22+), tq (S22U) | aarch64-elf- or clang |
| S5E9810 | Exynos 9820 | exynos2100 (Galaxy S21), o1s (S21U) | aarch64-elf- or clang |
| S5E8845 | Exynos 1330 | a54x (Galaxy A54) | aarch64-elf- or clang |
| S5E8535 | Exynos 850 | a14x (Galaxy A14) | aarch64-elf- or clang |
| S5E9830 | Exynos 990 | s5e9830 (Galaxy S20FE), c2s (Note20U) | aarch64-elf- or clang |
| S5E9825 | Exynos 9825 | c2 (Note10+), beyond (S10) | aarch64-elf- or clang |
| S5E5510 | Exynos 7570 | a04e (Galaxy A04) | aarch64-elf- or clang |

### Google Tensor
| SoC | Models | Supported Boards | Vendor Notes |
|-----|--------|----------------|--------------|
| TENSOR_REDFIN | Tensor | oriole (Pixel 6), raven (Pixel 6 Pro), bluejay-alt (Pixel 6a) | Same as Samsung toolchain |
| TENSOR_BLUEJAY | Tensor G2 | panther (Pixel 7), cheetah (Pixel 7 Pro), bluejay (Pixel 7a) | Same as Samsung toolchain |
| TENSOR_ZUMA | Tensor G3 | shiba/felix (Pixel 8), husky (Pixel 8 Pro) | Same as Samsung toolchain |

## Build Instructions

### Prerequisites
- clang (LLVM) toolchain
- aarch64-elf- cross-compiler
- dtc (device tree compiler)
- mkbootimg, mkbootfs
- adb, fastboot (for flashing)
- Heimdall (for Samsung devices)
- SP Flash Tool (for MediaTek download mode)

### Quick Start

```bash
# List all supported SoCs
make soc-list

# List boards for a specific SoC
make soc-board SOC=MT6893

# Build for a specific board
make build SOC=MT6893 BOARD=X01BD

# Build with custom output
./build.sh --soc MT6893 --board X01BD --output out/MT6893/ --config default
```

### Build Artifacts

| Artifact | Description |
|----------|-------------|
| boot.img | Kernel + ramdisk |
| dtbo.img | Device tree blob overlays |
| super.img | System partition (dynamic partitions) |
| vbmeta.img | Verified boot metadata (Qualcomm) |
| vendor_boot.img | Vendor modules (Qualcomm) |

## Flash Instructions

### Fastboot Mode
```bash
# Basic flash
./flash.sh --soc MT6893 --mode fastboot --output out/MT6893/

# Flash with wipe
./flash.sh --soc MT6893 --mode fastboot --wipesystem --wipedata

# Flash to specific slot
./flash.sh --soc SM8450 --mode fastboot --slot A
```

### Recovery Sideload
```bash
./flash.sh --soc MT6893 --mode recovery --output out/MT6893/
```

### MediaTek Download Mode
```bash
./flash.sh --soc MT6893 --mode download --output out/MT6893/
```

## Bootloader Unlock Instructions

### MediaTek
1. Enable Developer Options and OEM Unlocking
2. Enable "MTK Sec Bypass" in Engineering Mode (*#*#8888#*#*)
3. Reboot to fastboot: `adb reboot fastboot`
4. Unlock: `fastboot flashing unlock`

### Qualcomm
1. Enable OEM Unlocking in Developer Options
2. Reboot to bootloader: `adb reboot bootloader`
3. Unlock: `fastboot flashing unlock`
4. Some devices require EDL mode with firehose programmer

### Samsung
1. Enable OEM Unlocking (Samsung Unlock) in Developer Options
2. Generate unlock token via Samsung Developer website
3. Reboot to download mode: `adb reboot download`
4. Flash unlock token via Odin/Heimdall

### Google (Pixel)
1. Enable OEM Unlocking in Developer Options
2. Reboot to bootloader: `adb reboot bootloader`
3. Unlock: `fastboot flashing unlock`
4. Confirm on device screen

## Known Issues

### MediaTek
- MT6893: 32-bit bootloader requires 32-bit kernel offset in boot.img
- MT6983: Modem firmware blob extraction requires mtk_client
- MT6991: GPU thermal throttling may occur under load

### Qualcomm
- SM8450: Requires clang 15+ for LLVM-MVF integration
- SM8350: AVB 2.0 may reject unsigned boot images
- MSM8998: Legacy bootloader may not support fastboot v2

### Samsung
- S5E9925: Knox warranty void on bootloader unlock
- S5E8535: RKP (Real-time Kernel Protection) may block unsigned kernel
- All Exynos: Samsung USB driver issues on Windows

### Google
- TENSOR_ZUMA: Verified boot enforcement stricter on Pixel 8
- All: Factory reset required after bootloader unlock

## Toolchain Selection

The build system automatically selects the correct toolchain per vendor:

| Vendor | Toolchain | Notes |
|--------|-----------|-------|
| MediaTek | clang + aarch64-elf- | Modern Dimensity support |
| Qualcomm | clang + aarch64-elf- | Required for SM8450+ |
| Samsung | aarch64-elf- or clang | Either toolchain works |
| Google | clang + aarch64-elf- | Same as Samsung |

## Directory Structure

```
build-soc/
├── build.sh          # Master build script
├── flash.sh          # Universal flash script
├── fastboot_all.sh   # Vendor-specific fastboot
├── apply_patch.sh    # SoC/kernel patches
├── gen_bootimg.sh    # Boot image generator
├── create_super.sh   # Super partition creator
├── Makefile          # Build orchestration
├── README.md         # This file
├── configs/
│   ├── mediatek/     # MediaTek configs
│   ├── qualcomm/     # Qualcomm configs
│   ├── samsung/      # Samsung configs
│   └── google/       # Google configs
├── overlays/         # Device tree overlays
└── out/              # Build output
```

## License

BSD Licensed. See LICENSE file in root directory.