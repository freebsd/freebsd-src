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
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/priv.h>
#include <sys/proc.h>
#include <sys/sysproto.h>
#include <sys/uio.h>

#include <vm/vm.h>
#include <vm/pmap.h>
#include <vm/vm_kern.h>		/* for kernel_map */
#include <vm/vm_extern.h>

#include <machine/frame.h>
#include <machine/md_var.h>
#include <machine/pcb.h>
#include <machine/specialreg.h>
#include <machine/sysarch.h>
#include <machine/tss.h>
#include <machine/vmparam.h>

#include <security/audit/audit.h>

#define	MAX_LD		8192

int max_ldt_segment = 1024;
SYSCTL_INT(_machdep, OID_AUTO, max_ldt_segment, CTLFLAG_RD, &max_ldt_segment,
    0,
    "Maximum number of allowed LDT segments in the single address space");

static void
max_ldt_segment_init(void *arg __unused)
{

	TUNABLE_INT_FETCH("machdep.max_ldt_segment", &max_ldt_segment);
	if (max_ldt_segment <= 0)
		max_ldt_segment = 1;
	if (max_ldt_segment > MAX_LD)
		max_ldt_segment = MAX_LD;
}
SYSINIT(maxldt, SI_SUB_VM_CONF, SI_ORDER_ANY, max_ldt_segment_init, NULL);

#ifdef notyet
#ifdef SMP
static void set_user_ldt_rv(struct vmspace *vmsp);
#endif
#endif
static void user_ldt_derefl(struct proc_ldt *pldt);

#ifndef _SYS_SYSPROTO_H_
struct sysarch_args {
	int op;
	char *parms;
};
#endif

int
sysarch_ldt(struct thread *td, struct sysarch_args *uap, int uap_space)
{
	struct i386_ldt_args *largs, la;
	struct user_segment_descriptor *lp;
	int error = 0;

	/*
	 * XXXKIB check that the BSM generation code knows to encode
	 * the op argument.
	 */
	AUDIT_ARG_CMD(uap->op);
	if (uap_space == UIO_USERSPACE) {
		error = copyin(uap->parms, &la, sizeof(struct i386_ldt_args));
		if (error != 0)
			return (error);
		largs = &la;
	} else
		largs = (struct i386_ldt_args *)uap->parms;

	switch (uap->op) {
	case I386_GET_LDT:
		error = amd64_get_ldt(td, largs);
		break;
	case I386_SET_LDT:
		if (largs->descs != NULL && largs->num > max_ldt_segment)
			return (EINVAL);
		set_pcb_flags(td->td_pcb, PCB_FULL_IRET);
		if (largs->descs != NULL) {
			lp = malloc(largs->num * sizeof(struct
			    user_segment_descriptor), M_TEMP, M_WAITOK);
			error = copyin(largs->descs, lp, largs->num *
			    sizeof(struct user_segment_descriptor));
			if (error == 0)
				error = amd64_set_ldt(td, largs, lp);
			free(lp, M_TEMP);
		} else {
			error = amd64_set_ldt(td, largs, NULL);
		}
		break;
	}
	return (error);
}

void
update_gdt_gsbase(struct thread *td, uint32_t base)
{
	struct user_segment_descriptor *sd;

	if (td != curthread)
		return;
	set_pcb_flags(td->td_pcb, PCB_FULL_IRET);
	critical_enter();
	sd = PCPU_GET(gs32p);
	sd->sd_lobase = base & 0xffffff;
	sd->sd_hibase = (base >> 24) & 0xff;
	critical_exit();
}

void
update_gdt_fsbase(struct thread *td, uint32_t base)
{
	struct user_segment_descriptor *sd;

	if (td != curthread)
		return;
	set_pcb_flags(td->td_pcb, PCB_FULL_IRET);
	critical_enter();
	sd = PCPU_GET(fs32p);
	sd->sd_lobase = base & 0xffffff;
	sd->sd_hibase = (base >> 24) & 0xff;
	critical_exit();
}

