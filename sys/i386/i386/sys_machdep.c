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
 *	$Id: sys_machdep.c,v 1.27 1997/10/10 12:42:54 peter Exp $
 *
 */

#include "opt_user_ldt.h"
#include "opt_vm86.h"

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
#include <machine/pcb_ext.h>	/* pcb.h included by sys/user.h */
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
#ifdef VM86
static int i386_get_ioperm	__P((struct proc *, char *, int *));
static int i386_set_ioperm	__P((struct proc *, char *, int *));
int i386_extend_pcb	__P((struct proc *));
#endif

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
		error = i386_get_ldt(p, uap->parms, p->p_retval);
		break;

	case I386_SET_LDT:
		error = i386_set_ldt(p, uap->parms, p->p_retval);
		break;
#endif
#ifdef VM86
	case I386_GET_IOPERM:
		error = i386_get_ioperm(p, uap->parms, p->p_retval);
		break;
	case I386_SET_IOPERM:
		error = i386_set_ioperm(p, uap->parms, p->p_retval);
		break;
	case I386_VM86:
		error = vm86_sysarch(p, uap->parms, p->p_retval);
		break;
#endif
	default:
		error = EINVAL;
		break;
	}
	return (error);
}

#ifdef VM86
int
i386_extend_pcb(struct proc *p)
{
	int i, offset;
	u_long *addr;
	struct pcb_ext *ext;
	struct segment_descriptor sd;
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
	bzero(&ext->ext_tss, sizeof(struct i386tss)); 
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
	ext->ext_vm86.vm86_inited = 0;

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

struct i386_ioperm_args {
	u_short start;
	u_short length;
	u_char enable;
};

static int
i386_set_ioperm(p, args, retval)
	struct proc *p;
	char *args;
	int *retval;
{
	int i, error = 0;
	struct i386_ioperm_args ua;
	char *iomap;

	if (error = copyin(args, &ua, sizeof(struct i386_ioperm_args)))
		return (error);

        /* Only root can do this */
        if (error = suser(p->p_ucred, &p->p_acflag))
                return (error);
	/*
	 * XXX 
	 * While this is restricted to root, we should probably figure out
	 * whether any other driver is using this i/o address, as so not to
	 * cause confusion.  This probably requires a global 'usage registry'.
	 */

	if (p->p_addr->u_pcb.pcb_ext == 0)
		if (error = i386_extend_pcb(p))
			return (error);
	iomap = (char *)p->p_addr->u_pcb.pcb_ext->ext_iomap;

	if ((int)(ua.start + ua.length) > 0xffff)
		return (EINVAL);

	for (i = ua.start; i < (int)(ua.start + ua.length) + 1; i++) {
		if (ua.enable) 
			iomap[i >> 3] &= ~(1 << (i & 7));
		else
			iomap[i >> 3] |= (1 << (i & 7));
	}
	return (error);
}

static int
i386_get_ioperm(p, args, retval)
	struct proc *p;
	char *args;
	int *retval;
{
	int i, state, error = 0;
	struct i386_ioperm_args ua;
	char *iomap;

	if (error = copyin(args, &ua, sizeof(struct i386_ioperm_args)))
		return (error);

	if (p->p_addr->u_pcb.pcb_ext == 0) {
		ua.length = 0;
		goto done;
	}

	iomap = (char *)p->p_addr->u_pcb.pcb_ext->ext_iomap;

	state = (iomap[i >> 3] >> (i & 7)) & 1;
	ua.enable = !state;
	ua.length = 1;

	for (i = ua.start + 1; i < 0x10000; i++) {
		if (state != ((iomap[i >> 3] >> (i & 7)) & 1))
			break;
		ua.length++;
	}
			
done:
	error = copyout(&ua, args, sizeof(struct i386_ioperm_args));
	return (error);
}
#endif /* VM86 */

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
