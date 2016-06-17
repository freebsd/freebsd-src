#ifndef __ASM_SH64_HARDWARE_H
#define __ASM_SH64_HARDWARE_H

/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * include/asm-sh64/hardware.h
 *
 * Copyright (C) 2002 Stuart Menefy
 * Copyright (C) 2003 Paul Mundt
 *
 * Defitions of the locations of registers in the physical address space.
 */

#define	PHYS_PERIPHERAL_BLOCK	0x09000000
#define PHYS_DMAC_BLOCK		0x0e000000
#define PHYS_PCI_BLOCK		0x60000000

#ifndef __ASSEMBLY__
#include <linux/types.h>
#include <asm/io.h>

struct vcr_info {
	u8	perr_flags;	/* P-port Error flags */
	u8	merr_flags;	/* Module Error flags */
	u16	mod_vers;	/* Module Version */
	u16	mod_id;		/* Module ID */
	u8	bot_mb;		/* Bottom Memory block */
	u8	top_mb;		/* Top Memory block */
};

static inline struct vcr_info sh64_get_vcr_info(unsigned long base)
{
	unsigned long long tmp;

	tmp = sh64_in64(base);

	return *((struct vcr_info *)&tmp);
}

#endif /* __ASSEMBLY__ */

#endif /* __ASM_SH64_HARDWARE_H */
