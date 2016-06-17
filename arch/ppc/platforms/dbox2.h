/*
 * D-BOX2 board specific definitions
 * 
 * Copyright (c) 2001-2002 Florian Schirmer (jolt@tuxbox.org)
 */

#ifndef __MACH_DBOX2_H
#define __MACH_DBOX2_H

#include <linux/config.h>
 
#include <asm/ppcboot.h>

#define	DBOX2_IMMR_BASE	0xFF000000	/* phys. addr of IMMR */
#define	DBOX2_IMAP_SIZE	(64 * 1024)	/* size of mapped area */

#define	IMAP_ADDR	DBOX2_IMMR_BASE	/* physical base address of IMMR area */
#define IMAP_SIZE	DBOX2_IMAP_SIZE	/* mapped size of IMMR area */

/* We don't use the 8259.
*/
#define NR_8259_INTS	0

#endif	/* __MACH_DBOX2_H */
