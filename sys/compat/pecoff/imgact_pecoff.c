/* $NetBSD$	 */
/* $FreeBSD$       */

/*
 * Copyright (c) 2000 Masaru OKI
 * Copyright (c) 1994, 1995, 1998 Scott Bartram
 * Copyright (c) 1994 Adam Glass
 * Copyright (c) 1993, 1994 Christopher G. Demetriou
 *
 * originally from NetBSD kern/exec_ecoff.c
 *
 * Copyright (c) 2000 Takanori Watanabe
 * Copyright (c) 2000 KUROSAWA Takahiro
 * Copyright (c) 1995-1996 Sen Schmidt
 * Copyright (c) 1996 Peter Wemm
 * All rights reserved.
 *
 * originally from FreeBSD kern/imgact_elf.c
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
 *      This product includes software developed by Masaru OKI.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/imgact.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mman.h>
#include <sys/mutex.h>
#include <sys/namei.h>
#include <sys/proc.h>
#include <sys/signalvar.h>
#include <sys/sysent.h>
#include <sys/vnode.h>

#include <machine/reg.h>

#include <vm/vm.h>
#include <vm/vm_kern.h>
#include <vm/vm_param.h>
#include <vm/pmap.h>
#include <vm/vm_map.h>
#include <vm/vm_object.h>
#include <vm/vm_extern.h>

#include <sys/user.h>
#include <sys/exec.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <machine/cpu.h>
#include <sys/syscall.h>
#include <sys/sysent.h>
#include <machine/md_var.h>
#include <machine/pecoff_machdep.h>
#include <compat/pecoff/imgact_pecoff.h>

#include "opt_pecoff.h"

#define PECOFF_PE_SIGNATURE "PE\0\0"
static int      pecoff_fixup(register_t **, struct image_params *);
static int 
pecoff_coredump(register struct thread *, register struct vnode *,
		off_t);
#ifndef PECOFF_DEBUG
#define DPRINTF(a)
#else
#define DPRINTF(a) printf a
#endif
static struct sysentvec pecoff_sysvec = {
	SYS_MAXSYSCALL,
	sysent,
	0,
	0,
	NULL,
	0,
	NULL,
	NULL,
	pecoff_fixup,
	sendsig,
	sigcode,
	&szsigcode,
	0,
	"FreeBSD PECoff",
	pecoff_coredump,
	NULL,
	MINSIGSTKSZ,
	PAGE_SIZE,
	VM_MIN_ADDRESS,
	VM_MAXUSER_ADDRESS,
	USRSTACK,
	PS_STRINGS,
	VM_PROT_ALL,
	exec_copyout_strings,
	exec_setregs
	
};

static const char signature[] = PECOFF_PE_SIGNATURE;

static int 
exec_pecoff_coff_prep_omagic(struct image_params *,
			     struct coff_filehdr *,
			     struct coff_aouthdr *, int peoffs);
static int 
exec_pecoff_coff_prep_nmagic(struct image_params *,
			     struct coff_filehdr *,
			     struct coff_aouthdr *, int peoffs);
static int 
exec_pecoff_coff_prep_zmagic(struct image_params *,
			     struct coff_filehdr *,
			     struct coff_aouthdr *, int peoffs);

static int 
exec_pecoff_coff_makecmds(struct image_params *,
			  struct coff_filehdr *, int);

static int      pecoff_signature(struct thread *, struct vnode *, const struct pecoff_dos_filehdr *);
static int      pecoff_read_from(struct thread *, struct vnode *, int, caddr_t, int);
static int 
pecoff_load_section(struct thread * td,
		    struct vmspace * vmspace, struct vnode * vp,
	     vm_offset_t offset, caddr_t vmaddr, size_t memsz, size_t filsz,
		    vm_prot_t prot);

static int 
pecoff_fixup(register_t ** stack_base, struct image_params * imgp)
{
	int             len = sizeof(struct pecoff_args);
	struct pecoff_imghdr *ap;
	register_t     *pos;

	pos = *stack_base + (imgp->argc + imgp->envc + 2);
	ap = (struct pecoff_imghdr *) imgp->auxargs;
	if (copyout(ap, pos, len)) {
		return 0;
	}
	free(ap, M_TEMP);
	imgp->auxargs = NULL;
	(*stack_base)--;
	suword(*stack_base, (long) imgp->argc);
	return 0;
}


static int 
pecoff_coredump(register struct thread * td, register struct vnode * vp,
		off_t limit)
{
	register struct ucred *cred = td->td_ucred;
	struct proc *p = td->td_proc;
	register struct vmspace *vm = p->p_vmspace;
	char *tempuser;
	int             error;
#ifdef PECOFF_DEBUG
	struct vm_map  *map;
	struct vm_map_entry *ent;
	struct reg      regs;

#endif
	if (ctob((uarea_pages + kstack_pages) + vm->vm_dsize + vm->vm_ssize) >=
	    limit)
		return (EFAULT);
	tempuser = malloc(ctob(uarea_pages + kstack_pages), M_TEMP,
	    M_ZERO);
	if (tempuser == NULL)
		return (ENOMEM);
	PROC_LOCK(p);
	fill_kinfo_proc(p, &p->p_uarea->u_kproc);
	PROC_UNLOCK(p);
	bcopy(p->p_uarea, tempuser, sizeof(struct user));
	bcopy(td->td_frame,
	    tempuser + ctob(uarea_pages) +
	    ((caddr_t)td->td_frame - (caddr_t)td->td_kstack),
	    sizeof(struct trapframe));
#if PECOFF_DEBUG
	fill_regs(td, &regs);
	printf("EIP%x\n", regs.r_eip);
	printf("EAX%x EBX%x ECX%x EDI%x\n",
	       regs.r_eax, regs.r_ebx, regs.r_ecx, regs.r_edi);
	map = &vm->vm_map;
	ent = &map->header;
	printf("%p %p %p\n", ent, ent->prev, ent->next);
#endif
	error = vn_rdwr(UIO_WRITE, vp, (caddr_t)tempuser,
	    ctob(uarea_pages + kstack_pages),
	    (off_t)0, UIO_SYSSPACE, IO_UNIT, cred, NOCRED,
	    (int *)NULL, td);
	free(tempuser, M_TEMP);
	if (error == 0)
		error = vn_rdwr_inchunks(UIO_WRITE, vp, vm->vm_daddr,
		    (int)ctob(vm->vm_dsize),
		    (off_t)ctob((uarea_pages + kstack_pages)),
		    UIO_USERSPACE, IO_UNIT, cred, NOCRED, (int *)NULL, td);
	if (error == 0)
		error = vn_rdwr_inchunks(UIO_WRITE, vp,
		    (caddr_t)trunc_page(USRSTACK - ctob(vm->vm_ssize)),
		    round_page(ctob(vm->vm_ssize)),
		    (off_t)ctob((uarea_pages + kstack_pages)) +
		    ctob(vm->vm_dsize),
		    UIO_USERSPACE, IO_UNIT, cred, NOCRED, (int *)NULL, td);
	return (error);

}

static int 
pecoff_load_section(struct thread * td, struct vmspace * vmspace, struct vnode * vp, vm_offset_t offset, caddr_t vmaddr, size_t memsz, size_t filsz, vm_prot_t prot)
{
	size_t          map_len;
	vm_offset_t     map_addr;
	int             error, rv;
	size_t          copy_len;
	size_t          copy_map_len;
	size_t          copy_start;
	vm_object_t     object;
	vm_offset_t     copy_map_offset;
	vm_offset_t     file_addr;
	vm_offset_t     data_buf = 0;

	object = vp->v_object;
	error = 0;

	map_addr = trunc_page((vm_offset_t) vmaddr);
	file_addr = trunc_page(offset);
	DPRINTF(("SECARG:%x %p %x %x\n", offset, vmaddr, memsz, filsz));
	if (file_addr != offset) {
		/*
		 * The section is not on page  boundary. We can't use
		 * vm_map_insert(). Use copyin instead.
		 */
		map_len = round_page(memsz);
		copy_len = filsz;
		copy_map_offset = file_addr;
		copy_map_len = round_page(offset + filsz) - file_addr;
		copy_start = offset - file_addr;

		DPRINTF(("offset=%x vmaddr=%lx filsz=%x memsz=%x\n",
			 offset, (long)vmaddr, filsz, memsz));
		DPRINTF(("map_len=%x copy_len=%x copy_map_offset=%x"
			 " copy_map_len=%x copy_start=%x\n",
			 map_len, copy_len, copy_map_offset,
			 copy_map_len, copy_start));
	} else {

		map_len = trunc_page(filsz);

		if (map_len != 0) {
			vm_object_reference(object);
			vm_map_lock(&vmspace->vm_map);
			rv = vm_map_insert(&vmspace->vm_map,
					   object,
					   file_addr,	/* file offset */
					   map_addr,	/* virtual start */
					   map_addr + map_len,	/* virtual end */
					   prot,
					   VM_PROT_ALL,
					   MAP_COPY_ON_WRITE | MAP_PREFAULT);

			vm_map_unlock(&vmspace->vm_map);
			if (rv != KERN_SUCCESS) {
				vm_object_deallocate(object);
				return EINVAL;
			}
			/* we can stop now if we've covered it all */
			if (memsz == filsz)
				return 0;

		}
		copy_map_offset = trunc_page(offset + filsz);
		copy_map_len = PAGE_SIZE;
		copy_start = 0;
		copy_len = (offset + filsz) - trunc_page(offset + filsz);
		map_addr = trunc_page((vm_offset_t) vmaddr + filsz);
		map_len = round_page((vm_offset_t) vmaddr + memsz) - map_addr;

	}

	if (map_len != 0) {
		vm_map_lock(&vmspace->vm_map);
		rv = vm_map_insert(&vmspace->vm_map, NULL, 0,
				   map_addr, map_addr + map_len,
				   VM_PROT_ALL, VM_PROT_ALL, 0);
		vm_map_unlock(&vmspace->vm_map);
		DPRINTF(("EMP-rv:%d,%x %x\n", rv, map_addr, map_addr + map_len));
		if (rv != KERN_SUCCESS) {
			return EINVAL;
		}
	}
	DPRINTF(("COPYARG %x %x\n", map_addr, copy_len));
	if (copy_len != 0) {
		vm_object_reference(object);
		rv = vm_map_find(exec_map,
				 object,
				 copy_map_offset,
				 &data_buf,
				 copy_map_len,
				 TRUE,
				 VM_PROT_READ,
				 VM_PROT_ALL,
				 MAP_COPY_ON_WRITE | MAP_PREFAULT_PARTIAL);
		if (rv != KERN_SUCCESS) {
			vm_object_deallocate(object);
			return EINVAL;
		}
		/* send the page fragment to user space */

		error = copyout((caddr_t) data_buf + copy_start,
				(caddr_t) map_addr, copy_len);
		vm_map_remove(exec_map, data_buf, data_buf + copy_map_len);
		DPRINTF(("%d\n", error));
		if (error)
			return (error);
	}
	/*
	 * set it to the specified protection
	 */
	vm_map_protect(&vmspace->vm_map, map_addr,
		       map_addr + map_len, prot,
		       FALSE);
	return error;

}
static int 
pecoff_load_file(struct thread * td, const char *file, u_long * addr, u_long * entry, u_long * ldexport)
{

	struct nameidata nd;
	struct pecoff_dos_filehdr dh;
	struct coff_filehdr *fp = 0;
	struct coff_aouthdr *ap;
	struct pecoff_opthdr *wp;
	struct coff_scnhdr *sh = 0;
	struct vmspace *vmspace = td->td_proc->p_vmspace;
	struct vattr    attr;
	struct image_params image_params, *imgp;
	int             peofs;
	int             error, i, scnsiz;

	imgp = &image_params;
	/*
	 * Initialize part of the common data
	 */
	imgp->proc = td->td_proc;
	imgp->userspace_argv = NULL;
	imgp->userspace_envv = NULL;
	imgp->execlabel = NULL;
	imgp->attr = &attr;
	imgp->firstpage = NULL;

	NDINIT(&nd, LOOKUP, LOCKLEAF | FOLLOW, UIO_SYSSPACE, file, td);

	if ((error = namei(&nd)) != 0) {
		nd.ni_vp = NULL;
		goto fail;
	}
	NDFREE(&nd, NDF_ONLY_PNBUF);
	imgp->vp = nd.ni_vp;

	/*
	 * Check permissions, modes, uid, etc on the file, and "open" it.
	 */
	error = exec_check_permissions(imgp);
	if (error) {
		VOP_UNLOCK(nd.ni_vp, 0, td);
		goto fail;
	}
	VOP_UNLOCK(nd.ni_vp, 0, td);
	if (error)
		goto fail;
	if ((error = pecoff_read_from(td, imgp->vp, 0, (caddr_t) & dh, sizeof(dh))) != 0)
		goto fail;
	if ((error = pecoff_signature(td, imgp->vp, &dh) != 0))
		goto fail;
	fp = malloc(PECOFF_HDR_SIZE, M_TEMP, 0);
	peofs = dh.d_peofs + sizeof(signature) - 1;
	if ((error = pecoff_read_from(td, imgp->vp, peofs, (caddr_t) fp, PECOFF_HDR_SIZE) != 0))
		goto fail;
	if (COFF_BADMAG(fp)) {
		error = ENOEXEC;
		goto fail;
	}
	ap = (void *) ((char *) fp + sizeof(struct coff_filehdr));
	wp = (void *) ((char *) ap + sizeof(struct coff_aouthdr));
	/* read section header */
	scnsiz = sizeof(struct coff_scnhdr) * fp->f_nscns;
	sh = malloc(scnsiz, M_TEMP, 0);
	if ((error = pecoff_read_from(td, imgp->vp, peofs + PECOFF_HDR_SIZE,
				      (caddr_t) sh, scnsiz)) != 0)
		goto fail;

	/*
	 * Read Section infomation and map sections.
	 */

	for (i = 0; i < fp->f_nscns; i++) {
		int             prot = 0;

		if (sh[i].s_flags & COFF_STYP_DISCARD)
			continue;
		/* XXX ? */
		if ((sh[i].s_flags & COFF_STYP_TEXT) &&
		    (sh[i].s_flags & COFF_STYP_EXEC) == 0)
			continue;
		if ((sh[i].s_flags & (COFF_STYP_TEXT | COFF_STYP_DATA | COFF_STYP_BSS)) == 0)
			continue;

		prot |= (sh[i].s_flags & COFF_STYP_READ) ? VM_PROT_READ : 0;
		prot |= (sh[i].s_flags & COFF_STYP_WRITE) ? VM_PROT_WRITE : 0;
		prot |= (sh[i].s_flags & COFF_STYP_EXEC) ? VM_PROT_EXECUTE : 0;

		sh[i].s_vaddr += wp->w_base;	/* RVA --> VA */
		if ((error = pecoff_load_section(td, vmspace, imgp->vp, sh[i].s_scnptr
						 ,(caddr_t) sh[i].s_vaddr,
						 sh[i].s_paddr, sh[i].s_size
						 ,prot)) != 0)
			goto fail;

	}
	*entry = wp->w_base + ap->a_entry;
	*addr = wp->w_base;
	*ldexport = wp->w_imghdr[0].i_vaddr + wp->w_base;
fail:
	if (fp)
		free(fp, M_TEMP);
	if (sh)
		free(sh, M_TEMP);
	if (nd.ni_vp)
		vrele(nd.ni_vp);

	return error;
}
static int
exec_pecoff_coff_prep_omagic(struct image_params * imgp,
			     struct coff_filehdr * fp,
			     struct coff_aouthdr * ap, int peofs)
{
	return ENOEXEC;
}
static int
exec_pecoff_coff_prep_nmagic(struct image_params * imgp,
			     struct coff_filehdr * fp,
			     struct coff_aouthdr * ap, int peofs)
{
	return ENOEXEC;
}
static int
exec_pecoff_coff_prep_zmagic(struct image_params * imgp,
			     struct coff_filehdr * fp,
			     struct coff_aouthdr * ap, int peofs)
{
	int             scnsiz = sizeof(struct coff_scnhdr) * fp->f_nscns;
	int             error = ENOEXEC, i;
	int             prot;
	u_long          text_size = 0, data_size = 0, dsize;
	u_long          text_addr = 0, data_addr = VM_MAXUSER_ADDRESS;
	u_long          ldexport, ldbase;
	struct pecoff_opthdr *wp;
	struct coff_scnhdr *sh;
	struct vmspace *vmspace;
	struct pecoff_args *argp = NULL;

