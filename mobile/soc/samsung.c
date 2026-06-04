/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2024 FreeBSD Mobile Project
 * All rights reserved.
 *
 * Samsung Exynos SoC support implementation.
 */

#include <sys/types.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "samsung.h"
#include "soc.h"

/* Exynos device tree compatible strings */
#define EXYNOS_COMPATIBLE_2200   "samsung,exynos2200"
#define EXYNOS_COMPATIBLE_2100   "samsung,exynos2100"
#define EXYNOS_COMPATIBLE_1380   "samsung,exynos1380"
#define EXYNOS_COMPATIBLE_1330   "samsung,exynos1330"
#define EXYNOS_COMPATIBLE_990    "samsung,exynos990"
#define EXYNOS_COMPATIBLE_9825   "samsung,exynos9825"
#define EXYNOS_COMPATIBLE_850    "samsung,exynos850"

/* Exynos memory map */
static const uintptr_t exynos_reg_bases[] = {
    [EXYNOS_UART]    = 0x13820000,
    [EXYNOS_I2C]     = 0x13830000,
    [EXYNOS_SPI]     = 0x13840000,
    [EXYNOS_GPIO]    = 0x11400000,
    [EXYNOS_POWER]   = 0x15800000,
    [EXYNOS_CPU]     = 0x10000000,
    [EXYNOS_GIC]     = 0x10400000,
    [EXYNOS_MCT]     = 0x10500000,
    [EXYNOS_GPU]     = 0x12400000,  /* Xclipse GPU */
    [EXYNOS_DISPLAY] = 0x12500000,
    [EXYNOS_CAMERA]  = 0x12600000,
};

soc_model_t
exynos_chip_detect(void)
{
    const char *compatible = NULL;

    FILE *f = fopen("/sys/firmware/devicetree/base/compatible", "r");
    if (f) {
        char buf[256];
        if (fgets(buf, sizeof(buf), f) != NULL) {
            compatible = buf;
        }
        fclose(f);
    }

    if (compatible) {
        if (strstr(compatible, "exynos2200")) return SOC_MODEL_EXYNOS2200;
        if (strstr(compatible, "exynos2100")) return SOC_MODEL_EXYNOS2100;
        if (strstr(compatible, "exynos1380")) return SOC_MODEL_EXYNOS1380;
        if (strstr(compatible, "exynos1330")) return SOC_MODEL_EXYNOS1330;
        if (strstr(compatible, "exynos990"))  return SOC_MODEL_EXYNOS990;
        if (strstr(compatible, "exynos9825")) return SOC_MODEL_EXYNOS9825;
        if (strstr(compatible, "exynos850"))  return SOC_MODEL_EXYNOS850;
    }

    return SOC_MODEL_MAX;
}

uintptr_t
exynos_get_reg_base(int peripheral)
{
    if (peripheral < 0 || peripheral >= (int)(sizeof(exynos_reg_bases) / sizeof(exynos_reg_bases[0])))
        return 0;

    return exynos_reg_bases[peripheral];
}

int
exynos_gpu_init(int gpu_model)
{
    /* Initialize Xclipse GPU (AMD RDNA2-based) */
    /* Configure GPU clocks and power domains */
    switch (gpu_model) {
    case EXYNOS_GPU_XCLIPSE2200:
    case EXYNOS_GPU_XCLIPSE1634:
        /* RDNA2 GPU initialization */
        break;
    default:
        return -1;
    }
    return 0;
}

int
exynos_gpu_freq_set(uint32_t freq_mhz)
{
    /* Set GPU frequency via ASIMD or power domain */
    return 0;
}