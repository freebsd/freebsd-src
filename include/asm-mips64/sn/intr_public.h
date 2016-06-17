/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 1992 - 1997 Silicon Graphics, Inc.
 */
#ifndef __ASM_SN_INTR_PUBLIC_H
#define __ASM_SN_INTR_PUBLIC_H


/* REMEMBER: If you change these, the whole world needs to be recompiled.
 * It would also require changing the hubspl.s code and SN0/intr.c
 * Currently, the spl code has no support for multiple INTPEND1 masks.
 */

#define	N_INTPEND0_MASKS	1
#define	N_INTPEND1_MASKS	1

#define INTPEND0_MAXMASK	(N_INTPEND0_MASKS - 1)
#define INTPEND1_MAXMASK	(N_INTPEND1_MASKS - 1)

#ifndef __ASSEMBLY__
#include <asm/sn/arch.h>

struct intr_vecblk_s;	/* defined in asm/sn/intr.h */

/*
 * The following are necessary to create the illusion of a CEL
 * on the SN0 hub.  We'll add more priority levels soon, but for
 * now, any interrupt in a particular band effectively does an spl.
 * These must be in the PDA since they're different for each processor.
 * Users of this structure must hold the vector_lock in the appropriate vector
 * block before modifying the mask arrays.  There's only one vector block
 * for each Hub so a lock in the PDA wouldn't be adequate.
 */
typedef struct hub_intmasks_s {
	/*
	 * The masks are stored with the lowest-priority (most inclusive)
	 * in the lowest-numbered masks (i.e., 0, 1, 2...).
	 */
	/* INT_PEND0: */
	hubreg_t		intpend0_masks[N_INTPEND0_MASKS];
	/* INT_PEND1: */
	hubreg_t		intpend1_masks[N_INTPEND1_MASKS];
	/* INT_PEND0: */
	struct intr_vecblk_s	*dispatch0;
	/* INT_PEND1: */
	struct intr_vecblk_s	*dispatch1;
} hub_intmasks_t;

#endif /* !__ASSEMBLY__ */
#endif /* __ASM_SN_INTR_PUBLIC_H */
