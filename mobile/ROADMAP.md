# UOS(m) Mobile OS — Development Guide

> **Status:** Active development. QEMU RISC-V simulation boots and runs the full userspace stack (compositor, shell, audio, service manager, launcher, apps). Real-device SoC configs, drivers, and flash tooling are in progress. The actual code is in this tree under `mobile/`; treat this guide as the integration and upgrade manual.

---

## 1) Who this guide is for

You want to either:

- **A)** Run and explore UOS(m) in a VM (QEMU RISC-V), or
- **B)** Build, install, and run UOS(m) on a physical phone (MediaTek, Qualcomm, or Exynos phone with unlocked bootloader).

Read §2 for option **A** and §3–§8 for option **B**.

---

## 2) Quick-start with QEMU (recommended first step)

Host requirements: a Linux environment or WSL2 with `qemu-system-riscv64` installed.

```bash
# From the repo root:
cd E:/freebsd-src

# 1) One-command test: build kernel, create disk image, launch QEMU
bash tools/run-uos-qemu.sh

# Or with options:
bash tools/run-uos-qemu.sh --cores 4 --mem 2G
bash tools/run-uos-qemu.sh --headless          # VNC on :1
bash tools/run-uos-qemu.sh --gdb               # wait for GDB on :1234
```

Notes:

- You should see a QEMU window. The display device is `virtio-gpu-pci` running an OpenGL-backed GTK or SDL window; VNC falls back when no display server is present.
- The kernel boots under OpenSBI on QEMU's `virt` board and the rootfs is populated from `mobile/rootfs/`.
- The serial console is connected to stdio. The kernel logs are captured there.
- The kernel uses a 39-bit Sv39 virtual address space. The kernel base physical address is `0x8000_0000` and the DRAM size starts at 512 MB. See the wider device tree at `mobile/virtenv/devicetree/riscv-virt-mobile.dts` for the exact DRAM / UART0 / VirtIO block / VirtIO net base addresses the QEMU run expects.

For Mediatek and Qualcomm images you have the same setup being built out, but the only QEMU-supported target for the current virtenv is **RISC-V**; mobile-phone portability depends on the steps in §3 onward.

---

## 3) What you get with QEMU (full desktop stack even on RISC-V)

Even though this is a mini-kernel with QEMU VirtIO devices, the userspace is **not** a toy:

- Wayland compositor server (`mobile/wayland/`)
- DRM/KMS + GBM + EGL + OpenGL ES 2.0/3.0 rendering (`mobile/gfx/`)
- Linux evdev input backend (`mobile/input/`)
- Complete desktop environment: dock, top panel with applets, wallpapers, icon theme, lock screen, overview, workspace switcher, global search, system-wide notifications, Alt-Tab task switcher (`mobile/desktop/`)
- Services: PID 1 init, service manager with watchdog, displayd, powerd, networkd, audiod, timed, pkgd (`mobile/services/`)
- App frameworks: zygote sandbox, Binder-like IPC, permissions, lifecycle, intent-based navigation, view hierarchy, content resolver, notification manager (`mobile/frameworks/`)
- User apps: Home, Settings, Terminal, Browser, Contacts, Messages (`mobile/apps/`)
- Package manager with repo support (`mobile/pkg/`)
- Audio with DSP, ALSA-compatible mixer, Bluetooth routing (`mobile/audio/`)
- Power, sensors, camera, USB, WiFi, cellular, VPN, NFC (`mobile/power/`, `mobile/sensors/`, `mobile/camera/`, `mobile/usb/`, `mobile/connectivity/`)

---

## 4) Building for real mobile hardware

The hardware path is split by SoC vendor:

- `mobile/mediatek/` — MediaTek Dimensity devices
- `mobile/qualcomm/` — Qualcomm Snapdragon devices
- `mobile/samsung/` — Samsung Exynos devices
- `mobile/tensor/` — Google Tensor devices
- `mobile/soc/` — SoC abstraction + detection + peripheral register bases + common early init

