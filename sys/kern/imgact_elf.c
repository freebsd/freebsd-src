/*-
 * Copyright (c) 2000 David O'Brien
 * Copyright (c) 1995-1996 Søren Schmidt
 * Copyright (c) 1996 Peter Wemm
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in this position and unchanged.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_compat.h"

#include <sys/param.h>
#include <sys/exec.h>
#include <sys/fcntl.h>
#include <sys/imgact.h>
#include <sys/imgact_elf.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/mman.h>
#include <sys/namei.h>
#include <sys/pioctl.h>
#include <sys/proc.h>
#include <sys/procfs.h>
#include <sys/resourcevar.h>
#include <sys/systm.h>
#include <sys/signalvar.h>
#include <sys/stat.h>
#include <sys/sx.h>
#include <sys/syscall.h>
#include <sys/sysctl.h>
#include <sys/sysent.h>
#include <sys/vnode.h>

#include <vm/vm.h>
#include <vm/vm_kern.h>
#include <vm/vm_param.h>
#include <vm/pmap.h>
#include <vm/vm_map.h>
#include <vm/vm_object.h>
#include <vm/vm_extern.h>

#include <machine/elf.h>
#include <machine/md_var.h>

#if defined(COMPAT_IA32) && __ELF_WORD_SIZE == 32
#include <machine/fpu.h>
#include <compat/ia32/ia32_reg.h>
#endif

#define OLD_EI_BRAND	8

static int __elfN(check_header)(const Elf_Ehdr *hdr);
static Elf_Brandinfo *__elfN(get_brandinfo)(const Elf_Ehdr *hdr,
    const char *interp);
static int __elfN(load_file)(struct proc *p, const char *file, u_long *addr,
    u_long *entry, size_t pagesize);
static int __elfN(load_section)(struct proc *p,
    struct vmspace *vmspace, struct vnode *vp, vm_object_t object,
    vm_offset_t offset, caddr_t vmaddr, size_t memsz, size_t filsz,
    vm_prot_t prot, size_t pagesize);
static int __CONCAT(exec_, __elfN(imgact))(struct image_params *imgp);

SYSCTL_NODE(_kern, OID_AUTO, __CONCAT(elf, __ELF_WORD_SIZE), CTLFLAG_RW, 0,
    "");

int __elfN(fallback_brand) = -1;
SYSCTL_INT(__CONCAT(_kern_elf, __ELF_WORD_SIZE), OID_AUTO,
    fallback_brand, CTLFLAG_RW, &__elfN(fallback_brand), 0,
    __XSTRING(__CONCAT(ELF, __ELF_WORD_SIZE)) " brand of last resort");
TUNABLE_INT("kern.elf" __XSTRING(__ELF_WORD_SIZE) ".fallback_brand",
    &__elfN(fallback_brand));

static int elf_trace = 0;
SYSCTL_INT(_debug, OID_AUTO, __elfN(trace), CTLFLAG_RW, &elf_trace, 0, "");

static int elf_legacy_coredump = 0;
SYSCTL_INT(_debug, OID_AUTO, __elfN(legacy_coredump), CTLFLAG_RW, 
    &elf_legacy_coredump, 0, "");

static Elf_Brandinfo *elf_brand_list[MAX_BRANDS];

int
__elfN(insert_brand_entry)(Elf_Brandinfo *entry)
{
	int i;

	for (i = 0; i < MAX_BRANDS; i++) {
		if (elf_brand_list[i] == NULL) {
			elf_brand_list[i] = entry;
			break;
		}
	}
	if (i == MAX_BRANDS)
		return (-1);
	return (0);
}

int
__elfN(remove_brand_entry)(Elf_Brandinfo *entry)
{
	int i;

	for (i = 0; i < MAX_BRANDS; i++) {
		if (elf_brand_list[i] == entry) {
			elf_brand_list[i] = NULL;
			break;
		}
	}
	if (i == MAX_BRANDS)
		return (-1);
	return (0);
}

int
__elfN(brand_inuse)(Elf_Brandinfo *entry)
{
	struct proc *p;
	int rval = FALSE;

	sx_slock(&allproc_lock);
	LIST_FOREACH(p, &allproc, p_list) {
		if (p->p_sysent == entry->sysvec) {
			rval = TRUE;
			break;
		}
	}
	sx_sunlock(&allproc_lock);

	return (rval);
}

static Elf_Brandinfo *
__elfN(get_brandinfo)(const Elf_Ehdr *hdr, const char *interp)
{
	Elf_Brandinfo *bi;
	int i;

	/*
	 * We support three types of branding -- (1) the ELF EI_OSABI field
	 * that SCO added to the ELF spec, (2) FreeBSD 3.x's traditional string
	 * branding w/in the ELF header, and (3) path of the `interp_path'
	 * field.  We should also look for an ".note.ABI-tag" ELF section now
	 * in all Linux ELF binaries, FreeBSD 4.1+, and some NetBSD ones.
	 */

	/* If the executable has a brand, search for it in the brand list. */
	for (i = 0; i < MAX_BRANDS; i++) {
		bi = elf_brand_list[i];
		if (bi != NULL && hdr->e_machine == bi->machine &&
		    (hdr->e_ident[EI_OSABI] == bi->brand ||
		    strncmp((const char *)&hdr->e_ident[OLD_EI_BRAND],
		    bi->compat_3_brand, strlen(bi->compat_3_brand)) == 0))
			return (bi);
	}

	/* Lacking a known brand, search for a recognized interpreter. */
	if (interp != NULL) {
		for (i = 0; i < MAX_BRANDS; i++) {
			bi = elf_brand_list[i];
			if (bi != NULL && hdr->e_machine == bi->machine &&
			    strcmp(interp, bi->interp_path) == 0)
				return (bi);
		}
	}

	/* Lacking a recognized interpreter, try the default brand */
	for (i = 0; i < MAX_BRANDS; i++) {
		bi = elf_brand_list[i];
		if (bi != NULL && hdr->e_machine == bi->machine &&
		    __elfN(fallback_brand) == bi->brand)
			return (bi);
	}
	return (NULL);
}

