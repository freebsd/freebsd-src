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

## Building Mobile Components
1. Build mobile drivers:
   ```
   cd mobile/drivers/touchscreen && make
   cd mobile/drivers/sensors && make
   ```

2. Build UI framework:
   ```
   cd mobile/ui/compositor && make
   cd mobile/ui/toolkit && make
   ```

3. Build app framework:
   ```
   cd mobile/frameworks/app && make
   ```

## Installing
1. Install to device (requires custom bootloader support)
2. Flash kernel and userland to partitions
3. Load mobile kernel modules:
   ```
   kldload mobile_touchscreen
   kldload mobile_accel
   kldload mobile_compositor
   kldload mobile_ui
   kldload mobile_app
   ```

## Testing
- Use emulator for initial testing
- Deploy to test devices for hardware validation

## Chipset Optimizations
### MediaTek
- Enable MTK-specific drivers in kernel config
- Use std.mediatek for SoC support

### Qualcomm
- Enable QCOM-specific drivers
- Use Snapdragon optimizations

## Testing
- Use emulator for initial testing
- Deploy to test devices for hardware validation