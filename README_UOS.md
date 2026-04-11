# UOS - Universal Operating System (Mobile Port)

UOS is an advanced Mobile OS derived from FreeBSD's kernel infrastructure, specifically hardened and adapted for modern mobile and edge devices. It utilizes heavily customized OpenBSD-style security paradigms, low-latency mobile schedulers, and is capable of running Bionic-shim layers for backward compatibility. 

This repository expands FreeBSD to serve as a high-performance foundation for mobile systems across **MediaTek, Qualcomm, and RISC-V** SoC architectures.

---

## 📱 Supported Target Architectures

We focus on creating production environments for real hardware and matching 1:1 QEMU Virtualization nodes for isolated testing.

#### 1. Qualcomm (Snapdragon)
* **Targets**: Snapdragon series (SM8550, SM8650).
* **Environment**: `sys/arm64/conf/QCOM` (Hardware) / `QCOM-QEMU` (Virtualization).
* **Hardware Specifics**: Geared toward Flagship Snapdragon processing utilizing Cortex-X cores, integrating tightly with `pac-ret` pointer authentication and full BTI branching protection.

#### 2. MediaTek (Dimensity / Genio)
* **Targets**: MediaTek MT8395 (Genio 1200) equivalent e.g., Radxa NIO 12L.
* **Environment**: `sys/arm64/conf/MEDIATEK` (Hardware) / `MEDIATEK-QEMU` (Virtualization).
* **Hardware Specifics**: Focuses heavily on the MT8195 AFE (Audio Subsystem), DRM displays, and complex 4+4 core big.LITTLE architectures via SMP.

#### 3. RISC-V (StarFive)
* **Targets**: StarFive VisionFive 2 (JH7110 / RV64GC).
* **Environment**: `sys/riscv/conf/UOS-STARFIVE` (Hardware) / `UOS-RISCV-QEMU` (Virtualization).
* **Hardware Specifics**: Advanced port targeting RISC-V adoption in mobile form constraints, operating alongside OpenSBI bootstrapping.

---

## 🛠 Building the System

All mobile builds are routed through the `tools/` directory and leverage FreeBSD's internal `make.py` wrapper, allowing flexible cross-compilation from typical x86_64 host machines (WSL/Linux/macOS).

### Unified Build
To compile the kernels for **all configurations across all architectures** at once:
```bash
./tools/build_all_architectures.sh
```

### Specific Architecture Build
You can build targets individually by using their specific build shell components. By default, running these scripts targets the QEMU configuration.
```bash
# MediaTek
./tools/mediatek/build-mediatek-kernel.sh [MEDIATEK | MEDIATEK-QEMU]

# Qualcomm
./tools/qualcomm/build-qcom-kernel.sh [QCOM | QCOM-QEMU]

# RISC-V
./tools/riscv/build-riscv-kernel.sh [UOS-STARFIVE | UOS-RISCV-QEMU]
```

---

## 📦 Creating Mobile Boot Images

For Mobile OS flashing, we deploy via Android's `fastboot` specifications leveraging `mkbootimg`. These scripts generate `.img` targets that map specifically to device RAM profiles. 

To generate a boot image for a specific architecture, navigate to its tooling directory:
```bash
./tools/qualcomm/create-mobile-boot-image.sh QCOM
./tools/riscv/create-mobile-boot-image.sh UOS-STARFIVE
```

*Note: You must have Android's `mkbootimg` executable present in your `$PATH`.*

---

## ⚙️ Running Automated Tests

Testing is handled via QEMU virtualization instances matched directly to real hardware capability definitions.

### Executing All Tests
The unified test runner traverses all architectures and tests subsystems (Core logic, Display, GPU, Audio, UART, etc.).

```bash
./tools/run_unified_tests.sh
```
This is ideal for pre-commit or CI/CD pipelines to ensure cross-architecture feature parity mapping does not regress. 

### Executing Specific Tests
You can debug manually into specific systems by directly executing their sub-test endpoints. Be aware that most of these tests utilize QEMU directly inside the terminal.
```bash
# Example: Qualcomm Core QEMU evaluation
./tools/qualcomm/tests/test_qcom_core.sh

# Example: MediaTek GUI / GPU DRM validation subsystem layer
./tools/mediatek/tests/test_display_gpu.sh

# Example: RISC-V Mobile QEMU evaluation
./tools/riscv/tests/test_riscv_core.sh
```

For more specifics on MediaTek testing configurations, you can view `tools/mediatek/README_QEMU_TESTING.md`.
