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
 *    derived from this software withough specific prior written permission
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
 *	$Id: imgact_coff.c,v 1.2 1995/02/20 23:52:39 davidg Exp $
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/resourcevar.h>
#include <sys/exec.h>
#include <sys/mman.h>
#include <sys/imgact.h>
#include <sys/kernel.h>
#include <sys/file.h>
#include <sys/malloc.h>
#include <sys/mount.h>
#include <sys/sysent.h>
#include <vm/vm.h>
#include <vm/vm_kern.h>
#include "coff.h"
#include "ibcs2.h"

static int
load_coff_section(vmspace, vp, offset, vmaddr, memsz, filsz, prot)
	struct vmspace *vmspace;
	struct vnode *vp;
	vm_offset_t offset;
	caddr_t vmaddr;
	size_t memsz, filsz;
	vm_prot_t prot;
{
	size_t map_len;
	vm_offset_t map_offset;
	vm_offset_t map_addr;
	int error;
	unsigned char *data_buf = 0;
	size_t copy_len;

	map_offset = trunc_page(offset);
	map_addr = trunc_page(vmaddr);

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

if (ibcs2_trace & IBCS2_TRACE_COFF) {
printf("%s(%d):  vm_mmap(&vmspace->vm_map, &0x%08lx, 0x%x, 0x%x, "
	"VM_PROT_ALL, MAP_FILE | MAP_PRIVATE | MAP_FIXED, vp, 0x%x)\n",
	__FILE__, __LINE__, map_addr, map_len, prot, map_offset);
}

	if (error = vm_mmap(&vmspace->vm_map,
			     &map_addr,
			     map_len,
			     prot,
			     VM_PROT_ALL,
			     MAP_FILE | MAP_PRIVATE | MAP_FIXED,
			     (caddr_t) vp,
			     map_offset))
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
	map_addr = trunc_page(vmaddr + filsz);
	map_len = round_page(memsz) - trunc_page(filsz);

if (ibcs2_trace & IBCS2_TRACE_COFF) {
printf("%s(%d): vm_map_find(&vmspace->vm_map, NULL, 0, &0x%08lx, 0x%x, FALSE)\n",
	__FILE__, __LINE__, map_addr, map_len);
}

	if (map_len != 0) {
		error = vm_map_find(&vmspace->vm_map, NULL, 0, &map_addr,
				    map_len, FALSE);
		if (error)
			return error;
	}

	if (error = vm_mmap(kernel_map,
			    (vm_offset_t *) &data_buf,
			    PAGE_SIZE,
			    VM_PROT_READ,
			    VM_PROT_READ,
			    MAP_FILE,
			    (caddr_t) vp,
			    trunc_page(offset + filsz)))
		return error;

	bcopy(data_buf, (caddr_t) map_addr, copy_len);

	if (vm_map_remove(kernel_map,
			  (vm_offset_t) data_buf,
			  (vm_offset_t) data_buf + PAGE_SIZE))
		panic("load_coff_section vm_map_remove failed");

	return 0;
}
				 
