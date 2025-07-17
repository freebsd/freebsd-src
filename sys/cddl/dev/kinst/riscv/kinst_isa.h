/*
 * SPDX-License-Identifier: CDDL 1.0
 *
 * Copyright (c) 2023 The FreeBSD Foundation
 *
 * This software was developed by Christos Margiolis <christos@FreeBSD.org>
 * under sponsorship from the FreeBSD Foundation.
 */

#ifndef _KINST_ISA_H_
#define _KINST_ISA_H_

#include <machine/riscvreg.h>
#include <machine/encoding.h>

#define KINST_PATCHVAL		MATCH_EBREAK
#define KINST_C_PATCHVAL	MATCH_C_EBREAK

/*
 * The trampoline contains [instruction, [nop padding], ebreak].
 */
#define KINST_TRAMP_SIZE	8

typedef uint32_t kinst_patchval_t;

struct kinst_probe_md {
	int	instlen;	/* original instr len */
	bool	emulate;	/* emulate in sw */
};

#endif /* _KINST_ISA_H_ */
