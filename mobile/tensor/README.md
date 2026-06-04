# Google Tensor SoC Support

UOS Mobile OS kernel configurations for Google Tensor-based devices.

## Supported SoCs

| SoC | Config | Device | CPU Configuration | Notes |
|-----|--------|--------|-----------------|-------|
| Tensor G1 (Redfin) | redfin.conf | Pixel 6, 6 Pro, 6a | 2xCortex-X1 + 2xA76 + 4xA55 | Based on Exynos 9840 |
| Tensor G2 (Bluejay) | bluejay.conf | Pixel 7, 7 Pro, 7a | 2xCortex-X1 + 2xA79 + 4xA510 | Enhanced NPU |
| Tensor G3 (Zuma) | zuma.conf | Pixel 8, 8 Pro | 1xCortex-X3 + 4xA715 + 4xA510 | Latest generation |

## Building

```bash
cd mobile/tensor
./build.sh --config=redfin --dtb --bootimg --output-dir=./output
```

### Build Options

| Option | Description |
|--------|-------------|
| `--config=NAME` | Kernel config (redfin, bluejay, zuma) |
| `--dtb` | Build device tree blob |
| `--bootimg` | Generate boot.img for Google bootloader |
| `--skip-verity` | Disable AVB verity (testing) |
| `--skip-verification` | Disable AVB verification (testing) |

## Tensor-Specific Features

### Tensor Processing Unit
- `TENSOR_TPU` - Tensor Processing Unit support
- `TENSOR_EDGETPU` - Edge TPU for ML acceleration

### Security
- `GOOGLE_TITAN_M` - Titan M security chip (G1 only)
- `OPTEE` - OP-TEE integration

## Flashing as Factory Image

```bash
# Flash via fastboot (after OEM unlock)
bash flash.sh --full
```

## Notes

Tensor SoCs are based on Samsung Exynos architectures with Google customizations:
- Tensor G1: Derived from Exynos 9840 with custom TPU
- Tensor G2: Enhanced NPU for improved ML performance
- Tensor G3: Latest architecture with IMG GPU

The configurations use the Exynos toolchain for building but produce output 
compatible with Google's verified boot (AVB) and fastboot flashing.