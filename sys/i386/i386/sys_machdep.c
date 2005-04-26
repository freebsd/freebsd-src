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

#include "opt_kstack_pages.h"
#include "opt_mac.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/lock.h>
#include <sys/mac.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/smp.h>
#include <sys/sysproto.h>

#include <vm/vm.h>
#include <vm/pmap.h>
#include <vm/vm_map.h>
#include <vm/vm_extern.h>

#include <machine/cpu.h>
#include <machine/pcb.h>
#include <machine/pcb_ext.h>
#include <machine/proc.h>
#include <machine/sysarch.h>

#include <vm/vm_kern.h>		/* for kernel_map */

#define MAX_LD 8192
#define LD_PER_PAGE 512
#define NEW_MAX_LD(num)  ((num + LD_PER_PAGE) & ~(LD_PER_PAGE-1))
#define SIZE_FROM_LARGEST_LD(num) (NEW_MAX_LD(num) << 3)



static int i386_get_ldt(struct thread *, char *);
static int i386_set_ldt(struct thread *, char *);
static int i386_set_ldt_data(struct thread *, int start, int num,
	union descriptor *descs);
static int i386_ldt_grow(struct thread *td, int len);
static int i386_get_ioperm(struct thread *, char *);
static int i386_set_ioperm(struct thread *, char *);
#ifdef SMP
static void set_user_ldt_rv(struct thread *);
#endif

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
	int error;
	uint32_t base;
	struct segment_descriptor sd, *sdp;


	mtx_lock(&Giant);
	switch(uap->op) {
	case I386_GET_LDT:
		error = i386_get_ldt(td, uap->parms);
		break;

	case I386_SET_LDT:
		error = i386_set_ldt(td, uap->parms);
		break;
	case I386_GET_IOPERM:
		error = i386_get_ioperm(td, uap->parms);
		break;
	case I386_SET_IOPERM:
		error = i386_set_ioperm(td, uap->parms);
		break;
	case I386_VM86:
		error = vm86_sysarch(td, uap->parms);
		break;
	case I386_GET_FSBASE:
		sdp = (struct segment_descriptor *)&td->td_pcb->pcb_fsd;
		base = sdp->sd_hibase << 24 | sdp->sd_lobase;
		error = copyout(&base, uap->parms, sizeof(base));
		break;
	case I386_SET_FSBASE:
		error = copyin(uap->parms, &base, sizeof(base));
		if (!error) {
			/*
			 * Construct a descriptor and store it in the pcb for
			 * the next context switch.  Also store it in the gdt
			 * so that the load of tf_fs into %fs will activate it
			 * at return to userland.
			 */
			sd.sd_lobase = base & 0xffffff;
			sd.sd_hibase = (base >> 24) & 0xff;
			sd.sd_lolimit = 0xffff;	/* 4GB limit, wraps around */
			sd.sd_hilimit = 0xf;
			sd.sd_type  = SDT_MEMRWA;
			sd.sd_dpl   = SEL_UPL;
			sd.sd_p     = 1;
			sd.sd_xx    = 0;
			sd.sd_def32 = 1;
			sd.sd_gran  = 1;
			critical_enter();
			*(struct segment_descriptor *)&td->td_pcb->pcb_fsd = sd;
			PCPU_GET(fsgs_gdt)[0] = sd;
			critical_exit();
			td->td_frame->tf_fs = GSEL(GUFS_SEL, SEL_UPL);
		}
		break;
	case I386_GET_GSBASE:
		sdp = (struct segment_descriptor *)&td->td_pcb->pcb_gsd;
		base = sdp->sd_hibase << 24 | sdp->sd_lobase;
		error = copyout(&base, uap->parms, sizeof(base));
		break;
	case I386_SET_GSBASE:
		error = copyin(uap->parms, &base, sizeof(base));
		if (!error) {
			/*
			 * Construct a descriptor and store it in the pcb for
			 * the next context switch.  Also store it in the gdt
			 * because we have to do a load_gs() right now.
			 */
			sd.sd_lobase = base & 0xffffff;
			sd.sd_hibase = (base >> 24) & 0xff;
			sd.sd_lolimit = 0xffff;	/* 4GB limit, wraps around */
			sd.sd_hilimit = 0xf;
			sd.sd_type  = SDT_MEMRWA;
			sd.sd_dpl   = SEL_UPL;
			sd.sd_p     = 1;
			sd.sd_xx    = 0;
			sd.sd_def32 = 1;
			sd.sd_gran  = 1;
			critical_enter();
			*(struct segment_descriptor *)&td->td_pcb->pcb_gsd = sd;
			PCPU_GET(fsgs_gdt)[1] = sd;
			critical_exit();
			load_gs(GSEL(GUGS_SEL, SEL_UPL));
		}
		break;
	default:
		error = EINVAL;
		break;
	}
	mtx_unlock(&Giant);
	return (error);
}

