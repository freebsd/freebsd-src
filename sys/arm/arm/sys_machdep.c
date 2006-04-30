/*-
 * Copyright (c) 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	from: @(#)sys_machdep.c	5.5 (Berkeley) 1/19/91
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/sysproto.h>
#include <sys/syscall.h>
#include <sys/sysent.h>

#include <machine/sysarch.h>

#ifndef _SYS_SYSPROTO_H_
struct sysarch_args {
	int op;
	char *parms;
};
#endif

/* Prototypes */
static int arm32_sync_icache (struct thread *, void *);
static int arm32_drain_writebuf(struct thread *, void *);

static int
arm32_sync_icache(struct thread *td, void *args)
{
	struct arm_sync_icache_args ua;
	int error;

	if ((error = copyin(args, &ua, sizeof(ua))) != 0)
		return (error);

	cpu_icache_sync_range(ua.addr, ua.len);

	td->td_retval[0] = 0;
	return(0);
}

static int
arm32_drain_writebuf(struct thread *td, void *args)
{
	/* No args. */

	td->td_retval[0] = 0;
	cpu_drain_writebuf();
	return(0);
}

static int
arm32_set_tp(struct thread *td, void *args)
{

	td->td_md.md_tp = args;
	return (0);
}

static int
arm32_get_tp(struct thread *td, void *args)
{

	td->td_retval[0] = (uint32_t)td->td_md.md_tp;
	return (0);
}

int
sysarch(td, uap)
	struct thread *td;
	register struct sysarch_args *uap;
{
	int error;

	switch (uap->op) {
	case ARM_SYNC_ICACHE : 
		error = arm32_sync_icache(td, uap->parms);
		break;
		
	case ARM_DRAIN_WRITEBUF : 
		error = arm32_drain_writebuf(td, uap->parms);
		break;
	case ARM_SET_TP:
		error = arm32_set_tp(td, uap->parms);
		break;
	case ARM_GET_TP:
		error = arm32_get_tp(td, uap->parms);
		break;
	default:
		error = EINVAL;
		break;
	}
	return (error);
}

