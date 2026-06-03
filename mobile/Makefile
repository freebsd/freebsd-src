# Mobile OS - Top-Level Makefile
# Builds the complete UOS(m) Mobile OS from one entry point

.PHONY: all clean config kernel ui pkg services apps drivers frameworks arch rootfs install

SUBDIRS = kernel ui pkg services
APPS = home_screen settings terminal browser contacts messages

all: kernel ui pkg services apps

config:
	@echo "Configuring Mobile OS..."
	cd kernel && $(MAKE) config.h

kernel:
	@echo "Building kernel..."
	cd kernel && $(MAKE) all

ui:
	@echo "Building UI framework..."
	cd ui && $(MAKE) all

pkg:
	@echo "Building package manager..."
	cd pkg && $(MAKE) all

services:
	@echo "Building system services..."
	cd services && $(MAKE) all

apps:
	@echo "Building first-party apps..."
	cd apps && $(MAKE) all

drivers:
	@echo "Building mobile drivers..."
	cd drivers/touchscreen && $(MAKE) all
	cd drivers/sensors && $(MAKE) all

frameworks:
	@echo "Building app frameworks..."
	cd frameworks/app && $(MAKE) all

arch:
	@echo "Building architecture support..."
	cd arch/riscv && $(MAKE) -f Makefile.riscv riscv-build

rootfs:
	@echo "Preparing root filesystem..."
	@mkdir -p rootfs/mobile/{bin,lib,share}
	@cp services/bin/* rootfs/mobile/bin/ 2>/dev/null || true
	@cp pkg/pkgctl rootfs/mobile/bin/ 2>/dev/null || true
	@mkdir -p rootfs/etc/pkgctl

install: all rootfs
	@echo "Installing to target rootfs..."
	@mkdir -p /mnt/uos
	cp -r rootfs/* /mnt/uos/ 2>/dev/null || \
		(cp -r rootfs/* ./install-staging/ && echo "Staged to ./install-staging/")

clean:
	cd kernel && $(MAKE) clean
	cd ui && $(MAKE) clean
	cd pkg && $(MAKE) clean
	cd services && $(MAKE) clean
	cd apps && $(MAKE) clean
	rm -f install-staging
	rm -f mobile/vmlinux.riscv64

quick:
	cd kernel && $(MAKE) all QEMU=1 CONFIG_QEMU=1

qemu: quick
	bash tools/riscv/qemu-riscv-run.sh

test:
	bash tools/run_unified_tests.sh

help:
	@echo "UOS(m) Mobile OS Build System"
	@echo ""
	@echo "Targets:"
	@echo "  all          - Build everything (default)"
	@echo "  kernel       - Build kernel only"
	@echo "  ui           - Build UI framework"
	@echo "  pkg          - Build package manager"
	@echo "  services     - Build system services"
	@echo "  apps         - Build first-party apps"
	@echo "  arch         - Build RISC-V architecture support"
	@echo "  rootfs       - Prepare root filesystem"
	@echo "  install      - Install to target"
	@echo "  quick        - Quick RISC-V QEMU build"
	@echo "  qemu         - Build + launch QEMU"
	@echo "  test         - Run test suite"
	@echo "  clean        - Clean all artifacts"
