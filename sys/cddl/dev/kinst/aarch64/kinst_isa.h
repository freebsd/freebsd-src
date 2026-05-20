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

#define KINST_PATCHVAL		DTRACE_PATCHVAL

/*
 * The trampoline contains [instruction, brk].
 */
#define KINST_TRAMP_SIZE	8

typedef uint32_t kinst_patchval_t;

enum kinst_instr {
	KINST_INSTR_ADR,	/* adr/adrp */
	KINST_INSTR_B,
	KINST_INSTR_BCOND,
	KINST_INSTR_BL,
	KINST_INSTR_CBZ,	/* cbz/cbnz */
	KINST_INSTR_TBZ,	/* tbnz/tbz */
	KINST_INSTR_LDR_LITERAL,
	KINST_INSTR_LDX,
	KINST_INSTR_STX,
	KINST_INSTR_COMMON,
};

struct kinst_probe_md {
	enum kinst_instr	kp_type;
};

#endif /* _KINST_ISA_H_ */
