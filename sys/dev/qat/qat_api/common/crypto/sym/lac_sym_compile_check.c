/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright(c) 2007-2025 Intel Corporation */

/**
 ***************************************************************************
 * @file lac_sym_compile_check.c
 *
 * @ingroup LacSym
 *
 * This file checks at compile time that some assumptions about the layout
 * of key structures are as expected.
 *
 *
 ***************************************************************************/

#include "cpa.h"

#include "lac_common.h"
#include "icp_accel_devices.h"
#include "icp_adf_debug.h"
#include "lac_sym.h"
#include "cpa_cy_sym_dp.h"

#define COMPILE_TIME_ASSERT(pred)                                              \
	switch (0) {                                                           \
	case 0:                                                                \
	case pred:;                                                            \
	}

void
LacSym_CompileTimeAssertions(void)
{
	/* *************************************************************
	 * Check sessionCtx is at the same location in bulk cookie and
	 * CpaCySymDpOpData.
	 * This is required for the callbacks to work as expected -
	 * see LacSymCb_ProcessCallback
	 * ************************************************************* */

	COMPILE_TIME_ASSERT(offsetof(lac_sym_bulk_cookie_t, sessionCtx) ==
			    offsetof(CpaCySymDpOpData, sessionCtx));
}
