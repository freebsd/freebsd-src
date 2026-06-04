/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2024 FreeBSD Mobile Project
 * All rights reserved.
 *
 * MediaTek SoC support implementation.
 */

#include <sys/types.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "mediatek.h"
#include "soc.h"

/* MediaTek device tree compatible strings */
#define MTK_COMPATIBLE_MT6893    "mediatek,mt6893"
#define MTK_COMPATIBLE_MT6891    "mediatek,mt6891"
#define MTK_COMPATIBLE_MT6877    "mediatek,mt6877"
#define MTK_COMPATIBLE_MT6895    "mediatek,mt6895"
#define MTK_COMPATIBLE_MT6983    "mediatek,mt6983"
#define MTK_COMPATIBLE_MT6985    "mediatek,mt6895"
#define MTK_COMPATIBLE_MT6989    "mediatek,mt6989"
#define MTK_COMPATIBLE_MT6991    "mediatek,mt6991"
#define MTK_COMPATIBLE_MT6833    "mediatek,mt6833"
#define MTK_COMPATIBLE_MT6769    "mediatek,mt6769"
#define MTK_COMPATIBLE_MT6768    "mediatek,mt6768"
#define MTK_COMPATIBLE_MT6765    "mediatek,mt6765"
#define MTK_COMPATIBLE_MT6739    "mediatek,mt6739"
#define MTK_COMPATIBLE_MT6762    "mediatek,mt6762"

/* MediaTek memory map - typical addresses */
static const uintptr_t mtk_reg_bases[] = {
    [MTK_UART0]     = 0x11020000,
    [MTK_UART1]     = 0x11030000,
    [MTK_I2C0]      = 0x11040000,
    [MTK_SPI0]      = 0x11050000,
    [MTK_GPIO]      = 0x11060000,
    [MTK_PERICFG]   = 0x10003000,
    [MTK_APMIXED]   = 0x10003200,
    [MTK_INFRACFG]  = 0x10003400,
    [MTK_PMIC_WRAP] = 0x10006000,
    [MTK_EFUSE]     = 0x10038000,
    [MTK_GCE]       = 0x10212000,
    [MTK_SMI]       = 0x10400000,
    [MTK_M4U]       = 0x10410000,
    [MTK_DRAM]      = 0x00000000,
    [MTK_GPU]       = 0x13FB0000,
    [MTK_AUDIO]     = 0x11220000,
    [MTK_WLAN]      = 0x11300000,
    [MTK_MD]        = 0x11400000,
    [MTK_SSUSB]     = 0x11200000,
};

static soc_model_t current_model = SOC_MODEL_MAX;

static const char *mtk_get_compatible(soc_model_t model)
{
    switch (model) {
    case SOC_MODEL_MT6893: return MTK_COMPATIBLE_MT6893;
    case SOC_MODEL_MT6891: return MTK_COMPATIBLE_MT6891;
    case SOC_MODEL_MT6877: return MTK_COMPATIBLE_MT6877;
    case SOC_MODEL_MT6895: return MTK_COMPATIBLE_MT6895;
    case SOC_MODEL_MT6983: return MTK_COMPATIBLE_MT6983;
    case SOC_MODEL_MT6985: return MTK_COMPATIBLE_MT6985;
    case SOC_MODEL_MT6989: return MTK_COMPATIBLE_MT6989;
    case SOC_MODEL_MT6991: return MTK_COMPATIBLE_MT6991;
    case SOC_MODEL_MT6833: return MTK_COMPATIBLE_MT6833;
    case SOC_MODEL_MT6769: return MTK_COMPATIBLE_MT6769;
    case SOC_MODEL_MT6768: return MTK_COMPATIBLE_MT6768;
    case SOC_MODEL_MT6765: return MTK_COMPATIBLE_MT6765;
    case SOC_MODEL_MT6739: return MTK_COMPATIBLE_MT6739;
    case SOC_MODEL_MT6762: return MTK_COMPATIBLE_MT6762;
    default: return NULL;
    }
}

