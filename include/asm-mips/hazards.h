/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 2003 Ralf Baechle
 */
#ifndef _ASM_HAZARDS_H
#define _ASM_HAZARDS_H

#include <linux/config.h>

#ifdef __ASSEMBLY__

/*
 * RM9000 hazards.  When the JTLB is updated by tlbwi or tlbwr, a subsequent
 * use of the JTLB for instructions should not occur for 4 cpu cycles and use
 * for data translations should not occur for 3 cpu cycles.
 */
#ifdef CONFIG_CPU_RM9000
#define rm9000_tlb_hazard						\
	.set	push;							\
	.set	mips32;							\
	ssnop; ssnop; ssnop; ssnop;					\
	.set	pop
#else
#define rm9000_tlb_hazard
#endif

#else

/*
 * RM9000 hazards.  When the JTLB is updated by tlbwi or tlbwr, a subsequent
 * use of the JTLB for instructions should not occur for 4 cpu cycles and use
 * for data translations should not occur for 3 cpu cycles.
 */
#ifdef CONFIG_CPU_RM9000
#define rm9000_tlb_hazard()						\
	__asm__ __volatile__(						\
		".set\tmips32\n\t"					\
		"ssnop; ssnop; ssnop; ssnop\n\t"			\
		".set\tmips0")
#else
#define rm9000_tlb_hazard() do { } while (0)
#endif

#endif

#endif /* _ASM_HAZARDS_H */
