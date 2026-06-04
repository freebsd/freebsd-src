/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2024 FreeBSD Mobile Project
 * All rights reserved.
 *
 * Common SoC initialization framework implementation.
 */

#include <sys/types.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "soc.h"
#include "soc_common.h"
#include "mediatek.h"
#include "qualcomm.h"
#include "samsung.h"
#include "google.h"
#include "apple.h"

#define MAX_VENDOR_NAME 32

static soc_vendor_t detected_vendor = SOC_VENDOR_UNKNOWN;
static soc_model_t detected_model = SOC_MODEL_MAX;
static int initialized = 0;

/* Core count lookup table */
static const int soc_cpu_cores[] = {
    /* MediaTek */
    [SOC_MODEL_MT6893] = 8,
    [SOC_MODEL_MT6891] = 8,
    [SOC_MODEL_MT6877] = 8,
    [SOC_MODEL_MT6895] = 8,
    [SOC_MODEL_MT6983] = 8,
    [SOC_MODEL_MT6985] = 8,
    [SOC_MODEL_MT6989] = 8,
    [SOC_MODEL_MT6991] = 8,
    [SOC_MODEL_MT6833] = 8,
    [SOC_MODEL_MT6769] = 8,
    [SOC_MODEL_MT6768] = 8,
    [SOC_MODEL_MT6765] = 8,
    [SOC_MODEL_MT6739] = 8,
    [SOC_MODEL_MT6762] = 8,
    /* Qualcomm */
    [SOC_MODEL_SM8450] = 8,
    [SOC_MODEL_SM8350] = 8,
    [SOC_MODEL_SM8475] = 8,
    [SOC_MODEL_SM8550] = 8,
    [SOC_MODEL_SM7450] = 8,
    [SOC_MODEL_SM7350] = 8,
    [SOC_MODEL_SM6375] = 8,
    [SOC_MODEL_SM6450] = 8,
    [SOC_MODEL_SM6625] = 8,
    [SOC_MODEL_MSM8998] = 8,
    [SOC_MODEL_MSM8996] = 8,
    /* Samsung */
    [SOC_MODEL_EXYNOS2200] = 8,
    [SOC_MODEL_EXYNOS2100] = 8,
    [SOC_MODEL_EXYNOS1380] = 8,
    [SOC_MODEL_EXYNOS1330] = 8,
    [SOC_MODEL_EXYNOS990] = 8,
    [SOC_MODEL_EXYNOS9825] = 8,
    [SOC_MODEL_EXYNOS850] = 8,
    /* Google */
    [SOC_MODEL_TENSOR_G3] = 8,
    [SOC_MODEL_TENSOR_G2] = 8,
    [SOC_MODEL_TENSOR_G1] = 8,
    /* Apple */
    [SOC_MODEL_A17_PRO] = 8,
    [SOC_MODEL_A16] = 6,
    [SOC_MODEL_A15] = 6,
    [SOC_MODEL_A14] = 6,
};

