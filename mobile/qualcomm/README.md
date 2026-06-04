# Qualcomm Snapdragon SoC Support

This directory contains kernel configurations and build scripts for Qualcomm Snapdragon SoCs in the BSD-based Mobile OS project.

## Supported SoCs

| Config        | SoC                    | CPU                              | GPU          |
|---------------|------------------------|----------------------------------|--------------|
| SM8450        | 8cx Gen 3 / 8 Gen 2    | X3+A715+A510 (1+3+4/8)           | Adreno 730   |
| SM8350        | 8cx Gen 2              | X1+A78+A55 (1+3+4)               | Adreno 680   |
| SM7450        | 7cx Gen 3              | A78+A55 (8x)                     | Adreno 642   |
| SM7350        | 7 Gen 1                | A710+A510 (8x)                   | Adreno 662   |
| SM6375        | 7s Gen 2               | A78+A55 (4+4)                    | Adreno 710   |
| SM6450        | 6 Gen 1                | A78+A55 (4+4)                    | Adreno 640   |
| SM6625        | 4 Gen 2                | A78+A55 (2+6)                    | Adreno 610   |
| MSM8998       | Snapdragon 835 (legacy)| Kryo 280 custom (8x)             | Adreno 540   |

## Directory Structure

```
mobile/qualcomm/
├── configs/          # Kernel configuration files
│   ├── SM8450.conf
│   ├── SM8350.conf
│   ├── SM7450.conf
│   ├── SM7350.conf
│   ├── SM6375.conf
│   ├── SM6450.conf
│   ├── SM6625.conf
│   └── MSM8998.conf
├── flash/            # Device flashing scripts
│   └── fastboot.sh
├── tools/            # Helper tools
└── build.sh          # Main build script
```

## Building

### Prerequisites

- FreeBSD base system with clang/LLVM toolchain
- `dtc` (Device Tree Compiler)
- `mkdtimg` (for DTBO packing)
- `mkbootimg` (Android boot image tool, optional)

### Quick Build

```bash
cd E:\freebsd-src\mobile\qualcomm
./build.sh SM8450
```

Outputs are placed in `E:\freebsd-src\out\SM8450\`:
- `boot.img` - Android boot image (zImage + ramdisk + DTBO)
- `dtbo.img` - Device tree overlay
- `vendor_boot.img` - Vendor boot partition
- `super.img` - Dynamic super partition
- `vbmeta.img` - Verification metadata
- `kernel` - Raw zImage

### Environment Variables

```bash
TARGET=SM8450           # SoC target
FREEBSD_SRC=/path/to/freebsd-src  # FreeBSD source tree
```

## Flashing

```bash
cd E:\freebsd-src\mobile\qualcomm\flash
./fastboot.sh SM8450
```

This flashes boot, dtbo, vendor_boot, super, and vbmeta partitions, then reboots the device.

## Kernel Configuration Options

Each config includes:

- **SMP** with up to 8 cores
- **QCOM_GCC** - Global Clock Controller
- **QCOM_PDC** - Power Domain Controller
- **QCOM_RPMH** - Resource Power Manager Hardware
- **QCOM_RPM** - Resource Power Manager
- **QCOM_PMIC** - PMIC support
- **QCOM_CLK/QCOM_REGULATOR** - Clock and regulator framework
- **QCOM_APCS** - Application Processor Communication Subsystem
- **QCOM_GICV3** - GICv3 interrupt controller
- **QCOM_HARDENING** - Security hardening
- **DRM/KMS** - Graphics and display
- **QCOM_MDSS/DSI/SDE** - Display subsystem
- **QCOM_RMNET/QMI** - Modem connectivity
- **QCOM_APR/DSP_Q6** - Audio DSP
- **QCOM_UFS/MMC** - Storage
- **QCOM_USB/XHCI** - USB host
- **QCOM_WCNSS** - WiFi/BT/FM
- **CPUFREQ/QCOM_CPUFREQ** - CPU frequency scaling
- **QCOM_TSENS** - Thermal sensors

## License

BSD 2-Clause
