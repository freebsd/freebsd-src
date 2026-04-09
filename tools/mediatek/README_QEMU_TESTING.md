# UOS MediaTek — QEMU Testing Guide

## Overview

This guide explains how to build and test UOS (FreeBSD-based mobile OS) with
MediaTek MT8395 support in QEMU on WSL, and then on real hardware (Radxa NIO 12L).

```
           ┌──────────────────────────────────────┐
           │         UOS Development Flow          │
           │                                      │
           │  WSL Build ──► QEMU VIRT (GUI test)  │
           │                     │                │
           │                     ▼                │
           │              Real HW (NIO 12L)       │
           └──────────────────────────────────────┘
```

---

## Prerequisites

- **OS**: Windows 10/11 with WSL2 (Ubuntu 22.04 recommended)
- **WSL2 display**: WSLg (Windows 11) or VcXsrv/X410 (Windows 10)
- **Disk space**: ~20 GB free in WSL home directory
- **RAM**: 8+ GB host RAM (QEMU will use 4 GB)

---

## Step 1: Set Up WSL Toolchain

```bash
# In WSL terminal:
cd /mnt/d/Github_Projects/freebsd-src/tools/mediatek

bash setup-wsl-toolchain.sh

# Load the environment (add to ~/.bashrc for persistence)
source ~/.uos-mediatek-env
```

This installs: `qemu-system-aarch64`, `clang-17`, `dtc`, `gdb-multiarch`, and
all other required tools.

---

## Step 2: Build the Kernel

### QEMU Build (recommended first)
```bash
bash build-mediatek-kernel.sh MEDIATEK-QEMU
```

### Real Hardware Build
```bash
bash build-mediatek-kernel.sh MEDIATEK
```

> **Note**: First-time builds require `BUILD_WORLD=yes` to compile the
> FreeBSD userland. This takes 60–90 minutes. Subsequent kernel-only
> rebuilds take ~5 minutes.

```bash
# Full first-time build:
BUILD_WORLD=yes bash build-mediatek-kernel.sh MEDIATEK-QEMU
```

---

## Step 3: Create Disk Image

```bash
bash create-disk-image.sh MEDIATEK-QEMU
```

Creates `~/uos-mediatek.img` (8 GB sparse image):

| Partition | Type   | Size   | Contents         |
|-----------|--------|--------|------------------|
| p1        | FAT32  | 256 MB | UEFI + kernel    |
| p2        | UFS2   | 7.7 GB | FreeBSD rootfs   |

---

## Step 4: Run in QEMU (GUI Mode)

```bash
bash qemu-mediatek-run.sh
```

### What you'll see:

1. **Serial console** (this terminal): FreeBSD boot messages at 921600n8
2. **GTK/SDL window**: VirtIO GPU framebuffer → virtual terminal (vt)

### QEMU machine configuration:

| Setting       | Value                          | Notes                    |
|---------------|--------------------------------|--------------------------|
| Machine       | `virt` with GIC v3 + SMMU v3  | ARM64 virt board         |
| CPU           | Cortex-A55 × 4                 | Matches MT8395 A55 cores |
| RAM           | 4 GB                           |                          |
| GPU           | VirtIO-GPU (1280×800)          | GUI window               |
| Input         | VirtIO keyboard + mouse        |                          |
| Disk          | VirtIO block                   | uos-mediatek.img         |
| Network       | VirtIO (vtnet0) + DHCP         | SSH on port 2222         |
| Serial        | stdio                          | Boot log + console       |
| Monitor       | telnet localhost 4444          | QEMU control             |

### Headless mode (no display):
```bash
bash qemu-mediatek-run.sh ~/uos-mediatek.img --headless
# Connect via VNC: localhost:5901
# Or SSH: ssh -p 2222 root@localhost
```

---

## Step 5: Debug with GDB (Optional)

In terminal 1:
```bash
bash qemu-mediatek-debug.sh
# QEMU pauses, waiting for GDB...
```

In terminal 2:
```bash
gdb-multiarch ~/obj/.../sys/MEDIATEK-QEMU/kernel.debug
(gdb) target remote localhost:1234
(gdb) break mt8395_clk_probe
(gdb) continue
```

---

