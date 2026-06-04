/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2024 FreeBSD Mobile Project
 * All rights reserved.
 *
 * Google Tensor SoC support.
 */

#ifndef _MOBILE_GOOGLE_H_
#define _MOBILE_GOOGLE_H_

#include <sys/types.h>
#include <stdint.h>
#include "soc.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Tensor peripheral register bases (based on Exynos patterns) */
#define TENSOR_UART      0
#define TENSOR_I2C       1
#define TENSOR_SPI       2
#define TENSOR_GPIO      3
#define TENSOR_POWER     4
#define TENSOR_CPU       5
#define TENSOR_GIC       6
#define TENSOR_MCT       7
#define TENSOR_GPU       8
#define TENSOR_DISPLAY   9
#define TENSOR_CAMERA   10
#define TENSOR_TPU      11
#define TENSOR_TITAN_M  12

/* TPU frequency states */
#define TENSOR_TPU_MIN_FREQ    200000  /* 200 MHz */
#define TENSOR_TPU_MAX_FREQ    1060000  /* 1060 MHz */

/* Chip detection */
soc_model_t tensor_chip_detect(void);

/* Register access */
uintptr_t tensor_get_reg_base(int peripheral);

/* ML acceleration stub */
int tensor_tpu_init(void);
int tensor_tpu_submit(void *cmd, size_t len);
int tensor_tpu_freq_set(uint32_t freq_mhz);

/* Security coprocessor */
int tensor_titan_m_init(void);
int tensor_titan_m_command(uint32_t cmd, void *data, size_t len);

#ifdef __cplusplus
}
#endif

#endif /* _MOBILE_GOOGLE_H_ */