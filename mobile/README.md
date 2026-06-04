# UOS(m) Mobile OS — Developer & Usage Guide

> **Branch:** `mobile-os` (ahead of origin by several commits)
> **Kernel target:** `mobile/kernel` builds `vmlinux.riscv64` for QEMU RISC-V virt
> **Userspace:** complete: compositor, shell, apps, services, frameworks

---

## 1. What you are looking at

This tree contains the full source of UOS(m):

- `mobile/kernel/` — freestanding kernel for RISC-V (entry.S, linker.ld, kernel.c, Makefile)
- `mobile/virtenv/` — virtual test environment (scripted QEMU, DTB, set-up wizard)
- `mobile/ui/` — compositor, window manager, input layer, system ui, gesture recogniser
- `mobile/desktop/` — dock, panel, wallpaper, lockscreen, notifications, global search
- `mobile/frameworks/` — app runtime: sandbox, IPC, permissions, lifecycle, activities
- `mobile/apps/` — Home, Settings, Terminal, Browser, Contacts, Messages
- `mobile/services/` — init, service manager, displayd, powerd, networkd, audiod, timed
- `mobile/gfx/` — DRM/KMS + GBM + EGL + OpenGL ES 2.0 renderer + software fallback
- `mobile/wayland/` — Wayland compositor server (DRM backend, seat, xdg-shell)
- `mobile/audio/` — audio pipeline, mixer, DSP, ALSA compat, Bluetooth audio routing
- `mobile/bluetooth/` — HCI core + A2DP/HFP/HSP profiles
- `mobile/connectivity/` — WiFi, DHCP, network manager, cellular, VPN, NFC
- `mobile/power/` — cpufreq, thermal, suspend/resume, display power
- `mobile/sensors/` — accelerometer, gyroscope, sensor fusion (Madgwick), GPS
- `mobile/camera/` — Camera2 API + mock test pattern
- `mobile/usb/` — libusb-compatible host controller
- `mobile/pkg/` — `pkgctl` CLI + flat-file/SQLite backend + repository support
- `mobile/soc/` — SoC abstraction for MediaTek / Qualcomm / Samsung / Google / Apple
- `mobile/mediatek/` — Dimensity kernel configs (MT6893/MT6877/MT6833/MT6895/MT8983)
- `mobile/qualcomm/` — Snapdragon kernel configs (SM8450 … MSM8998)
- `mobile/samsung/` — Exynos kernel configs (S5E9925 … S5E5510)
- `mobile/tensor/` — Google Tensor configs (redfin/bluejay/zuma)
- `mobile/build-soc/` — universal multi-SoC build + flash (fastboot / heimdall / SP Flash)

---

## 2. Try it in QEMU (no phone required)

### Script entry point

```bash
# From repo root:
bash tools/run-uos-qemu.sh                  # 4 vCPUs, 2 GB RAM
bash tools/run-uos-qemu.sh --cores 8 --mem 4G   # bigger VM
bash tools/run-uos-qemu.sh --headless          # VNC on :5900
bash tools/run-uos-qemu.sh --gdb               # GDB stub on :1234
```

### What the script does

1. Checks for `qemu-system-riscv64`, `qemu-img`, an optional RISC-V GDB
2. Rebuilds `mobile/vmlinux.riscv64` if it is missing or empty
3. Creates `tools/riscv/images/uos-riscv.img` (4 GB raw) if it does not exist
4. Prints the connection table:

| Service | Address |
|---------|---------|
| Graphical desktop | GTK / SDL window (or VNC :5900 with `--headless`) |
| Serial / debug log | this terminal |
| SSH | `ssh -p 2222 root@localhost` |
| HTTP rootfs | `http://localhost:8080` |

5. `exec`s `qemu-system-riscv64`. When that process ends the script ends too.

### QEMU one-liner

The same VM can be launched via the Makefile target:

```bash
make -C mobile qemu
# identical to:
# cd mobile && make qemu
```

This runs `make quick` followed by `bash tools/run-uos-qemu.sh`.

### Quick sanity test

```bash
# Inside the VM serial console:
root@uos:~ # uname -a
root@uos:~ # cat /etc/issue
root@uos:~ # service audiod status
root@uos:~ # ls /mobile/share/applications/
```

### Host requirements

- Linux or WSL2
- `qemu-system-riscv64`
- `qemu-img`
- `gcc-riscv64-unknown-elf` (optional; build falls back without it)
- `device-tree-compiler` (`dtc`, optional)

---

