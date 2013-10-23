/*-
 * Copyright (c) 2009-2013 Klaus P. Ohrhallinger <k@7he.at>
 * All rights reserved.
 *
 * Development of this software was partly funded by:
 *    TransIP.nl <http://www.transip.nl/>
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

static const char vpsid[] =
    "$Id$";

#include <sys/cdefs.h>

#include <sys/param.h>
#include <sys/conf.h>
#include <sys/types.h>
#include <sys/kernel.h>
#include <sys/libkern.h>
#include <sys/malloc.h>
#include <sys/proc.h>
#include <sys/sysent.h>
#include <sys/sysproto.h>

#include <machine/cputypes.h>
#include <machine/cpu.h>
#include <machine/pcb.h>
#include <machine/frame.h>
#include <machine/vmparam.h>
#include <machine/vps_md.h>

#include <vm/vm.h>
#include <vm/pmap.h>
#include <vm/vm_page.h>
#include <vm/vm_object.h>
#include <vm/vm_map.h>

#include <vps/vps.h>
#include <vps/vps2.h>
#include <vps/vps_int.h>
#include <vps/vps_libdump.h>
#include <vps/vps_snapst.h>

#ifdef VPS

void
vps_md_print_thread(struct thread *td)
{

	DBGCORE("%s: thread %p/%d kernel stack:\n"
		"td->td_pcb->rsp=%016lx\n"
		"td->td_frame->tf_rax=%016lx\n"
		"td->td_frame->tf_rsp=%016lx\n"
		"td->td_frame->tf_rbp=%016lx\n"
		"td->td_frame->tf_rip=%016lx\n"
		"trace:\n",
		__func__,
		td,
		td->td_tid,
		td->td_pcb->pcb_rsp,
		td->td_frame->tf_rax,
		td->td_frame->tf_rsp,
		td->td_frame->tf_rbp,
		td->td_frame->tf_rip);
}

int
vps_md_snapshot_thread(struct vps_dump_thread *vdtd, struct thread *td)
{

	return (0);
}

int
vps_md_restore_thread(struct vps_dump_thread *vdtd, struct thread *ntd,
    struct proc *p)
{

	if (vps_func->vps_restore_return == NULL) {
		printf("%s: vps_restore module not loaded ? "
		    "vps_func->vps_restore_return == NULL",
		    __func__);
		return (EOPNOTSUPP);
	}

	ntd->td_pcb->pcb_cr3 =
	    DMAP_TO_PHYS((vm_offset_t)vmspace_pmap(p->p_vmspace)->pm_pml4);
	ntd->td_pcb->pcb_r12 = (uint64_t)vps_func->vps_restore_return;
	ntd->td_pcb->pcb_rbp = 0;
	ntd->td_pcb->pcb_rsp = (uint64_t)ntd->td_frame - sizeof(void *);
	ntd->td_pcb->pcb_rbx = (uint64_t)ntd;
	ntd->td_pcb->pcb_rip = (uint64_t)fork_trampoline;
	ntd->td_md.md_spinlock_count = 1;
	ntd->td_md.md_saved_flags = PSL_KERNEL | PSL_I;
	ntd->td_errno = vdtd->td_errno;
	ntd->td_retval[0] = vdtd->td_retval[0];
	ntd->td_retval[1] = vdtd->td_retval[1];

	/* db_trace_thread(ntd, 10); */
	DBGCORE("%s: td_pcb = %p; td_frame = %p; pcb_rsp = %016lx\n",
	    __func__, ntd->td_pcb, ntd->td_frame, ntd->td_pcb->pcb_rsp);

	return (0);
}

int
vps_md_snapshot_sysentvec(struct sysentvec *sv, long *svtype)
{
	int error = 0;

	if (sv == &elf64_freebsd_sysvec) {
		DBGCORE("%s: elf64_freebsd_sysvec\n", __func__);
		*svtype = VPS_SYSENTVEC_ELF64;
#ifdef COMPAT_FREEBSD32
	} else if (sv == &ia32_freebsd_sysvec) {
		DBGCORE("%s: ia32_freebsd_sysvec\n", __func__);
		*svtype = VPS_SYSENTVEC_ELF32;
#endif
	} else if (sv == &null_sysvec) {
		DBGCORE("%s: null_sysvec\n", __func__);
		*svtype = VPS_SYSENTVEC_NULL;
        } else {
		DBGCORE("%s: unknown sysentvec %p\n", __func__, sv);
		error = EINVAL;
        }

	return (error);
}

