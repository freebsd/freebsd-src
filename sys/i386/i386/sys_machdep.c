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

#include "opt_user_ldt.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/sysproto.h>
#include <sys/malloc.h>
#include <sys/proc.h>

#include <vm/vm.h>
#include <sys/lock.h>
#include <vm/pmap.h>
#include <vm/vm_map.h>
#include <vm/vm_extern.h>

#include <sys/user.h>

#include <machine/cpu.h>
#include <machine/ipl.h>
#include <machine/pcb_ext.h>	/* pcb.h included by sys/user.h */
#include <machine/sysarch.h>
#ifdef SMP
#include <machine/smp.h>
#endif

#include <vm/vm_kern.h>		/* for kernel_map */

#define MAX_LD 8192
#define LD_PER_PAGE 512
#define NEW_MAX_LD(num)  ((num + LD_PER_PAGE) & ~(LD_PER_PAGE-1))
#define SIZE_FROM_LARGEST_LD(num) (NEW_MAX_LD(num) << 3)



#ifdef USER_LDT
static int i386_get_ldt	__P((struct proc *, char *));
static int i386_set_ldt	__P((struct proc *, char *));
#endif
static int i386_get_ioperm	__P((struct proc *, char *));
static int i386_set_ioperm	__P((struct proc *, char *));
int i386_extend_pcb	__P((struct proc *));

#ifndef _SYS_SYSPROTO_H_
struct sysarch_args {
	int op;
	char *parms;
};
#endif

int
sysarch(p, uap)
	struct proc *p;
	register struct sysarch_args *uap;
{
	int error = 0;

	switch(uap->op) {
#ifdef	USER_LDT
	case I386_GET_LDT:
		error = i386_get_ldt(p, uap->parms);
		break;

	case I386_SET_LDT:
		error = i386_set_ldt(p, uap->parms);
		break;
#endif
	case I386_GET_IOPERM:
		error = i386_get_ioperm(p, uap->parms);
		break;
	case I386_SET_IOPERM:
		error = i386_set_ioperm(p, uap->parms);
		break;
	case I386_VM86:
		error = vm86_sysarch(p, uap->parms);
		break;
	default:
		error = EOPNOTSUPP;
		break;
	}
	return (error);
}

int
i386_extend_pcb(struct proc *p)
{
	int i, offset;
	u_long *addr;
	struct pcb_ext *ext;
	struct soft_segment_descriptor ssd = {
		0,			/* segment base address (overwritten) */
		ctob(IOPAGES + 1) - 1,	/* length */
		SDT_SYS386TSS,		/* segment type */
		0,			/* priority level */
		1,			/* descriptor present */
		0, 0,
		0,			/* default 32 size */
		0			/* granularity */
	};

	ext = (struct pcb_ext *)kmem_alloc(kernel_map, ctob(IOPAGES+1));
	if (ext == 0)
		return (ENOMEM);
	p->p_addr->u_pcb.pcb_ext = ext;
	bzero(ext, sizeof(struct pcb_ext)); 
	ext->ext_tss.tss_esp0 = (unsigned)p->p_addr + ctob(UPAGES) - 16;
	ext->ext_tss.tss_ss0 = GSEL(GDATA_SEL, SEL_KPL);
	/*
	 * The last byte of the i/o map must be followed by an 0xff byte.
	 * We arbitrarily allocate 16 bytes here, to keep the starting
	 * address on a doubleword boundary.
	 */
	offset = PAGE_SIZE - 16;
	ext->ext_tss.tss_ioopt = 
	    (offset - ((unsigned)&ext->ext_tss - (unsigned)ext)) << 16;
	ext->ext_iomap = (caddr_t)ext + offset;
	ext->ext_vm86.vm86_intmap = (caddr_t)ext + offset - 32;

	addr = (u_long *)ext->ext_vm86.vm86_intmap;
	for (i = 0; i < (ctob(IOPAGES) + 32 + 16) / sizeof(u_long); i++)
		*addr++ = ~0;

	ssd.ssd_base = (unsigned)&ext->ext_tss;
	ssd.ssd_limit -= ((unsigned)&ext->ext_tss - (unsigned)ext);
	ssdtosd(&ssd, &ext->ext_tssd);
	
	/* switch to the new TSS after syscall completes */
	need_resched();

	return 0;
}

static int
i386_set_ioperm(p, args)
	struct proc *p;
	char *args;
{
	int i, error;
	struct i386_ioperm_args ua;
	char *iomap;

	if ((error = copyin(args, &ua, sizeof(struct i386_ioperm_args))) != 0)
		return (error);

	if ((error = suser(p)) != 0)
		return (error);
	if (securelevel > 0)
		return (EPERM);
	/*
	 * XXX 
	 * While this is restricted to root, we should probably figure out
	 * whether any other driver is using this i/o address, as so not to
	 * cause confusion.  This probably requires a global 'usage registry'.
	 */

	if (p->p_addr->u_pcb.pcb_ext == 0)
		if ((error = i386_extend_pcb(p)) != 0)
			return (error);
	iomap = (char *)p->p_addr->u_pcb.pcb_ext->ext_iomap;

	if (ua.start + ua.length > IOPAGES * PAGE_SIZE * NBBY)
		return (EINVAL);

	for (i = ua.start; i < ua.start + ua.length; i++) {
		if (ua.enable) 
			iomap[i >> 3] &= ~(1 << (i & 7));
		else
			iomap[i >> 3] |= (1 << (i & 7));
	}
	return (error);
}

