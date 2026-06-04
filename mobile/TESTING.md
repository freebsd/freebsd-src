# UOS(m) Mobile OS — Testing Guide

## Testing Environments

UOS(m) supports two primary testing environments:

1. **QEMU Virtual Machine** — Fastest iteration cycle, no physical hardware needed
2. **Physical Hardware** — Real device testing via fastboot/SP Flash Tool/Heimdall

---

## Quick Start: QEMU VM Testing

### Prerequisites

```bash
# Debian/Ubuntu/Kali
sudo apt-get install qemu-system-riscv64 qemu-utils gcc-riscv64-unknown-elf device-tree-compiler

# macOS
brew install qemu-system-riscv64 riscv-tools dtc

# Arch Linux
sudo pacman -S qemu-system-riscv riscv64-elf-gcc dtc
```

### One-Command Test

```bash
cd E:/freebsd-src
bash tools/run-uos-qemu.sh
```

### Options

```bash
# High-performance VM
bash tools/run-uos-qemu.sh --cores 8 --mem 4G

# Headless (SSH/VNC only)
bash tools/run-uos-qemu.sh --headless --cores 4 --mem 1G

# With GDB debugging
bash tools/run-uos-qemu.sh --gdb --cores 2

# Default values
bash tools/run-uos-qemu.sh --cores 4 --mem 2G
```

### What the Script Does

1. **Phase 1** — Checks for `qemu-system-riscv64`, `qemu-img`, toolchain
2. **Phase 2** — Builds the RISC-V mini-kernel from `mobile/kernel/` (auto-rebuilds if stale)
3. **Phase 3** — Creates a 4GB raw disk image at `tools/riscv/images/uos-riscv.img`
4. **Phase 4** — Assembles QEMU command line with:
   - QEMU `virt` machine with RV64GC CPU
   - 4 vCPUs, 2GB RAM (configurable)
   - OpenSBI firmware for RISC-V boot
   - VirtIO GPU (GTK/SDL/VNC display fallback)
   - VirtIO block disk (4GB raw image)
   - VirtIO network (SSH on host port 2222)
   - Serial console on stdio
   - QEMU monitor on telnet port 4445
5. **Phase 5** — Displays configuration summary and launches QEMU

### QEMU Controls

| Key | Action |
|-----|--------|
| `Ctrl+A, X` | Quit QEMU |
| `Ctrl+A, C` | Enter QEMU monitor |
| `Ctrl+A, H` | Toggle display |
| `Ctrl+A, ?` | Help |

### Accessing the VM

| Service | Address | Credentials |
|---------|---------|-------------|
| Serial console | stdio (terminal) | — |
| SSH | `ssh -p 2222 root@localhost` | `root` / `uos` |
| VNC (headless) | `localhost:1` | — |
| QEMU monitor | `telnet localhost 4445` | — |
| HTTP (rootfs) | `http://localhost:8080` | — |

### Debugging with GDB

```bash
# Terminal 1: Launch with GDB stub
bash tools/run-uos-qemu.sh --gdb

# Terminal 2: Connect GDB
riscv64-unknown-elf-gdb mobile/vmlinux.riscv64
(gdb) target remote localhost:1234
(gdb) break kernel_init
(gdb) continue
(gdb) info registers
```

### Troubleshooting QEMU

| Issue | Solution |
|-------|----------|
| `qemu-system-riscv64: command not found` | `sudo apt-get install qemu-system-riscv64` |
| GTK/display errors | Use `--headless` and connect via VNC |
| OpenSBI firmware missing | `sudo apt-get install opensbi` |
| Keyboard not working | Check QEMU input settings, try `--no-display` |
| Network unreachable | SSH on port 2222 may take 30s after boot |
| Kernel hangs | Check serial output for panic messages |

---

## Testing Components Individually

### Test the Kernel Only

```bash
cd mobile/kernel
make all QEMU=1 CONFIG_QEMU=1
bash ../../../tools/run-uos-qemu.sh
```

### Test the Desktop Environment

```bash
# Build everything
make -C mobile all

# Then launch QEMU
bash tools/run-uos-qemu.sh
```

### Test Services Manually

```bash
# Boot into QEMU with single-user mode
# Then inside the VM:
service displayd start
service powerd start
service networkd start
service audiod start
```

---

## Flash to Real Hardware

### Prerequisites for All Devices