## 3. SoC support matrix (real hardware)

UOS(m) ships kernel configs and flash logic for the most widely deployed
mobile SoCs in the wild today.

### MediaTek — `mobile/mediatek/`

| Config | SoC | CPU | GPU |
|--------|-----|-----|-----|
| `MT6893` | Dimensity 1100 | 4×A78 + 4×A55 | Mali-G77 MC9 |
| `MT6877` | Dimensity 920 | 2×A78 + 6×A55 | Mali-G57 MC2 |
| `MT6833` | Dimensity 800U | 2×A76 + 6×A55 | Mali-G57 MC2 |
| `MT6895` | Dimensity 9000 | 4×X2 + 3×A710 + 4×A510 | Mali-G710 MC10 |
| `MT8983` | Dimensity 9300 | next-gen | Mali-G725 |
| `MT6991` | Dimensity 9400 | Cortex-X5-class | latest Mali |

Flash: `fastboot` or `SP Flash Tool` (Download / BROM mode).

### Qualcomm — `mobile/qualcomm/`

| Config | SoC | CPU | GPU |
|--------|-----|-----|-----|
| `SM8450` | Snapdragon 8cx Gen 3 / 8 Gen 2 | X3 + A715 + A510 | Adreno 730 |
| `SM8350` | Snapdragon 8cx Gen 2 | X1 + A78 + A55 | Adreno 680 |
| `SM7450` | Snapdragon 7cx Gen 3 | A78 + A55 | Adreno 642 |
| `SM7350` | Snapdragon 7 Gen 1 | A710 + A510 | Adreno 662 |
| `SM6375` | Snapdragon 7s Gen 2 | 4×A78 + 4×A55 | Adreno 710 |
| `SM6450` | Snapdragon 6 Gen 1 | 4×A78 + 4×A55 | Adreno 640 |
| `SM6625` | Snapdragon 4 Gen 2 | 2×A78 + 6×A55 | Adreno 610 |
| `MSM8998` | Snapdragon 835 | Kryo 280 | Adreno 540 |

Flash: `fastboot` (or EDL download mode with QFIL).

### Samsung Exynos — `mobile/samsung/`

| Config | SoC | CPU | GPU |
|--------|-----|-----|-----|
| `S5E9925` | Exynos 2200 | X2 + A710 + A510 | Xclipse 2200 (RDNA2) |
| `S5E9810` | Exynos 2100 | X1 + A78 + A55 | Xclipse 920 |
| `S5E8845` | Exynos 1380 | 4×A78 + 4×A55 | Mali-G68 MP4 |
| `S5E8535` | Exynos 1330 | 2×A78 + 6×A55 | Mali-G68 MP2 |
| `S5E9830` | Exynos 990 | 2×M5 + 2×A76 + 4×A55 | Mali-G77 MP11 |
| `S5E9825` | Exynos 9825 | 2×M4 + 2×A75 + 4×A55 | Mali-G76 MP12 |
| `S5E5510` | Exynos 850 | 8×A55 | Mali-G52 |

Flash: Samsung Download mode via `heimdall` or Odin.

### Google Tensor — `mobile/tensor/`

| Config | SoC | CPU |
|--------|-----|-----|
| `redfin` | Tensor G1 (Pixel 6/6 Pro/6a) | 2×X1 + 2×A76 + 4×A55 |
| `bluejay` | Tensor G2 (Pixel 7/7 Pro/7a) | 2×X1 + 2×A79 + 4×A510 |
| `zuma` | Tensor G3 (Pixel 8/8 Pro) | 1×X3 + 4×A715 + 4×A510 |

Flash: `fastboot` (factory-image style); `--disable-verification` for dev.

---

## 4. Build + flash with build-soc

`mobile/build-soc/` is the unified build orchestrator.

```bash
cd E:\freebsd-src

# List every supported SoC
bash mobile/build-soc/build.sh --list-socs

# List boards for one SoC
bash mobile/build-soc/build.sh --list-boards --soc MT6893

# Build for a specific phone
bash mobile/build-soc/build.sh --soc MT6893 --board X01BD --output out/X01BD/

# Auto-flash to whatever device is connected
bash mobile/build-soc/flash.sh --soc MT6893 --board X01BD

# fastboot path (all vendors)
bash mobile/build-soc/fastboot_all.sh --soc MT6893 --slot a

# Patch DT before building (UFS, display, modem, GPU, sensors)
bash mobile/build-soc/apply_patch.sh \
    --soc MT6893 --board X01BD --dtb in.dtb --out out.dtb

# Wrap kernel+ramdisk+dtb in a vendor-compatible boot image
bash mobile/build-soc/gen_bootimg.sh \
    --kernel arch/arm64/boot/Image \
    --ramdisk boot/ramdisk.cpio.gz \
    --dtb out.dtb \
    --output out/X01BD/boot.img \
    --pagesize 4096 \
    --base 0x40000000 \
    --kernel_offset 0x00008000 \
    --ramdisk_offset 0x02000000 \
    --tags_offset 0x00000100 \
    --header_version 4

# Build the super partition (system_ext, vendor, odm, product, cache)
bash mobile/build-soc/create_super.sh --config out/X01BD/config.json
```

