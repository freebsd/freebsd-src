/*
 * Copyright (c) 2025 The FreeBSD Foundation
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#ifndef _I6300ESBWD_H_
#define _I6300ESBWD_H_

#define WDT_CONFIG_REG	  0x60
#define WDT_LOCK_REG	  0x68

#define WDT_PRELOAD_1_REG 0x00
#define WDT_PRELOAD_2_REG 0x04
#define WDT_INTR_REG	  0x08
#define WDT_RELOAD_REG	  0x0C

/* For config register */
#define WDT_OUTPUT_EN		  (0x1 << 5)
#define WDT_PRE_SEL		  (0x1 << 2)
#define WDT_INT_TYPE_BITS	  (0x3)
#define WDT_INT_TYPE_IRQ_VAL	  (0x0)
#define WDT_INT_TYPE_RES_VAL	  (0x1)
#define WDT_INT_TYPE_SMI_VAL	  (0x2)
#define WDT_INT_TYPE_DISABLED_VAL (0x3)

/* For lock register */
#define WDT_TOUT_CNF_WT_MODE (0x0 << 2)
#define WDT_TOUT_CNF_FR_MODE (0x1 << 2)
#define WDT_ENABLE	     (0x02)
#define WDT_LOCK	     (0x01)

/* For preload 1/2 registers */
#define WDT_PRELOAD_BIT	 20
#define WDT_PRELOAD_BITS ((0x1 << WDT_PRELOAD_BIT) - 1)

/* For interrupt register */
#define WDT_INTR_ACT (0x01 << 0)

/* For reload register */
#define WDT_TIMEOUT	     (0x01 << 9)
#define WDT_RELOAD	     (0x01 << 8)
#define WDT_UNLOCK_SEQ_1_VAL 0x80
#define WDT_UNLOCK_SEQ_2_VAL 0x86

#endif /* _I6300ESBWD_H_ */