static int
__elfN(check_header)(const Elf_Ehdr *hdr)
{
	Elf_Brandinfo *bi;
	int i;

	if (!IS_ELF(*hdr) ||
	    hdr->e_ident[EI_CLASS] != ELF_TARG_CLASS ||
	    hdr->e_ident[EI_DATA] != ELF_TARG_DATA ||
	    hdr->e_ident[EI_VERSION] != EV_CURRENT ||
	    hdr->e_phentsize != sizeof(Elf_Phdr) ||
	    hdr->e_version != ELF_TARG_VER)
		return (ENOEXEC);

	/*
	 * Make sure we have at least one brand for this machine.
	 */

	for (i = 0; i < MAX_BRANDS; i++) {
		bi = elf_brand_list[i];
		if (bi != NULL && bi->machine == hdr->e_machine)
			break;
	}
	if (i == MAX_BRANDS)
		return (ENOEXEC);

	return (0);
}

static int
__elfN(map_partial)(vm_map_t map, vm_object_t object, vm_ooffset_t offset,
	vm_offset_t start, vm_offset_t end, vm_prot_t prot,
	vm_prot_t max)
{
	int error, rv;
	vm_offset_t off;
	vm_offset_t data_buf = 0;

	/*
	 * Create the page if it doesn't exist yet. Ignore errors.
	 */
	vm_map_lock(map);
	vm_map_insert(map, NULL, 0, trunc_page(start), round_page(end), max,
	    max, 0);
	vm_map_unlock(map);

	/*
	 * Find the page from the underlying object.
	 */
	if (object) {
		vm_object_reference(object);
		rv = vm_map_find(exec_map,
				 object,
				 trunc_page(offset),
				 &data_buf,
				 PAGE_SIZE,
				 TRUE,
				 VM_PROT_READ,
				 VM_PROT_ALL,
				 MAP_COPY_ON_WRITE | MAP_PREFAULT_PARTIAL);
		if (rv != KERN_SUCCESS) {
			vm_object_deallocate(object);
			return (rv);
		}

		off = offset - trunc_page(offset);
		error = copyout((caddr_t)data_buf + off, (caddr_t)start,
		    end - start);
		vm_map_remove(exec_map, data_buf, data_buf + PAGE_SIZE);
		if (error) {
			return (KERN_FAILURE);
		}
	}

	return (KERN_SUCCESS);
}

static int
__elfN(map_insert)(vm_map_t map, vm_object_t object, vm_ooffset_t offset,
	vm_offset_t start, vm_offset_t end, vm_prot_t prot,
	vm_prot_t max, int cow)
{
	vm_offset_t data_buf, off;
	vm_size_t sz;
	int error, rv;

	if (start != trunc_page(start)) {
		rv = __elfN(map_partial)(map, object, offset, start,
		    round_page(start), prot, max);
		if (rv)
			return (rv);
		offset += round_page(start) - start;
		start = round_page(start);
	}
	if (end != round_page(end)) {
		rv = __elfN(map_partial)(map, object, offset +
		    trunc_page(end) - start, trunc_page(end), end, prot, max);
		if (rv)
			return (rv);
		end = trunc_page(end);
	}
	if (end > start) {
		if (offset & PAGE_MASK) {
			/*
			 * The mapping is not page aligned. This means we have
			 * to copy the data. Sigh.
			 */
			rv = vm_map_find(map, 0, 0, &start, end - start,
			    FALSE, prot, max, 0);
			if (rv)
				return (rv);
			data_buf = 0;
			while (start < end) {
				vm_object_reference(object);
				rv = vm_map_find(exec_map,
						 object,
						 trunc_page(offset),
						 &data_buf,
						 2 * PAGE_SIZE,
						 TRUE,
						 VM_PROT_READ,
						 VM_PROT_ALL,
						 (MAP_COPY_ON_WRITE
						  | MAP_PREFAULT_PARTIAL));
				if (rv != KERN_SUCCESS) {
					vm_object_deallocate(object);
					return (rv);
				}
				off = offset - trunc_page(offset);
				sz = end - start;
				if (sz > PAGE_SIZE)
					sz = PAGE_SIZE;
				error = copyout((caddr_t)data_buf + off,
				    (caddr_t)start, sz);
				vm_map_remove(exec_map, data_buf,
				    data_buf + 2 * PAGE_SIZE);
				if (error) {
					return (KERN_FAILURE);
				}
				start += sz;
			}
			rv = KERN_SUCCESS;
		} else {
			vm_map_lock(map);
			rv = vm_map_insert(map, object, offset, start, end,
			    prot, max, cow);
			vm_map_unlock(map);
		}
		return (rv);
	} else {
		return (KERN_SUCCESS);
	}
}