/* Max frequency lookup table (MHz) */
static const uint32_t soc_max_freqs[] = {
    /* MediaTek */
    [SOC_MODEL_MT6893] = 2600,
    [SOC_MODEL_MT6891] = 2600,
    [SOC_MODEL_MT6877] = 2200,
    [SOC_MODEL_MT6895] = 3200,
    [SOC_MODEL_MT6983] = 3200,
    [SOC_MODEL_MT6985] = 3200,
    [SOC_MODEL_MT6989] = 3200,
    [SOC_MODEL_MT6991] = 3200,
    [SOC_MODEL_MT6833] = 2200,
    [SOC_MODEL_MT6769] = 2000,
    [SOC_MODEL_MT6768] = 1800,
    [SOC_MODEL_MT6765] = 1800,
    [SOC_MODEL_MT6739] = 1500,
    [SOC_MODEL_MT6762] = 1500,
    /* Qualcomm */
    [SOC_MODEL_SM8450] = 3200,
    [SOC_MODEL_SM8350] = 3000,
    [SOC_MODEL_SM8475] = 3200,
    [SOC_MODEL_SM8550] = 3200,
    [SOC_MODEL_SM7450] = 3000,
    [SOC_MODEL_SM7350] = 2800,
    [SOC_MODEL_SM6375] = 2800,
    [SOC_MODEL_SM6450] = 2600,
    [SOC_MODEL_SM6625] = 2400,
    [SOC_MODEL_MSM8998] = 2500,
    [SOC_MODEL_MSM8996] = 2200,
    /* Samsung */
    [SOC_MODEL_EXYNOS2200] = 2800,
    [SOC_MODEL_EXYNOS2100] = 2800,
    [SOC_MODEL_EXYNOS1380] = 2400,
    [SOC_MODEL_EXYNOS1330] = 2000,
    [SOC_MODEL_EXYNOS990] = 2400,
    [SOC_MODEL_EXYNOS9825] = 2200,
    [SOC_MODEL_EXYNOS850] = 1800,
    /* Google */
    [SOC_MODEL_TENSOR_G3] = 3200,
    [SOC_MODEL_TENSOR_G2] = 2800,
    [SOC_MODEL_TENSOR_G1] = 2600,
    /* Apple */
    [SOC_MODEL_A17_PRO] = 3800,
    [SOC_MODEL_A16] = 3500,
    [SOC_MODEL_A15] = 3300,
    [SOC_MODEL_A14] = 3100,
};

/* SoC names */
static const char *soc_names[] = {
    /* MediaTek */
    [SOC_MODEL_MT6893] = "MediaTek Dimensity 1100",
    [SOC_MODEL_MT6891] = "MediaTek Dimensity 1200",
    [SOC_MODEL_MT6877] = "MediaTek Dimensity 920",
    [SOC_MODEL_MT6895] = "MediaTek Dimensity 9000",
    [SOC_MODEL_MT6983] = "MediaTek Dimensity 9100",
    [SOC_MODEL_MT6985] = "MediaTek Dimensity 9200",
    [SOC_MODEL_MT6989] = "MediaTek Dimensity 9300",
    [SOC_MODEL_MT6991] = "MediaTek Dimensity 9400",
    [SOC_MODEL_MT6833] = "MediaTek Dimensity 800U/720",
    [SOC_MODEL_MT6769] = "MediaTek Helio G90/G95",
    [SOC_MODEL_MT6768] = "MediaTek Helio G80",
    [SOC_MODEL_MT6765] = "MediaTek Helio G25",
    [SOC_MODEL_MT6739] = "MediaTek Helio entry",
    [SOC_MODEL_MT6762] = "MediaTek Helio G35",
    /* Qualcomm */
    [SOC_MODEL_SM8450] = "Qualcomm Snapdragon 8cx Gen 3",
    [SOC_MODEL_SM8350] = "Qualcomm Snapdragon 8cx Gen 2",
    [SOC_MODEL_SM8475] = "Qualcomm Snapdragon 8+ Gen 1",
    [SOC_MODEL_SM8550] = "Qualcomm Snapdragon 8 Gen 2",
    [SOC_MODEL_SM7450] = "Qualcomm Snapdragon 7cx Gen 3",
    [SOC_MODEL_SM7350] = "Qualcomm Snapdragon 7 Gen 1",
    [SOC_MODEL_SM6375] = "Qualcomm Snapdragon 7s Gen 2",
    [SOC_MODEL_SM6450] = "Qualcomm Snapdragon 6 Gen 1",
    [SOC_MODEL_SM6625] = "Qualcomm Snapdragon 4 Gen 2",
    [SOC_MODEL_MSM8998] = "Qualcomm Snapdragon 835",
    [SOC_MODEL_MSM8996] = "Qualcomm Snapdragon 821/820",
    /* Samsung */
    [SOC_MODEL_EXYNOS2200] = "Samsung Exynos 2200 (S5E9925)",
    [SOC_MODEL_EXYNOS2100] = "Samsung Exynos 2100 (S5E9810)",
    [SOC_MODEL_EXYNOS1380] = "Samsung Exynos 1380 (S5E8845)",
    [SOC_MODEL_EXYNOS1330] = "Samsung Exynos 1330 (S5E8535)",
    [SOC_MODEL_EXYNOS990] = "Samsung Exynos 990 (S5E9830)",
    [SOC_MODEL_EXYNOS9825] = "Samsung Exynos 9825 (S5E9825)",
    [SOC_MODEL_EXYNOS850] = "Samsung Exynos 850 (S5E5510)",
    /* Google */
    [SOC_MODEL_TENSOR_G3] = "Google Tensor G3",
    [SOC_MODEL_TENSOR_G2] = "Google Tensor G2",
    [SOC_MODEL_TENSOR_G1] = "Google Tensor G1",
    /* Apple */
    [SOC_MODEL_A17_PRO] = "Apple A17 Pro",
    [SOC_MODEL_A16] = "Apple A16",
    [SOC_MODEL_A15] = "Apple A15",
    [SOC_MODEL_A14] = "Apple A14",
};