## Step 6: Run Device Tests

Copy tests to the guest:
```bash
scp -P 2222 -r tools/mediatek/tests/ root@localhost:/uos-tests/
```

Run all tests:
```bash
ssh -p 2222 root@localhost "sh /uos-tests/run_all_tests.sh --verbose"
```

Run individual test:
```bash
ssh -p 2222 root@localhost "sh /uos-tests/test_ethernet.sh --verbose"
```

---

## Real Hardware: Radxa NIO 12L (MT8395)

### Flash kernel to eMMC

```bash
# From WSL, connect NIO 12L via USB (download/maskrom mode)
# Flash using mtk-su or fastboot:

# Option 1: Using dd (if U-Boot is functional)
dd if=uos-mediatek.img of=/dev/sdX bs=4M status=progress

# Option 2: Using MTK Download Tool (Windows host)
# Use SP Flash Tool with the scatter file
```

### Serial console (UART0)

Connect a USB-UART adapter to the NIO 12L debug header:

| Pin  | Signal | Notes           |
|------|--------|-----------------|
| 1    | GND    |                 |
| 3    | TX     | Board TX → host RX |
| 5    | RX     | Board RX → host TX |

Settings: **921600 8N1** (as set in SOCDEV_PA and loader.conf)

```bash
# WSL:
screen /dev/ttyUSB0 921600
# or:
minicom -D /dev/ttyUSB0 -b 921600
```

### Expected dmesg on NIO 12L

```
GDB: no kernel debugging
KDB: debugger backends: ddb
KDB: current backend: ddb
Copyright (c) 1992-2024 The FreeBSD Project.
Copyright (c) 1979, 1980, 1983, 1986, 1988...
FreeBSD 14.x-CURRENT MEDIATEK_NIO12L arm64
...
mt8395_clk0: <MediaTek MT8395 Clock Controller> mem ...
mt8395_pinctrl0: <MediaTek MT8395 GPIO/Pinctrl> mem ... (202 pins)
uart0: <MediaTek mt6577-uart> at mem 0x11002000...
...
```

---

## Troubleshooting

### QEMU won't start
```bash
# Check QEMU version (need 7.0+)
qemu-system-aarch64 --version

# Check UEFI firmware exists
ls /usr/share/qemu-efi-aarch64/QEMU_EFI.fd
```

### No GUI window in WSL
```bash
# WSL2 + WSLg (Windows 11): should work out of the box
# WSL2 + Windows 10: install VcXsrv, then:
export DISPLAY=$(cat /etc/resolv.conf | grep nameserver | awk '{print $2}'):0.0
bash qemu-mediatek-run.sh
```

### Kernel panics on boot
```bash
# Use debug mode to catch the panic:
bash qemu-mediatek-debug.sh
# In GDB: (gdb) break panic
```

### Clock driver not attaching
Check that `mt8395-topckgen` compatible string is in your DTB:
```sh
dtc -I dtb -O dts /boot/dtb/mediatek/mt8395-radxa-nio-12l.dtb | grep topckgen
```

---

## Architecture Notes

### MT8395 Bring-up Clock Ordering

```
1. XTAL 26MHz (hardcoded FRATE in mt8395_clk.c)
2. TOPCKGEN muxes → select UART clock source (uart_sel → univpll_192m)
3. INFRACFG gates → enable UART0 (CLK_INFRA_UART0)
4. UART0 attaches → kernel console active
5. GPIO/Pinctrl attaches → I2C/SPI/MMC pins configured
6. I2C attaches → PMIC (MT6359) probed
7. MMC/MSDC attaches → eMMC/SD accessible
8. USB xHCI attaches
9. Ethernet attaches → DHCP
```

### Bionic / Android App Support

UOS supports running Android NDK binaries through:

1. **COMPAT_LINUX64**: FreeBSD Linux syscall emulation
2. **uos_binder.ko**: `/dev/binder` device for Android IPC
3. **uos_ashmem.ko**: `/dev/ashmem` for Android shared memory
4. **Bionic libc**: provided via `/system/lib64/` from an Android arm64 image

To enable Android apps, place an Android arm64 `/system` partition contents
at `/system` on the UOS root filesystem and load `linux64.ko`.
