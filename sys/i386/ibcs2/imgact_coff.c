/*-
 * Copyright (c) 1994 Sean Eric Fagan
 * Copyright (c) 1994 Søren Schmidt
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
 *
 * $FreeBSD$
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/fcntl.h>
#include <sys/imgact.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mman.h>
#include <sys/mount.h>
#include <sys/namei.h>
#include <sys/vnode.h>

#include <vm/vm.h>
#include <vm/pmap.h>
#include <vm/vm_map.h>
#include <vm/vm_kern.h>
#include <vm/vm_extern.h>

#include <i386/ibcs2/coff.h>
#include <i386/ibcs2/ibcs2_util.h>

MODULE_DEPEND(coff, ibcs2, 1, 1, 1);

extern struct sysentvec ibcs2_svr3_sysvec;

static int coff_load_file(struct thread *td, char *name);
static int exec_coff_imgact(struct image_params *imgp);

static int load_coff_section(struct vmspace *vmspace, struct vnode *vp, vm_offset_t offset, caddr_t vmaddr, size_t memsz, size_t filsz, vm_prot_t prot);

static int
load_coff_section(struct vmspace *vmspace, struct vnode *vp, vm_offset_t offset,
		  caddr_t vmaddr, size_t memsz, size_t filsz, vm_prot_t prot)
{
	size_t map_len;
	vm_offset_t map_offset;
	vm_offset_t map_addr;
	int error;
	unsigned char *data_buf = 0;
	size_t copy_len;

	map_offset = trunc_page(offset);
	map_addr = trunc_page((vm_offset_t)vmaddr);

	if (memsz > filsz) {
		/*
		 * We have the stupid situation that
		 * the section is longer than it is on file,
		 * which means it has zero-filled areas, and
		 * we have to work for it.  Stupid iBCS!
		 */
		map_len = trunc_page(offset + filsz) - trunc_page(map_offset);
	} else {
		/*
		 * The only stuff we care about is on disk, and we
		 * don't care if we map in more than is really there.
		 */
		map_len = round_page(offset + filsz) - trunc_page(map_offset);
	}

	DPRINTF(("%s(%d):  vm_mmap(&vmspace->vm_map, &0x%08lx, 0x%x, 0x%x, "
		"VM_PROT_ALL, MAP_PRIVATE | MAP_FIXED, vp, 0x%x)\n",
		__FILE__, __LINE__, map_addr, map_len, prot, map_offset));

	if ((error = vm_mmap(&vmspace->vm_map,
			     &map_addr,
			     map_len,
			     prot,
			     VM_PROT_ALL,
			     MAP_PRIVATE | MAP_FIXED,
			     (caddr_t) vp,
			     map_offset)) != 0)
		return error;

	if (memsz == filsz) {
		/* We're done! */
		return 0;
	}

	/*
	 * Now we have screwball stuff, to accomodate stupid COFF.
	 * We have to map the remaining bit of the file into the kernel's
	 * memory map, allocate some anonymous memory, copy that last
	 * bit into it, and then we're done. *sigh*
	 * For clean-up reasons, we actally map in the file last.
	 */

	copy_len = (offset + filsz) - trunc_page(offset + filsz);
	map_addr = trunc_page((vm_offset_t)vmaddr + filsz);
	map_len = round_page((vm_offset_t)vmaddr + memsz) - map_addr;

	DPRINTF(("%s(%d): vm_map_find(&vmspace->vm_map, NULL, 0, &0x%08lx,0x%x, FALSE, VM_PROT_ALL, VM_PROT_ALL, 0)\n", __FILE__, __LINE__, map_addr, map_len));

	if (map_len != 0) {
		error = vm_map_find(&vmspace->vm_map, NULL, 0, &map_addr,
				    map_len, FALSE, VM_PROT_ALL, VM_PROT_ALL, 0);
		if (error)
			return error;
	}

	if ((error = vm_mmap(kernel_map,
			    (vm_offset_t *) &data_buf,
			    PAGE_SIZE,
			    VM_PROT_READ,
			    VM_PROT_READ,
			    0,
			    (caddr_t) vp,
			    trunc_page(offset + filsz))) != 0)
		return error;

	error = copyout(data_buf, (caddr_t) map_addr, copy_len);

	if (vm_map_remove(kernel_map,
			  (vm_offset_t) data_buf,
			  (vm_offset_t) data_buf + PAGE_SIZE))
		panic("load_coff_section vm_map_remove failed");

	return error;
}