Each vendor directory exports three things:

1. **Kernel configs** (`configs/*.conf`) in FreeBSD `arm64/conf/` format
2. **Build scripts** that invoke the FreeBSD build system and wrap `make buildkernel KERNCONF=...`
3. **Flash helpers** that drive `fastboot`/`heimdall`/SP Flash Tool

The master orchestrator is `mobile/build-soc/build.sh`.

---

## 5) Building for MediaTek

### Supported SoCs

| Config | SoC | Device class |
|---|---|---|
| `MT6893` | Dimensity 1100 | 4xA78+4xA55, Mali-G77 MC9 |
| `MT6877` | Dimensity 920 | 2xA78+6xA55, Mali-G57 MC2 |
| `MT6833` | Dimensity 800U | 2xA76+6xA55, Mali-G57 MC2 |
| `MT6895` | Dimensity 9000 | 4xX2+3xA710+4xA510, Mali-G710 MC10 |
| `MT8983` | Dimensity 9300 | next-gen + Mali-G725 |

### Build

```bash
cd E:/freebsd-src
# Requires FreeBSD source tree and an aarch64 toolchain
make -C sys build KERNCONF=MT6893 \
    TARGET=arm64 TARGET_ARCH=aarch64

# Or use the wrapper script
bash mobile/mediatek/build.sh MT6893
```

### Flash

```bash
# fastboot
bash mobile/mediatek/flash/fastboot.sh

# Or SP Flash Tool (Download mode) via the Python tool
python3 mobile/mediatek/tools/mtk_dtbtool.py --mode download-mode --preloader <preloader.bin>
```

Notes:

- Use the vendor dtb from the stock board, not the QEMU one.
- The DTBO overlay applies display/modem/UFS nodes using the `dtc` compiler.
- For MTK boards in download mode, you need the `SP Flash Tool`-compatible port on the lower PC USB pins (BROM).
- Always keep a **hardware backup** (NAND/eMMC readback via `dd` + `ufs/mtk`) before flashing. Successful flash is not guaranteed.

---

## 6) Building for Qualcomm

### Supported SoCs

| Config | SoC | Device class |
|---|---|---|
| `SM8650` | Snapdragon 8 Gen 3 | X4 + A720 + A520, Adreno 750 |
| `SM8450` | Snapdragon 8cx Gen 3 / 8 Gen 2 | X3 + A715 + A510, Adreno 730 |
| `SM8350` | Snapdragon 8cx Gen 2 | X1, Adreno 680 |
| `SM7450` | Snapdragon 7cx Gen 3 | A78 + A55, Adreno 642 |
| `SM7350` | Snapdragon 7 Gen 1 | A710 + A510, Adreno 662 |
| `SM6375` | Snapdragon 7s Gen 2 | 4xA78 + 4xA55, Adreno 710 |
| `SM6450` | Snapdragon 6 Gen 1 | 4xA78 + 4xA55, Adreno 640 |
| `SM6625` | Snapdragon 4 Gen 2 | 2xA78 + 6xA55, Adreno 610 |
| `MSM8998` | Snapdragon 835 | Kryo 280, Adreno 540 |

### Build

```bash
make -C sys build KERNCONF=SM8450 \
    TARGET=arm64 TARGET_ARCH=aarch64 XCC=clang XCXX=clang++ XCPP=clang-cpp

# Or via wrapper
bash mobile/qualcomm/build.sh SM8450
```

Key build options:

- `QCOM_USE_CLANG=1` is the default. LLVM is required for Hexagon + SDE.
- The dtb is assembled via `mkbootimg` using offsets from `mobile/build-soc/gen_bootimg.sh`.
- `QCOM_GCC`, `QCOM_RPM`, `QCOM_RPMH`, `QCOM_TSENS`, `QCOM_TZ` must all match the board's rpm memory layout.

### Flash