static int
__elfN(load_section)(struct proc *p, struct vmspace *vmspace,
	struct vnode *vp, vm_object_t object, vm_offset_t offset,
	caddr_t vmaddr, size_t memsz, size_t filsz, vm_prot_t prot,
	size_t pagesize)
{
	size_t map_len;
	vm_offset_t map_addr;
	int error, rv, cow;
	size_t copy_len;
	vm_offset_t file_addr;
	vm_offset_t data_buf = 0;

	error = 0;

	/*
	 * It's necessary to fail if the filsz + offset taken from the
	 * header is greater than the actual file pager object's size.
	 * If we were to allow this, then the vm_map_find() below would
	 * walk right off the end of the file object and into the ether.
	 *
	 * While I'm here, might as well check for something else that
	 * is invalid: filsz cannot be greater than memsz.
	 */
	if ((off_t)filsz + offset > object->un_pager.vnp.vnp_size ||
	    filsz > memsz) {
		uprintf("elf_load_section: truncated ELF file\n");
		return (ENOEXEC);
	}

#define trunc_page_ps(va, ps)	((va) & ~(ps - 1))
#define round_page_ps(va, ps)	(((va) + (ps - 1)) & ~(ps - 1))

	map_addr = trunc_page_ps((vm_offset_t)vmaddr, pagesize);
	file_addr = trunc_page_ps(offset, pagesize);

	/*
	 * We have two choices.  We can either clear the data in the last page
	 * of an oversized mapping, or we can start the anon mapping a page
	 * early and copy the initialized data into that first page.  We
	 * choose the second..
	 */
	if (memsz > filsz)
		map_len = trunc_page_ps(offset + filsz, pagesize) - file_addr;
	else
		map_len = round_page_ps(offset + filsz, pagesize) - file_addr;

	if (map_len != 0) {
		vm_object_reference(object);

		/* cow flags: don't dump readonly sections in core */
		cow = MAP_COPY_ON_WRITE | MAP_PREFAULT |
		    (prot & VM_PROT_WRITE ? 0 : MAP_DISABLE_COREDUMP);

		rv = __elfN(map_insert)(&vmspace->vm_map,
				      object,
				      file_addr,	/* file offset */
				      map_addr,		/* virtual start */
				      map_addr + map_len,/* virtual end */
				      prot,
				      VM_PROT_ALL,
				      cow);
		if (rv != KERN_SUCCESS) {
			vm_object_deallocate(object);
			return (EINVAL);
		}

		/* we can stop now if we've covered it all */
		if (memsz == filsz) {
			return (0);
		}
	}


	/*
	 * We have to get the remaining bit of the file into the first part
	 * of the oversized map segment.  This is normally because the .data
	 * segment in the file is extended to provide bss.  It's a neat idea
	 * to try and save a page, but it's a pain in the behind to implement.
	 */
	copy_len = (offset + filsz) - trunc_page_ps(offset + filsz, pagesize);
	map_addr = trunc_page_ps((vm_offset_t)vmaddr + filsz, pagesize);
	map_len = round_page_ps((vm_offset_t)vmaddr + memsz, pagesize) -
	    map_addr;

	/* This had damn well better be true! */
	if (map_len != 0) {
		rv = __elfN(map_insert)(&vmspace->vm_map, NULL, 0, map_addr,
		    map_addr + map_len, VM_PROT_ALL, VM_PROT_ALL, 0);
		if (rv != KERN_SUCCESS) {
			return (EINVAL);
		}
	}

	if (copy_len != 0) {
		vm_offset_t off;
		vm_object_reference(object);
		rv = vm_map_find(exec_map,
				 object,
				 trunc_page(offset + filsz),
				 &data_buf,
				 PAGE_SIZE,
				 TRUE,
				 VM_PROT_READ,
				 VM_PROT_ALL,
				 MAP_COPY_ON_WRITE | MAP_PREFAULT_PARTIAL);
		if (rv != KERN_SUCCESS) {
			vm_object_deallocate(object);
			return (EINVAL);
		}

		/* send the page fragment to user space */
		off = trunc_page_ps(offset + filsz, pagesize) -
		    trunc_page(offset + filsz);
		error = copyout((caddr_t)data_buf + off, (caddr_t)map_addr,
		    copy_len);
		vm_map_remove(exec_map, data_buf, data_buf + PAGE_SIZE);
		if (error) {
			return (error);
		}
	}

	/*
	 * set it to the specified protection.
	 * XXX had better undo the damage from pasting over the cracks here!
	 */
	vm_map_protect(&vmspace->vm_map, trunc_page(map_addr),
	    round_page(map_addr + map_len),  prot, FALSE);

	return (error);
}

/*
 * Load the file "file" into memory.  It may be either a shared object
 * or an executable.
 *
 * The "addr" reference parameter is in/out.  On entry, it specifies
 * the address where a shared object should be loaded.  If the file is
 * an executable, this value is ignored.  On exit, "addr" specifies
 * where the file was actually loaded.
 *
 * The "entry" reference parameter is out only.  On exit, it specifies
 * the entry point for the loaded file.
 */