soc_vendor_t
soc_detect(void)
{
    const char *path = "/sys/firmware/devicetree/base/compatible";
    FILE *f;
    char buf[512];

    f = fopen(path, "r");
    if (f) {
        if (fgets(buf, sizeof(buf), f)) {
            if (strstr(buf, "mediatek")) return SOC_VENDOR_MEDIATEK;
            if (strstr(buf, "qcom") || strstr(buf, "qti")) return SOC_VENDOR_QUALCOMM;
            if (strstr(buf, "samsung") || strstr(buf, "exynos")) return SOC_VENDOR_SAMSUNG;
            if (strstr(buf, "google") || strstr(buf, "tensor")) return SOC_VENDOR_GOOGLE;
            if (strstr(buf, "apple")) return SOC_VENDOR_APPLE;
        }
        fclose(f);
    }

    return SOC_VENDOR_UNKNOWN;
}

soc_model_t
soc_get_model(void)
{
    if (detected_model == SOC_MODEL_MAX) {
        detected_vendor = soc_detect();
        switch (detected_vendor) {
        case SOC_VENDOR_MEDIATEK:
            detected_model = mtk_chip_detect();
            break;
        case SOC_VENDOR_QUALCOMM:
            detected_model = qcom_chip_detect();
            break;
        case SOC_VENDOR_SAMSUNG:
            detected_model = exynos_chip_detect();
            break;
        case SOC_VENDOR_GOOGLE:
            detected_model = tensor_chip_detect();
            break;
        case SOC_VENDOR_APPLE:
            detected_model = apple_chip_detect();
            break;
        default:
            detected_model = SOC_MODEL_MAX;
        }
    }
    return detected_model;
}

const char *
soc_get_name(void)
{
    soc_model_t model = soc_get_model();
    if (model >= SOC_MODEL_MAX) return "Unknown SoC";
    return soc_names[model];
}

int
soc_get_cpus(void)
{
    soc_model_t model = soc_get_model();
    if (model >= SOC_MODEL_MAX) return 0;
    return soc_cpu_cores[model];
}

uint32_t
soc_get_max_freq(void)
{
    soc_model_t model = soc_get_model();
    if (model >= SOC_MODEL_MAX) return 0;
    return soc_max_freqs[model];
}

const soc_desc_t *
soc_get_desc(void)
{
    static soc_desc_t desc;
    soc_model_t model = soc_get_model();

    if (model >= SOC_MODEL_MAX) return NULL;

    desc.vendor = detected_vendor;
    desc.model = model;
    desc.name = soc_names[model];
    desc.cpu_cores = soc_cpu_cores[model];
    desc.max_freq_mhz = soc_max_freqs[model];
    desc.compatible = soc_names[model];

    return &desc;
}

