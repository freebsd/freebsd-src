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
 *	$Id: sys_machdep.c,v 1.11 1995/11/12 07:10:47 bde Exp $
 *
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/sysproto.h>
#include <sys/proc.h>

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/vm_prot.h>
#include <vm/lock.h>
#include <vm/pmap.h>
#include <vm/vm_map.h>
#include <vm/vm_extern.h>

#include <sys/user.h>

#include <machine/cpu.h>
#include <machine/sysarch.h>

#include <vm/vm_kern.h>		/* for kernel_map */

void set_user_ldt	__P((struct pcb *pcb));
int i386_get_ldt	__P((struct proc *, char *, int *));
int i386_set_ldt	__P((struct proc *, char *, int *));

#ifndef _SYS_SYSPROTO_H_
struct sysarch_args {
	int op;
	char *parms;
};
#endif

int
sysarch(p, uap, retval)
	struct proc *p;
	register struct sysarch_args *uap;
	int *retval;
{
	int error = 0;

	switch(uap->op) {
#ifdef	USER_LDT
	case I386_GET_LDT:
		error = i386_get_ldt(p, uap->parms, retval);
		break;

	case I386_SET_LDT:
		error = i386_set_ldt(p, uap->parms, retval);
		break;
#endif
	default:
		error = EINVAL;
		break;
	}
	return(error);
}

#ifdef USER_LDT
void
set_user_ldt(struct pcb *pcb)
{
	gdt_segs[GUSERLDT_SEL].ssd_base = (unsigned)pcb->pcb_ldt;
	gdt_segs[GUSERLDT_SEL].ssd_limit = (pcb->pcb_ldt_len * sizeof(union descriptor)) - 1;
	ssdtosd(&gdt_segs[GUSERLDT_SEL], &gdt[GUSERLDT_SEL].sd);
	lldt(GSEL(GUSERLDT_SEL, SEL_KPL));
	currentldt = GSEL(GUSERLDT_SEL, SEL_KPL);
}

struct i386_get_ldt_args {
	int start;
	union descriptor *desc;
	int num;
};

int
i386_get_ldt(p, args, retval)
	struct proc *p;
	char *args;
	int *retval;
{
	int error = 0;
	struct pcb *pcb = &p->p_addr->u_pcb;
	int nldt, num;
	union descriptor *lp;
	int s;
	struct i386_get_ldt_args ua, *uap;

	if ((error = copyin(args, &ua, sizeof(struct i386_get_ldt_args))) < 0)
		return(error);

	uap = &ua;
#ifdef	DEBUG
	printf("i386_get_ldt: start=%d num=%d descs=%x\n", uap->start, uap->num, uap->desc);
#endif

	if (uap->start < 0 || uap->num < 0)
		return(EINVAL);

	s = splhigh();

	if (pcb->pcb_ldt) {
		nldt = pcb->pcb_ldt_len;
		num = min(uap->num, nldt);
		lp = &((union descriptor *)(pcb->pcb_ldt))[uap->start];
	} else {
		nldt = sizeof(ldt)/sizeof(ldt[0]);
		num = min(uap->num, nldt);
		lp = &ldt[uap->start];
	}
	if (uap->start > nldt) {
		splx(s);
		return(EINVAL);
	}

	error = copyout(lp, uap->desc, num * sizeof(union descriptor));
	if (!error)
		*retval = num;

	splx(s);
	return(error);
}

struct i386_set_ldt_args {
	int start;
	union descriptor *desc;
	int num;
};

int
i386_set_ldt(p, args, retval)
	struct proc *p;
	char *args;
	int *retval;
{
	int error = 0, i, n;
	struct pcb *pcb = &p->p_addr->u_pcb;
	union descriptor *lp;
	int s;
	struct i386_set_ldt_args ua, *uap;

	if ((error = copyin(args, &ua, sizeof(struct i386_set_ldt_args))) < 0)
		return(error);

	uap = &ua;

#ifdef	DEBUG
	printf("i386_set_ldt: start=%d num=%d descs=%x\n", uap->start, uap->num, uap->desc);
#endif

	if (uap->start < 0 || uap->num < 0)
		return(EINVAL);

	/* XXX Should be 8192 ! */
	if (uap->start > 512 ||
	    (uap->start + uap->num) > 512)
		return(EINVAL);

	/* allocate user ldt */
	if (!pcb->pcb_ldt) {
		union descriptor *new_ldt =
			(union descriptor *)kmem_alloc(kernel_map, 512*sizeof(union descriptor));
		bcopy(ldt, new_ldt, sizeof(ldt));
		pcb->pcb_ldt = (caddr_t)new_ldt;
		pcb->pcb_ldt_len = 512;		/* XXX need to grow */
#ifdef DEBUG
		printf("i386_set_ldt(%d): new_ldt=%x\n", p->p_pid, new_ldt);
#endif
	}

	/* Check descriptors for access violations */
	for (i = 0, n = uap->start; i < uap->num; i++, n++) {
		union descriptor desc, *dp;
		dp = &uap->desc[i];
		error = copyin(dp, &desc, sizeof(union descriptor));
		if (error)
			return(error);

		/* Only user (ring-3) descriptors */
		if (desc.sd.sd_dpl != SEL_UPL)
			return(EACCES);

		/* Must be "present" */
		if (desc.sd.sd_p == 0)
			return(EACCES);

		switch (desc.sd.sd_type) {
		case SDT_SYSNULL:
		case SDT_SYS286CGT:
		case SDT_SYS386CGT:
			break;
		case SDT_MEMRO:
		case SDT_MEMROA:
		case SDT_MEMRW:
		case SDT_MEMRWA:
		case SDT_MEMROD:
		case SDT_MEMRODA:
		case SDT_MEME:
		case SDT_MEMEA:
		case SDT_MEMER:
		case SDT_MEMERA:
		case SDT_MEMEC:
		case SDT_MEMEAC:
		case SDT_MEMERC:
		case SDT_MEMERAC: {
#if 0
			unsigned long base = (desc.sd.sd_hibase << 24)&0xFF000000;
			base |= (desc.sd.sd_lobase&0x00FFFFFF);
			if (base >= KERNBASE)
				return(EACCES);
#endif
			break;
		}
		default:
			return(EACCES);
			/*NOTREACHED*/
		}
	}

	s = splhigh();

	/* Fill in range */
	for (i = 0, n = uap->start; i < uap->num && !error; i++, n++) {
		union descriptor desc, *dp;
		dp = &uap->desc[i];
		lp = &((union descriptor *)(pcb->pcb_ldt))[n];
#ifdef DEBUG
		printf("i386_set_ldt(%d): ldtp=%x\n", p->p_pid, lp);
#endif
		error = copyin(dp, lp, sizeof(union descriptor));
	}
	if (!error) {
		*retval = uap->start;
/*		need_resched(); */
	}

	splx(s);
	return(error);
}
#endif	/* USER_LDT */
