/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2024 FreeBSD Mobile Project
 * All rights reserved.
 *
 * Google Tensor SoC support implementation.
 */

#include <sys/types.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "google.h"
#include "soc.h"

/* Tensor device tree compatible strings */
#define TENSOR_COMPATIBLE_G3   "google,zuma"
#define TENSOR_COMPATIBLE_G2   "google,zuma"
#define TENSOR_COMPATIBLE_G1   "google,redfin"

/* Tensor memory map (custom Exynos-based) */
static const uintptr_t tensor_reg_bases[] = {
    [TENSOR_UART]    = 0x13820000,
    [TENSOR_I2C]     = 0x13830000,
    [TENSOR_SPI]     = 0x13840000,
    [TENSOR_GPIO]    = 0x11400000,
    [TENSOR_POWER]   = 0x15800000,
    [TENSOR_CPU]     = 0x10000000,
    [TENSOR_GIC]     = 0x10400000,
    [TENSOR_MCT]     = 0x10500000,
    [TENSOR_GPU]     = 0x12400000,
    [TENSOR_DISPLAY] = 0x12500000,
    [TENSOR_CAMERA]  = 0x12600000,
    [TENSOR_TPU]     = 0x12800000,  /* Tensor Processing Unit */
    [TENSOR_TITAN_M] = 0x12200000,  /* Titan M security coprocessor */
};

soc_model_t
tensor_chip_detect(void)
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
        if (strstr(compatible, "zuma")) return SOC_MODEL_TENSOR_G3;
        if (strstr(compatible, "redfin")) return SOC_MODEL_TENSOR_G1;
    }

    return SOC_MODEL_MAX;
}

uintptr_t
tensor_get_reg_base(int peripheral)
{
    if (peripheral < 0 || peripheral >= (int)(sizeof(tensor_reg_bases) / sizeof(tensor_reg_bases[0])))
        return 0;

    return tensor_reg_bases[peripheral];
}

int
tensor_tpu_init(void)
{
    /* Initialize TPU for ML acceleration */
    /* Configure AXI and memory interfaces */
    return 0;
}

int
tensor_tpu_submit(void *cmd, size_t len)
{
    /* Submit command buffer to TPU */
    /* Would trigger doorbell or mailbox */
    return 0;
}

int
tensor_tpu_freq_set(uint32_t freq_mhz)
{
    /* Set TPU frequency - requires power domain control */
    return 0;
}

int
tensor_titan_m_init(void)
{
    /* Initialize Titan M security coprocessor */
    /* Verify secure boot state */
    return 0;
}

int
tensor_titan_m_command(uint32_t cmd, void *data, size_t len)
{
    /* Send command to Titan M via secure channel */
    return 0;
}