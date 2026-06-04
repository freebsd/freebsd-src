/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2024 FreeBSD Mobile Project
 * All rights reserved.
 *
 * MediaTek SoC support.
 */

#ifndef _MOBILE_MEDIATEK_H_
#define _MOBILE_MEDIATEK_H_

#include <sys/types.h>
#include <stdint.h>
#include "soc.h"

#ifdef __cplusplus
extern "C" {
#endif

/* MediaTek peripheral register bases */
#define MTK_UART0        0
#define MTK_UART1        1
#define MTK_I2C0         2
#define MTK_SPI0         3
#define MTK_GPIO         4
#define MTK_PERICFG      5
#define MTK_APMIXED      6
#define MTK_INFRACFG     7
#define MTK_PMIC_WRAP    8
#define MTK_EFUSE        9
#define MTK_GCE         10
#define MTK_SMI         11
#define MTK_M4U         12
#define MTK_DRAM        13
#define MTK_GPU         14
#define MTK_AUDIO       15
#define MTK_WLAN        16
#define MTK_MD          17
#define MTK_SSUSB       18

/* Chip detection */
soc_model_t mtk_chip_detect(void);

/* Register access */
uintptr_t mtk_get_reg_base(int peripheral);

/* Power management */
int mtk_power_init(void);
int mtk_power_set_freq(uint32_t freq_mhz);
uint32_t mtk_power_get_cpu_volt(uint32_t freq_mhz);

/* Model-specific configuration */
uint32_t mtk_get_max_freq(soc_model_t model);
int mtk_get_cpu_cores(soc_model_t model);

#ifdef __cplusplus
}
#endif

#endif /* _MOBILE_MEDIATEK_H_ */