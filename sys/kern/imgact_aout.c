/*
 * Copyright (c) 1993, David Greenman
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
 *
 * $FreeBSD$
 */

#include <sys/param.h>
#include <sys/resourcevar.h>
#include <sys/exec.h>
#include <sys/fcntl.h>
#include <sys/imgact.h>
#include <sys/imgact_aout.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/namei.h>
#include <sys/pioctl.h>
#include <sys/proc.h>
#include <sys/signalvar.h>
#include <sys/stat.h>
#include <sys/sysent.h>
#include <sys/syscall.h>
#include <sys/vnode.h>
#include <sys/systm.h>
#include <machine/md_var.h>

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <sys/lock.h>
#include <vm/pmap.h>
#include <vm/vm_map.h>
#include <vm/vm_object.h>
#include <sys/user.h>

static int	exec_aout_imgact __P((struct image_params *imgp));

struct sysentvec aout_sysvec = {
	SYS_MAXSYSCALL,
	sysent,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	sendsig,
	sigcode,
	&szsigcode,
	0,
	"FreeBSD a.out",
	aout_coredump,
	NULL,
	MINSIGSTKSZ
};

static int
exec_aout_imgact(imgp)
	struct image_params *imgp;
{
	const struct exec *a_out = (const struct exec *) imgp->image_header;
	struct vmspace *vmspace;
	struct vnode *vp;
	vm_map_t map;
	vm_object_t object;
	vm_offset_t text_end, data_end;
	unsigned long virtual_offset;
	unsigned long file_offset;
	unsigned long bss_size;
	int error;

	/*
	 * Linux and *BSD binaries look very much alike,
	 * only the machine id is different:
	 * 0x64 for Linux, 0x86 for *BSD, 0x00 for BSDI.
	 * NetBSD is in network byte order.. ugh.
	 */
	if (((a_out->a_magic >> 16) & 0xff) != 0x86 &&
	    ((a_out->a_magic >> 16) & 0xff) != 0 &&
	    ((((int)ntohl(a_out->a_magic)) >> 16) & 0xff) != 0x86)
                return -1;

	/*
	 * Set file/virtual offset based on a.out variant.
	 *	We do two cases: host byte order and network byte order
	 *	(for NetBSD compatibility)
	 */
	switch ((int)(a_out->a_magic & 0xffff)) {
	case ZMAGIC:
		virtual_offset = 0;
		if (a_out->a_text) {
			file_offset = PAGE_SIZE;
		} else {
			/* Bill's "screwball mode" */
			file_offset = 0;
		}
		break;
	case QMAGIC:
		virtual_offset = PAGE_SIZE;
		file_offset = 0;
		/* Pass PS_STRINGS for BSD/OS binaries only. */
		if (N_GETMID(*a_out) == MID_ZERO)
			imgp->ps_strings = PS_STRINGS;
		break;
	default:
		/* NetBSD compatibility */
		switch ((int)(ntohl(a_out->a_magic) & 0xffff)) {
		case ZMAGIC:
		case QMAGIC:
			virtual_offset = PAGE_SIZE;
			file_offset = 0;
			break;
		default:
			return (-1);
		}
	}

	bss_size = roundup(a_out->a_bss, PAGE_SIZE);

	/*
	 * Check various fields in header for validity/bounds.
	 */
	if (/* entry point must lay with text region */
	    a_out->a_entry < virtual_offset ||
	    a_out->a_entry >= virtual_offset + a_out->a_text ||

	    /* text and data size must each be page rounded */
	    a_out->a_text & PAGE_MASK || a_out->a_data & PAGE_MASK)
		return (-1);

	/* text + data can't exceed file size */
	if (a_out->a_data + a_out->a_text > imgp->attr->va_size)
		return (EFAULT);

	/*
	 * text/data/bss must not exceed limits
	 */
	if (/* text can't exceed maximum text size */
	    a_out->a_text > maxtsiz ||

	    /* data + bss can't exceed rlimit */
	    a_out->a_data + bss_size >
		imgp->proc->p_rlimit[RLIMIT_DATA].rlim_cur)
			return (ENOMEM);