int
sysarch(td, uap)
	struct thread *td;
	register struct sysarch_args *uap;
{
	int error = 0;
	struct pcb *pcb = curthread->td_pcb;
	uint32_t i386base;
	uint64_t a64base;
	struct i386_ioperm_args iargs;

	if (uap->op == I386_GET_LDT || uap->op == I386_SET_LDT)
		return (sysarch_ldt(td, uap, UIO_USERSPACE));
	/*
	 * XXXKIB check that the BSM generation code knows to encode
	 * the op argument.
	 */
	AUDIT_ARG_CMD(uap->op);
	switch (uap->op) {
	case I386_GET_IOPERM:
	case I386_SET_IOPERM:
		if ((error = copyin(uap->parms, &iargs,
		    sizeof(struct i386_ioperm_args))) != 0)
			return (error);
		break;
	default:
		break;
	}

	switch (uap->op) {
	case I386_GET_IOPERM:
		error = amd64_get_ioperm(td, &iargs);
		if (error == 0)
			error = copyout(&iargs, uap->parms,
			    sizeof(struct i386_ioperm_args));
		break;
	case I386_SET_IOPERM:
		error = amd64_set_ioperm(td, &iargs);
		break;
	case I386_GET_FSBASE:
		i386base = pcb->pcb_fsbase;
		error = copyout(&i386base, uap->parms, sizeof(i386base));
		break;
	case I386_SET_FSBASE:
		error = copyin(uap->parms, &i386base, sizeof(i386base));
		if (!error) {
			pcb->pcb_fsbase = i386base;
			td->td_frame->tf_fs = _ufssel;
			set_pcb_flags(pcb, PCB_FULL_IRET);
			update_gdt_fsbase(td, i386base);
		}
		break;
	case I386_GET_GSBASE:
		i386base = pcb->pcb_gsbase;
		error = copyout(&i386base, uap->parms, sizeof(i386base));
		break;
	case I386_SET_GSBASE:
		error = copyin(uap->parms, &i386base, sizeof(i386base));
		if (!error) {
			pcb->pcb_gsbase = i386base;
			set_pcb_flags(pcb, PCB_FULL_IRET);
			td->td_frame->tf_gs = _ugssel;
			update_gdt_gsbase(td, i386base);
		}
		break;
	case AMD64_GET_FSBASE:
		error = copyout(&pcb->pcb_fsbase, uap->parms, sizeof(pcb->pcb_fsbase));
		break;
		
	case AMD64_SET_FSBASE:
		error = copyin(uap->parms, &a64base, sizeof(a64base));
		if (!error) {
			if (a64base < VM_MAXUSER_ADDRESS) {
				pcb->pcb_fsbase = a64base;
				set_pcb_flags(pcb, PCB_FULL_IRET);
				td->td_frame->tf_fs = _ufssel;
			} else
				error = EINVAL;
		}
		break;

	case AMD64_GET_GSBASE:
		error = copyout(&pcb->pcb_gsbase, uap->parms, sizeof(pcb->pcb_gsbase));
		break;

	case AMD64_SET_GSBASE:
		error = copyin(uap->parms, &a64base, sizeof(a64base));
		if (!error) {
			if (a64base < VM_MAXUSER_ADDRESS) {
				pcb->pcb_gsbase = a64base;
				set_pcb_flags(pcb, PCB_FULL_IRET);
				td->td_frame->tf_gs = _ugssel;
			} else
				error = EINVAL;
		}
		break;

	default:
		error = EINVAL;
		break;
	}
	return (error);
}

int
amd64_set_ioperm(td, uap)
	struct thread *td;
	struct i386_ioperm_args *uap;
{
	int i, error;
	char *iomap;
	struct amd64tss *tssp;
	struct system_segment_descriptor *tss_sd;
	u_long *addr;
	struct pcb *pcb;

	if ((error = priv_check(td, PRIV_IO)) != 0)
		return (error);
	if ((error = securelevel_gt(td->td_ucred, 0)) != 0)
		return (error);
	if (uap->start + uap->length > IOPAGES * PAGE_SIZE * NBBY)
		return (EINVAL);