1. **Unlocked bootloader** — varies by vendor (see below)
2. **ADB and Fastboot** installed on host
3. **Built images** from `mobile/build-soc/build.sh`

### MediaTek (Dimensity / Helio)

#### Supported Devices

| SoC Config | SoC | Devices |
|-----------|------|---------|
| MT6893 | Dimensity 1100 | Realme GT, POCO F3 |
| MT6877 | Dimensity 920 | Redmi Note 11 Pro |
| MT6833 | Dimensity 800U | Redmi Note 10 |
| MT6895 | Dimensity 9000 | Xiaomi 12T |
| MT8983 | Dimensity 9300 | Vivo X100 |

#### Unlock Bootloader

```bash
# 1. Enable Developer Options (tap Build Number 7x)
# 2. Enable OEM Unlock and USB Debugging
# 3. Reboot to bootloader
adb reboot bootloader

# 4. Unlock
fastboot oem unlock
# or for newer devices:
fastboot flashing unlock
```

#### Flash Images

```bash
# Using fastboot
bash mobile/mediatek/flash/fastboot.sh

# Or manually:
fastboot flash boot out/MT6893/boot.img
fastboot flash dtbo out/MT6893/dtbo.img
fastboot flash super out/MT6893/super.img
fastboot flash vbmeta out/MT6893/vbmeta.img --disable-verity
fastboot reboot
```

#### SP Flash Tool (Download Mode)

For devices that don't boot to fastboot:

```bash
# 1. Power off device
# 2. Hold Vol+ and Vol- while connecting USB
# 3. Run:
python3 mobile/mediatek/tools/mtk_dtbtool.py \
    --mode download \
    --preloader preloader_MT6893.bin \
    --scatter scatter_MT6893.txt

# 4. In SP Flash Tool:
#    - Load scatter file
#    - Select "Download Only"
#    - Click "Download"
#    - Connect powered-off device
```

### Qualcomm (Snapdragon)

#### Supported Devices

| SoC Config | SoC | Devices |
|-----------|------|---------|
| SM8450 | Snapdragon 8 Gen 2 | Galaxy S23, OnePlus 11 |
| SM8350 | Snapdragon 888 | Galaxy S21, ROG Phone 5 |
| SM7450 | Snapdragon 778G | Nothing Phone (1) |
| SM7350 | Snapdragon 7+ Gen 2 | Nothing Phone (2) |
| SM6375 | Snapdragon 7s Gen 2 | Various mid-range |
| SM6450 | Snapdragon 6 Gen 1 | Budget devices |
| SM6625 | Snapdragon 4 Gen 2 | Entry-level |
| MSM8998 | Snapdragon 835 | Legacy (OnePlus 5T) |

#### Unlock Bootloader

```bash
# Different methods per OEM:
# Xiaomi: fastboot oem unlock
# OnePlus: fastboot oem unlock
# Google Pixel: fastboot flashing unlock
# Samsung Exynos: Odin/Heimdall

# Check unlock status:
fastboot getvar unlocked
fastboot getvar secureboot
```

#### Flash Images

```bash
# Standard fastboot flash
bash mobile/qualcomm/flash/fastboot.sh

# Manual flash:
fastboot flash boot out/SM8450/boot.img
fastboot flash dtbo out/SM8450/dtbo.img
fastboot flash vendor_boot out/SM8450/vendor_boot.img
fastboot flash super out/SM8450/super.img
fastboot flash vbmeta out/SM8450/vbmeta.img --disable-verity --disable-verification

# For A/B partition devices:
fastboot --set-active=a flash boot out/SM8450/boot.img
fastboot --set-active=b flash boot out/SM8450/boot.img

fastboot reboot
```

#### USB Mode Selection

```bash
# Qualcomm devices have multiple USB modes:
# 1. Fastboot mode (standard)
#    adb reboot bootloader
#    fastboot devices

# 2. Emergency Download Mode (EDL)
#    Power off + hold Vol+ while connecting USB
#    Use QFIL/Qualcomm Flash Image Loader:

# 3. Recovery mode
#    adb reboot recovery
#    adb sideload update.zip
```

### Samsung Exynos

#### Supported Devices

| SoC Config | SoC | Devices |
|-----------|------|---------|
| S5E9925 | Exynos 2200 | Galaxy S22 series |
| S5E9810 | Exynos 2100 | Galaxy S21 series |
| S5E8845 | Exynos 1380 | Galaxy A54/A34 |
| S5E8535 | Exynos 1330 | Galaxy A14/A24 |
| S5E9830 | Exynos 990 | Galaxy S20 FE |
| S5E9825 | Exynos 9825 | Galaxy Note10+ |
| S5E5510 | Exynos 850 | Galaxy A13/A04 |