static int
coff_load_file(struct thread *td, char *name)
{
	struct proc *p = td->td_proc;
  	struct vmspace *vmspace = p->p_vmspace;
  	int error;
  	struct nameidata nd;
  	struct vnode *vp;
  	struct vattr attr;
  	struct filehdr *fhdr;
  	struct aouthdr *ahdr;
  	struct scnhdr *scns;
  	char *ptr = 0;
  	int nscns;
  	unsigned long text_offset = 0, text_address = 0, text_size = 0;
  	unsigned long data_offset = 0, data_address = 0, data_size = 0;
  	unsigned long bss_size = 0;
  	int i;

	NDINIT(&nd, LOOKUP, LOCKLEAF | FOLLOW | SAVENAME, UIO_SYSSPACE, name, td);

  	error = namei(&nd);
  	if (error)
    		return error;

  	vp = nd.ni_vp;
  	if (vp == NULL)
    		return ENOEXEC;

  	if (vp->v_writecount) {
    		error = ETXTBSY;
    		goto fail;
  	}

  	if ((error = VOP_GETATTR(vp, &attr, td->td_ucred, td)) != 0)
    		goto fail;

  	if ((vp->v_mount->mnt_flag & MNT_NOEXEC)
	    || ((attr.va_mode & 0111) == 0)
	    || (attr.va_type != VREG))
    		goto fail;

  	if (attr.va_size == 0) {
    		error = ENOEXEC;
    		goto fail;
  	}

  	if ((error = VOP_ACCESS(vp, VEXEC, td->td_ucred, td)) != 0)
    		goto fail;

  	if ((error = VOP_OPEN(vp, FREAD, td->td_ucred, td)) != 0)
    		goto fail;

	/*
	 * Lose the lock on the vnode. It's no longer needed, and must not
	 * exist for the pagefault paging to work below.
	 */
	VOP_UNLOCK(vp, 0, td);

  	if ((error = vm_mmap(kernel_map,
			    (vm_offset_t *) &ptr,
			    PAGE_SIZE,
			    VM_PROT_READ,
		       	    VM_PROT_READ,
			    0,
			    (caddr_t) vp,
			    0)) != 0)
		goto unlocked_fail;

  	fhdr = (struct filehdr *)ptr;

  	if (fhdr->f_magic != I386_COFF) {
    		error = ENOEXEC;
    		goto dealloc_and_fail;
  	}

  	nscns = fhdr->f_nscns;

  	if ((nscns * sizeof(struct scnhdr)) > PAGE_SIZE) {
    		/*
     		 * XXX -- just fail.  I'm so lazy.
     		 */
    		error = ENOEXEC;
    		goto dealloc_and_fail;
  	}

  	ahdr = (struct aouthdr*)(ptr + sizeof(struct filehdr));

  	scns = (struct scnhdr*)(ptr + sizeof(struct filehdr)
			  + sizeof(struct aouthdr));

  	for (i = 0; i < nscns; i++) {
    		if (scns[i].s_flags & STYP_NOLOAD)
      			continue;
    		else if (scns[i].s_flags & STYP_TEXT) {
      			text_address = scns[i].s_vaddr;
      			text_size = scns[i].s_size;
      			text_offset = scns[i].s_scnptr;
    		}
		else if (scns[i].s_flags & STYP_DATA) {
      			data_address = scns[i].s_vaddr;
      			data_size = scns[i].s_size;
      			data_offset = scns[i].s_scnptr;
    		} else if (scns[i].s_flags & STYP_BSS) {
      			bss_size = scns[i].s_size;
    		}
  	}

  	if ((error = load_coff_section(vmspace, vp, text_offset,
				      (caddr_t)(void *)(uintptr_t)text_address,
				      text_size, text_size,
				      VM_PROT_READ | VM_PROT_EXECUTE)) != 0) {
    		goto dealloc_and_fail;
  	}
  	if ((error = load_coff_section(vmspace, vp, data_offset,
				      (caddr_t)(void *)(uintptr_t)data_address,
				      data_size + bss_size, data_size,
				      VM_PROT_ALL)) != 0) {
    		goto dealloc_and_fail;
  	}

  	error = 0;

 dealloc_and_fail:
	if (vm_map_remove(kernel_map,
			  (vm_offset_t) ptr,
			  (vm_offset_t) ptr + PAGE_SIZE))
    		panic("%s vm_map_remove failed", __func__);

 fail:
	VOP_UNLOCK(vp, 0, td);
 unlocked_fail:
	NDFREE(&nd, NDF_ONLY_PNBUF);
	vrele(nd.ni_vp);
  	return error;
}

