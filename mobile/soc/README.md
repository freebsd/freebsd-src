# SoC Abstraction Framework

BSD-based Mobile OS System-on-Chip abstraction layer supporting multiple mobile SoC vendors.

## Overview

This framework provides a unified interface for detecting and controlling mobile SoCs from various vendors, abstracting hardware differences for portable driver development.

## Supported Platforms

### MediaTek
- **Dimensity series**: MT6893 (1100), MT6891 (1200), MT6877 (920), MT6895 (9000), MT6983 (9100), MT6985 (9200), MT6989 (9300), MT6991 (9400)
- **Helio series**: MT6833 (800U/720), MT6769 (G90/G95), MT6768 (G80), MT6765 (G25), MT6739 (entry), MT6762 (G35)

### Qualcomm
- **Snapdragon compute**: SM8450 (8cx Gen 3), SM8350 (8cx Gen 2), SM8475 (8+ Gen 1), SM8550 (8 Gen 2)
- **Snapdragon mobile**: SM7450 (7cx Gen 3), SM7350 (7 Gen 1), SM6375 (7s Gen 2), SM6450 (6 Gen 1), SM6625 (4 Gen 2)
- **Legacy**: MSM8998 (835), MSM8996 (821/820)

### Samsung
- Samsung Exynos 2200 (S5E9925), 2100 (S5E9810), 1380 (S5E8845), 1330 (S5E8535), 990 (S5E9830), 9825 (S5E9825), 850 (S5E5510)
- Xclipse GPU (AMD RDNA2-based)

### Google
- Tensor G3 (Zuma), Tensor G2 (Zuma), Tensor G1 (Redfin)
- TPU (Tensor Processing Unit) for ML acceleration
- Titan M2 security coprocessor

### Apple
- A17 Pro (T8112), A16 (T8110), A15 (T8101), A14 (T8100)
- Apple Neural Engine (ANE)
- Apple GPU (G14P/G13C)

## Building

```sh
cd mobile/soc
make
```

## Usage

```c
#include <mobile/soc.h>

/* Early initialization */
soc_early_init();

/* Full detection and init */
soc_init();

/* Get SoC information */
const char *name = soc_get_name();
int cores = soc_get_cpus();
uint32_t max_freq = soc_get_max_freq();
const soc_desc_t *desc = soc_get_desc();

/* Vendor-specific access */
uintptr_t base = mtk_get_reg_base(MTK_UART0);  /* or qcom_get_reg_base() etc. */
```

## Detection

SoC detection uses the device tree compatible string at `/sys/firmware/devicetree/base/compatible`. Each vendor module implements its own detection function that parses vendor-specific identifiers.

## License

BSD-2-Clause