/*
 * Various register offset definitions for debuggers, core file
 * examiners and whatnot.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 1995, 1999 by Ralf Baechle
 */
#ifndef __ASM_MIPS_REG_H
#define __ASM_MIPS_REG_H

/*
 * This defines/structures correspond to the register layout on stack -
 * if the order here is changed, it needs to be updated in
 * include/asm-mips/stackframe.h
 */
#define EF_REG0			6
#define EF_REG1			7
#define EF_REG2			8
#define EF_REG3			9
#define EF_REG4			10
#define EF_REG5			11
#define EF_REG6			12
#define EF_REG7			13
#define EF_REG8			14
#define EF_REG9			15
#define EF_REG10		16
#define EF_REG11		17
#define EF_REG12		18
#define EF_REG13		19
#define EF_REG14		20
#define EF_REG15		21
#define EF_REG16		22
#define EF_REG17		23
#define EF_REG18		24
#define EF_REG19		25
#define EF_REG20		26
#define EF_REG21		27
#define EF_REG22		28
#define EF_REG23		29
#define EF_REG24		30
#define EF_REG25		31
/*
 * k0/k1 unsaved
 */
#define EF_REG28		34
#define EF_REG29		35
#define EF_REG30		36
#define EF_REG31		37

/*
 * Saved special registers
 */
#define EF_LO			38
#define EF_HI			39

#define EF_CP0_EPC		40
#define EF_CP0_BADVADDR		41
#define EF_CP0_STATUS		42
#define EF_CP0_CAUSE		43

#define EF_SIZE			180	/* size in bytes */

#endif /* __ASM_MIPS_REG_H */