	/*
	 * XXX
	 * While this is restricted to root, we should probably figure out
	 * whether any other driver is using this i/o address, as so not to
	 * cause confusion.  This probably requires a global 'usage registry'.
	 */
	pcb = td->td_pcb;
	if (pcb->pcb_tssp == NULL) {
		tssp = (struct amd64tss *)kmem_alloc(kernel_map,
		    ctob(IOPAGES+1));
		if (tssp == NULL)
			return (ENOMEM);
		iomap = (char *)&tssp[1];
		addr = (u_long *)iomap;
		for (i = 0; i < (ctob(IOPAGES) + 1) / sizeof(u_long); i++)
			*addr++ = ~0;
		critical_enter();
		/* Takes care of tss_rsp0. */
		memcpy(tssp, &common_tss[PCPU_GET(cpuid)],
		    sizeof(struct amd64tss));
		tssp->tss_iobase = sizeof(*tssp);
		pcb->pcb_tssp = tssp;
		tss_sd = PCPU_GET(tss);
		tss_sd->sd_lobase = (u_long)tssp & 0xffffff;
		tss_sd->sd_hibase = ((u_long)tssp >> 24) & 0xfffffffffful;
		tss_sd->sd_type = SDT_SYSTSS;
		ltr(GSEL(GPROC0_SEL, SEL_KPL));
		PCPU_SET(tssp, tssp);
		critical_exit();
	} else
		iomap = (char *)&pcb->pcb_tssp[1];
	for (i = uap->start; i < uap->start + uap->length; i++) {
		if (uap->enable)
			iomap[i >> 3] &= ~(1 << (i & 7));
		else
			iomap[i >> 3] |= (1 << (i & 7));
	}
	return (error);
}

int
amd64_get_ioperm(td, uap)
	struct thread *td;
	struct i386_ioperm_args *uap;
{
	int i, state;
	char *iomap;

	if (uap->start >= IOPAGES * PAGE_SIZE * NBBY)
		return (EINVAL);
	if (td->td_pcb->pcb_tssp == NULL) {
		uap->length = 0;
		goto done;
	}

	iomap = (char *)&td->td_pcb->pcb_tssp[1];

	i = uap->start;
	state = (iomap[i >> 3] >> (i & 7)) & 1;
	uap->enable = !state;
	uap->length = 1;

	for (i = uap->start + 1; i < IOPAGES * PAGE_SIZE * NBBY; i++) {
		if (state != ((iomap[i >> 3] >> (i & 7)) & 1))
			break;
		uap->length++;
	}

done:
	return (0);
}

/*
 * Update the GDT entry pointing to the LDT to point to the LDT of the
 * current process.
 */
void
set_user_ldt(struct mdproc *mdp)
{

	critical_enter();
	*PCPU_GET(ldt) = mdp->md_ldt_sd;
	lldt(GSEL(GUSERLDT_SEL, SEL_KPL));
	critical_exit();
}

#ifdef notyet
#ifdef SMP
static void
set_user_ldt_rv(struct vmspace *vmsp)
{
	struct thread *td;

	td = curthread;
	if (vmsp != td->td_proc->p_vmspace)
		return;

	set_user_ldt(&td->td_proc->p_md);
}
#endif
#endif

struct proc_ldt *
user_ldt_alloc(struct proc *p, int force)
{
	struct proc_ldt *pldt, *new_ldt;
	struct mdproc *mdp;
	struct soft_segment_descriptor sldt;

	mtx_assert(&dt_lock, MA_OWNED);
	mdp = &p->p_md;
	if (!force && mdp->md_ldt != NULL)
		return (mdp->md_ldt);
	mtx_unlock(&dt_lock);
	new_ldt = malloc(sizeof(struct proc_ldt), M_SUBPROC, M_WAITOK);
	new_ldt->ldt_base = (caddr_t)kmem_alloc(kernel_map,
	     max_ldt_segment * sizeof(struct user_segment_descriptor));
	if (new_ldt->ldt_base == NULL) {
		FREE(new_ldt, M_SUBPROC);
		mtx_lock(&dt_lock);
		return (NULL);
	}
	new_ldt->ldt_refcnt = 1;
	sldt.ssd_base = (uint64_t)new_ldt->ldt_base;
	sldt.ssd_limit = max_ldt_segment *
	    sizeof(struct user_segment_descriptor) - 1;
	sldt.ssd_type = SDT_SYSLDT;
	sldt.ssd_dpl = SEL_KPL;
	sldt.ssd_p = 1;
	sldt.ssd_long = 0;
	sldt.ssd_def32 = 0;
	sldt.ssd_gran = 0;
	mtx_lock(&dt_lock);
	pldt = mdp->md_ldt;
	if (pldt != NULL && !force) {
		kmem_free(kernel_map, (vm_offset_t)new_ldt->ldt_base,
		    max_ldt_segment * sizeof(struct user_segment_descriptor));
		free(new_ldt, M_SUBPROC);
		return (pldt);
	}

	if (pldt != NULL) {
		bcopy(pldt->ldt_base, new_ldt->ldt_base, max_ldt_segment *
		    sizeof(struct user_segment_descriptor));
		user_ldt_derefl(pldt);
	}
	ssdtosyssd(&sldt, &p->p_md.md_ldt_sd);
	atomic_store_rel_ptr((volatile uintptr_t *)&mdp->md_ldt,
	    (uintptr_t)new_ldt);
	if (p == curproc)
		set_user_ldt(mdp);

	return (mdp->md_ldt);
}

