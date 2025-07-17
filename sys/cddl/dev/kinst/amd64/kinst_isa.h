/*
 * SPDX-License-Identifier: CDDL 1.0
 *
 * Copyright (c) 2022 Christos Margiolis <christos@FreeBSD.org>
 * Copyright (c) 2022 Mark Johnston <markj@FreeBSD.org>
 * Copyright (c) 2023 The FreeBSD Foundation
 *
 * Portions of this software were developed by Christos Margiolis
 * <christos@FreeBSD.org> under sponsorship from the FreeBSD Foundation.
 */

#ifndef _KINST_ISA_H_
#define _KINST_ISA_H_

#include <sys/types.h>

#define KINST_PATCHVAL		0xcc

/*
 * Each trampoline is 32 bytes long and contains [instruction, jmp]. Since we
 * have 2 instructions stored in the trampoline, and each of them can take up
 * to 16 bytes, 32 bytes is enough to cover even the worst case scenario.
 */
#define	KINST_TRAMP_SIZE	32

typedef uint8_t kinst_patchval_t;

struct kinst_probe_md {
	int	flags;
	int	instlen;	/* original instr len */
	int	tinstlen;	/* trampoline instr len */
	uint8_t	template[16];	/* copied into thread tramps */
	int	dispoff;	/* offset of rip displacement */

	/* operands to "call" instruction branch target */
	int	reg1;
	int	reg2;
	int	scale;
	int64_t	disp;
};

#endif /* _KINST_ISA_H_ */
