# Mobile OS Build Instructions

## Prerequisites
- FreeBSD build environment
- Cross-compilation tools for ARM64
- Android device with supported chipset (MediaTek or Qualcomm)

## Building the Kernel
1. Configure for mobile target:
   ```
   make -C sys/arm64/conf MOBILE
   ```
2. Build kernel:
   ```
   make buildkernel KERNCONF=MOBILE
   ```

## Building Userland
1. Build world with mobile options:
   ```
   make buildworld MOBILE=yes
   ```

## Installing
1. Install to device (requires custom bootloader support)
2. Flash kernel and userland to partitions

## Chipset Optimizations
- MediaTek: Enable MTK-specific drivers in kernel config
- Qualcomm: Enable QCOM-specific drivers and power management

## Testing
- Use emulator for initial testing
- Deploy to test devices for hardware validation