void
user_ldt_free(struct thread *td)
{
	struct proc *p = td->td_proc;
	struct mdproc *mdp = &p->p_md;
	struct proc_ldt *pldt;

	mtx_assert(&dt_lock, MA_OWNED);
	if ((pldt = mdp->md_ldt) == NULL) {
		mtx_unlock(&dt_lock);
		return;
	}

	mdp->md_ldt = NULL;
	bzero(&mdp->md_ldt_sd, sizeof(mdp->md_ldt_sd));
	if (td == curthread)
		lldt(GSEL(GNULL_SEL, SEL_KPL));
	user_ldt_deref(pldt);
}

static void
user_ldt_derefl(struct proc_ldt *pldt)
{

	if (--pldt->ldt_refcnt == 0) {
		kmem_free(kernel_map, (vm_offset_t)pldt->ldt_base,
		    max_ldt_segment * sizeof(struct user_segment_descriptor));
		free(pldt, M_SUBPROC);
	}
}

void
user_ldt_deref(struct proc_ldt *pldt)
{

	mtx_assert(&dt_lock, MA_OWNED);
	user_ldt_derefl(pldt);
	mtx_unlock(&dt_lock);
}

/*
 * Note for the authors of compat layers (linux, etc): copyout() in
 * the function below is not a problem since it presents data in
 * arch-specific format (i.e. i386-specific in this case), not in
 * the OS-specific one.
 */
int
amd64_get_ldt(td, uap)
	struct thread *td;
	struct i386_ldt_args *uap;
{
	int error = 0;
	struct proc_ldt *pldt;
	int num;
	struct user_segment_descriptor *lp;

#ifdef	DEBUG
	printf("amd64_get_ldt: start=%d num=%d descs=%p\n",
	    uap->start, uap->num, (void *)uap->descs);
#endif

	if ((pldt = td->td_proc->p_md.md_ldt) != NULL) {
		lp = &((struct user_segment_descriptor *)(pldt->ldt_base))
		    [uap->start];
		num = min(uap->num, max_ldt_segment);
	} else
		return (EINVAL);

	if ((uap->start > (unsigned int)max_ldt_segment) ||
	    ((unsigned int)num > (unsigned int)max_ldt_segment) ||
	    ((unsigned int)(uap->start + num) > (unsigned int)max_ldt_segment))
		return(EINVAL);

	error = copyout(lp, uap->descs, num *
	    sizeof(struct user_segment_descriptor));
	if (!error)
		td->td_retval[0] = num;

	return(error);
}

int
amd64_set_ldt(td, uap, descs)
	struct thread *td;
	struct i386_ldt_args *uap;
	struct user_segment_descriptor *descs;
{
	int error = 0, i;
	int largest_ld;
	struct mdproc *mdp = &td->td_proc->p_md;
	struct proc_ldt *pldt;
	struct user_segment_descriptor *dp;
	struct proc *p;

#ifdef	DEBUG
	printf("amd64_set_ldt: start=%d num=%d descs=%p\n",
	    uap->start, uap->num, (void *)uap->descs);
#endif

