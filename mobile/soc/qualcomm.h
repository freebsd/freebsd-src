/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2024 FreeBSD Mobile Project
 * All rights reserved.
 *
 * Qualcomm Snapdragon SoC support.
 */

#ifndef _MOBILE_QUALCOMM_H_
#define _MOBILE_QUALCOMM_H_

#include <sys/types.h>
#include <stdint.h>
#include "soc.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Qualcomm peripheral register bases */
#define QCOM_UART_BT       0
#define QCOM_UART_GSBI     1
#define QCOM_I2C           2
#define QCOM_SPI           3
#define QCOM_GPIO          4
#define QCOM_GICC          5
#define QCOM_GICD          6
#define QCOM_GT            7
#define QCOM_TIMER         8
#define QCOM_APCS          9
#define QCOM_RPM           10
#define QCOM_RPMH          11
#define QCOM_SMMU          12
#define QCOM_ADSS          13
#define QCOM_GPU           14
#define QCOM_CC            15
#define QCOM_SWAY_DDR      16
#define QCOM_MDSS          17
#define QCOM_DSI           18
#define QCOM_PCIE          19
#define QCOM_USB           20
#define QCOM_RESET_HW      21

/* GPU models */
#define QCOM_GPU_A730     0  /* Adreno 730 */
#define QCOM_GPU_A660     1  /* Adreno 660 */
#define QCOM_GPU_A640     2  /* Adreno 640 */
#define QCOM_GPU_A510     3  /* Adreno 510 */

/* Chip detection */
soc_model_t qcom_chip_detect(void);

/* Register access */
uintptr_t qcom_get_reg_base(int peripheral);

/* Power management */
int qcom_power_init(void);
int qcom_cpufreq_set(uint32_t freq_mhz);

/* Voltage scaling */
uint32_t qcom_get_cpu_volt(soc_model_t model, uint32_t freq_mhz);

#ifdef __cplusplus
}
#endif

#endif /* _MOBILE_QUALCOMM_H_ */