static int
exec_coff_imgact(imgp)
	struct image_params *imgp;
{
	const struct filehdr *fhdr = (const struct filehdr*)imgp->image_header;
	const struct aouthdr *ahdr;
	const struct scnhdr *scns;
	int i;
	struct vmspace *vmspace;
	int nscns;
	int error;
	unsigned long text_offset = 0, text_address = 0, text_size = 0;
	unsigned long data_offset = 0, data_address = 0, data_size = 0;
	unsigned long bss_size = 0;
	caddr_t hole;
	struct thread *td = curthread;

	if (fhdr->f_magic != I386_COFF ||
	    !(fhdr->f_flags & F_EXEC)) {

		 DPRINTF(("%s(%d): return -1\n", __FILE__, __LINE__));
		 return -1;
	}

	nscns = fhdr->f_nscns;
	if ((nscns * sizeof(struct scnhdr)) > PAGE_SIZE) {
	  	/*
	   	 * For now, return an error -- need to be able to
	   	 * read in all of the section structures.
	   	 */

		DPRINTF(("%s(%d): return -1\n", __FILE__, __LINE__));
		return -1;
	}

	ahdr = (const struct aouthdr*)
	       ((const char*)(imgp->image_header) + sizeof(struct filehdr));
	imgp->entry_addr = ahdr->entry;

	scns = (const struct scnhdr*)
	       ((const char*)(imgp->image_header) + sizeof(struct filehdr) +
		sizeof(struct aouthdr));

	VOP_UNLOCK(imgp->vp, 0, td);

	if ((error = exec_extract_strings(imgp)) != 0) {
		DPRINTF(("%s(%d):  return %d\n", __FILE__, __LINE__, error));
		goto fail;
	}

	exec_new_vmspace(imgp, VM_MIN_ADDRESS, VM_MAXUSER_ADDRESS, USRSTACK);
	vmspace = imgp->proc->p_vmspace;

	for (i = 0; i < nscns; i++) {

	  DPRINTF(("i = %d, scns[i].s_name = %s, scns[i].s_vaddr = %08lx, "
		   "scns[i].s_scnptr = %d\n", i, scns[i].s_name,
		   scns[i].s_vaddr, scns[i].s_scnptr));
	  if (scns[i].s_flags & STYP_NOLOAD) {
	    	/*
	     	 * A section that is not loaded, for whatever
	     	 * reason.  It takes precedance over other flag
	     	 * bits...
	     	 */
	    	continue;
	  } else if (scns[i].s_flags & STYP_TEXT) {
	    	text_address = scns[i].s_vaddr;
	    	text_size = scns[i].s_size;
	    	text_offset = scns[i].s_scnptr;
	  } else if (scns[i].s_flags & STYP_DATA) {
	    	/* .data section */
	    	data_address = scns[i].s_vaddr;
	    	data_size = scns[i].s_size;
	    	data_offset = scns[i].s_scnptr;
	  } else if (scns[i].s_flags & STYP_BSS) {
	    	/* .bss section */
	    	bss_size = scns[i].s_size;
	  } else if (scns[i].s_flags & STYP_LIB) {
	    	char *buf = 0;
	    	int foff = trunc_page(scns[i].s_scnptr);
	    	int off = scns[i].s_scnptr - foff;
	    	int len = round_page(scns[i].s_size + PAGE_SIZE);
	    	int j;

	    	if ((error = vm_mmap(kernel_map,
				    (vm_offset_t *) &buf,
				    len,
				    VM_PROT_READ,
				    VM_PROT_READ,
				    0,
				    (caddr_t) imgp->vp,
				    foff)) != 0) {
	      		error = ENOEXEC;
			goto fail;
	    	}
		if(scns[i].s_size) {
			char *libbuf;
			int emul_path_len = strlen(ibcs2_emul_path);

			libbuf = malloc(MAXPATHLEN + emul_path_len,
					M_TEMP, M_WAITOK);
			strcpy(libbuf, ibcs2_emul_path);

		    	for (j = off; j < scns[i].s_size + off;) {
				long stroff, nextoff;
	      			char *libname;

				nextoff = 4 * *(long *)(buf + j);
				stroff = 4 * *(long *)(buf + j + sizeof(long));

		      		libname = buf + j + stroff;
		      		j += nextoff;

				DPRINTF(("%s(%d):  shared library %s\n",
					 __FILE__, __LINE__, libname));
				strcpy(&libbuf[emul_path_len], libname);
/* XXXKSE only 1:1 in coff */  	error = coff_load_file(
				    FIRST_THREAD_IN_PROC(imgp->proc), libbuf);
		      		if (error)
	      				error = coff_load_file(
					    FIRST_THREAD_IN_PROC(imgp->proc),
					    libname);
		      		if (error)
					break;
		    	}
			free(libbuf, M_TEMP);
		}
		if (vm_map_remove(kernel_map,
				  (vm_offset_t) buf,
				  (vm_offset_t) buf + len))
	      		panic("exec_coff_imgact vm_map_remove failed");
	    	if (error)
	      		goto fail;
	  	}
	}
	/*
	 * Map in .text now
	 */