int
vps_md_restore_sysentvec(long svtype, struct sysentvec **sv)
{
	int error = 0;

	if (svtype == VPS_SYSENTVEC_ELF64)
		*sv = &elf64_freebsd_sysvec;
#ifdef COMPAT_FREEBSD32
	else if (svtype == VPS_SYSENTVEC_ELF32)
		*sv = &ia32_freebsd_sysvec;
#endif
	else if (svtype == VPS_SYSENTVEC_NULL)
		*sv = &null_sysvec;
	else {
		DBGCORE("%s: unknown sysentvec type: %ld\n",
			__func__, svtype);
		error = EINVAL;
	}

	return (error);
}

int
vps_md_restore_checkarch(uint8 ptrsize, uint8 byteorder)
{
	int error;

	if (ptrsize == VPS_DUMPH_64BIT && byteorder == VPS_DUMPH_LSB)
		error = 0;
	else
		error = EINVAL;

	return (error);
}

int
vps_md_snapshot_thread_savefpu(struct vps_snapst_ctx *ctx, struct vps *vps,
    struct thread *td)
{
        struct vps_dumpobj *o1;
        struct vps_dump_savefpu *vdsf;

	KASSERT(td->td_pcb != NULL && td->td_pcb->pcb_save != NULL,
		("%s: td->td_pcb == NULL || td->td_pcb->pcb_save == NULL\n",
		__func__));

	if ((o1 = vdo_create(ctx, VPS_DUMPOBJT_SAVEFPU, M_NOWAIT)) ==
	    NULL) {
		vdo_discard(ctx, o1);
		return (ENOMEM);
	}

	if ((vdsf = vdo_space(ctx, sizeof(*vdsf), M_NOWAIT)) == NULL) {
		vdo_discard(ctx, o1);
		return (ENOMEM);
	}
	vdsf->sf_length = sizeof(struct savefpu);

	if (vdo_append(ctx, (void *)td->td_pcb->pcb_save, vdsf->sf_length,
	    M_NOWAIT)) {
		vdo_discard(ctx, o1);
		return (ENOMEM);
	}

	vdo_close(ctx);

	return (0);
}

int
vps_md_restore_thread_savefpu(struct vps_snapst_ctx *ctx, struct vps *vps,
    struct thread *td)
{
	struct vps_dumpobj *o1;
	struct vps_dump_savefpu *vdsf;

	/* caller verified type. */
	o1 = vdo_next(ctx);

	vdsf = (struct vps_dump_savefpu *)o1->data;

	/*
	 * XXX Verify that we can't harm the system (kernel space)
	 *     by restoring an invalid savefpu context.
	 */
	if (vdsf->sf_length != sizeof(struct savefpu)) {
		DBGCORE("%s: vdsf->sf_length != sizeof(struct savefpu) "
		    "(%u != %lu)\n", __func__, vdsf->sf_length,
		    sizeof(struct savefpu));
		return (EINVAL);
	}

	KASSERT(td->td_pcb != NULL && td->td_pcb->pcb_save != NULL,
		("%s: td->td_pcb == NULL || td->td_pcb->pcb_save == NULL\n",
		__func__));

	DBGCORE("%s: td->td_pcb->pcb_save=%p\n",
	    __func__, td->td_pcb->pcb_save);
	memcpy(td->td_pcb->pcb_save, vdsf->sf_data, vdsf->sf_length);

	return (0);
}