### SoC abstraction layer

`mobile/soc/` exposes vendor-neutral APIs:

```c
#include <mobile/soc/soc.h>

soc_vendor_t v = soc_detect();          // MEDIATEK / QUALCOMM / SAMSUNG / GOOGLE
soc_model_t  m = soc_get_model();
const char  *name = soc_get_name();     // "Dimensity 920", "Snapdragon 8cx", ...
int cpus = soc_get_cpus();              // 4, 6, 8 …
uint32_t max_mhz = soc_get_max_freq();  // 2200, 3000 …
```

Peripherals are accessed via base-address tables in:
- `mobile/soc/mediatek.h` — UART, I2C, SPI, GPIO, APMIXED, AUDIO, WLAN, MD …
- `mobile/soc/qualcomm.h` — GSBI, RPM/RPMH, GICC/GICD, ADreno GPU, MDSS, DSI …
- `mobile/soc/samsung.h` — PMU, MCT, DPU, FIMC, modem
- `mobile/soc/google.h` — TPU, Titan M

---

## 5. Flash by vendor

### MediaTek

```bash
# fastboot
bash mobile/mediatek/flash/fastboot.sh

# SP Flash Tool (BROM / download mode)
python3 mobile/mediatek/tools/mtk_dtbtool.py --mode download \
    --preloader preloader_MT6893.bin --scatter scatter_MT6893.txt

# Unlock
fastboot oem unlock
```

### Qualcomm

```bash
bash mobile/qualcomm/flash/fastboot.sh

# EDL (Emergency Download)
# Power off + hold Vol+ while plugging USB, then:
python3 tools/qfil.py --mode emmc --firehose prog_firehose_ddr.elf

# Unlock
fastboot flashing unlock_critical
```

### Samsung

```bash
bash mobile/samsung/flash.sh --board S5E9925

# Or Heimdall directly
heimdall flash --BOOT out/S5E9925/boot.img --DTBO out/S5E9925/dtbo.img

# Odin-compatible tar
tar -H posix -cf flash_S5E9925.tar boot.img dtbo.img super.img
md5sum flash_S5E9925.tar > flash_S5E9925.tar.md5
```

---

## 6. Partition layout (the storage story)

UOS(m) uses a standard Android-compatible GPT / f2fs+ext4 layout on phones
and a raw-IDE VirtIO layout inside QEMU.

```
Phone storage (GPT):
│─ boot       256 MB  ext4        boot + kernel
│─ vbmeta       1 MB  ext4        verified-boot metadata
│─ dtbo         8 MB  ext4        device-tree overlay
│─ super      4-8 GB  ext4/f2fs   dynamic partition
│                ↳ system_ext       base OS + apps
│                ↳ vendor           HAL + firmware blobs
│                ↳ odm              device-specific
│                ↳ product          carrier / google features
│─ cache     512 MB  f2fs         dalvik-cache, misc caches
│─ userdata   rest    f2fs        apps + user data
```

`mobile/rootfs/etc/fstab` is the canonical mount table; QEMU uses the
simplified variant baked into `tools/run-uos-qemu.sh` via `root=/dev/vda rw`.

---

## 7. Live “experience” checklist

After `bash tools/run-uos-qemu.sh` succeeds you are running UOS(m):

1. **Compositor** — the Wayland shell draws a desktop using OpenGL ES.
2. **Wayland clients** — Home, Browser, Contacts, … run as sandboxed clients.
3. **SSH** — reach the VM from the host on `:2222` to poke around as root.
4. **Telnet monitor** — `telnet localhost 4445` gives you `info version`,
   `info registers`, and drive/file QMP.
5. **Shared folder** — `mount -t 9p -o trans=virtio,version=9p2000.L hostshare /mnt/host`
   exposes the freebsd-src tree inside the VM.