	sh = malloc(scnsiz, M_TEMP, 0);

	wp = (void *) ((char *) ap + sizeof(struct coff_aouthdr));
	error = pecoff_read_from(FIRST_THREAD_IN_PROC(imgp->proc), imgp->vp,
	    peofs + PECOFF_HDR_SIZE, (caddr_t) sh, scnsiz);
	if ((error = exec_extract_strings(imgp)) != 0)
		goto fail;
	exec_new_vmspace(imgp, &pecoff_sysvec);
	vmspace = imgp->proc->p_vmspace;
	for (i = 0; i < fp->f_nscns; i++) {
		prot = VM_PROT_WRITE;	/* XXX for relocation? */
		prot |= (sh[i].s_flags & COFF_STYP_READ) ? VM_PROT_READ : 0;
		prot |= (sh[i].s_flags & COFF_STYP_WRITE) ? VM_PROT_WRITE : 0;
		prot |= (sh[i].s_flags & COFF_STYP_EXEC) ? VM_PROT_EXECUTE : 0;
		sh[i].s_vaddr += wp->w_base;
		if (sh[i].s_flags & COFF_STYP_DISCARD)
			continue;
		if ((sh[i].s_flags & COFF_STYP_TEXT) != 0) {

			error = pecoff_load_section(
			    FIRST_THREAD_IN_PROC(imgp->proc),
			    vmspace, imgp->vp, sh[i].s_scnptr,
			    (caddr_t) sh[i].s_vaddr, sh[i].s_paddr,
			    sh[i].s_size ,prot);
			DPRINTF(("ERROR%d\n", error));
			if (error)
				goto fail;
			text_addr = trunc_page(sh[i].s_vaddr);
			text_size = trunc_page(sh[i].s_size + sh[i].s_vaddr - text_addr);

		}
		if ((sh[i].s_flags & (COFF_STYP_DATA|COFF_STYP_BSS)) != 0) {
			if (pecoff_load_section(
			    FIRST_THREAD_IN_PROC(imgp->proc), vmspace,
			    imgp->vp, sh[i].s_scnptr, (caddr_t) sh[i].s_vaddr,
			    sh[i].s_paddr, sh[i].s_size, prot) != 0)
				goto fail;
			data_addr = min(trunc_page(sh[i].s_vaddr), data_addr);
			dsize = round_page(sh[i].s_vaddr + sh[i].s_paddr)
				- data_addr;
			data_size = max(dsize, data_size);

		}
	}
	vmspace->vm_tsize = text_size >> PAGE_SHIFT;
	vmspace->vm_taddr = (caddr_t) (uintptr_t) text_addr;
	vmspace->vm_dsize = data_size >> PAGE_SHIFT;
	vmspace->vm_daddr = (caddr_t) (uintptr_t) data_addr;
	argp = malloc(sizeof(struct pecoff_args), M_TEMP, 0);
	if (argp == NULL) {
		error = ENOMEM;
		goto fail;
	}
	argp->a_base = wp->w_base;
	argp->a_entry = wp->w_base + ap->a_entry;
	argp->a_end = data_addr + data_size;
	argp->a_subsystem = wp->w_subvers;
	error = pecoff_load_file(FIRST_THREAD_IN_PROC(imgp->proc),
	    "/usr/libexec/ld.so.dll", &ldbase, &imgp->entry_addr, &ldexport);
	if (error)
		goto fail;