int
coff_load_file(struct proc *p, char *name)
{
  	struct vmspace *vmspace = p->p_vmspace;
  	int error;
  	struct nameidata nd;
  	struct vnode *vnodep;
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

  	nd.ni_cnd.cn_nameiop = LOOKUP;
	nd.ni_cnd.cn_flags = LOCKLEAF | FOLLOW | SAVENAME;
	nd.ni_cnd.cn_proc = curproc;
	nd.ni_cnd.cn_cred = curproc->p_cred->pc_ucred;
  	nd.ni_segflg = UIO_SYSSPACE;
  	nd.ni_dirp = name;
	
  	error = namei(&nd);
  	if (error)
    		return error;
	
  	vnodep = nd.ni_vp;
  	if (vnodep == NULL)
    		return ENOEXEC;
	
  	if (vnodep->v_writecount) {
    		error = ETXTBSY;
    		goto fail;
  	}

  	if (error = VOP_GETATTR(vnodep, &attr, p->p_ucred, p))
    		goto fail;

  	if ((vnodep->v_mount->mnt_flag & MNT_NOEXEC)
	    || ((attr.va_mode & 0111) == 0)
	    || (attr.va_type != VREG))
    		goto fail;

  	if (attr.va_size == 0) {
    		error = ENOEXEC;
    		goto fail;
  	}

  	if (error = VOP_ACCESS(vnodep, VEXEC, p->p_ucred, p))
    		goto fail;

  	if (error = VOP_OPEN(vnodep, FREAD, p->p_ucred, p))
    		goto fail;

  	if (error = vm_mmap(kernel_map,
			    (vm_offset_t *) &ptr,
			    PAGE_SIZE,
			    VM_PROT_READ,
		       	    VM_PROT_READ,
			    MAP_FILE,
			    (caddr_t) vnodep,
			    0))
    	goto fail;

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

  	if (error = load_coff_section(vmspace, vnodep, text_offset,
				      (caddr_t)text_address,
				      text_size, text_size,
				      VM_PROT_READ | VM_PROT_EXECUTE)) {
    		goto dealloc_and_fail;
  	}
  	if (error = load_coff_section(vmspace, vnodep, data_offset,
				      (caddr_t)data_address,
				      data_size + bss_size, data_size,
				      VM_PROT_ALL)) {
    		goto dealloc_and_fail;
  	}

  	error = 0;

 	dealloc_and_fail:
	if (vm_map_remove(kernel_map,
			  (vm_offset_t) ptr,
			  (vm_offset_t) ptr + PAGE_SIZE))
    		panic(__FUNCTION__ " vm_map_remove failed");

 fail:
  	vput(nd.ni_vp);
  	FREE(nd.ni_cnd.cn_pnbuf, M_NAMEI);
  	return error;
}

int
exec_coff_imgact(iparams)
	struct image_params *iparams;
{
	struct filehdr *fhdr = (struct filehdr*)iparams->image_header;
	struct aouthdr *ahdr;
	struct scnhdr *scns;
	int i;
	struct vmspace *vmspace = iparams->proc->p_vmspace;
	unsigned long vmaddr;
	int nscns;
	int error, len;
	extern struct sysentvec ibcs2_svr3_sysvec;
	unsigned long text_offset = 0, text_address = 0, text_size = 0;
	unsigned long data_offset = 0, data_address = 0, data_size = 0;
	unsigned long bss_size = 0;
	int need_hack_p;
	unsigned long data_end;
	unsigned long data_map_start, data_map_len, data_map_addr = 0;
	unsigned long bss_address, bss_map_start, data_copy_len, bss_map_len;
	unsigned char *data_buf = 0;
	caddr_t hole;

	if (fhdr->f_magic != I386_COFF ||
	    !(fhdr->f_flags & F_EXEC)) {

if (ibcs2_trace & IBCS2_TRACE_COFF) {
printf("%s(%d): return -1\n", __FILE__, __LINE__);
}

	  	return -1;
	}

	nscns = fhdr->f_nscns;
	if ((nscns * sizeof(struct scnhdr)) > PAGE_SIZE) {
	  	/*
	   	 * For now, return an error -- need to be able to
	   	 * read in all of the section structures.
	   	 */

if (ibcs2_trace & IBCS2_TRACE_COFF) {
printf("%s(%d): return -1\n", __FILE__, __LINE__);
}
	  	return -1;
	}

	ahdr = (struct aouthdr*)((char*)(iparams->image_header) +
				 sizeof(struct filehdr));
	iparams->entry_addr = ahdr->entry;

	scns = (struct scnhdr*)((char*)(iparams->image_header) +
				sizeof(struct filehdr) +
				sizeof(struct aouthdr));

	if (error = exec_extract_strings(iparams)) {

if (ibcs2_trace & IBCS2_TRACE_COFF) {
printf("%s(%d):  return %d\n", __FILE__, __LINE__, error);
}
	  	return error;
	}

	exec_new_vmspace(iparams);