static int
__elfN(load_file)(struct proc *p, const char *file, u_long *addr,
	u_long *entry, size_t pagesize)
{
	struct {
		struct nameidata nd;
		struct vattr attr;
		struct image_params image_params;
	} *tempdata;
	const Elf_Ehdr *hdr = NULL;
	const Elf_Phdr *phdr = NULL;
	struct nameidata *nd;
	struct vmspace *vmspace = p->p_vmspace;
	struct vattr *attr;
	struct image_params *imgp;
	vm_prot_t prot;
	u_long rbase;
	u_long base_addr = 0;
	int error, i, numsegs;

	if (curthread->td_proc != p)
		panic("elf_load_file - thread");	/* XXXKSE DIAGNOSTIC */

	tempdata = malloc(sizeof(*tempdata), M_TEMP, M_WAITOK);
	nd = &tempdata->nd;
	attr = &tempdata->attr;
	imgp = &tempdata->image_params;

	/*
	 * Initialize part of the common data
	 */
	imgp->proc = p;
	imgp->attr = attr;
	imgp->firstpage = NULL;
	imgp->image_header = NULL;
	imgp->object = NULL;
	imgp->execlabel = NULL;

	/* XXXKSE */
	NDINIT(nd, LOOKUP, LOCKLEAF|FOLLOW, UIO_SYSSPACE, file, curthread);

	if ((error = namei(nd)) != 0) {
		nd->ni_vp = NULL;
		goto fail;
	}
	NDFREE(nd, NDF_ONLY_PNBUF);
	imgp->vp = nd->ni_vp;

	/*
	 * Check permissions, modes, uid, etc on the file, and "open" it.
	 */
	error = exec_check_permissions(imgp);
	if (error) {
		VOP_UNLOCK(nd->ni_vp, 0, curthread); /* XXXKSE */
		goto fail;
	}

	error = exec_map_first_page(imgp);
	/*
	 * Also make certain that the interpreter stays the same, so set
	 * its VV_TEXT flag, too.
	 */
	if (error == 0)
		nd->ni_vp->v_vflag |= VV_TEXT;

	imgp->object = nd->ni_vp->v_object;
	vm_object_reference(imgp->object);

	VOP_UNLOCK(nd->ni_vp, 0, curthread); /* XXXKSE */
	if (error)
		goto fail;

	hdr = (const Elf_Ehdr *)imgp->image_header;
	if ((error = __elfN(check_header)(hdr)) != 0)
		goto fail;
	if (hdr->e_type == ET_DYN)
		rbase = *addr;
	else if (hdr->e_type == ET_EXEC)
		rbase = 0;
	else {
		error = ENOEXEC;
		goto fail;
	}

	/* Only support headers that fit within first page for now      */
	/*    (multiplication of two Elf_Half fields will not overflow) */
	if ((hdr->e_phoff > PAGE_SIZE) ||
	    (hdr->e_phentsize * hdr->e_phnum) > PAGE_SIZE - hdr->e_phoff) {
		error = ENOEXEC;
		goto fail;
	}

	phdr = (const Elf_Phdr *)(imgp->image_header + hdr->e_phoff);

	for (i = 0, numsegs = 0; i < hdr->e_phnum; i++) {
		if (phdr[i].p_type == PT_LOAD) {	/* Loadable segment */
			prot = 0;
			if (phdr[i].p_flags & PF_X)
  				prot |= VM_PROT_EXECUTE;
			if (phdr[i].p_flags & PF_W)
  				prot |= VM_PROT_WRITE;
			if (phdr[i].p_flags & PF_R)
  				prot |= VM_PROT_READ;

			if ((error = __elfN(load_section)(p, vmspace,
			    nd->ni_vp, imgp->object, phdr[i].p_offset,
			    (caddr_t)(uintptr_t)phdr[i].p_vaddr + rbase,
			    phdr[i].p_memsz, phdr[i].p_filesz, prot,
			    pagesize)) != 0)
				goto fail;
			/*
			 * Establish the base address if this is the
			 * first segment.
			 */
			if (numsegs == 0)
  				base_addr = trunc_page(phdr[i].p_vaddr +
				    rbase);
			numsegs++;
		}
	}
	*addr = base_addr;
	*entry = (unsigned long)hdr->e_entry + rbase;

fail:
	if (imgp->firstpage)
		exec_unmap_first_page(imgp);
	if (imgp->object)
		vm_object_deallocate(imgp->object);

	if (nd->ni_vp)
		vrele(nd->ni_vp);

	free(tempdata, M_TEMP);

	return (error);
}

