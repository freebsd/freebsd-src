/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2024 FreeBSD Mobile Project
 * All rights reserved.
 *
 * Qualcomm Snapdragon SoC support implementation.
 */

#include <sys/types.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "qualcomm.h"
#include "soc.h"

/* Qualcomm device tree compatible strings */
#define QCOM_COMPATIBLE_SM8450   "qti,sm8450"
#define QCOM_COMPATIBLE_SM8350   "qti,sm8350"
#define QCOM_COMPATIBLE_SM8475   "qti,sm8475"
#define QCOM_COMPATIBLE_SM8550   "qti,sm8550"
#define QCOM_COMPATIBLE_SM7450   "qti,sm7450"
#define QCOM_COMPATIBLE_SM7350   "qti,sm7350"
#define QCOM_COMPATIBLE_SM6375   "qti,sm6375"
#define QCOM_COMPATIBLE_SM6450   "qti,sm6450"
#define QCOM_COMPATIBLE_SM6625   "qti,sm6625"
#define QCOM_COMPATIBLE_MSM8998   "qcom,msm8998"
#define QCOM_COMPATIBLE_MSM8996   "qcom,msm8996"

/* Qualcomm memory map - typical addresses for newer platforms */
static const uintptr_t qcom_reg_bases[] = {
    [QCOM_UART_BT]   = 0x02200000,
    [QCOM_UART_GSBI] = 0x02300000,
    [QCOM_I2C]       = 0x02A00000,
    [QCOM_SPI]       = 0x02D00000,
    [QCOM_GPIO]      = 0x02D00000,
    [QCOM_GICC]      = 0x04000000,
    [QCOM_GICD]      = 0x04100000,
    [QCOM_GT]        = 0x04200000,
    [QCOM_TIMER]     = 0x04A00000,
    [QCOM_APCS]      = 0x04B00000,
    [QCOM_RPM]       = 0x05400000,
    [QCOM_RPMH]      = 0x05500000,
    [QCOM_SMMU]      = 0x06000000,
    [QCOM_ADSS]      = 0x06100000,
    [QCOM_GPU]       = 0x06200000,
    [QCOM_CC]        = 0x04000000,
    [QCOM_SWAY_DDR]  = 0x06600000,
    [QCOM_MDSS]      = 0x06A00000,
    [QCOM_DSI]       = 0x06A00000,
    [QCOM_PCIE]      = 0x06B00000,
    [QCOM_USB]       = 0x06C00000,
    [QCOM_RESET_HW]  = 0x04400000,
};

soc_model_t
qcom_chip_detect(void)
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
        if (strstr(compatible, "sm8450")) return SOC_MODEL_SM8450;
        if (strstr(compatible, "sm8350")) return SOC_MODEL_SM8350;
        if (strstr(compatible, "sm8475")) return SOC_MODEL_SM8475;
        if (strstr(compatible, "sm8550")) return SOC_MODEL_SM8550;
        if (strstr(compatible, "sm7450")) return SOC_MODEL_SM7450;
        if (strstr(compatible, "sm7350")) return SOC_MODEL_SM7350;
        if (strstr(compatible, "sm6375")) return SOC_MODEL_SM6375;
        if (strstr(compatible, "sm6450")) return SOC_MODEL_SM6450;
        if (strstr(compatible, "sm6625")) return SOC_MODEL_SM6625;
        if (strstr(compatible, "msm8998")) return SOC_MODEL_MSM8998;
        if (strstr(compatible, "msm8996")) return SOC_MODEL_MSM8996;
    }

    /* Fallback: RPM/RPMh hardware access */
    /* Read hardware ID from RPM message RAM */
    return SOC_MODEL_MAX;
}

uintptr_t
qcom_get_reg_base(int peripheral)
{
    if (peripheral < 0 || peripheral >= (int)(sizeof(qcom_reg_bases) / sizeof(qcom_reg_bases[0])))
        return 0;

    return qcom_reg_bases[peripheral];
}

int
qcom_power_init(void)
{
    /* Initialize Qualcomm power domains via RPMh */
    /* Configure PMIC and voltage rails */
    return 0;
}

int
qcom_cpufreq_set(uint32_t freq_mhz)
{
    /* Set CPU frequency via RPMh or hardware slider */
    /* Frequency table lookup and voltage scaling */
    return 0;
}

uint32_t
qcom_get_cpu_volt(soc_model_t model, uint32_t freq_mhz)
{
    /* Voltage in uV based on model and frequency */
    switch (model) {
    case SOC_MODEL_SM8550:
    case SOC_MODEL_SM8450:
        if (freq_mhz >= 3200) return 1150000;
        if (freq_mhz >= 3000) return 1100000;
        if (freq_mhz >= 2800) return 1050000;
        if (freq_mhz >= 2400) return 1000000;
        if (freq_mhz >= 2000) return 900000;
        return 850000;
    case SOC_MODEL_SM8475:
    case SOC_MODEL_SM7450:
        if (freq_mhz >= 3100) return 1200000;
        if (freq_mhz >= 2800) return 1100000;
        if (freq_mhz >= 2400) return 1000000;
        return 900000;
    case SOC_MODEL_SM7350:
    case SOC_MODEL_SM6375:
        if (freq_mhz >= 2800) return 1100000;
        if (freq_mhz >= 2400) return 1000000;
        if (freq_mhz >= 2000) return 900000;
        return 850000;
    case SOC_MODEL_MSM8998:
    case SOC_MODEL_MSM8996:
        if (freq_mhz >= 2500) return 1100000;
        if (freq_mhz >= 2200) return 1000000;
        if (freq_mhz >= 1900) return 900000;
        return 850000;
    default:
        return 900000;
    }
}