int
vps_md_reboot_copyout(struct thread *td, struct execve_args *args)
{
	vm_offset_t addr;
	struct proc *p;
	int error = 0;

	/*
	 * We push the arguments to execve() onto the
	 * userspace stack of our process.
	 */
	p = td->td_proc;
	addr = p->p_sysent->sv_usrstack - PAGE_SIZE;
	if (p->p_vmspace->vm_ssize < 1 /* page */) {
		/* Should not ever happen in theory ! */
		return (ENOSPC);
	}

	if (p->p_sysent == &elf64_freebsd_sysvec) {
		copyout("/sbin/init", (void *)(addr + 0x40), 11);
		suword64((void *)(addr + 0x0), (addr + 0x40));
		suword64((void *)(addr + 0x8), (vm_offset_t)NULL);
#ifdef COMPAT_FREEBSD32
	} else if (p->p_sysent == &ia32_freebsd_sysvec) {
		copyout("/sbin/init", (void *)(addr + 0x40), 11);
		suword32((void *)(addr + 0x0), (addr + 0x40));
		suword32((void *)(addr + 0x4), (vm_offset_t)NULL);
#endif
	} else {
		error = EINVAL;
	}

	args->fname = (char *)(addr + 0x40);
	args->argv = (char **)addr;
	args->envv = NULL;

#if 0
// notyet
        KASSERT(pargs != NULL,
            ("%s: vps=%p, lost pargs somewhere, don't know what to boot\n",
            __func__, vps));
        arglen = pargs->ar_length;
        /*
        if (arglen > PAGE_SIZE)
                arglen = PAGE_SIZE - 1;
        copyout(pargs->ar_args, (void *)addr, arglen);
        subyte((char *)(addr + PAGE_SIZE - 1), 0x0);
        */
        if (exec_alloc_args(&imgargs)) {
                DBGCORE("%s: exec_alloc_args() returned error\n", __func__);
                pargs_drop(pargs);
                goto fail;
        }  
        if (arglen > PATH_MAX + ARG_MAX)
                arglen = PATH_MAX + ARG_MAX - 1;
        memcpy(imgargs.buf, pargs->ar_args, arglen);
        addr = (vm_offset_t)imgargs.buf;
        // ---
        imgargs.fname = (char *)addr;
        imgargs.begin_argv = (char *)(addr + 0x0);
        imgargs.begin_envv = (char *)(addr + arglen);
        imgargs.endp = (char *)(addr + arglen);
        imgargs.envc = 0;
        imgargs.argc = 1; /* XXX */
        imgargs.stringspace = 0;
#endif

	return (error);
}