static int
__CONCAT(exec_, __elfN(imgact))(struct image_params *imgp)
{
	const Elf_Ehdr *hdr = (const Elf_Ehdr *)imgp->image_header;
	const Elf_Phdr *phdr;
	Elf_Auxargs *elf_auxargs = NULL;
	struct vmspace *vmspace;
	vm_prot_t prot;
	u_long text_size = 0, data_size = 0, total_size = 0;
	u_long text_addr = 0, data_addr = 0;
	u_long seg_size, seg_addr;
	u_long addr, entry = 0, proghdr = 0;
	int error = 0, i;
	const char *interp = NULL;
	Elf_Brandinfo *brand_info;
	char *path;
	struct thread *td = curthread;
	struct sysentvec *sv;

	/*
	 * Do we have a valid ELF header ?
	 */
	if (__elfN(check_header)(hdr) != 0 || hdr->e_type != ET_EXEC)
		return (-1);

	/*
	 * From here on down, we return an errno, not -1, as we've
	 * detected an ELF file.
	 */

	if ((hdr->e_phoff > PAGE_SIZE) ||
	    (hdr->e_phoff + hdr->e_phentsize * hdr->e_phnum) > PAGE_SIZE) {
		/* Only support headers in first page for now */
		return (ENOEXEC);
	}
	phdr = (const Elf_Phdr *)(imgp->image_header + hdr->e_phoff);

	/*
	 * From this point on, we may have resources that need to be freed.
	 */

	VOP_UNLOCK(imgp->vp, 0, td);

	for (i = 0; i < hdr->e_phnum; i++) {
		switch (phdr[i].p_type) {
	  	case PT_INTERP:	/* Path to interpreter */
			if (phdr[i].p_filesz > MAXPATHLEN ||
			    phdr[i].p_offset + phdr[i].p_filesz > PAGE_SIZE) {
				error = ENOEXEC;
				goto fail;
			}
			interp = imgp->image_header + phdr[i].p_offset;
			break;
		default:
			break;
		}
	}

	brand_info = __elfN(get_brandinfo)(hdr, interp);
	if (brand_info == NULL) {
		uprintf("ELF binary type \"%u\" not known.\n",
		    hdr->e_ident[EI_OSABI]);
		error = ENOEXEC;
		goto fail;
	}
	sv = brand_info->sysvec;
	if (interp != NULL && brand_info->interp_newpath != NULL)
		interp = brand_info->interp_newpath;

	exec_new_vmspace(imgp, sv);

	vmspace = imgp->proc->p_vmspace;

	for (i = 0; i < hdr->e_phnum; i++) {
		switch (phdr[i].p_type) {
		case PT_LOAD:	/* Loadable segment */
			prot = 0;
			if (phdr[i].p_flags & PF_X)
  				prot |= VM_PROT_EXECUTE;
			if (phdr[i].p_flags & PF_W)
  				prot |= VM_PROT_WRITE;
			if (phdr[i].p_flags & PF_R)
  				prot |= VM_PROT_READ;

#if defined(__ia64__) && __ELF_WORD_SIZE == 32 && defined(IA32_ME_HARDER)
			/*
			 * Some x86 binaries assume read == executable,
			 * notably the M3 runtime and therefore cvsup
			 */
			if (prot & VM_PROT_READ)
				prot |= VM_PROT_EXECUTE;
#endif

			if ((error = __elfN(load_section)(imgp->proc, vmspace,
			    imgp->vp, imgp->object, phdr[i].p_offset,
			    (caddr_t)(uintptr_t)phdr[i].p_vaddr,
			    phdr[i].p_memsz, phdr[i].p_filesz, prot,
			    sv->sv_pagesize)) != 0)
  				goto fail;

			/*
			 * If this segment contains the program headers,
			 * remember their virtual address for the AT_PHDR
			 * aux entry. Static binaries don't usually include
			 * a PT_PHDR entry.
			 */
			if (phdr[i].p_offset == 0 &&
			    hdr->e_phoff + hdr->e_phnum * hdr->e_phentsize
				<= phdr[i].p_filesz)
				proghdr = phdr[i].p_vaddr + hdr->e_phoff;

			seg_addr = trunc_page(phdr[i].p_vaddr);
			seg_size = round_page(phdr[i].p_memsz +
			    phdr[i].p_vaddr - seg_addr);

			/*
			 * Is this .text or .data?  We can't use
			 * VM_PROT_WRITE or VM_PROT_EXEC, it breaks the
			 * alpha terribly and possibly does other bad
			 * things so we stick to the old way of figuring
			 * it out:  If the segment contains the program
			 * entry point, it's a text segment, otherwise it
			 * is a data segment.
			 *
			 * Note that obreak() assumes that data_addr + 
			 * data_size == end of data load area, and the ELF
			 * file format expects segments to be sorted by
			 * address.  If multiple data segments exist, the
			 * last one will be used.
			 */
			if (hdr->e_entry >= phdr[i].p_vaddr &&
			    hdr->e_entry < (phdr[i].p_vaddr +
			    phdr[i].p_memsz)) {
				text_size = seg_size;
				text_addr = seg_addr;
				entry = (u_long)hdr->e_entry;
			} else {
				data_size = seg_size;
				data_addr = seg_addr;
			}
			total_size += seg_size;
			break;
		case PT_PHDR: 	/* Program header table info */
			proghdr = phdr[i].p_vaddr;
			break;
		default:
			break;
		}
	}
	
	if (data_addr == 0 && data_size == 0) {
		data_addr = text_addr;
		data_size = text_size;
	}

	/*
	 * Check limits.  It should be safe to check the
	 * limits after loading the segments since we do
	 * not actually fault in all the segments pages.
	 */
	PROC_LOCK(imgp->proc);
	if (data_size > lim_cur(imgp->proc, RLIMIT_DATA) ||
	    text_size > maxtsiz ||
	    total_size > lim_cur(imgp->proc, RLIMIT_VMEM)) {
		PROC_UNLOCK(imgp->proc);
		error = ENOMEM;
		goto fail;
	}

	vmspace->vm_tsize = text_size >> PAGE_SHIFT;
	vmspace->vm_taddr = (caddr_t)(uintptr_t)text_addr;
	vmspace->vm_dsize = data_size >> PAGE_SHIFT;
	vmspace->vm_daddr = (caddr_t)(uintptr_t)data_addr;

	/*
	 * We load the dynamic linker where a userland call
	 * to mmap(0, ...) would put it.  The rationale behind this
	 * calculation is that it leaves room for the heap to grow to
	 * its maximum allowed size.
	 */
	addr = round_page((vm_offset_t)imgp->proc->p_vmspace->vm_daddr +
	    lim_max(imgp->proc, RLIMIT_DATA));
	PROC_UNLOCK(imgp->proc);

	imgp->entry_addr = entry;

	imgp->proc->p_sysent = sv;
	if (interp != NULL && brand_info->emul_path != NULL &&
	    brand_info->emul_path[0] != '\0') {
		path = malloc(MAXPATHLEN, M_TEMP, M_WAITOK);
		snprintf(path, MAXPATHLEN, "%s%s", brand_info->emul_path,
		    interp);
		error = __elfN(load_file)(imgp->proc, path, &addr,
		    &imgp->entry_addr, sv->sv_pagesize);
		free(path, M_TEMP);
		if (error == 0)
			interp = NULL;
	}
	if (interp != NULL) {
		error = __elfN(load_file)(imgp->proc, interp, &addr,
		    &imgp->entry_addr, sv->sv_pagesize);
		if (error != 0) {
			uprintf("ELF interpreter %s not found\n", interp);
			goto fail;
		}
	}

	/*
	 * Construct auxargs table (used by the fixup routine)
	 */
	elf_auxargs = malloc(sizeof(Elf_Auxargs), M_TEMP, M_WAITOK);
	elf_auxargs->execfd = -1;
	elf_auxargs->phdr = proghdr;
	elf_auxargs->phent = hdr->e_phentsize;
	elf_auxargs->phnum = hdr->e_phnum;
	elf_auxargs->pagesz = PAGE_SIZE;
	elf_auxargs->base = addr;
	elf_auxargs->flags = 0;
	elf_auxargs->entry = entry;
	elf_auxargs->trace = elf_trace;

	imgp->auxargs = elf_auxargs;
	imgp->interpreted = 0;

fail:
	vn_lock(imgp->vp, LK_EXCLUSIVE | LK_RETRY, td);
	return (error);
}