	for (i = 0; i < nscns; i++) {

if (ibcs2_trace & IBCS2_TRACE_COFF) {
printf("i = %d, scns[i].s_name = %s, scns[i].s_vaddr = %08lx, "
       "scns[i].s_scnptr = %d\n", i, scns[i].s_name,
       scns[i].s_vaddr, scns[i].s_scnptr);
}
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
	    	char *buf = 0, *ptr;
	    	int foff = trunc_page(scns[i].s_scnptr);
	    	int off = scns[i].s_scnptr - foff;
	    	int len = round_page(scns[i].s_size + PAGE_SIZE);
	    	int j;

	    	if (error = vm_mmap(kernel_map,
				    (vm_offset_t *) &buf,
				    len,
				    VM_PROT_READ,
				    VM_PROT_READ,
				    MAP_FILE,
				    (caddr_t) iparams->vnodep,
				    foff)) {
	      		return ENOEXEC;
	    	}
	    	for (j = off; j < scns[i].s_size + off; j++) {
	      		char *libname;
	      		libname = buf + j + 4 * *(long*)(buf + j + 4);
	      		j += 4* *(long*)(buf + j);

if (ibcs2_trace & IBCS2_TRACE_COFF) {
printf("%s(%d):  shared library %s\n", __FILE__, __LINE__, libname);
}
	      		error = coff_load_file(iparams->proc, libname);
	      		if (error)
			break;
	    	}
		if (vm_map_remove(kernel_map,
				  (vm_offset_t) buf,
				  (vm_offset_t) buf + len))
	      		panic("exec_coff_imgact vm_map_remove failed");
	    	if (error)
	      		return error;
	  	}
	}
	/*
	 * Map in .text now
	 */

if (ibcs2_trace & IBCS2_TRACE_COFF) {
printf("%s(%d):  load_coff_section(vmspace, "
	"iparams->vnodep, %08lx, %08lx, 0x%x, 0x%x, 0x%x)\n",
	__FILE__, __LINE__, text_offset, text_address,
	text_size, text_size, VM_PROT_READ | VM_PROT_EXECUTE);
}
	if (error = load_coff_section(vmspace, iparams->vnodep,
				      text_offset, (caddr_t)text_address,
				      text_size, text_size,
				      VM_PROT_READ | VM_PROT_EXECUTE)) {

if (ibcs2_trace & IBCS2_TRACE_COFF) {
printf("%s(%d): error = %d\n", __FILE__, __LINE__, error);
}
		return error;
       	}
	/*
	 * Map in .data and .bss now
	 */


if (ibcs2_trace & IBCS2_TRACE_COFF) {
printf("%s(%d): load_coff_section(vmspace, "
	"iparams->vnodep, 0x%08lx, 0x%08lx, 0x%x, 0x%x, 0x%x)\n",
	__FILE__, __LINE__, data_offset, data_address,
	data_size + bss_size, data_size, VM_PROT_ALL);
}
	if (error = load_coff_section(vmspace, iparams->vnodep,
				      data_offset, (caddr_t)data_address,
				      data_size + bss_size, data_size,
				      VM_PROT_ALL)) {

if (ibcs2_trace & IBCS2_TRACE_COFF) {
printf("%s(%d): error = %d\n", __FILE__, __LINE__, error);
}
		return error;
	}

	iparams->interpreted = 0;
	iparams->proc->p_sysent = &ibcs2_svr3_sysvec;

	vmspace->vm_tsize = round_page(text_size) >> PAGE_SHIFT;
	vmspace->vm_dsize = round_page(data_size + bss_size) >> PAGE_SHIFT;
	vmspace->vm_taddr = (caddr_t)text_address;
	vmspace->vm_daddr = (caddr_t)data_address;

	hole = (caddr_t)trunc_page(vmspace->vm_daddr) + ctob(vmspace->vm_dsize);


if (ibcs2_trace & IBCS2_TRACE_COFF) {
printf("%s(%d): vm_map_find(&vmspace->vm_map, NULL, 0, &0x%08lx, PAGE_SIZE, FALSE)\n",
	__FILE__, __LINE__, hole);
printf("imgact: error = %d\n", error);
}
	error = vm_map_find(&vmspace->vm_map, NULL, 0,
			    (vm_offset_t *) &hole, PAGE_SIZE, FALSE);

if (ibcs2_trace & IBCS2_TRACE_COFF) {
printf("IBCS2: start vm_dsize = 0x%x, vm_daddr = 0x%x end = 0x%x\n", 
	ctob(vmspace->vm_dsize), vmspace->vm_daddr,
	ctob(vmspace->vm_dsize) + vmspace->vm_daddr );
printf("%s(%d):  returning successfully!\n", __FILE__, __LINE__);
}
	return 0;
}

/*
 * Tell kern_execve.c about it, with a little help from the linker.
 * Since `const' objects end up in the text segment, TEXT_SET is the
 * correct directive to use.
 */
const struct execsw coff_execsw = { exec_coff_imgact, "coff" };
TEXT_SET(execsw_set, coff_execsw);