```bash
# Bootloader must be unlocked (fastboot oem unlock)
fastboot devices
bash mobile/qualcomm/flash/fastboot.sh
```

Some Pixel/Android-13-based boards require `--disable-verity --disable-verification`.

---

## 7) Building for Exynos / Samsung

### Supported SoCs

| Config | SoC | Device class |
|---|---|---|
| `S5E9925` | Exynos 2200 | X2 + A710 + A510, Xclipse 2200 (RDNA2) |
| `S5E9810` | Exynos 2100 | X1, Xclipse 920 |
| `S5E8845` | Exynos 1380 | 4xA78 + 4xA55, Mali-G68 MP4 |
| `S5E8535` | Exynos 1330 | 2xA78 + 6xA55, Mali-G68 MP2 |
| `S5E9830` | Exynos 990 | 2xM5 + 2xA76, Mali-G77 MP11 |
| `S5E9825` | Exynos 9825 | 2xM4 + 2xA75, Mali-G76 MP12 |
| `S5E5510` | Exynos 850 | 8xA55, Mali-G52 |

### Build

```bash
bash mobile/samsung/build.sh S5E9925
# or directly
make -C sys build KERNCONF=S5E9925 \
    TARGET=arm64 TARGET_ARCH=aarch64 XCC=clang XCXX=clang++ XCPP=clang-cpp
```

### Flash

Samsung devices use the Download (Odin / Heimdall) protocol:

```bash
bash mobile/samsung/flash.sh --board S5E9925
```

---

## 8) Building for Google Tensor (Pixel 6/7/8 lineage)

### Supported SoCs

| Config | SoC | Device class |
|---|---|---|
| `redfin` | Tensor G1 (Pixel 6/6 Pro/6a) | 2xX1 + 2xA76 + 4xA55 |
| `bluejay` | Tensor G2 (Pixel 7/7 Pro/7a) | 2xX1 + 2xA79 + 4xA510 |
| `zuma` | Tensor G3 (Pixel 8/8 Pro) | 1xX3 + 4xA715 + 4xA510 |

### Build

```bash
bash mobile/tensor/build.sh redfin
```

### Flash

```bash
# Factory image style
bash mobile/tensor/flash.sh --skip-verity --skip-verification --slot B

# Or sideload over adb recovery
adb sideload out/redfin/update.zip
```

---

## 9) Universal build + flash via build-soc

Instead of per-vendor scripts, `mobile/build-soc/build.sh` is the unified entry:

```bash
# List supported SoCs and boards
bash mobile/build-soc/Makefile soc-list
bash mobile/build-soc/Makefile soc-board SOC=MT6893

# Build
bash mobile/build-soc/build.sh --soc MT6893 --board X01BD --output out/MT6893/

# Flash (auto-detects vendor)
bash mobile/build-soc/flash.sh --soc MT6893 --mode fastboot

# Fastboot for all vendors at once
bash mobile/build-soc/fastboot_all.sh --soc MT6893

# Apply SoC-specific DT overlay before building
bash mobile/build-soc/apply_patch.sh --soc MT6893 --board X01BD --dtb in.dtb --out out.dtb

# Create super partition layout
bash mobile/build-soc/create_super.sh --config out/MT6893/
```

### apply_patch.sh details

The patch script:

1. Starts from the SoC's base DTB (or generated `riscv-virt-mobile.dtb` for the virtenv).
2. Overlays per-board properties using `dtc -I dts -O dtb -o out.dtb merged.dts`.
3. Overlay DTS files live at the per-board path: `mobile/soc/mediatek/board/X01BD-overlay.dts` (not yet committed, regenerated by `apply_patch.sh`).

### gen_bootimg.sh details

mkbootimg offsets vary per SoC. The script uses:

```bash
# MTK: 32-bit kernel + 64-bit ramdisk for legacy 32-bit bootloaders
MKBOOTIMG_ARGS=( \
  --kernel arch/arm64/boot/Image \
  --ramdisk boot/ramdisk.cpio.gz \
  --pagesize 4096 \
  --base 0x40000000 \
  --kernel_offset 0x00008000 \
  --ramdisk_offset 0x02000000 \
  --tags_offset 0x00000100 \
  --os_version 13.0.0 \
  --os_patch_level 2024-01-01 \
  --header_version 4)

# QCOM/Google: DTB appended to zImage + cmdline
MKBOOTIMG_ARGS=( \
  --kernel /boot/Image-dtb \
  --ramdisk boot/ramdisk.cpio.gz \
  --cmdline "console=ttyMSM0,115200n8 quiet root=/dev/sda1" \
  --pagesize 4096 \
  --base 0x00000000 \
  --tags_offset 0x01E00000 \
  --kernel_offset 0x00008000 \
  --ramdisk_offset 0x02000000 \
  --dtb_offset 0x01F00000 \
  --header_version 4)
```

### create_super.sh details

Dynamic super partition layout (ext4 for recovery or f2fs for performance):

```
/boot      - 256 MB (ext4)   raw boot image
/recovery  - 128 MB (ext4)   recovery + fastbootd
/system_ext - 3-4 GB         apps, base OS
/vendor    - 800 MB          HAL, blobs
/odm       - 512 MB          device-specific
/product   - 1 GB            features by carrier/google
/cache     - 512 MB (f2fs)  dalvik-cache, cache files
```

`create_super.sh` auto-sizes partition sizes to the output image and writes an MBR-style partition table.

---

## 10) Running and testing the system

### Desktop / QEMU

After you build and launch the QEMU virtenv (the virtenv in this repo uses the RISC-V virtualized guest as the "OS-in-VM" platform; the host is x86_64 running QEMU):

- **Terminal / virtcon**: serial stdio / VNC on :1
- **QEMU monitor**: telnet localhost 4444
- **GDB**: `qemu-system-riscv64 -gdb tcp::1234 -S`, then `riscv64-unknown-elf-gdb mobile/vmlinux.riscv64`

The desktop rendering path uses the following components at runtime:

- `displayd` (daemon): initializes display, manages brightness, DPMS, screen blanking
- `compositor` (userspace): draws surfaces via `mobile/gfx/GLES2` onto shared memory buffer / DRM gem dumb buffer
- `Wayland surface` (protocol): clients render via EGL, the compositor composites them
- `input backend`: evdev -> multitouch pool -> gesture recognizer -> desktop
- `panel/dock/overview`: managed in userspace by the desktop shell and the `mobile/desktop/` module

### Mobile (on-device)

#### Prerequisites

- Unlocked bootloader (`fastboot oem unlock` or vendor-specific)
- Working `fastboot`/`adb` or Heimdall (Samsung) or SP Flash Tool (MediaTek)
- 3–4 GB free on the phone internal storage (for `/boot`, `/system_ext`, `/vendor`, `/odm`, `/product`)

#### Flash flow (real hardware)

```bash
# 1) Boot into bootloader
adb reboot bootloader

# 2) Check device
fastboot devices

# 3) Build
bash mobile/build-soc/build.sh --soc MT6893 --board X01BD --output out/X01BD/

# 4) Flash a subset
fastboot flash boot   out/X01BD/boot.img
fastboot flash dtbo   out/X01BD/dtbo.img
fastboot flash system_ext out/X01BD/super.img
# For testing, omit vendor/odm/product to save time

# 5) Reboot
fastboot reboot
```

#### Vendor-specific unlock steps

MediaTek — **SP Flash Tool** (Download mode):

```bash
# 1) Power off
# 2) Hold Vol+ + Vol- + plug USB cable (BROM)
# 3) Run
python3 mobile/mediatek/tools/mtk_dtbtool.py --mode download-mode --preloader <preloader.bin>
```

Qualcomm — **fastboot** (EEL / UFS devices):