#define	suword __CONCAT(suword, __ELF_WORD_SIZE)

int
__elfN(freebsd_fixup)(register_t **stack_base, struct image_params *imgp)
{
	Elf_Auxargs *args = (Elf_Auxargs *)imgp->auxargs;
	Elf_Addr *base;
	Elf_Addr *pos;

	base = (Elf_Addr *)*stack_base;
	pos = base + (imgp->args->argc + imgp->args->envc + 2);

	if (args->trace) {
		AUXARGS_ENTRY(pos, AT_DEBUG, 1);
	}
	if (args->execfd != -1) {
		AUXARGS_ENTRY(pos, AT_EXECFD, args->execfd);
	}
	AUXARGS_ENTRY(pos, AT_PHDR, args->phdr);
	AUXARGS_ENTRY(pos, AT_PHENT, args->phent);
	AUXARGS_ENTRY(pos, AT_PHNUM, args->phnum);
	AUXARGS_ENTRY(pos, AT_PAGESZ, args->pagesz);
	AUXARGS_ENTRY(pos, AT_FLAGS, args->flags);
	AUXARGS_ENTRY(pos, AT_ENTRY, args->entry);
	AUXARGS_ENTRY(pos, AT_BASE, args->base);
	AUXARGS_ENTRY(pos, AT_NULL, 0);

	free(imgp->auxargs, M_TEMP);
	imgp->auxargs = NULL;

	base--;
	suword(base, (long)imgp->args->argc);
	*stack_base = (register_t *)base;
	return (0);
}

/*
 * Code for generating ELF core dumps.
 */

typedef void (*segment_callback)(vm_map_entry_t, void *);

/* Closure for cb_put_phdr(). */
struct phdr_closure {
	Elf_Phdr *phdr;		/* Program header to fill in */
	Elf_Off offset;		/* Offset of segment in core file */
};

/* Closure for cb_size_segment(). */
struct sseg_closure {
	int count;		/* Count of writable segments. */
	size_t size;		/* Total size of all writable segments. */
};

static void cb_put_phdr(vm_map_entry_t, void *);
static void cb_size_segment(vm_map_entry_t, void *);
static void each_writable_segment(struct thread *, segment_callback, void *);
static int __elfN(corehdr)(struct thread *, struct vnode *, struct ucred *,
    int, void *, size_t);
static void __elfN(puthdr)(struct thread *, void *, size_t *, int);
static void __elfN(putnote)(void *, size_t *, const char *, int,
    const void *, size_t);

extern int osreldate;

int
__elfN(coredump)(td, vp, limit)
	struct thread *td;
	struct vnode *vp;
	off_t limit;
{
	struct ucred *cred = td->td_ucred;
	int error = 0;
	struct sseg_closure seginfo;
	void *hdr;
	size_t hdrsize;

	/* Size the program segments. */
	seginfo.count = 0;
	seginfo.size = 0;
	each_writable_segment(td, cb_size_segment, &seginfo);

	/*
	 * Calculate the size of the core file header area by making
	 * a dry run of generating it.  Nothing is written, but the
	 * size is calculated.
	 */
	hdrsize = 0;
	__elfN(puthdr)(td, (void *)NULL, &hdrsize, seginfo.count);

	if (hdrsize + seginfo.size >= limit)
		return (EFAULT);

	/*
	 * Allocate memory for building the header, fill it up,
	 * and write it out.
	 */
	hdr = malloc(hdrsize, M_TEMP, M_WAITOK);
	if (hdr == NULL) {
		return (EINVAL);
	}
	error = __elfN(corehdr)(td, vp, cred, seginfo.count, hdr, hdrsize);

	/* Write the contents of all of the writable segments. */
	if (error == 0) {
		Elf_Phdr *php;
		off_t offset;
		int i;

		php = (Elf_Phdr *)((char *)hdr + sizeof(Elf_Ehdr)) + 1;
		offset = hdrsize;
		for (i = 0; i < seginfo.count; i++) {
			error = vn_rdwr_inchunks(UIO_WRITE, vp,
			    (caddr_t)(uintptr_t)php->p_vaddr,
			    php->p_filesz, offset, UIO_USERSPACE,
			    IO_UNIT | IO_DIRECT, cred, NOCRED, NULL,
			    curthread); /* XXXKSE */
			if (error != 0)
				break;
			offset += php->p_filesz;
			php++;
		}
	}
	free(hdr, M_TEMP);

	return (error);
}

/*
 * A callback for each_writable_segment() to write out the segment's
 * program header entry.
 */
static void
cb_put_phdr(entry, closure)
	vm_map_entry_t entry;
	void *closure;
{
	struct phdr_closure *phc = (struct phdr_closure *)closure;
	Elf_Phdr *phdr = phc->phdr;

	phc->offset = round_page(phc->offset);

	phdr->p_type = PT_LOAD;
	phdr->p_offset = phc->offset;
	phdr->p_vaddr = entry->start;
	phdr->p_paddr = 0;
	phdr->p_filesz = phdr->p_memsz = entry->end - entry->start;
	phdr->p_align = PAGE_SIZE;
	phdr->p_flags = 0;
	if (entry->protection & VM_PROT_READ)
		phdr->p_flags |= PF_R;
	if (entry->protection & VM_PROT_WRITE)
		phdr->p_flags |= PF_W;
	if (entry->protection & VM_PROT_EXECUTE)
		phdr->p_flags |= PF_X;

	phc->offset += phdr->p_filesz;
	phc->phdr++;
}