soc_model_t
mtk_chip_detect(void)
{
    const char *compatible = NULL;

    /* Read compatible string from device tree */
    FILE *f = fopen("/sys/firmware/devicetree/base/compatible", "r");
    if (f) {
        char buf[256];
        if (fgets(buf, sizeof(buf), f) != NULL) {
            compatible = buf;
        }
        fclose(f);
    }

    if (compatible) {
        if (strstr(compatible, "mt6893")) return SOC_MODEL_MT6893;
        if (strstr(compatible, "mt6891")) return SOC_MODEL_MT6891;
        if (strstr(compatible, "mt6877")) return SOC_MODEL_MT6877;
        if (strstr(compatible, "mt6895")) return SOC_MODEL_MT6895;
        if (strstr(compatible, "mt6983")) return SOC_MODEL_MT6983;
        if (strstr(compatible, "mt6985")) return SOC_MODEL_MT6985;
        if (strstr(compatible, "mt6989")) return SOC_MODEL_MT6989;
        if (strstr(compatible, "mt6991")) return SOC_MODEL_MT6991;
        if (strstr(compatible, "mt6833")) return SOC_MODEL_MT6833;
        if (strstr(compatible, "mt6769")) return SOC_MODEL_MT6769;
        if (strstr(compatible, "mt6768")) return SOC_MODEL_MT6768;
        if (strstr(compatible, "mt6765")) return SOC_MODEL_MT6765;
        if (strstr(compatible, "mt6739")) return SOC_MODEL_MT6739;
        if (strstr(compatible, "mt6762")) return SOC_MODEL_MT6762;
    }

    /* Fallback: check hardware ID via /dev/mem */
    /* MT6895, MT6893: 4xA78 + 4xA55 */
    /* MT6877: 2xA78 + 6xA55 */
    return SOC_MODEL_MAX;
}

uintptr_t
mtk_get_reg_base(int peripheral)
{
    if (peripheral < 0 || peripheral >= (int)(sizeof(mtk_reg_bases) / sizeof(mtk_reg_bases[0])))
        return 0;

    return mtk_reg_bases[peripheral];
}

uint32_t
mtk_get_max_freq(soc_model_t model)
{
    switch (model) {
    case SOC_MODEL_MT6895:  /* Dimensity 9000 */
    case SOC_MODEL_MT6983:  /* Dimensity 9100 */
    case SOC_MODEL_MT6985:  /* Dimensity 9200 */
    case SOC_MODEL_MT6989:  /* Dimensity 9300 */
    case SOC_MODEL_MT6991:  /* Dimensity 9400 */
        return 3200;  /* MHz */
    case SOC_MODEL_MT6893:  /* Dimensity 1100 */
    case SOC_MODEL_MT6891:  /* Dimensity 1200 */
        return 2600;
    case SOC_MODEL_MT6877:  /* Dimensity 920 */
    case SOC_MODEL_MT6833:  /* Dimensity 800U/720 */
        return 2200;
    case SOC_MODEL_MT6769:  /* Helio G90/G95 */
        return 2000;
    case SOC_MODEL_MT6768:  /* Helio G80 */
    case SOC_MODEL_MT6765:  /* Helio G25 */
        return 1800;
    case SOC_MODEL_MT6739:  /* Entry-level */
    case SOC_MODEL_MT6762:  /* Helio G35 */
        return 1500;
    default:
        return 2000;
    }
}

int
mtk_get_cpu_cores(soc_model_t model)
{
    switch (model) {
    case SOC_MODEL_MT6895:
    case SOC_MODEL_MT6983:
    case SOC_MODEL_MT6985:
    case SOC_MODEL_MT6989:
    case SOC_MODEL_MT6991:
    case SOC_MODEL_MT6893:
    case SOC_MODEL_MT6891:
        return 8;  /* 4xA78 + 4xA55 */
    case SOC_MODEL_MT6877:
        return 8;  /* 2xA78 + 6xA55 */
    case SOC_MODEL_MT6833:
        return 8;  /* 2xA76 + 6xA55 */
    case SOC_MODEL_MT6769:
    case SOC_MODEL_MT6768:
        return 8;  /* Octa-core */
    case SOC_MODEL_MT6765:
    case SOC_MODEL_MT6739:
    case SOC_MODEL_MT6762:
        return 8;
    default:
        return 8;
    }
}

int
mtk_power_init(void)
{
    /* Initialize MediaTek power domains */
    /* APMIXEDSYS setup for PLL control */
    return 0;
}

int
mtk_power_set_freq(uint32_t freq_mhz)
{
    /* Configure PLL frequency via APMIXEDSYS */
    /* This is SoC-specific - would need model context */
    return 0;
}

uint32_t
mtk_power_get_cpu_volt(uint32_t freq_mhz)
{
    /* Return voltage in uV based on frequency */
    /* Simplified - real implementation would use PMIC table */
    if (freq_mhz >= 3000) return 1100000;  /* 1.1V */
    if (freq_mhz >= 2600) return 1050000;  /* 1.05V */
    if (freq_mhz >= 2200) return 1000000;  /* 1.0V */
    if (freq_mhz >= 1800) return 900000;   /* 0.9V */
    return 850000;  /* 0.85V */
}