int
soc_early_init(void)
{
    /* Initialize basic hardware for early boot */
    uart_init();
    timer_init();
    gic_init();
    return 0;
}

int
soc_init(void)
{
    if (initialized) return 0;

    detected_vendor = soc_detect();
    switch (detected_vendor) {
    case SOC_VENDOR_MEDIATEK:
        detected_model = mtk_chip_detect();
        mtk_power_init();
        break;
    case SOC_VENDOR_QUALCOMM:
        detected_model = qcom_chip_detect();
        qcom_power_init();
        break;
    case SOC_VENDOR_SAMSUNG:
        detected_model = exynos_chip_detect();
        break;
    case SOC_VENDOR_GOOGLE:
        detected_model = tensor_chip_detect();
        tensor_tpu_init();
        tensor_titan_m_init();
        break;
    case SOC_VENDOR_APPLE:
        detected_model = apple_chip_detect();
        apple_ane_init();
        apple_gpu_init();
        break;
    default:
        return -1;
    }

    initialized = 1;
    return 0;
}

int
soc_late_init(void)
{
    /* Initialize optional peripherals after main system is up */
    switch (detected_vendor) {
    case SOC_VENDOR_MEDIATEK:
        /* GPU, modem, WLAN initialization */
        break;
    case SOC_VENDOR_QUALCOMM:
        /* Adreno GPU, modem initialization */
        break;
    case SOC_VENDOR_SAMSUNG:
        exynos_gpu_init(EXYNOS_GPU_XCLIPSE2200);
        break;
    case SOC_VENDOR_GOOGLE:
        /* TPU already initialized in soc_init */
        break;
    case SOC_VENDOR_APPLE:
        /* GPU, ANE already initialized */
        break;
    default:
        break;
    }
    return 0;
}

int
soc_boot_flow(void)
{
    /* SoC-specific boot sequence */
    switch (detected_vendor) {
    case SOC_VENDOR_MEDIATEK:
        /* BootROM -> Preloader -> ATF -> Kernel */
        break;
    case SOC_VENDOR_QUALCOMM:
        /* PBL -> XBL -> ABL -> Kernel */
        break;
    case SOC_VENDOR_SAMSUNG:
        /* BootROM -> SBOOT -> Kernel */
        break;
    case SOC_VENDOR_GOOGLE:
        /* Download mode -> ABOOT -> Kernel */
        break;
    case SOC_VENDOR_APPLE:
        /* SecureROM -> iBoot -> XNU/FreeBSD */
        break;
    default:
        return -1;
    }
    return 0;
}

int
soc_has_modem(void)
{
    /* MediaTek and Qualcomm have integrated modems */
    return (detected_vendor == SOC_VENDOR_MEDIATEK ||
            detected_vendor == SOC_VENDOR_QUALCOMM);
}

int
soc_has_gpu(void)
{
    /* All mobile SoCs have GPUs except entry models */
    switch (detected_vendor) {
    case SOC_VENDOR_MEDIATEK:
        return (detected_model != SOC_MODEL_MT6739 &&
                detected_model != SOC_MODEL_MT6762);
    case SOC_VENDOR_SAMSUNG:
    case SOC_VENDOR_GOOGLE:
        return 1;
    case SOC_VENDOR_APPLE:
        return 1;
    default:
        return 0;
    }
}

int
soc_has_ml_accelerator(void)
{
    /* Google Tensor and Apple Silicon have ML accelerators */
    return (detected_vendor == SOC_VENDOR_GOOGLE ||
            detected_vendor == SOC_VENDOR_APPLE);
}

/* Stub implementations for early init */
void uart_init(void) { }
void timer_init(void) { }
void gic_init(void) { }