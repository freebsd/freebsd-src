/*-
 * Copyright (c) 2011-2014 Robert N. M. Watson
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

#include "opt_ddb.h"

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/syscall.h>
#include <sys/sysctl.h>

#include <ddb/ddb.h>
#include <sys/kdb.h>

#include <machine/atomic.h>
#include <machine/cheri.h>
#include <machine/pcb.h>
#include <machine/sysarch.h>

/*
 * Only allow most system calls from either ambient authority, or from
 * sandboxes that have been explicitly delegated CHERI_PERM_SYSCALL via their
 * code capability.  Note that CHERI_PERM_SYSCALL effectively implies ambient
 * authority, as the kernel does not [currently] interpret pointers/lengths
 * via userspace $c0.
 */
int
cheri_syscall_authorize(struct thread *td, u_int code, int nargs,
    register_t *args)
{
	uintmax_t c_perms;

	/*
	 * Allow the cycle counter to be read via sysarch.
	 *
	 * XXXRW: Now that we support a userspace cycle counter, we should
	 * remove this.
	 */
	if (code == SYS_sysarch && args[0] == MIPS_GET_COUNT)
		return (0);

	/*
	 * Check whether userspace holds the rights defined in
	 * cheri_capability_set_user() in $PCC.  Note that object type doesn't
	 * come into play here.
	 *
	 * XXXRW: Possibly ECAPMODE should be EPROT or ESANDBOX?
	 */
	CHERI_CLC(CHERI_CR_CTEMP0, CHERI_CR_KDC,
	    &td->td_pcb->pcb_cheriframe.cf_pcc, 0);
	CHERI_CGETPERM(c_perms, CHERI_CR_CTEMP0);
	if ((c_perms & CHERI_PERM_SYSCALL) == 0) {
		atomic_add_int(&security_cheri_syscall_violations, 1);

#if DDB
		if (security_cheri_debugger_on_sandbox_syscall)
			kdb_enter(KDB_WHY_CHERI,
			    "Syscall rejected in CHERI sandbox");
#endif
		return (ECAPMODE);
	}
	return (0);
}
