# UOS(m) Mobile OS - Top-Level Build System
.PHONY: all clean config kernel ui gfx wayland input desktop pkg services frameworks apps power sensors camera usb connectivity bluetooth audio rootfs install quick qemu test help

all: kernel ui gfx wayland input desktop pkg services frameworks apps power sensors camera usb connectivity bluetooth audio rootfs

config:
	@echo "Configuring Mobile OS..."
	cd kernel && $(MAKE) config.h

kernel:
	@echo "Building kernel..."
	cd kernel && $(MAKE) all

ui:
	@echo "Building UI framework..."
	cd ui && $(MAKE) all

gfx:
	@echo "Building graphics stack (DRM/KMS + GBM + EGL + GLES)..."
	cd gfx && $(MAKE) all

wayland:
	@echo "Building Wayland compositor..."
	cd wayland && $(MAKE) all

input:
	@echo "Building input subsystem..."
	cd input && $(MAKE) all

desktop:
	@echo "Building desktop environment..."
	cd desktop && $(MAKE) all

pkg:
	@echo "Building package manager..."
	cd pkg && $(MAKE) all

services:
	@echo "Building system services..."
	cd services && $(MAKE) all

frameworks:
	@echo "Building app frameworks..."
	cd frameworks && $(MAKE) all

apps:
	@echo "Building first-party apps..."
	cd apps && $(MAKE) all

power:
	@echo "Building power management..."
	cd power && $(MAKE) all
	cd sensors && $(MAKE) all

sensors:
	@echo "Building sensor framework..."
	cd sensors && $(MAKE) all

camera:
	@echo "Building camera subsystem..."
	cd camera && $(MAKE) all

usb:
	@echo "Building USB subsystem..."
	cd usb && $(MAKE) all

connectivity:
	@echo "Building connectivity (WiFi/Cellular/VPN/NFC)..."
	cd connectivity && $(MAKE) all

bluetooth:
	@echo "Building Bluetooth stack..."
	cd bluetooth && $(MAKE) all

audio:
	@echo "Building audio subsystem..."
	cd audio && $(MAKE) all

rootfs:
	@echo "Preparing root filesystem..."
	@mkdir -p rootfs/mobile/{bin,lib,share,lib/modules,lib/frameworks}
	@cp services/bin/* rootfs/mobile/bin/ 2>/dev/null || true
	@mkdir -p rootfs/etc/pkgctl

install: all rootfs
	@echo "Installing to target rootfs..."
	mkdir -p /mnt/uos
	cp -r rootfs/* /mnt/uos/ 2>/dev/null || \
		(cp -r rootfs/* ./install-staging/ && echo "Staged to ./install-staging")

clean:
	cd kernel && $(MAKE) clean
	cd ui && $(MAKE) clean
	cd gfx && $(MAKE) clean
	cd wayland && $(MAKE) clean
	cd input && $(MAKE) clean
	cd desktop && $(MAKE) clean
	cd pkg && $(MAKE) clean
	cd services && $(MAKE) clean
	cd frameworks && $(MAKE) clean
	cd apps && $(MAKE) clean
	cd power && $(MAKE) clean
	cd sensors && $(MAKE) clean
	cd camera && $(MAKE) clean
	cd usb && $(MAKE) clean
	cd connectivity && $(MAKE) clean
	cd bluetooth && $(MAKE) clean
	cd audio && $(MAKE) clean
	rm -f install-staging
	rm -f mobile/vmlinux.riscv64

quick:
	cd kernel && $(MAKE) all QEMU=1 CONFIG_QEMU=1

qemu: quick
	bash tools/run-uos-qemu.sh

vm-test: quick
	bash tools/run-uos-qemu.sh --headless --cores 4 --mem 2G

test:
	bash tools/run_unified_tests.sh

help:
	@echo "UOS(m) Mobile OS Build System"
	@echo ""
	@echo "Targets:"
	@echo "  all            - Build everything (default)"
	@echo "  kernel         - Build kernel"
	@echo "  ui             - Build UI framework"
	@echo "  gfx            - Build DRM/KMS + GBM + EGL + GLES"
	@echo "  wayland        - Build Wayland compositor"
	@echo "  input          - Build input subsystem (evdev, multi-touch)"
	@echo "  desktop        - Build desktop environment (dock, panel, wallpaper)"
	@echo "  pkg            - Build package manager"
	@echo "  services       - Build system services (init, displayd, powerd, etc)"
	@echo "  frameworks     - Build app frameworks (sandbox, IPC, lifecycle)"
	@echo "  apps           - Build first-party apps"
	@echo "  power          - Build power/thermal/suspend"
	@echo "  sensors        - Build sensor framework"
	@echo "  camera         - Build camera subsystem"
	@echo "  usb            - Build USB subsystem"
	@echo "  connectivity   - Build WiFi/Cellular/VPN/NFC"
	@echo "  bluetooth      - Build Bluetooth stack"
	@echo "  audio          - Build ALSA + Bluetooth audio"
	@echo "  rootfs         - Prepare root filesystem"
	@echo "  install        - Install to target"
	@echo "  quick          - Quick RISC-V QEMU build"
	@echo "  qemu           - Build + launch QEMU"
	@echo "  test           - Run test suite"
	@echo "  clean          - Clean all artifacts"
