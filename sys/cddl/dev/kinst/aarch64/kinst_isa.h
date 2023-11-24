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

struct kinst_probe_md {
	bool	emulate;		/* emulate in sw */
};

#endif /* _KINST_ISA_H_ */