	argp->a_ldbase = ldbase;
	argp->a_ldexport = ldexport;
	memcpy(argp->a_imghdr, wp->w_imghdr, sizeof(struct pecoff_imghdr) * 16);
	for (i = 0; i < 16; i++) {
		argp->a_imghdr[i].i_vaddr += wp->w_base;
	}
	imgp->proc->p_sysent = &pecoff_sysvec;
	if (error)
		goto fail;
	imgp->auxargs = argp;
	imgp->auxarg_size = sizeof(struct pecoff_args);
	imgp->interpreted = 0;

	if (sh != NULL)
		free(sh, M_TEMP);
	return 0;
fail:
	error = (error) ? error : ENOEXEC;
	if (sh != NULL)
		free(sh, M_TEMP);
	if (argp != NULL)
		free(argp, M_TEMP);

	return error;
}

int
exec_pecoff_coff_makecmds(struct image_params * imgp,
			  struct coff_filehdr * fp, int peofs)
{
	struct coff_aouthdr *ap;
	int             error;

	if (COFF_BADMAG(fp)) {
		return ENOEXEC;
	}
	ap = (void *) ((char *) fp + sizeof(struct coff_filehdr));
	switch (ap->a_magic) {
	case COFF_OMAGIC:
		error = exec_pecoff_coff_prep_omagic(imgp, fp, ap, peofs);
		break;
	case COFF_NMAGIC:
		error = exec_pecoff_coff_prep_nmagic(imgp, fp, ap, peofs);
		break;
	case COFF_ZMAGIC:
		error = exec_pecoff_coff_prep_zmagic(imgp, fp, ap, peofs);
		break;
	default:
		return ENOEXEC;
	}

