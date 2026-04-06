# Mobile Hardware Drivers

This directory contains drivers for mobile-specific hardware.

## Supported Hardware
- Touchscreens (multi-touch)
- Accelerometers/Gyroscopes
- GPS receivers
- Cameras (front/back)
- WiFi/Bluetooth modules
- Cellular modems
- Battery management
- Display controllers

## Chipset Support
### MediaTek
- MTK SoC drivers
- Power management optimizations
- GPU acceleration

### Qualcomm
- Snapdragon drivers
- Adreno GPU support
- FastCV optimizations

## Adding New Drivers
1. Create driver source in appropriate subdirectory
2. Update kernel config files
3. Test on target hardware