static int
i386_get_ioperm(p, args)
	struct proc *p;
	char *args;
{
	int i, state, error;
	struct i386_ioperm_args ua;
	char *iomap;

	if ((error = copyin(args, &ua, sizeof(struct i386_ioperm_args))) != 0)
		return (error);
	if (ua.start >= IOPAGES * PAGE_SIZE * NBBY)
		return (EINVAL);

	if (p->p_addr->u_pcb.pcb_ext == 0) {
		ua.length = 0;
		goto done;
	}

	iomap = (char *)p->p_addr->u_pcb.pcb_ext->ext_iomap;

	i = ua.start;
	state = (iomap[i >> 3] >> (i & 7)) & 1;
	ua.enable = !state;
	ua.length = 1;

	for (i = ua.start + 1; i < IOPAGES * PAGE_SIZE * NBBY; i++) {
		if (state != ((iomap[i >> 3] >> (i & 7)) & 1))
			break;
		ua.length++;
	}
			
done:
	error = copyout(&ua, args, sizeof(struct i386_ioperm_args));
	return (error);
}

#ifdef USER_LDT
/*
 * Update the GDT entry pointing to the LDT to point to the LDT of the
 * current process.  Do not staticize.
 */   
void
set_user_ldt(struct pcb *pcb)
{
	struct pcb_ldt *pcb_ldt;

	if (pcb != curpcb)
		return;

	pcb_ldt = pcb->pcb_ldt;
#ifdef SMP
	gdt[cpuid * NGDT + GUSERLDT_SEL].sd = pcb_ldt->ldt_sd;
#else
	gdt[GUSERLDT_SEL].sd = pcb_ldt->ldt_sd;
#endif
	lldt(GSEL(GUSERLDT_SEL, SEL_KPL));
	currentldt = GSEL(GUSERLDT_SEL, SEL_KPL);
}

struct pcb_ldt *
user_ldt_alloc(struct pcb *pcb, int len)
{
	struct pcb_ldt *pcb_ldt, *new_ldt;

	MALLOC(new_ldt, struct pcb_ldt *, sizeof(struct pcb_ldt),
		M_SUBPROC, M_WAITOK);
	if (new_ldt == NULL)
		return NULL;

	new_ldt->ldt_len = len = NEW_MAX_LD(len);
	new_ldt->ldt_base = (caddr_t)kmem_alloc(kernel_map,
		len * sizeof(union descriptor));
	if (new_ldt->ldt_base == NULL) {
		FREE(new_ldt, M_SUBPROC);
		return NULL;
	}
	new_ldt->ldt_refcnt = 1;
	new_ldt->ldt_active = 0;

	gdt_segs[GUSERLDT_SEL].ssd_base = (unsigned)new_ldt->ldt_base;
	gdt_segs[GUSERLDT_SEL].ssd_limit = len * sizeof(union descriptor) - 1;
	ssdtosd(&gdt_segs[GUSERLDT_SEL], &new_ldt->ldt_sd);

	if ((pcb_ldt = pcb->pcb_ldt)) {
		if (len > pcb_ldt->ldt_len)
			len = pcb_ldt->ldt_len;
		bcopy(pcb_ldt->ldt_base, new_ldt->ldt_base,
			len * sizeof(union descriptor));
	} else {
		bcopy(ldt, new_ldt->ldt_base, sizeof(ldt));
	}
	return new_ldt;
}

void
user_ldt_free(struct pcb *pcb)
{
	struct pcb_ldt *pcb_ldt = pcb->pcb_ldt;

	if (pcb_ldt == NULL)
		return;

	if (pcb == curpcb) {
		lldt(_default_ldt);
		currentldt = _default_ldt;
	}

	if (--pcb_ldt->ldt_refcnt == 0) {
		kmem_free(kernel_map, (vm_offset_t)pcb_ldt->ldt_base,
			pcb_ldt->ldt_len * sizeof(union descriptor));
		FREE(pcb_ldt, M_SUBPROC);
	}
	pcb->pcb_ldt = NULL;
}

