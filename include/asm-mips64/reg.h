/*
 * Various register offset definitions for debuggers, core file
 * examiners and whatnot.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 1995, 1999 Ralf Baechle
 * Copyright (C) 1995, 1999 Silicon Graphics
 */
#ifndef _ASM_REG_H
#define _ASM_REG_H

/*
 * This defines/structures correspond to the register layout on stack -
 * if the order here is changed, it needs to be updated in
 * include/asm-mips/stackframe.h
 */
#define EF_REG0			 0
#define EF_REG1			 1
#define EF_REG2			 2
#define EF_REG3			 3
#define EF_REG4			 4
#define EF_REG5			 5
#define EF_REG6			 6
#define EF_REG7			 7
#define EF_REG8			 8
#define EF_REG9			 9
#define EF_REG10		10
#define EF_REG11		11
#define EF_REG12		12
#define EF_REG13		13
#define EF_REG14		14
#define EF_REG15		15
#define EF_REG16		16
#define EF_REG17		17
#define EF_REG18		18
#define EF_REG19		19
#define EF_REG20		20
#define EF_REG21		21
#define EF_REG22		22
#define EF_REG23		23
#define EF_REG24		24
#define EF_REG25		25
/*
 * k0/k1 unsaved
 */
#define EF_REG28		28
#define EF_REG29		29
#define EF_REG30		30
#define EF_REG31		31

/*
 * Saved special registers
 */
#define EF_LO			32
#define EF_HI			33

#define EF_CP0_EPC		34
#define EF_CP0_BADVADDR		35
#define EF_CP0_STATUS		36
#define EF_CP0_CAUSE		37

#define EF_SIZE			304	/* size in bytes */

#endif /* _ASM_REG_H */
