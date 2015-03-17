/*-
 * Copyright (c) 2014-2015 Robert N. M. Watson
 * Copyright (c) 2015 SRI International
 * All rights reserved.
 *
 * This software was developed by SRI International and the University of
 * Cambridge Computer Laboratory under DARPA/AFRL contract (FA8750-10-C-0237)
 * ("CTSRD"), as part of the DARPA CRASH research programme.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>

#if !__has_feature(capabilities)
#error "This code requires a CHERI-aware compiler"
#endif

#include <sys/types.h>
#include <sys/sysctl.h>
#include <sys/time.h>

#include <machine/cheri.h>
#include <machine/cheric.h>
#include <machine/cpuregs.h>
#include <machine/sysarch.h>

#include <errno.h>
#include <inttypes.h>

int
cheri_unwind_stack(void)
{
	struct cheri_stack cs;
	struct cheri_stack_frame *csfp;
	u_int stack_depth, stack_frames;
	int retval;

	/* Validate stack as retrieved. */
	retval = sysarch(CHERI_GET_STACK, &cs);
	if (retval != 0)
		return (-1);

	/* Does stack layout look sensible enough to continue? */
	if ((cs.cs_tsize % CHERI_FRAME_SIZE) != 0)
		return (-1);
	stack_depth = cs.cs_tsize / CHERI_FRAME_SIZE;

	if ((cs.cs_tsp % CHERI_FRAME_SIZE) != 0)
		return (-1);

	stack_frames = (cs.cs_tsize - cs.cs_tsp) / CHERI_FRAME_SIZE;
	/* If there are zero or one frames we don't need to unwind. */
	if (stack_frames < 2)
		return (0);

	/* Validate that the first is a saved ambient context. */
	csfp = &cs.cs_frames[stack_depth - 1];
	if (cheri_getbase(csfp->csf_pcc) != cheri_getbase(cheri_getpcc()) ||
	    cheri_getlen(csfp->csf_pcc) != cheri_getlen(cheri_getpcc()))
		return (-1);

	cs.cs_tsp += (stack_frames - 1) * CHERI_FRAME_SIZE;

	/* Update kernel view of trusted stack. */
	retval = sysarch(CHERI_SET_STACK, &cs);
	if (retval != 0)
		return (-1);

	return (0);
}
