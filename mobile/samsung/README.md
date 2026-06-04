# Samsung Exynos SoC Support

UOS Mobile OS kernel configurations for Samsung Exynos-based devices.

## Supported SoCs

| Config | SoC | Device | CPU Configuration | GPU |
|--------|-----|--------|-----------------|-----|
| S5E9925 | Exynos 2200 | Galaxy S22, S22+, S22 Ultra | 1xCortex-X2 + 3xA710 + 4xA510 | Xclipse 2200 (RDNA2) |
| S5E9810 | Exynos 2100 | Galaxy S21, S21+, S21 Ultra | 1xCortex-X1 + 3xA78 + 4xA55 | Xclipse 920 (RDNA1) |
| S5E8845 | Exynos 1380 | Galaxy A54 5G, A34 5G | 4xA78 + 4xA55 | Mali-G68 MP4 |
| S5E8535 | Exynos 1330 | Galaxy A14, A24, M14 | 2xA78 + 6xA55 | Mali-G68 MP2 |
| S5E9830 | Exynos 990 | Galaxy S20, S20+, Note20 | 2xM5 + 2xA76 + 4xA55 | Mali-G77 MP11 |
| S5E9825 | Exynos 9825 | Galaxy Note10+ | 2xM4 + 2xA75 + 4xA55 | Mali-G76 MP12 |
| S5E5510 | Exynos 850 | Galaxy A13, A04 series | 8xA55 | Mali-G52 MP2 |

## Building

```bash
cd mobile/samsung
./build.sh --config=S5E9925 --dtb --bootimg --output-dir=./output
```

### Build Options

| Option | Description |
|--------|-------------|
| `--config=NAME` | Kernel config (required) |
| `--dtb` | Build device tree blob |
| `--bootimg` | Generate boot.img via mkbootimg |
| `--dtbo` | Generate dtbo.img for overlays |
| `--skip-verity` | Disable AVB verity (testing) |
| `--skip-verification` | Disable AVB verification (testing) |

## Exynos-Specific Features

### Power Management
- `EXYNOS_OSCC` - On-SoC Clock control
- `EXYNOS_PM` - Power Management
- `EXYNOS_PMU` - Power Management Unit
- `EXYNOS_CPUFREQ` - CPU frequency scaling
- `EXYNOS_THERMAL` - Thermal monitoring

### Display Subsystem
- `EXYNOS_MIPI` - MIPI DSI host
- `EXYNOS_DSIM` - DSI Master controller
- `EXYNOS_LCD` - LCD panel interface
- `EXYNOS_DPU` - Display Processing Unit

### USB / Type-C
- `EXYNOS_USB` - USB device controller
- `EXYNOS_TYPEC` - USB Type-C support
- `EXYNOS_USBPD` - USB Power Delivery
- `PDC` - Power Delivery Controller

### Camera Subsystem
- `EXYNOS_FIMC` - Frame Interface for MediaController
- `EXYNOS_CSIS` - Camera Serial Interface
- `EXYNOS_JPEG` - JPEG codec
- `EXYNOS_SCALER` - Image scaler

### Modem Interface
- `S52XX_SHMC` - Shared Memory Controller (Exynos Modem 5200)
- `EXYNOS_MODEM` - Modem interface

### Security
- `EXYNOS_TZ` - TrustZone support
- `ARM_TRUSTED_FIRMWARE` - ATF integration
- `OPTEE` - OP-TEE support

## Bootloader Support

Samsung uses proprietary bootloaders:
- **Loke** - Older Samsung bootloader (Exynos 990 and earlier)
- **Spring** - Newer Samsung bootloader (Exynos 2100+)

Both are supported via ELF loader with custom header format handled by mkbootimg.

## Flash Tool

For flashing as factory image:
```bash
# Flash via download mode (Odin/heimdall compatible)
./flash.sh --full
```

See `flash.sh` for Samsung-specific flashing utilities.