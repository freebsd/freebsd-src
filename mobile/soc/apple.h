/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2024 FreeBSD Mobile Project
 * All rights reserved.
 *
 * Apple Silicon SoC support.
 */

#ifndef _MOBILE_APPLE_H_
#define _MOBILE_APPLE_H_

#include <sys/types.h>
#include <stdint.h>
#include "soc.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Apple peripheral register bases */
#define APPLE_UART       0
#define APPLE_GPIO       1
#define APPLE_TIMER      2
#define APPLE_AIC        3
#define APPLE_GPU        4
#define APPLE_ANE        5
#define APPLE_ISP        6

/* Apple silicon variants */
#define APPLE_CHIP_M1      0
#define APPLE_CHIP_M2      1
#define APPLE_CHIP_M3      2

/* Chip detection */
soc_model_t apple_chip_detect(void);

/* Register access */
uintptr_t apple_get_reg_base(int peripheral);

/* Apple Neural Engine */
int apple_ane_init(void);
int apple_ane_submit(void *cmd, size_t len);

/* GPU support */
int apple_gpu_init(void);
int apple_gpu_freq_set(uint32_t freq_mhz);

#ifdef __cplusplus
}
#endif

#endif /* _MOBILE_APPLE_H_ */