int
vps_md_syscall_fixup(struct vps *vps, struct thread *td,
    register_t *ret_code, register_t **ret_args, int *ret_narg)
{
	struct trapframe *frame;
	struct sysentvec *sv;
	struct proc *p;
	caddr_t params;
	register_t code;
	register_t args[8];
	int narg;
	int error = 0;
	int i;
	int ia32_emul = 0;
	struct ucred *save_ucred = curthread->td_ucred;

	if (vps_func->vps_access_vmspace == NULL)
		return (EOPNOTSUPP);

	p = td->td_proc;
	frame = td->td_frame;
	sv = p->p_sysent;

	if (frame->tf_trapno != 0x80) {
		DBGCORE("%s: thread %p was not in syscall: "
		    "tf_trapno=0x%x tf_rip=%p\n", __func__, td,
		    frame->tf_trapno, (void*)frame->tf_rip);

		/* nothing to do ? */
		error = 0;
		goto out;
	}

	if (sv == &elf64_freebsd_sysvec) {
		DBGCORE("%s: proc=%p/%u elf64_freebsd_sysvec\n",
		    __func__, p, p->p_pid);
#ifdef COMPAT_FREEBSD32
	} else if (sv == &ia32_freebsd_sysvec) {
		DBGCORE("%s: proc=%p/%u ia32_freebsd_sysvec\n",
		    __func__, p, p->p_pid);
		ia32_emul = 1;
#endif
	} else {
		DBGCORE("%s: proc=%p/%u unknown sysentvec %p\n",
		    __func__, p, p->p_pid, sv);
		panic("%s: proc=%p/%u unknown sysentvec %p\n",
		    __func__, p, p->p_pid, sv);
	}

	/* Just in case vm objects are split/copied/... */
	curthread->td_ucred = td->td_ucred;

	/*
	 * XXX: special handling for
	 *      sa->code == SYS_syscall || sa->code == SYS___syscall
	 */

	memset((caddr_t)args, 0, sizeof(args));

	code = frame->tf_rax;

	if (sv->sv_mask)
		code &= sv->sv_mask;
	if (code >= sv->sv_size)
		code = 0;

	narg = (&sv->sv_table[code])->sy_narg;

	KASSERT(narg * sizeof(register_t) <= sizeof(args),
	    ("%s: argument space on stack too small, narg=%d\n",
	    __func__, narg));

	if (ia32_emul) {
		uint32_t args32[8];

		memset(args32, 0, sizeof(args32));

		params = (caddr_t)frame->tf_rsp + sizeof(uint32_t);

		if (params != NULL && narg > 0)
			if ((vps_func->vps_access_vmspace(p->p_vmspace,
			    (vm_offset_t)params, narg * sizeof(uint32_t),
			    (caddr_t)args32, VM_PROT_READ))) {
				error = EFAULT;
				goto out;
			}

		for (i = 0; i < narg; i++)
			args[i] = (uint64_t)args32[i];

	} else {

		params = (caddr_t)frame->tf_rsp + sizeof(register_t);

		args[0] = frame->tf_rdi;
		args[1] = frame->tf_rsi;
		args[2] = frame->tf_rdx;
		args[3] = frame->tf_rcx;
		args[4] = frame->tf_r8;
		args[5] = frame->tf_r9;

		for (i = 0; i < 6; i++)
			if (i >= narg)
				args[i] = 0;

		/* XXX only need this in case narg > regcnt (6 on amd64)
		if ((vps_func->vps_access_vmspace(p->p_vmspace,
		    (vm_offset_t)params, narg * sizeof(register_t),
		    (caddr_t)args, VM_PROT_READ))) {
			error = EFAULT;
			goto out;
		}
		*/

	}

	DBGCORE("%s: code=%lu/0x%lx narg=%u args: %016lx %016lx %016lx "
	    "%016lx %016lx %016lx\n", __func__, code, code, narg,
	    args[0], args[1], args[2], args[3], args[4], args[5]);

	DBGCORE("SYSCALL: tid=%d pid=%d syscall=%ld retval[0]=%zx "
	    "retval[1]=%zx errno=%d\n",
	    td->td_tid, td->td_proc->p_pid, code, td->td_retval[0],
	    td->td_retval[1], td->td_errno);

	KASSERT(*ret_narg >= narg,
	    ("%s: supplied args array too small (narg=%d *ret_narg=%d)\n",
	    __func__, narg, *ret_narg));
	*ret_code = code;
	*ret_narg= narg;
	memcpy(ret_args, &args, narg * sizeof(args[0]));

 out:
	curthread->td_ucred = save_ucred;

	return (error);
}

int
vps_md_syscall_fixup_setup_inthread(struct vps *vps, struct thread *td,
    register_t code)
{

	DBGCORE("%s\n", __func__);

	if (vps_func->vps_syscall_fixup_inthread == NULL)
		return (EOPNOTSUPP);

	td->td_pcb->pcb_r12 =
	    (uint64_t)vps_func->vps_syscall_fixup_inthread;
	td->td_pcb->pcb_rip = (uint64_t)fork_trampoline;
	td->td_pcb->pcb_rsp = (uint64_t)td->td_frame -
	    sizeof(void *);
	td->td_pcb->pcb_rbx = (uint64_t)code;
	td->td_pcb->pcb_rbp = 0;

	return (0);
}

void
vps_md_print_pcb(struct thread *td)
{
        struct pcb *p;

        p = td->td_pcb;

        DBGCORE("%s: td=%p\n"
                "pcb_cr3: 0x%16lx\n"
                "pcb_rbp: 0x%16lx\n"
                "pcb_rsp: 0x%16lx\n"
                "pcb_rbx: 0x%16lx\n"
                "pcb_rip: 0x%16lx\n"
                , __func__, td
                , p->pcb_cr3
                , p->pcb_rbp
                , p->pcb_rsp
                , p->pcb_rbx
                , p->pcb_rip
                );
}

#endif /* VPS */

/* EOF */
