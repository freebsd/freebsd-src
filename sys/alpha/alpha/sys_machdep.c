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
 * $FreeBSD$
 *
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/sysproto.h>
#include <sys/sysent.h>
#include <sys/proc.h>

#include <vm/vm.h>
#include <sys/lock.h>
#include <vm/pmap.h>
#include <vm/vm_map.h>
#include <vm/vm_extern.h>

#include <sys/user.h>

#include <machine/cpu.h>
#include <machine/sysarch.h>

#include <vm/vm_kern.h>		/* for kernel_map */

#include <machine/fpu.h>

#ifndef _SYS_SYSPROTO_H_
struct sysarch_args {
	int op;
	char *parms;
};
#endif

static int alpha_sethae(struct proc *p, char *args);
static int alpha_get_fpmask(struct proc *p, char *args);
static int alpha_set_fpmask(struct proc *p, char *args);
static int alpha_set_uac(struct proc *p, char *args);
static int alpha_get_uac(struct proc *p, char *args);

int
sysarch(p, uap)
	struct proc *p;
	register struct sysarch_args *uap;
{
	int error = 0;

	switch(SCARG(uap,op)) {
	case ALPHA_SETHAE:
		error = alpha_sethae(p, uap->parms);
		break;
	case ALPHA_GET_FPMASK:
		error = alpha_get_fpmask(p, uap->parms);
		break;
	case ALPHA_SET_FPMASK:
		error = alpha_set_fpmask(p, uap->parms);
		break;
	case ALPHA_SET_UAC:
		error = alpha_set_uac(p, uap->parms);
		break;
	case ALPHA_GET_UAC:
		error = alpha_get_uac(p, uap->parms);
		break;
	    
	default:
		error = EINVAL;
		break;
	}
	return (error);
}

struct alpha_sethae_args {
	u_int64_t hae;
};

static int
alpha_sethae(struct proc *p, char *args)
{
	int error;
	struct alpha_sethae_args ua;

	error = copyin(args, &ua, sizeof(struct alpha_sethae_args));
	if (error)
		return (error);

	if (securelevel > 0)
		return (EPERM);

	error = suser(p);
	if (error)
		return (error);

	p->p_md.md_flags |= MDP_HAEUSED;
	p->p_md.md_hae = ua.hae;

	return (0);
}

struct alpha_fpmask_args {
	u_int64_t mask;
};

static	int
alpha_get_fpmask(struct proc *p, char *args)
{
	int error;
	struct alpha_fpmask_args ua;

	ua.mask = p->p_addr->u_pcb.pcb_fp_control;
	error = copyout(&ua, args, sizeof(struct alpha_fpmask_args));

	return (error);
}

static	int
alpha_set_fpmask(struct proc *p, char *args)
{
	int error;
	u_int64_t oldmask, *fp_control;
	struct alpha_fpmask_args ua;
	
	error = copyin(args, &ua, sizeof(struct alpha_fpmask_args));
	if (error)
		return (error);

	fp_control = &p->p_addr->u_pcb.pcb_fp_control;
	oldmask = *fp_control;
	*fp_control = ua.mask & IEEE_TRAP_ENABLE_MASK;
	ua.mask = oldmask;

	error = copyout(&ua, args, sizeof(struct alpha_fpmask_args));
	return (error);
}

static	int
alpha_set_uac(struct proc *p, char *args)
{
	int error, s;
	unsigned long uac;

	error = copyin(args, &uac, sizeof(uac));
	if (error)
		return (error);

	if (p->p_pptr) {
		s = splimp();
		if (p->p_pptr) {
			p->p_pptr->p_md.md_flags &= ~MDP_UAC_MASK;
			p->p_pptr->p_md.md_flags |= uac & MDP_UAC_MASK;
		} else
			error = ESRCH;
		splx(s);
	}
	return error;
}

static	int
alpha_get_uac(struct proc *p, char *args)
{
	int error, s;
	unsigned long uac;

	error = ESRCH;
	if (p->p_pptr) {
		s = splimp();
		if (p->p_pptr) {
			uac = p->p_pptr->p_md.md_flags & MDP_UAC_MASK;
			error = copyout(&uac, args, sizeof(uac));
		}
		splx(s);
	}
	return error;
}