	set_pcb_flags(td->td_pcb, PCB_FULL_IRET);
	p = td->td_proc;
	if (descs == NULL) {
		/* Free descriptors */
		if (uap->start == 0 && uap->num == 0)
			uap->num = max_ldt_segment;
		if (uap->num == 0)
			return (EINVAL);
		if ((pldt = mdp->md_ldt) == NULL ||
		    uap->start >= max_ldt_segment)
			return (0);
		largest_ld = uap->start + uap->num;
		if (largest_ld > max_ldt_segment)
			largest_ld = max_ldt_segment;
		i = largest_ld - uap->start;
		mtx_lock(&dt_lock);
		bzero(&((struct user_segment_descriptor *)(pldt->ldt_base))
		    [uap->start], sizeof(struct user_segment_descriptor) * i);
		mtx_unlock(&dt_lock);
		return (0);
	}

	if (!(uap->start == LDT_AUTO_ALLOC && uap->num == 1)) {
		/* verify range of descriptors to modify */
		largest_ld = uap->start + uap->num;
		if (uap->start >= max_ldt_segment ||
		    largest_ld > max_ldt_segment)
			return (EINVAL);
	}

	/* Check descriptors for access violations */
	for (i = 0; i < uap->num; i++) {
		dp = &descs[i];

		switch (dp->sd_type) {
		case SDT_SYSNULL:	/* system null */
			dp->sd_p = 0;
			break;
		case SDT_SYS286TSS:
		case SDT_SYSLDT:
		case SDT_SYS286BSY:
		case SDT_SYS286CGT:
		case SDT_SYSTASKGT:
		case SDT_SYS286IGT:
		case SDT_SYS286TGT:
		case SDT_SYSNULL2:
		case SDT_SYSTSS:
		case SDT_SYSNULL3:
		case SDT_SYSBSY:
		case SDT_SYSCGT:
		case SDT_SYSNULL4:
		case SDT_SYSIGT:
		case SDT_SYSTGT:
			/* I can't think of any reason to allow a user proc
			 * to create a segment of these types.  They are
			 * for OS use only.
			 */
			return (EACCES);
			/*NOTREACHED*/

		/* memory segment types */
		case SDT_MEMEC:   /* memory execute only conforming */
		case SDT_MEMEAC:  /* memory execute only accessed conforming */
		case SDT_MEMERC:  /* memory execute read conforming */
		case SDT_MEMERAC: /* memory execute read accessed conforming */
			 /* Must be "present" if executable and conforming. */
			if (dp->sd_p == 0)
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
		if ((dp->sd_p != 0) && (dp->sd_dpl != SEL_UPL))
			return (EACCES);
	}

	if (uap->start == LDT_AUTO_ALLOC && uap->num == 1) {
		/* Allocate a free slot */
		mtx_lock(&dt_lock);
		pldt = user_ldt_alloc(p, 0);
		if (pldt == NULL) {
			mtx_unlock(&dt_lock);
			return (ENOMEM);
		}

		/*
		 * start scanning a bit up to leave room for NVidia and
		 * Wine, which still user the "Blat" method of allocation.
		 */
		i = 16;
		dp = &((struct user_segment_descriptor *)(pldt->ldt_base))[i];
		for (; i < max_ldt_segment; ++i, ++dp) {
			if (dp->sd_type == SDT_SYSNULL)
				break;
		}
		if (i >= max_ldt_segment) {
			mtx_unlock(&dt_lock);
			return (ENOSPC);
		}
		uap->start = i;
		error = amd64_set_ldt_data(td, i, 1, descs);
		mtx_unlock(&dt_lock);
	} else {
		largest_ld = uap->start + uap->num;
		if (largest_ld > max_ldt_segment)
			return (EINVAL);
		mtx_lock(&dt_lock);
		if (user_ldt_alloc(p, 0) != NULL) {
			error = amd64_set_ldt_data(td, uap->start, uap->num,
			    descs);
		}
		mtx_unlock(&dt_lock);
	}
	if (error == 0)
		td->td_retval[0] = uap->start;
	return (error);
}

int
amd64_set_ldt_data(struct thread *td, int start, int num,
    struct user_segment_descriptor *descs)
{
	struct mdproc *mdp = &td->td_proc->p_md;
	struct proc_ldt *pldt = mdp->md_ldt;

	mtx_assert(&dt_lock, MA_OWNED);

	/* Fill in range */
	bcopy(descs,
	    &((struct user_segment_descriptor *)(pldt->ldt_base))[start],
	    num * sizeof(struct user_segment_descriptor));
	return (0);
}