static int
i386_get_ldt(p, args)
	struct proc *p;
	char *args;
{
	int error = 0;
	struct pcb *pcb = &p->p_addr->u_pcb;
	struct pcb_ldt *pcb_ldt = pcb->pcb_ldt;
	int nldt, num;
	union descriptor *lp;
	int s;
	struct i386_ldt_args ua, *uap = &ua;

	if ((error = copyin(args, uap, sizeof(struct i386_ldt_args))) < 0)
		return(error);

#ifdef	DEBUG
	printf("i386_get_ldt: start=%d num=%d descs=%p\n",
	    uap->start, uap->num, (void *)uap->descs);
#endif

	/* verify range of LDTs exist */
	if ((uap->start < 0) || (uap->num <= 0))
		return(EINVAL);

	s = splhigh();

	if (pcb_ldt) {
		nldt = pcb_ldt->ldt_len;
		num = min(uap->num, nldt);
		lp = &((union descriptor *)(pcb_ldt->ldt_base))[uap->start];
	} else {
		nldt = sizeof(ldt)/sizeof(ldt[0]);
		num = min(uap->num, nldt);
		lp = &ldt[uap->start];
	}
	if (uap->start + num > nldt) {
		splx(s);
		return(EINVAL);
	}

	error = copyout(lp, uap->descs, num * sizeof(union descriptor));
	if (!error)
		p->p_retval[0] = num;

	splx(s);
	return(error);
}

static int
i386_set_ldt(p, args)
	struct proc *p;
	char *args;
{
	int error = 0, i, n;
	int largest_ld;
	struct pcb *pcb = &p->p_addr->u_pcb;
	struct pcb_ldt *pcb_ldt = pcb->pcb_ldt;
	union descriptor *descs;
	int descs_size, s;
	struct i386_ldt_args ua, *uap = &ua;

	if ((error = copyin(args, uap, sizeof(struct i386_ldt_args))) < 0)
		return(error);

#ifdef	DEBUG
	printf("i386_set_ldt: start=%d num=%d descs=%p\n",
	    uap->start, uap->num, (void *)uap->descs);
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
	if (!pcb_ldt || largest_ld >= pcb_ldt->ldt_len) {
		struct pcb_ldt *new_ldt = user_ldt_alloc(pcb, largest_ld);
		if (new_ldt == NULL)
			return ENOMEM;
		if (pcb_ldt) {
			pcb_ldt->ldt_sd = new_ldt->ldt_sd;
			kmem_free(kernel_map, (vm_offset_t)pcb_ldt->ldt_base,
				pcb_ldt->ldt_len * sizeof(union descriptor));
			pcb_ldt->ldt_base = new_ldt->ldt_base;
			pcb_ldt->ldt_len = new_ldt->ldt_len;
			FREE(new_ldt, M_SUBPROC);
		} else
			pcb->pcb_ldt = pcb_ldt = new_ldt;
#ifdef SMP
		/* signal other cpus to reload ldt */
		smp_rendezvous(NULL, (void (*)(void *))set_user_ldt, NULL, pcb);
#else
		set_user_ldt(pcb);
#endif
	}

	descs_size = uap->num * sizeof(union descriptor);
	descs = (union descriptor *)kmem_alloc(kernel_map, descs_size);
	if (descs == NULL)
		return (ENOMEM);
	error = copyin(&uap->descs[0], descs, descs_size);
	if (error) {
		kmem_free(kernel_map, (vm_offset_t)descs, descs_size);
		return (error);
	}
	/* Check descriptors for access violations */
	for (i = 0, n = uap->start; i < uap->num; i++, n++) {
		union descriptor *dp;
		dp = &descs[i];

		switch (dp->sd.sd_type) {
		case SDT_SYSNULL:	/* system null */ 
			dp->sd.sd_p = 0;
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
			kmem_free(kernel_map, (vm_offset_t)descs, descs_size);
			return EACCES;

		/* memory segment types */
		case SDT_MEMEC:   /* memory execute only conforming */
		case SDT_MEMEAC:  /* memory execute only accessed conforming */
		case SDT_MEMERC:  /* memory execute read conforming */
		case SDT_MEMERAC: /* memory execute read accessed conforming */
			/* Must be "present" if executable and conforming. */
			if (dp->sd.sd_p == 0) {
				kmem_free(kernel_map, (vm_offset_t)descs,
				    descs_size);
				return (EACCES);
			}
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
			kmem_free(kernel_map, (vm_offset_t)descs, descs_size);
			return(EINVAL);
			/*NOTREACHED*/
		}

		/* Only user (ring-3) descriptors may be present. */
		if ((dp->sd.sd_p != 0) && (dp->sd.sd_dpl != SEL_UPL)) {
			kmem_free(kernel_map, (vm_offset_t)descs, descs_size);
			return (EACCES);
		}
	}

	s = splhigh();

	/* Fill in range */
	bcopy(descs, 
		 &((union descriptor *)(pcb_ldt->ldt_base))[uap->start],
		uap->num * sizeof(union descriptor));
	p->p_retval[0] = uap->start;

	splx(s);
	kmem_free(kernel_map, (vm_offset_t)descs, descs_size);
	return (0);
}
#endif	/* USER_LDT */