#### Unlock Bootloader

```bash
# 1. Enable Developer Options > OEM Unlock
# 2. Power Off
# 3. Hold Vol Down + Vol Up + USB cable (Download Mode)
# 4. Long press Vol Up to confirm

# Or using Heimdall:
heimdall detect
```

#### Flash Images

```bash
# Using Heimdall (Samsung download mode)
bash mobile/samsung/flash.sh --board S5E9925

# Manual flash with Heimdall:
heimdall flash --BOOT out/S5E9925/boot.img
heimdall flash --DTBO out/S5E9925/dtbo.img
heimdall flash --CACHE out/S5E9925/cache.img
```

#### Odin Method

Odin can flash UOS images if packaged as a tar.md5:

```bash
# Build a flashable tar:
tar -H posix -cf flash_S5E9925.tar \
    boot.img \
    dtbo.img \
    super.img

# Add md5 hash for Odin:
md5sum flash_S5E9925.tar > flash_S5E9925.tar.md5

# In Odin: select AP tab, load the tar.md5 file
```

### Google Tensor (Pixel 6/7/8)

#### Supported Devices

| Config | SoC | Devices |
|--------|------|---------|
| redfin | Tensor G1 | Pixel 6, 6 Pro, 6a |
| bluejay | Tensor G2 | Pixel 7, 7 Pro, 7a |
| zuma | Tensor G3 | Pixel 8, 8 Pro |

#### Unlock Bootloader

```bash
# Pixel Unlock:
# 1. Settings > About Phone > tap "Build Number" 7x
# 2. Settings > System > Developer Options > OEM Unlock
# 3. Enable USB Debugging
# 4. Reboot to bootloader:
adb reboot bootloader

fastboot flashing unlock
```

#### Flash Images (Factory Image Style)

```bash
# Full factory image flash:
bash mobile/tensor/flash.sh --config redfin --wipe-data

# Or fastboot:
fastboot flash boot out/redfin/boot.img
fastboot flash dtbo out/redfin/dtbo.img
fastboot flash super out/redfin/super.img
fastboot flash vendor_boot out/redfin/vendor_boot.img
fastboot flash vbmeta out/redfin/vbmeta.img --disable-verity --disable-verification
fastboot reboot
```

---

## Universal Flash via build-soc

The `mobile/build-soc/` scripts provide a vendor-agnostic interface.

### Universal Build

```bash
cd mobile/build-soc

# List all supported SoCs
bash build.sh --list-socs

# List boards for a specific SoC
bash build.sh --list-boards --soc MT6893

# Build for a specific device
bash build.sh --soc MT6893 --board X01BD --output out/X01BD/

# The script automatically:
# 1. Selects the correct kernel config from mobile/mediatek/configs/
# 2. Applies board-specific DTB patches
# 3. Builds with correct toolchain
# 4. Generates boot.img, dtbo.img, super.img, vbmeta.img
```

### Universal Flash

```bash
# Flash to connected device (auto-detects vendor)
bash flash.sh --soc MT6893 --board X01BD

# With additional options:
bash flash.sh --soc SM8450 --board lavender --wipe-data --no-verity

# Wipe data before flashing
bash flash.sh --soc MT6893 --wipe-data --wipe-cache
```

### Fastboot Mode Flash

```bash
# Flash from fastboot mode
bash fastboot_all.sh --soc MT6893 --slot a
bash fastboot_all.sh --soc SM8450 --slot both  # A/B partitions
```

---

## Boot Image Generation

### Boot Image Structure

```
boot.img
├── boot header (1 page)
├── kernel (zImage or Image.gz)
├── ramdisk (cpio archive)
│   └── init binary
│   └── init.rc scripts
│   └── default.prop
├── dtb (device tree blob)
└── boot signature (optional)
```

### Generate Boot Image

```bash
# Use the included tool
bash tools/generate-bootimg.sh \
    --kernel out/Image.gz \
    --ramdisk out/ramdisk.cpio.gz \
    --dtb out/dtb.dtb \
    --output out/boot.img \
    --pagesize 4096 \
    --cmdline "console=ttyMSM0,115200n8 root=/dev/block/bootdevice/by-name/system rw"
```

---

