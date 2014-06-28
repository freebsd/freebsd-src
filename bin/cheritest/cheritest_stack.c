/*-
 * Copyright (c) 2014 Robert N. M. Watson
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

#include <cheri/cheri_enter.h>
#include <cheri/cheri_fd.h>
#include <cheri/sandbox.h>

#include <cheritest-helper.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <unistd.h>

#include "cheritest.h"

register_t
cheritest_libcheri_userfn_getstack(void)
{
	struct cheri_object sandbox_object;
	struct cheri_stack cs;
	struct cheri_stack_frame *csfp;
	int retval;

	retval = sysarch(CHERI_GET_STACK, &cs);
	if (retval != 0)
		cheritest_failure_err("sysarch(CHERI_GET_STACK) failed");

	/* Validate that two stack frames are found. */
	if (cs.cs_tsp != CHERI_STACK_SIZE - (2 * CHERI_FRAME_SIZE))
		cheritest_failure_errx("stack contains %d frames; expected "
		    "2", (CHERI_STACK_SIZE - (2 * CHERI_FRAME_SIZE)) /
		    CHERI_FRAME_SIZE);

	/* Validate that the first is a saved ambient context. */
	csfp = &cs.cs_frames[CHERI_STACK_DEPTH - 1];
	if (csfp->csf_pcc != cheri_getpcc())
		cheritest_failure_errx("frame 0: not global code cap");

	/* Validate that the second is cheritest_objectp. */
	csfp = &cs.cs_frames[CHERI_STACK_DEPTH - 2];
	if (csfp->csf_pcc !=
	    sandbox_object_getobject(cheritest_objectp).co_codecap)
		cheritest_failure_errx("frame 1: not sandbox code cap");
	return (0);
}

void
cheritest_getstack(void)
{
	__capability void *cclear;
	register_t v;

	cclear = cheri_zerocap();
	v = sandbox_object_cinvoke(cheritest_objectp,
	    CHERITEST_HELPER_LIBCHERI_USERFN, CHERITEST_USERFN_GETSTACK,
	    0, 0, 0, 0, 0, 0,
	    sandbox_object_getsystemobject(cheritest_objectp).co_codecap,
	    sandbox_object_getsystemobject(cheritest_objectp).co_datacap,
	    cclear, cclear, cclear, cclear, cclear, cclear);
	if (v != 0)
		cheritest_failure_errx("Incorrect return value 0x%ld"
		    " (expected 0)\n", v);
	cheritest_success();
}