	DPRINTF(("%s(%d):  load_coff_section(vmspace, "
		"imgp->vp, %08lx, %08lx, 0x%x, 0x%x, 0x%x)\n",
		__FILE__, __LINE__, text_offset, text_address,
		text_size, text_size, VM_PROT_READ | VM_PROT_EXECUTE));
	if ((error = load_coff_section(vmspace, imgp->vp,
				      text_offset,
				      (caddr_t)(void *)(uintptr_t)text_address,
				      text_size, text_size,
				      VM_PROT_READ | VM_PROT_EXECUTE)) != 0) {
		DPRINTF(("%s(%d): error = %d\n", __FILE__, __LINE__, error));
		goto fail;
       	}
	/*
	 * Map in .data and .bss now
	 */


	DPRINTF(("%s(%d): load_coff_section(vmspace, "
		"imgp->vp, 0x%08lx, 0x%08lx, 0x%x, 0x%x, 0x%x)\n",
		__FILE__, __LINE__, data_offset, data_address,
		data_size + bss_size, data_size, VM_PROT_ALL));
	if ((error = load_coff_section(vmspace, imgp->vp,
				      data_offset,
				      (caddr_t)(void *)(uintptr_t)data_address,
				      data_size + bss_size, data_size,
				      VM_PROT_ALL)) != 0) {

		DPRINTF(("%s(%d): error = %d\n", __FILE__, __LINE__, error));
		goto fail;
	}

	imgp->interpreted = 0;
	imgp->proc->p_sysent = &ibcs2_svr3_sysvec;

	vmspace->vm_tsize = round_page(text_size) >> PAGE_SHIFT;
	vmspace->vm_dsize = round_page(data_size + bss_size) >> PAGE_SHIFT;
	vmspace->vm_taddr = (caddr_t)(void *)(uintptr_t)text_address;
	vmspace->vm_daddr = (caddr_t)(void *)(uintptr_t)data_address;

	hole = (caddr_t)trunc_page((vm_offset_t)vmspace->vm_daddr) + ctob(vmspace->vm_dsize);


	DPRINTF(("%s(%d): vm_map_find(&vmspace->vm_map, NULL, 0, &0x%08lx, PAGE_SIZE, FALSE, VM_PROT_ALL, VM_PROT_ALL, 0)\n",
		__FILE__, __LINE__, hole));
        DPRINTF(("imgact: error = %d\n", error));

	error = vm_map_find(&vmspace->vm_map, NULL, 0,
			    (vm_offset_t *) &hole, PAGE_SIZE, FALSE,
				VM_PROT_ALL, VM_PROT_ALL, 0);

	DPRINTF(("IBCS2: start vm_dsize = 0x%x, vm_daddr = 0x%x end = 0x%x\n",
		ctob(vmspace->vm_dsize), vmspace->vm_daddr,
		ctob(vmspace->vm_dsize) + vmspace->vm_daddr ));
	DPRINTF(("%s(%d):  returning successfully!\n", __FILE__, __LINE__));

fail:
	vn_lock(imgp->vp, LK_EXCLUSIVE | LK_RETRY, td);

	return error;
}

/*
 * Tell kern_execve.c about it, with a little help from the linker.
 */
static struct execsw coff_execsw = { exec_coff_imgact, "coff" };
EXEC_SET(coff, coff_execsw);