## Secure Boot

### Overview

UOS(m) implements a chain of trust:

1. **BootROM** — hardware root of trust, verifies first-stage bootloader
2. **First Stage Bootloader** — verifies second-stage (LK/U-Boot)
3. **Second Stage Bootloader** — verifies kernel (dtb + kernel hash)
4. **Kernel** — verifies initramfs, loads policy

### Signing Keys

```bash
# Generate signing keys (do this once per device)
openssl genrsa -out devkey.pem 2048
openssl rsa -in devkey.pem -pubout -out devkey.pem.pub

# Sign kernel
bash tools/sign-kernel.sh --kernel out/Image \
    --key devkey.pem \
    --output out/Image-signed

# Sign boot image
bash tools/sign-bootimg.sh --bootimg out/boot.img \
    --key devkey.pem \
    --output out/boot.img-signed
```

### Verified Boot Configuration

```bash
# Configure verified boot in kernel config
# options VERIFIED_BOOT
# options VERIFIED_BOOT_SIGNING_KEY="/etc/signing/devkey.pem"
# options VERIFIED_BOOT_HASH_ALGO="sha256"

# Lock bootloader (production)
fastboot flashing lock

# Unlock (development only)
fastboot flashing unlock
```

---

## Disk Encryption

UOS(m) supports full-disk encryption for mobile devices:

```bash
# Enable encryption at build time
make -C sys build KERNCONF=MT6893 \
    OPTIONS="CRYPTO_AES_GCM CRYPTO_GCM CRYPTO_CHACHA20POLY1305"

# Generate encryption key
openssl rand -hex 32 > boot.key

# Create encrypted image
cryptsetup luksFormat out/encrypted.img boot.key
cryptsetup luksOpen out/encrypted.img cryptroot --key-file boot.key

# Mount and copy filesystem
mkfs.ext4 /dev/mapper/cryptroot
mount /dev/mapper/cryptroot /mnt
cp -r rootfs/* /mnt/
```

---

## Automated Testing

### Quick Test Suite

```bash
# Run basic validation
bash tools/test/uos-quick-test.sh

# Run full test suite
bash tools/test/uos-full-test.sh

# Run with coverage
bash tools/test/uos-test.sh --coverage --report html
```

### CI/CD Pipeline

```bash
# Run automated build and test
bash ci/build-and-test.sh --soc MT6893 --board X01BD --test-level full

# Build only
bash ci/build.sh --soc MT6893

# Deploy artifacts
bash ci/deploy.sh --bucket uos-releases --version 0.1.0
```

---

## Performance Testing

### Boot Time Measurement

```bash
# Enable boot timing in kernel
# options BOOT_TIMING

# Measure from QEMU:
time qemu-system-riscv64 ... 2>&1 | grep "FreeBSD/SMP"

# Or inside VM:
dmesg | grep "FreeBSD"
sysctl kern.boottime
```

### Graphics Performance

```bash
# Run GL benchmarks inside VM
./mobile/tests/gfx/glxgears
./mobile/tests/gfx/gpu-bench

# Measure compositor frame rate
./mobile/tests/gfx/fps-test --frames 1000
```

---

## Debugging

### Kernel Debugging

```bash
# Boot with debug options
# options KDB
# options KDB_TRACE
# options GDB

# Connect GDB
qemu-system-riscv64 -s -S ...
riscv64-unknown-elf-gdb kernel/vmlinux
(gdb) target remote :1234
```

### Userspace Debugging

```bash
# Inside VM:
gdb /mobile/bin/displayd
gdb /mobile/bin/compositor

# Core dumps
sysctl kern.coredump=1
ulimit -c unlimited
```

### Log Analysis

```bash
# View kernel log
dmesg
cat /var/log/messages

# View service logs
cat /var/log/displayd.log
cat /var/log/compositor.log

# Live log tail
tail -f /var/log/messages
```

---

## Release Checklist

Before creating a release image:

- [ ] Run full test suite: `bash tools/test/uos-full-test.sh`
- [ ] Build all SoC variants
- [ ] Verify boot images with `tools/verify-bootimg.sh`
- [ ] Sign all artifacts with release keys
- [ ] Generate release notes
- [ ] Create update.zip for recovery
- [ ] Upload to release server

```bash
# Create a release
bash ci/release.sh --version 0.1.0 \
    --socs MT6893,SM8450,S5E9925 \
    --signing-key release.pem \
    --upload
```