/*
 * A callback for each_writable_segment() to gather information about
 * the number of segments and their total size.
 */
static void
cb_size_segment(entry, closure)
	vm_map_entry_t entry;
	void *closure;
{
	struct sseg_closure *ssc = (struct sseg_closure *)closure;

	ssc->count++;
	ssc->size += entry->end - entry->start;
}

/*
 * For each writable segment in the process's memory map, call the given
 * function with a pointer to the map entry and some arbitrary
 * caller-supplied data.
 */
static void
each_writable_segment(td, func, closure)
	struct thread *td;
	segment_callback func;
	void *closure;
{
	struct proc *p = td->td_proc;
	vm_map_t map = &p->p_vmspace->vm_map;
	vm_map_entry_t entry;

	for (entry = map->header.next; entry != &map->header;
	    entry = entry->next) {
		vm_object_t obj;

		/*
		 * Don't dump inaccessible mappings, deal with legacy
		 * coredump mode.
		 *
		 * Note that read-only segments related to the elf binary
		 * are marked MAP_ENTRY_NOCOREDUMP now so we no longer
		 * need to arbitrarily ignore such segments.
		 */
		if (elf_legacy_coredump) {
			if ((entry->protection & VM_PROT_RW) != VM_PROT_RW)
				continue;
		} else {
			if ((entry->protection & VM_PROT_ALL) == 0)
				continue;
		}

		/*
		 * Dont include memory segment in the coredump if
		 * MAP_NOCORE is set in mmap(2) or MADV_NOCORE in
		 * madvise(2).  Do not dump submaps (i.e. parts of the
		 * kernel map).
		 */
		if (entry->eflags & (MAP_ENTRY_NOCOREDUMP|MAP_ENTRY_IS_SUB_MAP))
			continue;

		if ((obj = entry->object.vm_object) == NULL)
			continue;

		/* Find the deepest backing object. */
		while (obj->backing_object != NULL)
			obj = obj->backing_object;

		/* Ignore memory-mapped devices and such things. */
		if (obj->type != OBJT_DEFAULT &&
		    obj->type != OBJT_SWAP &&
		    obj->type != OBJT_VNODE)
			continue;

		(*func)(entry, closure);
	}
}

/*
 * Write the core file header to the file, including padding up to
 * the page boundary.
 */
static int
__elfN(corehdr)(td, vp, cred, numsegs, hdr, hdrsize)
	struct thread *td;
	struct vnode *vp;
	struct ucred *cred;
	int numsegs;
	size_t hdrsize;
	void *hdr;
{
	size_t off;

	/* Fill in the header. */
	bzero(hdr, hdrsize);
	off = 0;
	__elfN(puthdr)(td, hdr, &off, numsegs);

	/* Write it to the core file. */
	return (vn_rdwr_inchunks(UIO_WRITE, vp, hdr, hdrsize, (off_t)0,
	    UIO_SYSSPACE, IO_UNIT | IO_DIRECT, cred, NOCRED, NULL,
	    td)); /* XXXKSE */
}

#if defined(COMPAT_IA32) && __ELF_WORD_SIZE == 32
typedef struct prstatus32 elf_prstatus_t;
typedef struct prpsinfo32 elf_prpsinfo_t;
typedef struct fpreg32 elf_prfpregset_t;
typedef struct fpreg32 elf_fpregset_t;
typedef struct reg32 elf_gregset_t;
#else
typedef prstatus_t elf_prstatus_t;
typedef prpsinfo_t elf_prpsinfo_t;
typedef prfpregset_t elf_prfpregset_t;
typedef prfpregset_t elf_fpregset_t;
typedef gregset_t elf_gregset_t;
#endif

