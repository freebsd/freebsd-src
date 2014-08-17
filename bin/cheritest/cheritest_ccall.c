/*-
 * Copyright (c) 2012-2014 Robert N. M. Watson
 * Copyright (c) 2014 SRI International
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

#include <cheri/cheri_fd.h>
#include <cheri/sandbox.h>

#include <cheritest-helper.h>
#include <err.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <unistd.h>

#include "cheritest.h"
#include "cheritest_sandbox.h"

/*
 * Configure $c1 and $c2 to enter a simple sandbox.  Not suitable for more
 * complex tests as it has no notion of configuring heaps/stacks/etc.  For
 * that, we use libcheri.
 */
void
cheritest_sandbox_setup(void *sandbox_base, void *sandbox_end,
    register_t sandbox_pc, __capability void **codecapp,
    __capability void **datacapp)
{
	__capability void *codecap, *datacap, *basecap;

	basecap = cheri_ptrtype(sandbox_base, (uintptr_t)sandbox_end -
	    (uintptr_t)sandbox_base, sandbox_pc);

	codecap = cheri_andperm(basecap, CHERI_PERM_EXECUTE |
	    CHERI_PERM_SEAL | CHERI_PERM_STORE_EPHEM_CAP);
	codecap = cheri_sealcode(codecap);

	datacap = cheri_andperm(basecap, CHERI_PERM_LOAD | CHERI_PERM_STORE |
	    CHERI_PERM_LOAD_CAP | CHERI_PERM_STORE_CAP);
	datacap = cheri_sealdata(datacap, basecap);

	*codecapp = codecap;
	*datacapp = datacap;
}

/*
 * Trigger a CReturn underflow by trying to return from an unsandboxed
 * context.
 */
void
test_fault_creturn(const struct cheri_test *ctp __unused)
{

	CHERI_CRETURN();
}

/*
 * CCall code that will immediately CReturn.
 */
void
test_nofault_ccall_creturn(const struct cheri_test *ctp __unused)
{
	__capability void *codecap, *datacap;

	cheritest_sandbox_setup(&sandbox_creturn, &sandbox_creturn_end, 0,
	    &codecap, &datacap);
	cheritest_ccall(codecap, datacap);
	cheritest_success();
}

/*
 * CCall code that will execute a few NOPs, then CReturn.
 */
void
test_nofault_ccall_nop_creturn(const struct cheri_test *ctp __unused)
{
	__capability void *codecap, *datacap;

	cheritest_sandbox_setup(&sandbox_nop_creturn,
	    &sandbox_nop_creturn_end, 0, &codecap, &datacap);
	cheritest_ccall(codecap, datacap);
	cheritest_success();
}

/*
 * CCall code that will load a value (0x1234) into a return register,
 * which we can check for.  We use getpid() to ensure a well-known and quite
 * different value appears in the return register prior to CCall.
 */
#define	DLI_RETVAL	0x1234
void
test_nofault_ccall_dli_creturn(const struct cheri_test *ctp __unused)
{
	__capability void *codecap, *datacap;
	register_t v0;

	v0 = getpid();
	cheritest_sandbox_setup(&sandbox_dli_creturn,
	    &sandbox_dli_creturn_end, 0, &codecap, &datacap);
	v0 = cheritest_ccall(codecap, datacap);
	if (v0 != DLI_RETVAL)
		cheritest_failure_errx("Invalid return value (got: 0x%jx; "
		    "expected 0x%jx)", v0, DLI_RETVAL);
	else
		cheritest_success();
}
