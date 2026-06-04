# MediaTek (MTK) SoC Support

This directory contains kernel configurations and build tools for MediaTek SoCs
running on FreeBSD-based Mobile OS.

## Supported SoCs

| SoC | Config | CPU Cores | GPU | Devices |
|------|--------|-----------|-----|---------|
| MT6893 | Dimensity 1100 | 4xA78+4xA55 | Mali-G77 MC9 | Realme, POCO, Xiaomi |
| MT6877 | Dimensity 920 | 2xA78+6xA55 | Mali-G57 MC2 | Realme, Oppo |
| MT6833 | Dimensity 800U | 2xA76+6xA55 | Mali-G57 MC2 | Xiaomi, Realme |
| MT6895 | Dimensity 9000 | 4xX2+3xA710+4xA510 | Mali-G710 MC10 | vivo, Oppo |
| MT8983 | Dimensity 9300 | 4xX4+3xA720+4xA520 | Mali-G725 MC10 | vivo, Oppo |

## Building

```sh
# Basic build
bash build.sh MT6893

# Or with environment variable
MTK_BOARD=MT6893 bash build.sh
```

The build script uses clang/llvm toolchain (required for MediaTek platforms) and
outputs to `out/` directory.

## Flashing

### Fastboot mode
```sh
bash flash/fastboot.sh
```

### SP Flash Tool (download mode)
```sh
python tools/flash.py --mode=download-mode
```

## Directory Structure

```
mobile/mediatek/
├── configs/          # Kernel configuration files
├── flash/            # Flashing scripts
├── tools/            # Build and utility tools
├── out/              # Build output (created by build.sh)
├── build.sh          # Main build script
└── README.md         # This file
```

## License

BSD-2-Clause