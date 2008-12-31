/*-
 * Copyright (c) 2003 Peter Wemm.
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
__FBSDID("$FreeBSD: src/sys/amd64/amd64/sys_machdep.c,v 1.90.18.1 2008/11/25 02:59:29 kensmith Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/lock.h>
#include <sys/proc.h>
#include <sys/sysproto.h>
#include <machine/specialreg.h>
#include <machine/sysarch.h>
#include <machine/pcb.h>

#include <vm/vm.h>
#include <vm/pmap.h>
#include <machine/vmparam.h>

#ifndef _SYS_SYSPROTO_H_
struct sysarch_args {
	int op;
	char *parms;
};
#endif

int
sysarch(td, uap)
	struct thread *td;
	register struct sysarch_args *uap;
{
	int error = 0;
	struct pcb *pcb = curthread->td_pcb;
	uint32_t i386base;
	uint64_t a64base;

	switch(uap->op) {
	case I386_GET_FSBASE:
		i386base = pcb->pcb_fsbase;
		error = copyout(&i386base, uap->parms, sizeof(i386base));
		break;
	case I386_SET_FSBASE:
		error = copyin(uap->parms, &i386base, sizeof(i386base));
		if (!error) {
			critical_enter();
			wrmsr(MSR_FSBASE, i386base);
			pcb->pcb_fsbase = i386base;
			critical_exit();
		}
		break;
	case I386_GET_GSBASE:
		i386base = pcb->pcb_gsbase;
		error = copyout(&i386base, uap->parms, sizeof(i386base));
		break;
	case I386_SET_GSBASE:
		error = copyin(uap->parms, &i386base, sizeof(i386base));
		if (!error) {
			critical_enter();
			wrmsr(MSR_KGSBASE, i386base);
			pcb->pcb_gsbase = i386base;
			critical_exit();
		}
		break;
	case AMD64_GET_FSBASE:
		error = copyout(&pcb->pcb_fsbase, uap->parms, sizeof(pcb->pcb_fsbase));
		break;
		
	case AMD64_SET_FSBASE:
		error = copyin(uap->parms, &a64base, sizeof(a64base));
		if (!error) {
			if (a64base < VM_MAXUSER_ADDRESS) {
				critical_enter();
				wrmsr(MSR_FSBASE, a64base);
				pcb->pcb_fsbase = a64base;
				critical_exit();
			} else {
				error = EINVAL;
			}
		}
		break;

	case AMD64_GET_GSBASE:
		error = copyout(&pcb->pcb_gsbase, uap->parms, sizeof(pcb->pcb_gsbase));
		break;

	case AMD64_SET_GSBASE:
		error = copyin(uap->parms, &a64base, sizeof(a64base));
		if (!error) {
			if (a64base < VM_MAXUSER_ADDRESS) {
				critical_enter();
				wrmsr(MSR_KGSBASE, a64base);
				pcb->pcb_gsbase = a64base;
				critical_exit();
			} else {
				error = EINVAL;
			}
		}
		break;

	default:
		error = EINVAL;
		break;
	}
	return (error);
}