	return error;
}

static int
pecoff_signature(td, vp, dp)
	struct thread  *td;
	struct vnode   *vp;
	const struct pecoff_dos_filehdr *dp;
{
	int             error;
	char            buf[512];
	char           *pesig;
	if (DOS_BADMAG(dp)) {
		return ENOEXEC;
	}
	error = pecoff_read_from(td, vp, dp->d_peofs, buf, sizeof(buf));
	if (error) {
		return error;
	}
	pesig = buf;
	if (memcmp(pesig, signature, sizeof(signature) - 1) == 0) {
		return 0;
	}
	return EFTYPE;
}
int
pecoff_read_from(td, vp, pos, buf, siz)
	struct thread  *td;
	struct vnode   *vp;
	int             pos;
	caddr_t         buf;
	int             siz;
{
	int             error;
	size_t          resid;

	error = vn_rdwr(UIO_READ, vp, buf, siz, pos,
			UIO_SYSSPACE, IO_NODELOCKED, td->td_ucred, NOCRED,
			&resid, td);
	if (error)
		return error;

	if (resid != 0) {
		return ENOEXEC;
	}
	return 0;
}

static int 
imgact_pecoff(struct image_params * imgp)
{
	const struct pecoff_dos_filehdr *dp = (const struct pecoff_dos_filehdr *)
	imgp->image_header;
	struct coff_filehdr *fp;
	int             error, peofs;
	struct thread *td = curthread;

	error = pecoff_signature(FIRST_THREAD_IN_PROC(imgp->proc),
	    imgp->vp, dp);
	if (error) {
		return -1;
	}
	VOP_UNLOCK(imgp->vp, 0, td);

	peofs = dp->d_peofs + sizeof(signature) - 1;
	fp = malloc(PECOFF_HDR_SIZE, M_TEMP, 0);
	error = pecoff_read_from(FIRST_THREAD_IN_PROC(imgp->proc),
	     imgp->vp, peofs, (caddr_t) fp, PECOFF_HDR_SIZE);
	if (error)
		goto fail;

	error = exec_pecoff_coff_makecmds(imgp, fp, peofs);
fail:   
	free(fp, M_TEMP);
        vn_lock(imgp->vp, LK_EXCLUSIVE | LK_RETRY, td);
	return error;
}

static struct execsw pecoff_execsw = {imgact_pecoff, "FreeBSD PEcoff"};
EXEC_SET(pecoff, pecoff_execsw);
