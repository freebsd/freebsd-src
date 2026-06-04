/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2024 FreeBSD Mobile Project
 * All rights reserved.
 *
 * SoC framework public interface implementation.
 */

#include <sys/types.h>
#include <stdint.h>
#include "soc.h"

/* External function declarations for vendor-specific implementations */
extern soc_model_t mtk_chip_detect(void);
extern soc_model_t qcom_chip_detect(void);
extern soc_model_t exynos_chip_detect(void);
extern soc_model_t tensor_chip_detect(void);
extern soc_model_t apple_chip_detect(void);

/* Implemented in soc_common.c */
extern soc_vendor_t soc_vendor_detect(void);

static soc_vendor_t cached_vendor = -1;
static soc_model_t cached_model = SOC_MODEL_MAX;
static int detected = 0;

soc_vendor_t
soc_detect(void)
{
    if (cached_vendor == -1) {
        cached_vendor = soc_vendor_detect();
    }
    return cached_vendor;
}

soc_model_t
soc_get_model(void)
{
    if (!detected) {
        cached_vendor = soc_detect();
        switch (cached_vendor) {
        case SOC_VENDOR_MEDIATEK:
            cached_model = mtk_chip_detect();
            break;
        case SOC_VENDOR_QUALCOMM:
            cached_model = qcom_chip_detect();
            break;
        case SOC_VENDOR_SAMSUNG:
            cached_model = exynos_chip_detect();
            break;
        case SOC_VENDOR_GOOGLE:
            cached_model = tensor_chip_detect();
            break;
        case SOC_VENDOR_APPLE:
            cached_model = apple_chip_detect();
            break;
        default:
            cached_model = SOC_MODEL_MAX;
        }
        detected = 1;
    }
    return cached_model;
}