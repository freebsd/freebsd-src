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
 *	$Id: sys_machdep.c,v 1.21 1997/02/22 09:32:53 peter Exp $
 *
 */

#include "opt_user_ldt.h"
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/sysproto.h>
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

#define MAX_LD 8192
#define LD_PER_PAGE 512
#define NEW_MAX_LD(num)  ((num + LD_PER_PAGE) & ~(LD_PER_PAGE-1))
#define SIZE_FROM_LARGEST_LD(num) (NEW_MAX_LD(num) << 3)



void set_user_ldt	__P((struct pcb *pcb));
#ifdef USER_LDT
static int i386_get_ldt	__P((struct proc *, char *, int *));
static int i386_set_ldt	__P((struct proc *, char *, int *));
#endif

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
/*
 * Update the GDT entry pointing to the LDT to point to the LDT of the
 * current process.
 */   
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

static int
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
	struct i386_get_ldt_args ua;
	struct i386_get_ldt_args *uap = &ua;

	if ((error = copyin(args, uap, sizeof(struct i386_get_ldt_args))) < 0)
		return(error);

#ifdef	DEBUG
	printf("i386_get_ldt: start=%d num=%d descs=%x\n", uap->start,
		uap->num, uap->desc);
#endif

	/* verify range of LDTs exist */
	if ((uap->start < 0) || (uap->num <= 0))
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

static int
i386_set_ldt(p, args, retval)
	struct proc *p;
	char *args;
	int *retval;
{
	int error = 0, i, n;
 	int largest_ld;
	struct pcb *pcb = &p->p_addr->u_pcb;
	int s;
	struct i386_set_ldt_args ua, *uap;

	if ((error = copyin(args, &ua, sizeof(struct i386_set_ldt_args))) < 0)
		return(error);

	uap = &ua;

#ifdef	DEBUG
	printf("i386_set_ldt: start=%d num=%d descs=%x\n", uap->start, uap->num, uap->desc);
#endif

 	/* verify range of descriptors to modify */
 	if ((uap->start < 0) || (uap->start >= MAX_LD) || (uap->num < 0) ||
 		(uap->num > MAX_LD))
 	{
 		return(EINVAL);
 	}
 	largest_ld = uap->start + uap->num - 1;
 	if (largest_ld >= MAX_LD)
  		return(EINVAL);
  
  	/* allocate user ldt */
 	if (!pcb->pcb_ldt || (largest_ld >= pcb->pcb_ldt_len)) {
 		union descriptor *new_ldt = (union descriptor *)kmem_alloc(
 			kernel_map, SIZE_FROM_LARGEST_LD(largest_ld));
 		if (new_ldt == NULL) {
 			return ENOMEM;
 		}
 		if (pcb->pcb_ldt) {
 			bcopy(pcb->pcb_ldt, new_ldt, pcb->pcb_ldt_len
 				* sizeof(union descriptor));
 			kmem_free(kernel_map, (vm_offset_t)pcb->pcb_ldt,
 				pcb->pcb_ldt_len * sizeof(union descriptor));
 		} else {
 			bcopy(ldt, new_ldt, sizeof(ldt));
 		}
  		pcb->pcb_ldt = (caddr_t)new_ldt;
 		pcb->pcb_ldt_len = NEW_MAX_LD(largest_ld);
 		if (pcb == curpcb)
 		    set_user_ldt(pcb);
  	}

	/* Check descriptors for access violations */
	for (i = 0, n = uap->start; i < uap->num; i++, n++) {
		union descriptor desc, *dp;
		dp = &uap->desc[i];
		error = copyin(dp, &desc, sizeof(union descriptor));
		if (error)
			return(error);

		switch (desc.sd.sd_type) {
 		case SDT_SYSNULL:	/* system null */ 
 			desc.sd.sd_p = 0;
  			break;
 		case SDT_SYS286TSS: /* system 286 TSS available */
 		case SDT_SYSLDT:    /* system local descriptor table */
 		case SDT_SYS286BSY: /* system 286 TSS busy */
 		case SDT_SYSTASKGT: /* system task gate */
 		case SDT_SYS286IGT: /* system 286 interrupt gate */
 		case SDT_SYS286TGT: /* system 286 trap gate */
 		case SDT_SYSNULL2:  /* undefined by Intel */ 
 		case SDT_SYS386TSS: /* system 386 TSS available */
 		case SDT_SYSNULL3:  /* undefined by Intel */
 		case SDT_SYS386BSY: /* system 386 TSS busy */
 		case SDT_SYSNULL4:  /* undefined by Intel */ 
 		case SDT_SYS386IGT: /* system 386 interrupt gate */
 		case SDT_SYS386TGT: /* system 386 trap gate */
 		case SDT_SYS286CGT: /* system 286 call gate */ 
 		case SDT_SYS386CGT: /* system 386 call gate */
 			/* I can't think of any reason to allow a user proc
 			 * to create a segment of these types.  They are
 			 * for OS use only.
 			 */
     	    	    	return EACCES;
 
 		/* memory segment types */
 		case SDT_MEMEC:   /* memory execute only conforming */
 		case SDT_MEMEAC:  /* memory execute only accessed conforming */
 		case SDT_MEMERC:  /* memory execute read conforming */
 		case SDT_MEMERAC: /* memory execute read accessed conforming */
                         /* Must be "present" if executable and conforming. */
                         if (desc.sd.sd_p == 0)
                                 return (EACCES);
 			break;
 		case SDT_MEMRO:   /* memory read only */
 		case SDT_MEMROA:  /* memory read only accessed */
 		case SDT_MEMRW:   /* memory read write */
 		case SDT_MEMRWA:  /* memory read write accessed */
 		case SDT_MEMROD:  /* memory read only expand dwn limit */
 		case SDT_MEMRODA: /* memory read only expand dwn lim accessed */
 		case SDT_MEMRWD:  /* memory read write expand dwn limit */  
 		case SDT_MEMRWDA: /* memory read write expand dwn lim acessed */
 		case SDT_MEME:    /* memory execute only */ 
 		case SDT_MEMEA:   /* memory execute only accessed */
 		case SDT_MEMER:   /* memory execute read */
 		case SDT_MEMERA:  /* memory execute read accessed */
			break;
		default:
			return(EINVAL);
			/*NOTREACHED*/
		}
 
 		/* Only user (ring-3) descriptors may be present. */
 		if ((desc.sd.sd_p != 0) && (desc.sd.sd_dpl != SEL_UPL))
 			return (EACCES);
	}

	s = splhigh();

	/* Fill in range */
 	error = copyin(uap->desc, 
 		 &((union descriptor *)(pcb->pcb_ldt))[uap->start],
 		uap->num * sizeof(union descriptor));
 	if (!error)
  		*retval = uap->start;

	splx(s);
	return(error);
}
#endif	/* USER_LDT */