```bash
fastboot flashing unlock_critical
# or (older)
fastboot oem unlock
```

Samsung — **Odin mode** (Home + Vol-Down + USB cable), then use Heimdall:

```bash
heimdall flash --BOOT out/boot.img --DTBO out/dtbo.img
```

Google — **fastboot** (Pixel unlock via settings or `fastboot flashing unlock`):

```bash
fastboot flashing unlock
fastboot flash boot out/boot.img
```

---

## 11) Project directory map

```
freebsd-src/
├── tools/
│   ├── run-uos-qemu.sh          ← ONE COMMAND to build+test in QEMU
│   ├── riscv/
│   │   ├── build-riscv-kernel.sh
│   │   └── qemu-riscv-run.sh
│   ├── mediatek/
│   │   ├── build.sh
│   │   └── flash/
│   ├── qualcomm/
│   │   ├── build.sh
│   │   └── flash/
│   └── run_unified_tests.sh
├── mobile/                       ← ALL MOBILE OS CODE
│   ├── Makefile                  ← top-level build (17 targets)
│   ├── kernel/                   ← standalone RISC-V mini-kernel
│   │   ├── Makefile
│   │   ├── entry.S
│   │   └── linker.ld
│   ├── virtenv/                  ← QEMU virtenv (DTB, rootfs, scripts)
│   │   └── devicetree/
│   ├── gfx/                      ← DRM/KMS + GBM + EGL + GLES2
│   ├── wayland/                  ← Wayland compositor server
│   ├── desktop/                  ← Full DE (dock, panel, wallpaper, etc.)
│   ├── ui/                       ← UI toolkit, compositor, window mgr
│   ├── input/                    ← evdev, multitouch, gestures, keyboard
│   ├── frameworks/               ← App runtime (zygote, IPC, sandbox, etc.)
│   ├── apps/                     ← Home, Settings, Terminal, Browser, etc.
│   ├── services/                 ← init, displayd, powerd, networkd, audiod
│   ├── pkg/                      ← Package manager (pkgctl + libpkg)
│   ├── audio/                    ← Pipeline, mixer, DSP, ALSA, routing
│   ├── bluetooth/                ← HCI core, adapter, device, profiles
│   ├── connectivity/             ← WiFi, DHCP, network mgr, cellular, VPN, NFC
│   ├── power/                    ← cpufreq, thermal, suspend, display power
│   ├── sensors/                  ← accel, gyro, fusion, GPS
│   ├── camera/                   ← Camera2 API, mock test pattern
│   ├── usb/                      ← libusb-compatible API
│   ├── rootfs/                   ← /etc skeleton + rc.d scripts
│   ├── etc/                      ← Source rc.conf template
│   ├── soc/                      ← SoC abstraction (MTK/QCOM/Samsung/Google/Apple)
│   ├── mediatek/                 ← MTK configs + build + flash
│   │   └── configs/              ← MT6893/MT6877/MT6833/MT6895/MT8983
│   ├── qualcomm/                 ← QCOM configs + build + flash
│   │   └── configs/              ← SM8450/SM8350/SM7450/SM7350/SM6375/SM6450/SM6625/MSM8998
│   ├── samsung/                  ← Exynos configs + build + flash
│   │   └── configs/              ← S5E9925/S5E9810/S5E8845/S5E8535/S5E9830/S5E9825/S5E5510
│   ├── tensor/                   ← Google Tensor configs + build + flash
│   │   └── configs/              ← redfin/bluejay/zuma
│   └── build-soc/                ← Universal multi-SoC build + flash orchestrator
│       ├── build.sh
│       ├── flash.sh
│       ├── fastboot_all.sh
│       ├── apply_patch.sh
│       ├── gen_bootimg.sh
│       ├── create_super.sh
│       └── config/               ← Per-vendor default.mk
└── sys/
    └── arm64/conf/               ← FreeBSD kernel configs for each SoC
        ├── std.exynos
        └── MEDIATEK
```
