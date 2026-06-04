/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2024 FreeBSD Mobile Project
 * All rights reserved.
 *
 * Samsung Exynos SoC support.
 */

#ifndef _MOBILE_SAMSUNG_H_
#define _MOBILE_SAMSUNG_H_

#include <sys/types.h>
#include <stdint.h>
#include "soc.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Samsung/Exynos peripheral register bases */
#define EXYNOS_UART      0
#define EXYNOS_I2C       1
#define EXYNOS_SPI       2
#define EXYNOS_GPIO      3
#define EXYNOS_POWER     4
#define EXYNOS_CPU       5
#define EXYNOS_GIC       6
#define EXYNOS_MCT       7
#define EXYNOS_GPU       8
#define EXYNOS_DISPLAY   9
#define EXYNOS_CAMERA   10

/* Xclipse GPU models */
#define EXYNOS_GPU_XCLIPSE2200  0  /* Exynos 2200 RDNA2 */
#define EXYNOS_GPU_XCLIPSE1634  1  /* Earlier RDNA2 variant */

/* Chip detection */
soc_model_t exynos_chip_detect(void);

/* Register access */
uintptr_t exynos_get_reg_base(int peripheral);

/* GPU support */
int exynos_gpu_init(int gpu_model);
int exynos_gpu_freq_set(uint32_t freq_mhz);

#ifdef __cplusplus
}
#endif

#endif /* _MOBILE_SAMSUNG_H_ */