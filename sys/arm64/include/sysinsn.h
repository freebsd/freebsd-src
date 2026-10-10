/*
 * Copyright (c) 2026 Arm Ltd
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#ifndef _MACHINE_SYSINSN_H_
#define	_MACHINE_SYSINSN_H_

#include <machine/_armreg.h>

#define	TLBI_RVAAE1IS_op0	1
#define	TLBI_RVAAE1IS_op1	0
#define	TLBI_RVAAE1IS_CRn	8
#define	TLBI_RVAAE1IS_CRm	2
#define	TLBI_RVAAE1IS_op2	3
#define	tlbi_rvaae1is(x)	SYS_ARG(TLBI_RVAAE1IS, x)

#define	TLBI_RVAALE1IS_op0	1
#define	TLBI_RVAALE1IS_op1	0
#define	TLBI_RVAALE1IS_CRn	8
#define	TLBI_RVAALE1IS_CRm	2
#define	TLBI_RVAALE1IS_op2	7
#define	tlbi_rvaale1is(x)	SYS_ARG(TLBI_RVAALE1IS, x)

#define	TLBI_RVAE1IS_op0	1
#define	TLBI_RVAE1IS_op1	0
#define	TLBI_RVAE1IS_CRn	8
#define	TLBI_RVAE1IS_CRm	2
#define	TLBI_RVAE1IS_op2	1
#define	tlbi_rvae1is(x)		SYS_ARG(TLBI_RVAE1IS, x)

#define	TLBI_RVALE1IS_op0	1
#define	TLBI_RVALE1IS_op1	0
#define	TLBI_RVALE1IS_CRn	8
#define	TLBI_RVALE1IS_CRm	2
#define	TLBI_RVALE1IS_op2	5
#define	tlbi_rvale1is(x)	SYS_ARG(TLBI_RVALE1IS, x)

#endif
