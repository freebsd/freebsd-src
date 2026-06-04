/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2024 FreeBSD Mobile Project
 * All rights reserved.
 *
 * Common SoC initialization framework.
 */

#ifndef _MOBILE_SOC_COMMON_H_
#define _MOBILE_SOC_COMMON_H_

#include <sys/types.h>
#include <stdint.h>
#include "soc.h"

#ifdef __cplusplus
extern "C" {
#endif

/* SoC descriptor table */
extern const soc_desc_t soc_table[];

/* SoC initialization phases */
typedef enum {
    SOC_INIT_EARLY,    /* Before VM: UART, timer, interrupt controller */
    SOC_INIT_POST_VM,  /* After VM: basic peripherals */
    SOC_INIT_LATE      /* After drivers: GPU, modem, optional */
} soc_init_phase_t;

/* Initialize SoC based on detection */
int soc_init(void);
int soc_early_init(void);
int soc_late_init(void);

/* SoC boot flow control */
int soc_boot_flow(void);

/* Get current SoC descriptor */
const soc_desc_t *soc_get_desc(void);

/* SoC feature detection */
int soc_has_modem(void);
int soc_has_gpu(void);
int soc_has_ml_accelerator(void);

#ifdef __cplusplus
}
#endif

#endif /* _MOBILE_SOC_COMMON_H_ */