int
i386_extend_pcb(struct thread *td)
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

	if (td->td_proc->p_flag & P_SA)
		return (EINVAL);		/* XXXKSE */
/* XXXKSE  All the code below only works in 1:1   needs changing */
	ext = (struct pcb_ext *)kmem_alloc(kernel_map, ctob(IOPAGES+1));
	if (ext == 0)
		return (ENOMEM);
	bzero(ext, sizeof(struct pcb_ext)); 
	/* -16 is so we can convert a trapframe into vm86trapframe inplace */
	ext->ext_tss.tss_esp0 = td->td_kstack + ctob(KSTACK_PAGES) -
	    sizeof(struct pcb) - 16;
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

	KASSERT(td->td_proc == curthread->td_proc, ("giving TSS to !curproc"));
	KASSERT(td->td_pcb->pcb_ext == 0, ("already have a TSS!"));
	mtx_lock_spin(&sched_lock);
	td->td_pcb->pcb_ext = ext;
	
	/* switch to the new TSS after syscall completes */
	td->td_flags |= TDF_NEEDRESCHED;
	mtx_unlock_spin(&sched_lock);

	return 0;
}

static int
i386_set_ioperm(td, args)
	struct thread *td;
	char *args;
{
	int i, error;
	struct i386_ioperm_args ua;
	char *iomap;

	if ((error = copyin(args, &ua, sizeof(struct i386_ioperm_args))) != 0)
		return (error);

#ifdef MAC
	if ((error = mac_check_sysarch_ioperm(td->td_ucred)) != 0)
		return (error);
#endif
	if ((error = suser(td)) != 0)
		return (error);
	if ((error = securelevel_gt(td->td_ucred, 0)) != 0)
		return (error);
	/*
	 * XXX 
	 * While this is restricted to root, we should probably figure out
	 * whether any other driver is using this i/o address, as so not to
	 * cause confusion.  This probably requires a global 'usage registry'.
	 */

	if (td->td_pcb->pcb_ext == 0)
		if ((error = i386_extend_pcb(td)) != 0)
			return (error);
	iomap = (char *)td->td_pcb->pcb_ext->ext_iomap;

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
i386_get_ioperm(td, args)
	struct thread *td;
	char *args;
{
	int i, state, error;
	struct i386_ioperm_args ua;
	char *iomap;

	if ((error = copyin(args, &ua, sizeof(struct i386_ioperm_args))) != 0)
		return (error);
	if (ua.start >= IOPAGES * PAGE_SIZE * NBBY)
		return (EINVAL);

	if (td->td_pcb->pcb_ext == 0) {
		ua.length = 0;
		goto done;
	}

	iomap = (char *)td->td_pcb->pcb_ext->ext_iomap;

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

/*
 * Update the GDT entry pointing to the LDT to point to the LDT of the
 * current process.
 *
 * This must be called with sched_lock held.  Unfortunately, we can't use a
 * mtx_assert() here because cpu_switch() calls this function after changing
 * curproc but before sched_lock's owner is updated in mi_switch().
 */   
void
set_user_ldt(struct mdproc *mdp)
{
	struct proc_ldt *pldt;

	pldt = mdp->md_ldt;
#ifdef SMP
	gdt[PCPU_GET(cpuid) * NGDT + GUSERLDT_SEL].sd = pldt->ldt_sd;
#else
	gdt[GUSERLDT_SEL].sd = pldt->ldt_sd;
#endif
	lldt(GSEL(GUSERLDT_SEL, SEL_KPL));
	PCPU_SET(currentldt, GSEL(GUSERLDT_SEL, SEL_KPL));
}

#ifdef SMP
static void
set_user_ldt_rv(struct thread *td)
{

	if (td->td_proc != curthread->td_proc)
		return;

	set_user_ldt(&td->td_proc->p_md);
}
#endif

/*
 * Must be called with either sched_lock free or held but not recursed.
 * If it does not return NULL, it will return with it owned.
 */
struct proc_ldt *
user_ldt_alloc(struct mdproc *mdp, int len)
{
	struct proc_ldt *pldt, *new_ldt;

	if (mtx_owned(&sched_lock))
		mtx_unlock_spin(&sched_lock);
	mtx_assert(&sched_lock, MA_NOTOWNED);
	MALLOC(new_ldt, struct proc_ldt *, sizeof(struct proc_ldt),
		M_SUBPROC, M_WAITOK);

	new_ldt->ldt_len = len = NEW_MAX_LD(len);
	new_ldt->ldt_base = (caddr_t)kmem_alloc(kernel_map,
		len * sizeof(union descriptor));
	if (new_ldt->ldt_base == NULL) {
		FREE(new_ldt, M_SUBPROC);
		return NULL;
	}
	new_ldt->ldt_refcnt = 1;
	new_ldt->ldt_active = 0;

	mtx_lock_spin(&sched_lock);
	gdt_segs[GUSERLDT_SEL].ssd_base = (unsigned)new_ldt->ldt_base;
	gdt_segs[GUSERLDT_SEL].ssd_limit = len * sizeof(union descriptor) - 1;
	ssdtosd(&gdt_segs[GUSERLDT_SEL], &new_ldt->ldt_sd);

	if ((pldt = mdp->md_ldt)) {
		if (len > pldt->ldt_len)
			len = pldt->ldt_len;
		bcopy(pldt->ldt_base, new_ldt->ldt_base,
		    len * sizeof(union descriptor));
	} else {
		bcopy(ldt, new_ldt->ldt_base, sizeof(ldt));
	}
	return new_ldt;
}

/*
 * Must be called either with sched_lock free or held but not recursed.
 * If md_ldt is not NULL, it will return with sched_lock released.
 */
void
user_ldt_free(struct thread *td)
{
	struct mdproc *mdp = &td->td_proc->p_md;
	struct proc_ldt *pldt = mdp->md_ldt;

	if (pldt == NULL)
		return;

	if (!mtx_owned(&sched_lock))
		mtx_lock_spin(&sched_lock);
	mtx_assert(&sched_lock, MA_OWNED | MA_NOTRECURSED);
	if (td == PCPU_GET(curthread)) {
		lldt(_default_ldt);
		PCPU_SET(currentldt, _default_ldt);
	}

	mdp->md_ldt = NULL;
	if (--pldt->ldt_refcnt == 0) {
		mtx_unlock_spin(&sched_lock);
		kmem_free(kernel_map, (vm_offset_t)pldt->ldt_base,
			pldt->ldt_len * sizeof(union descriptor));
		FREE(pldt, M_SUBPROC);
	} else
		mtx_unlock_spin(&sched_lock);
}

static int
i386_get_ldt(td, args)
	struct thread *td;
	char *args;
{
	int error = 0;
	struct proc_ldt *pldt = td->td_proc->p_md.md_ldt;
	int nldt, num;
	union descriptor *lp;
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

	if (pldt) {
		nldt = pldt->ldt_len;
		num = min(uap->num, nldt);
		lp = &((union descriptor *)(pldt->ldt_base))[uap->start];
	} else {
		nldt = sizeof(ldt)/sizeof(ldt[0]);
		num = min(uap->num, nldt);
		lp = &ldt[uap->start];
	}
	if (uap->start + num > nldt)
		return(EINVAL);

	error = copyout(lp, uap->descs, num * sizeof(union descriptor));
	if (!error)
		td->td_retval[0] = num;

	return(error);
}

static int ldt_warnings;
#define NUM_LDT_WARNINGS 10

static int
i386_set_ldt(td, args)
	struct thread *td;
	char *args;
{
	int error = 0, i;
	int largest_ld;
	struct mdproc *mdp = &td->td_proc->p_md;
	struct proc_ldt *pldt = 0;
	struct i386_ldt_args ua, *uap = &ua;
	union descriptor *descs, *dp;
	int descs_size;

	if ((error = copyin(args, uap, sizeof(struct i386_ldt_args))) < 0)
		return(error);

#ifdef	DEBUG
	printf("i386_set_ldt: start=%d num=%d descs=%p\n",
	    uap->start, uap->num, (void *)uap->descs);
#endif

	if (uap->descs == NULL) {
		/* Free descriptors */
		if (uap->start == 0 && uap->num == 0) {
			/*
			 * Treat this as a special case, so userland needn't
			 * know magic number NLDT.
		 	 */
			uap->start = NLDT;
			uap->num = MAX_LD - NLDT;
		}
		if (uap->start <= LUDATA_SEL || uap->num <= 0)
			return (EINVAL);
		mtx_lock_spin(&sched_lock);
		pldt = mdp->md_ldt;
		if (pldt == NULL || uap->start >= pldt->ldt_len) {
			mtx_unlock_spin(&sched_lock);
			return (0);
		}
		largest_ld = uap->start + uap->num;
		if (largest_ld > pldt->ldt_len)
			largest_ld = pldt->ldt_len;
		i = largest_ld - uap->start;
		bzero(&((union descriptor *)(pldt->ldt_base))[uap->start],
		    sizeof(union descriptor) * i);
		mtx_unlock_spin(&sched_lock);
		return (0);
	}

	if (!(uap->start == LDT_AUTO_ALLOC && uap->num == 1)) {
		/* complain a for a while if using old methods */
		if (ldt_warnings++ < NUM_LDT_WARNINGS) {
			printf("Warning: pid %d used static ldt allocation.\n",
			    td->td_proc->p_pid);
			printf("See the i386_set_ldt man page for more info\n");
		}
		/* verify range of descriptors to modify */
		largest_ld = uap->start + uap->num;
		if (uap->start >= MAX_LD ||
		    uap->num < 0 || largest_ld > MAX_LD) {
			return (EINVAL);
		}
	}

	descs_size = uap->num * sizeof(union descriptor);
	descs = (union descriptor *)kmem_alloc(kernel_map, descs_size);
	if (descs == NULL)
		return (ENOMEM);
	error = copyin(uap->descs, descs, descs_size);
	if (error) {
		kmem_free(kernel_map, (vm_offset_t)descs, descs_size);
		return (error);
	}

	/* Check descriptors for access violations */
	for (i = 0; i < uap->num; i++) {
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
			return (EACCES);
			/*NOTREACHED*/

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

	if (uap->start == LDT_AUTO_ALLOC && uap->num == 1) {
		/* Allocate a free slot */
		pldt = mdp->md_ldt;
		if (pldt == NULL) {
			error = i386_ldt_grow(td, NLDT+1);
			if (error) {
				kmem_free(kernel_map, (vm_offset_t)descs,
				    descs_size);
				return (error);
			}
			pldt = mdp->md_ldt;
		}
again:
		mtx_lock_spin(&sched_lock);
		/*
		 * start scanning a bit up to leave room for NVidia and
		 * Wine, which still user the "Blat" method of allocation.
		 */
		dp = &((union descriptor *)(pldt->ldt_base))[NLDT];
		for (i = NLDT; i < pldt->ldt_len; ++i) {
			if (dp->sd.sd_type == SDT_SYSNULL)
				break;
			dp++;
		}
		if (i >= pldt->ldt_len) {
			mtx_unlock_spin(&sched_lock);
			error = i386_ldt_grow(td, pldt->ldt_len+1);
			if (error) {
				kmem_free(kernel_map, (vm_offset_t)descs,
				    descs_size);
				return (error);
			}
			goto again;
		}
		uap->start = i;
		error = i386_set_ldt_data(td, i, 1, descs);
		mtx_unlock_spin(&sched_lock);
	} else {
		largest_ld = uap->start + uap->num;
		error = i386_ldt_grow(td, largest_ld);
		if (error == 0) {
			mtx_lock_spin(&sched_lock);
			error = i386_set_ldt_data(td, uap->start, uap->num,
			    descs);
			mtx_unlock_spin(&sched_lock);
		}
	}
	kmem_free(kernel_map, (vm_offset_t)descs, descs_size);
	if (error == 0)
		td->td_retval[0] = uap->start;
	return (error);
}

static int
i386_set_ldt_data(struct thread *td, int start, int num,
	union descriptor *descs)
{
	struct mdproc *mdp = &td->td_proc->p_md;
	struct proc_ldt *pldt = mdp->md_ldt;

	mtx_assert(&sched_lock, MA_OWNED);

	/* Fill in range */
	bcopy(descs,
	    &((union descriptor *)(pldt->ldt_base))[start],
	    num * sizeof(union descriptor));
	return (0);
}

static int
i386_ldt_grow(struct thread *td, int len) 
{
	struct mdproc *mdp = &td->td_proc->p_md;
	struct proc_ldt *pldt;
	caddr_t old_ldt_base;
	int old_ldt_len;

	if (len > MAX_LD)
		return (ENOMEM);
	if (len < NLDT+1)
		len = NLDT+1;
	pldt = mdp->md_ldt;
	/* allocate user ldt */
	if (!pldt || len > pldt->ldt_len) {
		struct proc_ldt *new_ldt = user_ldt_alloc(mdp, len);
		if (new_ldt == NULL)
			return (ENOMEM);
		pldt = mdp->md_ldt;
		/* sched_lock was held by user_ldt_alloc */
		if (pldt) {
			if (new_ldt->ldt_len > pldt->ldt_len) {
				old_ldt_base = pldt->ldt_base;
				old_ldt_len = pldt->ldt_len;
				pldt->ldt_sd = new_ldt->ldt_sd;
				pldt->ldt_base = new_ldt->ldt_base;
				pldt->ldt_len = new_ldt->ldt_len;
				mtx_unlock_spin(&sched_lock);
				kmem_free(kernel_map, (vm_offset_t)old_ldt_base,
					old_ldt_len * sizeof(union descriptor));
				FREE(new_ldt, M_SUBPROC);
				mtx_lock_spin(&sched_lock);
			} else {
				/*
				 * If other threads already did the work,
				 * do nothing
				 */
				mtx_unlock_spin(&sched_lock);
				kmem_free(kernel_map,
				   (vm_offset_t)new_ldt->ldt_base,
				   new_ldt->ldt_len * sizeof(union descriptor));
				FREE(new_ldt, M_SUBPROC);
				return (0);
			}
		} else {
			mdp->md_ldt = pldt = new_ldt;
		}
#ifdef SMP
		mtx_unlock_spin(&sched_lock);
		/* signal other cpus to reload ldt */
		smp_rendezvous(NULL, (void (*)(void *))set_user_ldt_rv,
		    NULL, td);
#else
		set_user_ldt(mdp);
		mtx_unlock_spin(&sched_lock);
#endif
	}
	return (0);
}