static void
__elfN(puthdr)(struct thread *td, void *dst, size_t *off, int numsegs)
{
	struct {
		elf_prstatus_t status;
		elf_prfpregset_t fpregset;
		elf_prpsinfo_t psinfo;
	} *tempdata;
	elf_prstatus_t *status;
	elf_prfpregset_t *fpregset;
	elf_prpsinfo_t *psinfo;
	struct proc *p;
	struct thread *thr;
	size_t ehoff, noteoff, notesz, phoff;

	p = td->td_proc;

	ehoff = *off;
	*off += sizeof(Elf_Ehdr);

	phoff = *off;
	*off += (numsegs + 1) * sizeof(Elf_Phdr);

	noteoff = *off;
	/*
	 * Don't allocate space for the notes if we're just calculating
	 * the size of the header. We also don't collect the data.
	 */
	if (dst != NULL) {
		tempdata = malloc(sizeof(*tempdata), M_TEMP, M_ZERO|M_WAITOK);
		status = &tempdata->status;
		fpregset = &tempdata->fpregset;
		psinfo = &tempdata->psinfo;
	} else {
		tempdata = NULL;
		status = NULL;
		fpregset = NULL;
		psinfo = NULL;
	}

	if (dst != NULL) {
		psinfo->pr_version = PRPSINFO_VERSION;
		psinfo->pr_psinfosz = sizeof(elf_prpsinfo_t);
		strlcpy(psinfo->pr_fname, p->p_comm, sizeof(psinfo->pr_fname));
		/*
		 * XXX - We don't fill in the command line arguments properly
		 * yet.
		 */
		strlcpy(psinfo->pr_psargs, p->p_comm,
		    sizeof(psinfo->pr_psargs));
	}
	__elfN(putnote)(dst, off, "FreeBSD", NT_PRPSINFO, psinfo,
	    sizeof *psinfo);

	/*
	 * To have the debugger select the right thread (LWP) as the initial
	 * thread, we dump the state of the thread passed to us in td first.
	 * This is the thread that causes the core dump and thus likely to
	 * be the right thread one wants to have selected in the debugger.
	 */
	thr = td;
	while (thr != NULL) {
		if (dst != NULL) {
			status->pr_version = PRSTATUS_VERSION;
			status->pr_statussz = sizeof(elf_prstatus_t);
			status->pr_gregsetsz = sizeof(elf_gregset_t);
			status->pr_fpregsetsz = sizeof(elf_fpregset_t);
			status->pr_osreldate = osreldate;
			status->pr_cursig = p->p_sig;
			status->pr_pid = thr->td_tid;
#if defined(COMPAT_IA32) && __ELF_WORD_SIZE == 32
			fill_regs32(thr, &status->pr_reg);
			fill_fpregs32(thr, fpregset);
#else
			fill_regs(thr, &status->pr_reg);
			fill_fpregs(thr, fpregset);
#endif
		}
		__elfN(putnote)(dst, off, "FreeBSD", NT_PRSTATUS, status,
		    sizeof *status);
		__elfN(putnote)(dst, off, "FreeBSD", NT_FPREGSET, fpregset,
		    sizeof *fpregset);
		/*
		 * Allow for MD specific notes, as well as any MD
		 * specific preparations for writing MI notes.
		 */
		__elfN(dump_thread)(thr, dst, off);

		thr = (thr == td) ? TAILQ_FIRST(&p->p_threads) :
		    TAILQ_NEXT(thr, td_plist);
		if (thr == td)
			thr = TAILQ_NEXT(thr, td_plist);
	}

	notesz = *off - noteoff;

	if (dst != NULL)
		free(tempdata, M_TEMP);

	/* Align up to a page boundary for the program segments. */
	*off = round_page(*off);

	if (dst != NULL) {
		Elf_Ehdr *ehdr;
		Elf_Phdr *phdr;
		struct phdr_closure phc;

		/*
		 * Fill in the ELF header.
		 */
		ehdr = (Elf_Ehdr *)((char *)dst + ehoff);
		ehdr->e_ident[EI_MAG0] = ELFMAG0;
		ehdr->e_ident[EI_MAG1] = ELFMAG1;
		ehdr->e_ident[EI_MAG2] = ELFMAG2;
		ehdr->e_ident[EI_MAG3] = ELFMAG3;
		ehdr->e_ident[EI_CLASS] = ELF_CLASS;
		ehdr->e_ident[EI_DATA] = ELF_DATA;
		ehdr->e_ident[EI_VERSION] = EV_CURRENT;
		ehdr->e_ident[EI_OSABI] = ELFOSABI_FREEBSD;
		ehdr->e_ident[EI_ABIVERSION] = 0;
		ehdr->e_ident[EI_PAD] = 0;
		ehdr->e_type = ET_CORE;
#if defined(COMPAT_IA32) && __ELF_WORD_SIZE == 32
		ehdr->e_machine = EM_386;
#else
		ehdr->e_machine = ELF_ARCH;
#endif
		ehdr->e_version = EV_CURRENT;
		ehdr->e_entry = 0;
		ehdr->e_phoff = phoff;
		ehdr->e_flags = 0;
		ehdr->e_ehsize = sizeof(Elf_Ehdr);
		ehdr->e_phentsize = sizeof(Elf_Phdr);
		ehdr->e_phnum = numsegs + 1;
		ehdr->e_shentsize = sizeof(Elf_Shdr);
		ehdr->e_shnum = 0;
		ehdr->e_shstrndx = SHN_UNDEF;

		/*
		 * Fill in the program header entries.
		 */
		phdr = (Elf_Phdr *)((char *)dst + phoff);

		/* The note segement. */
		phdr->p_type = PT_NOTE;
		phdr->p_offset = noteoff;
		phdr->p_vaddr = 0;
		phdr->p_paddr = 0;
		phdr->p_filesz = notesz;
		phdr->p_memsz = 0;
		phdr->p_flags = 0;
		phdr->p_align = 0;
		phdr++;

		/* All the writable segments from the program. */
		phc.phdr = phdr;
		phc.offset = *off;
		each_writable_segment(td, cb_put_phdr, &phc);
	}
}

static void
__elfN(putnote)(void *dst, size_t *off, const char *name, int type,
    const void *desc, size_t descsz)
{
	Elf_Note note;

	note.n_namesz = strlen(name) + 1;
	note.n_descsz = descsz;
	note.n_type = type;
	if (dst != NULL)
		bcopy(&note, (char *)dst + *off, sizeof note);
	*off += sizeof note;
	if (dst != NULL)
		bcopy(name, (char *)dst + *off, note.n_namesz);
	*off += roundup2(note.n_namesz, sizeof(Elf_Size));
	if (dst != NULL)
		bcopy(desc, (char *)dst + *off, note.n_descsz);
	*off += roundup2(note.n_descsz, sizeof(Elf_Size));
}

/*
 * Tell kern_execve.c about it, with a little help from the linker.
 */
static struct execsw __elfN(execsw) = {
	__CONCAT(exec_, __elfN(imgact)),
	__XSTRING(__CONCAT(ELF, __ELF_WORD_SIZE))
};
EXEC_SET(__CONCAT(elf, __ELF_WORD_SIZE), __elfN(execsw));
