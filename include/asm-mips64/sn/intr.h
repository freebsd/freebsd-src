/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 1992 - 1997 Silicon Graphics, Inc.
 */
#ifndef __ASM_SN_INTR_H
#define __ASM_SN_INTR_H

/* Number of interrupt levels associated with each interrupt register. */
#define N_INTPEND_BITS		64

#define INT_PEND0_BASELVL	0
#define INT_PEND1_BASELVL	64

#define	N_INTPENDJUNK_BITS	8
#define	INTPENDJUNK_CLRBIT	0x80

#include <asm/sn/intr_public.h>

#ifndef __ASSEMBLY__

/*
 * Macros to manipulate the interrupt register on the calling hub chip.
 */

#define LOCAL_HUB_SEND_INTR(_level)	LOCAL_HUB_S(PI_INT_PEND_MOD, \
						    (0x100|(_level)))
#define REMOTE_HUB_SEND_INTR(_hub, _level) \
		REMOTE_HUB_S((_hub), PI_INT_PEND_MOD, (0x100|(_level)))

/*
 * When clearing the interrupt, make sure this clear does make it
 * to the hub. Otherwise we could end up losing interrupts.
 * We do an uncached load of the int_pend0 register to ensure this.
 */

#define LOCAL_HUB_CLR_INTR(_level)	  \
                LOCAL_HUB_S(PI_INT_PEND_MOD, (_level)),	\
                LOCAL_HUB_L(PI_INT_PEND0)
#define REMOTE_HUB_CLR_INTR(_hub, _level) \
		REMOTE_HUB_S((_hub), PI_INT_PEND_MOD, (_level)),	\
                REMOTE_HUB_L((_hub), PI_INT_PEND0)

#else /* __ASSEMBLY__ */

#endif /* __ASSEMBLY__ */

/*
 * Hard-coded interrupt levels:
 */

/*
 *	L0 = SW1
 *	L1 = SW2
 *	L2 = INT_PEND0
 *	L3 = INT_PEND1
 *	L4 = RTC
 *	L5 = Profiling Timer
 *	L6 = Hub Errors
 *	L7 = Count/Compare (T5 counters)
 */


/* INT_PEND0 hard-coded bits. */
#ifdef SABLE
#define SDISK_INTR	63
#endif
#ifdef DEBUG_INTR_TSTAMP
/* hard coded interrupt level for interrupt latency test interrupt */
#define	CPU_INTRLAT_B	62
#define	CPU_INTRLAT_A	61
#endif

/* Hardcoded bits required by software. */
#define MSC_MESG_INTR	13
#define CPU_ACTION_B	11
#define CPU_ACTION_A	10

/* These are determined by hardware: */
#define CC_PEND_B	6
#define CC_PEND_A	5
#define UART_INTR	4
#define PG_MIG_INTR	3
#define GFX_INTR_B	2
#define GFX_INTR_A	1
#define RESERVED_INTR	0

/* INT_PEND1 hard-coded bits: */
#define MSC_PANIC_INTR	63
#define NI_ERROR_INTR	62
#define MD_COR_ERR_INTR	61
#define COR_ERR_INTR_B	60
#define COR_ERR_INTR_A	59
#define CLK_ERR_INTR	58
#define IO_ERROR_INTR	57	/* set up by prom */

#define	DEBUG_INTR_B	55	/* used by symmon to stop all cpus */
#define	DEBUG_INTR_A	54

#define BRIDGE_ERROR_INTR 53	/* Setup by PROM to catch Bridge Errors */

#define IP27_INTR_0	52	/* Reserved for PROM use */
#define IP27_INTR_1	51	/*   (do not use in Kernel) */
#define IP27_INTR_2	50
#define IP27_INTR_3	49
#define IP27_INTR_4	48
#define IP27_INTR_5	47
#define IP27_INTR_6	46
#define IP27_INTR_7	45

#define	TLB_INTR_B	44	/* used for tlb flush random */
#define	TLB_INTR_A	43

#define LLP_PFAIL_INTR_B 42	/* see ml/SN/SN0/sysctlr.c */
#define LLP_PFAIL_INTR_A 41

#define NI_BRDCAST_ERR_B 40
#define NI_BRDCAST_ERR_A 39

#endif /* __ASM_SN_INTR_H */