	/* copy in arguments and/or environment from old process */
	error = exec_extract_strings(imgp);
	if (error)
		return (error);

	/*
	 * Destroy old process VM and create a new one (with a new stack)
	 */
	exec_new_vmspace(imgp);

	/*
	 * The vm space can be changed by exec_new_vmspace
	 */
	vmspace = imgp->proc->p_vmspace;

	vp = imgp->vp;
	map = &vmspace->vm_map;
	vm_map_lock(map);
	VOP_GETVOBJECT(vp, &object);
	vm_object_reference(object);

	text_end = virtual_offset + a_out->a_text;
	error = vm_map_insert(map, object,
		file_offset,
		virtual_offset, text_end,
		VM_PROT_READ | VM_PROT_EXECUTE, VM_PROT_ALL,
		MAP_COPY_ON_WRITE | MAP_PREFAULT);
	if (error) {
		vm_map_unlock(map);
		return (error);
	}
	data_end = text_end + a_out->a_data;
	if (a_out->a_data) {
		vm_object_reference(object);
		error = vm_map_insert(map, object,
			file_offset + a_out->a_text,
			text_end, data_end,
			VM_PROT_ALL, VM_PROT_ALL,
			MAP_COPY_ON_WRITE | MAP_PREFAULT);
		if (error) {
			vm_map_unlock(map);
			return (error);
		}
	}

	if (bss_size) {
		error = vm_map_insert(map, NULL, 0,
			data_end, data_end + bss_size,
			VM_PROT_ALL, VM_PROT_ALL, 0);
		if (error) {
			vm_map_unlock(map);
			return (error);
		}
	}
	vm_map_unlock(map);

	/* Fill in process VM information */
	vmspace->vm_tsize = a_out->a_text >> PAGE_SHIFT;
	vmspace->vm_dsize = (a_out->a_data + bss_size) >> PAGE_SHIFT;
	vmspace->vm_taddr = (caddr_t) (uintptr_t) virtual_offset;
	vmspace->vm_daddr = (caddr_t) (uintptr_t)
			    (virtual_offset + a_out->a_text);

	/* Fill in image_params */
	imgp->interpreted = 0;
	imgp->entry_addr = a_out->a_entry;

	imgp->proc->p_sysent = &aout_sysvec;

	/* Indicate that this file should not be modified */
	imgp->vp->v_flag |= VTEXT;

	return (0);
}

/*
 * Dump core, into a file named as described in the comments for
 * expand_name(), unless the process was setuid/setgid.
 */
int
aout_coredump(p, vp, limit)
	register struct proc *p;
	register struct vnode *vp;
	off_t limit;
{
	register struct ucred *cred = p->p_ucred;
	register struct vmspace *vm = p->p_vmspace;
	int error;

	if (ctob(UPAGES + vm->vm_dsize + vm->vm_ssize) >= limit)
		return (EFAULT);
	bcopy(p, &p->p_addr->u_kproc.kp_proc, sizeof(struct proc));
	fill_eproc(p, &p->p_addr->u_kproc.kp_eproc);
	error = cpu_coredump(p, vp, cred);
	if (error == 0)
		error = vn_rdwr_inchunks(UIO_WRITE, vp, vm->vm_daddr,
		    (int)ctob(vm->vm_dsize), (off_t)ctob(UPAGES), UIO_USERSPACE,
		    IO_UNIT | IO_DIRECT, cred, (int *) NULL, p);
	if (error == 0)
		error = vn_rdwr_inchunks(UIO_WRITE, vp,
		    (caddr_t) trunc_page(USRSTACK - ctob(vm->vm_ssize)),
		    round_page(ctob(vm->vm_ssize)),
		    (off_t)ctob(UPAGES) + ctob(vm->vm_dsize), UIO_USERSPACE,
		    IO_UNIT | IO_DIRECT, cred, (int *) NULL, p);
	return (error);
}

/*
 * Tell kern_execve.c about it, with a little help from the linker.
 */
static struct execsw aout_execsw = { exec_aout_imgact, "a.out" };
EXEC_SET(aout, aout_execsw);
