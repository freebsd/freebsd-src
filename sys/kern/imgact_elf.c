/*-
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
 *	$Id: imgact_elf.c,v 1.22 1998/03/02 05:47:58 peter Exp $
 */

#include "opt_rlimit.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/exec.h>
#include <sys/mman.h>
#include <sys/imgact.h>
#include <sys/imgact_elf.h>
#include <sys/kernel.h>
#include <sys/sysent.h>
#include <sys/malloc.h>
#include <sys/namei.h>
#include <sys/proc.h>
#include <sys/syscall.h>
#include <sys/signalvar.h>
#include <sys/sysctl.h>
#include <sys/vnode.h>

#include <vm/vm.h>
#include <vm/vm_kern.h>
#include <vm/vm_param.h>
#include <vm/pmap.h>
#include <sys/lock.h>
#include <vm/vm_map.h>
#include <vm/vm_prot.h>
#include <vm/vm_extern.h>

#include <machine/md_var.h>

#define MAX_PHDR	32	/* XXX enough ? */

static int elf_check_header __P((const Elf32_Ehdr *hdr, int type));
static int elf_load_section __P((struct vmspace *vmspace, struct vnode *vp, vm_offset_t offset, caddr_t vmaddr, size_t memsz, size_t filsz, vm_prot_t prot));
static int elf_load_file __P((struct proc *p, char *file, u_long *addr, u_long *entry));
static int elf_freebsd_fixup __P((int **stack_base, struct image_params *imgp));
static int exec_elf_imgact __P((struct image_params *imgp));

static int elf_trace = 0;
SYSCTL_INT(_debug, OID_AUTO, elf_trace, CTLFLAG_RW, &elf_trace, 0, "");
#define UPRINTF if (elf_trace) uprintf

static struct sysentvec elf_freebsd_sysvec = {
        SYS_MAXSYSCALL,
        sysent,
        0,
        0,
        0,
        0,
        0,
        elf_freebsd_fixup,
        sendsig,
        sigcode,
        &szsigcode,
        0,
	"FreeBSD ELF"
};

static Elf32_Brandinfo freebsd_brand_info = {
						"FreeBSD",
						"",
						"/usr/libexec/ld-elf.so.1",
						&elf_freebsd_sysvec
					  };
static Elf32_Brandinfo *elf_brand_list[MAX_BRANDS] = {
							&freebsd_brand_info,
							NULL, NULL, NULL,
							NULL, NULL, NULL, NULL
						    };

int
elf_insert_brand_entry(Elf32_Brandinfo *entry)
{
	int i;

	for (i=1; i<MAX_BRANDS; i++) {
		if (elf_brand_list[i] == NULL) {
			elf_brand_list[i] = entry;
			break;
		}
	}
	if (i == MAX_BRANDS)
		return -1;
	return 0;
}

int
elf_remove_brand_entry(Elf32_Brandinfo *entry)
{
	int i;

	for (i=1; i<MAX_BRANDS; i++) {
		if (elf_brand_list[i] == entry) {
			elf_brand_list[i] = NULL;
			break;
		}
	}
	if (i == MAX_BRANDS)
		return -1;
	return 0;
}

static int
elf_check_header(const Elf32_Ehdr *hdr, int type)
{
	if (!(hdr->e_ident[EI_MAG0] == ELFMAG0 &&
	      hdr->e_ident[EI_MAG1] == ELFMAG1 &&
	      hdr->e_ident[EI_MAG2] == ELFMAG2 &&
	      hdr->e_ident[EI_MAG3] == ELFMAG3))
		return ENOEXEC;

	if (hdr->e_machine != EM_386 && hdr->e_machine != EM_486)
		return ENOEXEC;

	if (hdr->e_type != type)
		return ENOEXEC;
	
	return 0;
}

static int
elf_load_section(struct vmspace *vmspace, struct vnode *vp, vm_offset_t offset, caddr_t vmaddr, size_t memsz, size_t filsz, vm_prot_t prot)
{
	size_t map_len;
	vm_offset_t map_addr;
	int error;
	unsigned char *data_buf = 0;
	size_t copy_len;

	map_addr = trunc_page(vmaddr);

	if (memsz > filsz)
		map_len = trunc_page(offset+filsz) - trunc_page(offset);
	else
		map_len = round_page(offset+filsz) - trunc_page(offset);

	if (error = vm_mmap (&vmspace->vm_map,
			     &map_addr,
			     map_len,
			     prot,
			     VM_PROT_ALL,
			     MAP_PRIVATE | MAP_FIXED,
			     (caddr_t)vp,
			     trunc_page(offset)))
		return error;

	if (memsz == filsz)
		return 0;

	/*
	 * We have to map the remaining bit of the file into the kernel's
	 * memory map, allocate some anonymous memory, and copy that last
	 * bit into it. The remaining space should be .bss...
	 */
	copy_len = (offset + filsz) - trunc_page(offset + filsz);
	map_addr = trunc_page(vmaddr + filsz);
	map_len = round_page(vmaddr + memsz) - map_addr;

        if (map_len != 0) {
		if (error = vm_map_find(&vmspace->vm_map, NULL, 0,
					&map_addr, map_len, FALSE,
					VM_PROT_ALL, VM_PROT_ALL,0))
			return error; 
	}

	if (error = vm_mmap(exec_map,
			    (vm_offset_t *)&data_buf,
			    PAGE_SIZE,
			    VM_PROT_READ,
			    VM_PROT_READ,
			    0,
			    (caddr_t)vp,
			    trunc_page(offset + filsz)))
		return error;

	error = copyout(data_buf, (caddr_t)map_addr, copy_len);

        vm_map_remove(exec_map, (vm_offset_t)data_buf, 
		      (vm_offset_t)data_buf + PAGE_SIZE);

	/*
	 * set it to the specified protection
	 */
	vm_map_protect(&vmspace->vm_map, map_addr, map_addr + map_len,  prot,
		       FALSE);

	UPRINTF("bss size %d (%x)\n", map_len-copy_len, map_len-copy_len);
	return error;
}

static int
elf_load_file(struct proc *p, char *file, u_long *addr, u_long *entry)
{
	Elf32_Ehdr *hdr = NULL;
	Elf32_Phdr *phdr = NULL;
	struct nameidata nd;
	struct vmspace *vmspace = p->p_vmspace;
	struct vattr attr;
	struct image_params image_params, *imgp;
	vm_prot_t prot = 0;
	unsigned long text_size = 0, data_size = 0;
	unsigned long text_addr = 0, data_addr = 0;
	int header_size = 0;
        int error, i;

	imgp = &image_params;
	/*
	 * Initialize part of the common data
	 */
	imgp->proc = p;
	imgp->uap = NULL;
	imgp->attr = &attr;
	imgp->firstpage = NULL;
	imgp->image_header = (char *)kmem_alloc_wait(exec_map, PAGE_SIZE);

	if (imgp->image_header == NULL) {
		nd.ni_vp = NULL;
		error = ENOMEM;
		goto fail;
	}

        NDINIT(&nd, LOOKUP, LOCKLEAF|FOLLOW, UIO_SYSSPACE, file, p);   
			 
	if (error = namei(&nd)) {
		nd.ni_vp = NULL;
		goto fail;
	}

	imgp->vp = nd.ni_vp;

	/*
	 * Check permissions, modes, uid, etc on the file, and "open" it.
	 */
	error = exec_check_permissions(imgp);
	if (error) {
		VOP_UNLOCK(nd.ni_vp, 0, p);
		goto fail;
	}

	error = exec_map_first_page(imgp);
	VOP_UNLOCK(nd.ni_vp, 0, p);
	if (error)
                goto fail;

	hdr = (Elf32_Ehdr *)imgp->image_header;
	if (error = elf_check_header(hdr, ET_DYN))
		goto fail;

	/*
	 * ouch, need to bounds check in case user gives us a corrupted
	 * file with an insane header size
	 */
	if (hdr->e_phnum > MAX_PHDR) {	/* XXX: ever more than this? */
		error = ENOEXEC;
		goto fail;
	}

	header_size = hdr->e_phentsize * hdr->e_phnum;

	/* Only support headers that fit within first page for now */
	if (header_size + hdr->e_phoff > PAGE_SIZE) {
		error = ENOEXEC;
		goto fail;
	}

	phdr = (Elf32_Phdr *)(imgp->image_header + hdr->e_phoff);

	for (i = 0; i < hdr->e_phnum; i++) {
		switch(phdr[i].p_type) {

	   	case PT_NULL:	/* NULL section */
	    		UPRINTF ("ELF(file) PT_NULL section\n");
			break;
		case PT_LOAD:	/* Loadable segment */
		{
	    		UPRINTF ("ELF(file) PT_LOAD section ");
			if (phdr[i].p_flags & PF_X)
  				prot |= VM_PROT_EXECUTE;
			if (phdr[i].p_flags & PF_W)
  				prot |= VM_PROT_WRITE;
			if (phdr[i].p_flags & PF_R)
  				prot |= VM_PROT_READ;

			if (error = elf_load_section(vmspace, nd.ni_vp,
  						     phdr[i].p_offset,
  						     (caddr_t)phdr[i].p_vaddr +
							(*addr),
  						     phdr[i].p_memsz,
  						     phdr[i].p_filesz, prot)) 
				goto fail;

			/*
			 * Is this .text or .data ??
			 *
			 * We only handle one each of those yet XXX
			 */
			if (hdr->e_entry >= phdr[i].p_vaddr &&
			hdr->e_entry <(phdr[i].p_vaddr+phdr[i].p_memsz)) {
  				text_addr = trunc_page(phdr[i].p_vaddr+(*addr));
  				text_size = round_page(phdr[i].p_memsz +
						       phdr[i].p_vaddr -
						       trunc_page(phdr[i].p_vaddr));
				*entry=(unsigned long)hdr->e_entry+(*addr);
	    			UPRINTF(".text <%08x,%08x> entry=%08x\n",
					text_addr, text_size, *entry);
			} else {
  				data_addr = trunc_page(phdr[i].p_vaddr+(*addr));
  				data_size = round_page(phdr[i].p_memsz +
						       phdr[i].p_vaddr -
						       trunc_page(phdr[i].p_vaddr));
	    			UPRINTF(".data <%08x,%08x>\n",
					data_addr, data_size);
			}
		}
		break;

	   	case PT_DYNAMIC:/* Dynamic link information */
	    		UPRINTF ("ELF(file) PT_DYNAMIC section\n");
			break;
	  	case PT_INTERP:	/* Path to interpreter */
	    		UPRINTF ("ELF(file) PT_INTERP section\n");
			break;
	  	case PT_NOTE:	/* Note section */
	    		UPRINTF ("ELF(file) PT_NOTE section\n");
			break;
	  	case PT_SHLIB:	/* Shared lib section  */
	    		UPRINTF ("ELF(file) PT_SHLIB section\n");
			break;
		case PT_PHDR: 	/* Program header table info */
	    		UPRINTF ("ELF(file) PT_PHDR section\n");
			break;
		default:
	    		UPRINTF ("ELF(file) %d section ??\n", phdr[i].p_type );
		}
	}

fail:
	if (imgp->firstpage)
		exec_unmap_first_page(imgp);
	if (imgp->image_header)
		kmem_free_wakeup(exec_map, (vm_offset_t)imgp->image_header,
			PAGE_SIZE);
	if (nd.ni_vp)
		vrele(nd.ni_vp);

	return error;
}

static int
exec_elf_imgact(struct image_params *imgp)
{
	const Elf32_Ehdr *hdr = (const Elf32_Ehdr *) imgp->image_header;
	const Elf32_Phdr *phdr, *mapped_phdr = NULL;
	Elf32_Auxargs *elf_auxargs = NULL;
	struct vmspace *vmspace;
	vm_prot_t prot = 0;
	u_long text_size = 0, data_size = 0;
	u_long text_addr = 0, data_addr = 0;
	u_long addr, entry = 0, proghdr = 0;
	int error, i, header_size = 0;
	const char *interp = NULL;
	char *brand = NULL;
	char path[MAXPATHLEN];

	/*
	 * Do we have a valid ELF header ?
	 */
	if (elf_check_header(hdr, ET_EXEC))
		return -1;

	/*
	 * From here on down, we return an errno, not -1, as we've
	 * detected an ELF file.
	 */

	/*
	 * ouch, need to bounds check in case user gives us a corrupted
	 * file with an insane header size
	 */
	if (hdr->e_phnum > MAX_PHDR) {	/* XXX: ever more than this? */
		return ENOEXEC;
	}

	header_size = hdr->e_phentsize * hdr->e_phnum;

	if ((hdr->e_phoff > PAGE_SIZE) ||
	    (hdr->e_phoff + header_size) > PAGE_SIZE) {
		/* Only support headers in first page for now */
		return ENOEXEC;
	} else {
		phdr = (const Elf32_Phdr*)
		       ((const char *)imgp->image_header + hdr->e_phoff);
	}
	
	/*
	 * From this point on, we may have resources that need to be freed.
	 */
	if (error = exec_extract_strings(imgp))
		goto fail;

	exec_new_vmspace(imgp);

	vmspace = imgp->proc->p_vmspace;

	for (i = 0; i < hdr->e_phnum; i++) {
		switch(phdr[i].p_type) {

	   	case PT_NULL:	/* NULL section */
	    		UPRINTF ("ELF PT_NULL section\n");
			break;
		case PT_LOAD:	/* Loadable segment */
		{
	    		UPRINTF ("ELF PT_LOAD section ");
			if (phdr[i].p_flags & PF_X)
  				prot |= VM_PROT_EXECUTE;
			if (phdr[i].p_flags & PF_W)
  				prot |= VM_PROT_WRITE;
			if (phdr[i].p_flags & PF_R)
  				prot |= VM_PROT_READ;

			if (error = elf_load_section(vmspace, imgp->vp,
  						     phdr[i].p_offset,
  						     (caddr_t)phdr[i].p_vaddr,
  						     phdr[i].p_memsz,
  						     phdr[i].p_filesz, prot)) 
  				goto fail;

			/*
			 * Is this .text or .data ??
			 *
			 * We only handle one each of those yet XXX
			 */
			if (hdr->e_entry >= phdr[i].p_vaddr &&
			hdr->e_entry <(phdr[i].p_vaddr+phdr[i].p_memsz)) {
  				text_addr = trunc_page(phdr[i].p_vaddr);
  				text_size = round_page(phdr[i].p_memsz +
						       phdr[i].p_vaddr -
						       text_addr);
				entry = (u_long)hdr->e_entry;
	    			UPRINTF(".text <%08x,%08x> entry=%08x\n",
					text_addr, text_size, entry);
			} else {
  				data_addr = trunc_page(phdr[i].p_vaddr);
  				data_size = round_page(phdr[i].p_memsz +
						       phdr[i].p_vaddr -
						       data_addr);
	    			UPRINTF(".data <%08x,%08x>\n",
					data_addr, data_size);
			}
		}
		break;

	   	case PT_DYNAMIC:/* Dynamic link information */
	    		UPRINTF ("ELF PT_DYNAMIC section ??\n");
			break;
	  	case PT_INTERP:	/* Path to interpreter */
	    		UPRINTF ("ELF PT_INTERP section ");
			if (phdr[i].p_filesz > MAXPATHLEN ||
			    phdr[i].p_offset + phdr[i].p_filesz > PAGE_SIZE) {
				error = ENOEXEC;
				goto fail;
			}
			interp = imgp->image_header + phdr[i].p_offset;
			UPRINTF("<%s>\n", interp);
			break;
	  	case PT_NOTE:	/* Note section */
	    		UPRINTF ("ELF PT_NOTE section\n");
			break;
	  	case PT_SHLIB:	/* Shared lib section  */
	    		UPRINTF ("ELF PT_SHLIB section\n");
			break;
		case PT_PHDR: 	/* Program header table info */
	    		UPRINTF ("ELF PT_PHDR section <%x>\n", phdr[i].p_vaddr);
			proghdr = phdr[i].p_vaddr;
			break;
		default:
	    		UPRINTF ("ELF %d section ??\n", phdr[i].p_type);
		}
	}

	vmspace->vm_tsize = text_size >> PAGE_SHIFT;
	vmspace->vm_taddr = (caddr_t)text_addr;
	vmspace->vm_dsize = data_size >> PAGE_SHIFT;
	vmspace->vm_daddr = (caddr_t)data_addr;

	addr = 2*MAXDSIZ; /* May depend on OS type XXX */

	imgp->entry_addr = entry;

	/* 
	 * So which kind (brand) of ELF binary do we have at hand
	 * FreeBSD, Linux, SVR4 or something else ??
	 * If its has a interpreter section try that first
	 */
        if (interp) {
                for (i=0; i<MAX_BRANDS; i++) {
                        if (elf_brand_list[i] != NULL) {
                                if (!strcmp(interp, elf_brand_list[i]->interp_path)) {
                                        imgp->proc->p_sysent =
                                                elf_brand_list[i]->sysvec;
                                        strcpy(path, elf_brand_list[i]->emul_path);
                                        strcat(path, elf_brand_list[i]->interp_path);
                                        UPRINTF("interpreter=<%s> %s\n",
                                                elf_brand_list[i]->interp_path,
                                                elf_brand_list[i]->emul_path);
                                        break;
                                }
                        }
                }
        }

	/*
	 * If there is no interpreter, or recognition of it
	 * failed, se if the binary is branded.
	 */
	if (!interp || i == MAX_BRANDS) {
		brand = (char *)&(hdr->e_ident[EI_BRAND]);
		for (i=0; i<MAX_BRANDS; i++) {
			if (elf_brand_list[i] != NULL) {
				if (!strcmp(brand, elf_brand_list[i]->brand)) {
					imgp->proc->p_sysent = elf_brand_list[i]->sysvec;
					if (interp) {
						strcpy(path, elf_brand_list[i]->emul_path);
						strcat(path, elf_brand_list[i]->interp_path);
						UPRINTF("interpreter=<%s> %s\n",
						elf_brand_list[i]->interp_path,
						elf_brand_list[i]->emul_path);
					}
					break;
				}
			}
		}
	}
	if (i == MAX_BRANDS) {
		uprintf("ELF binary type not known\n");
		error = ENOEXEC;
		goto fail;
	}
	if (interp) {
                if (error = elf_load_file(imgp->proc,
                                          path,
                                          &addr,        /* XXX */
                                          &imgp->entry_addr)) {
                        uprintf("ELF interpreter %s not found\n", path);
                        goto fail;
                }
	}

	UPRINTF("Executing %s binary\n", elf_brand_list[i]->brand);

	/*
	 * Construct auxargs table (used by the fixup routine)
	 */
	elf_auxargs = malloc(sizeof(Elf32_Auxargs), M_TEMP, M_WAITOK);
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

	/* don't allow modifying the file while we run it */
	imgp->vp->v_flag |= VTEXT;
	
fail:
	return error;
}

static int
elf_freebsd_fixup(int **stack_base, struct image_params *imgp)
{
	Elf32_Auxargs *args = (Elf32_Auxargs *)imgp->auxargs;
	int *pos;

	pos = *stack_base + (imgp->argc + imgp->envc + 2);

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

	(*stack_base)--;
	**stack_base = (int)imgp->argc;
	return 0;
} 

/*
 * Tell kern_execve.c about it, with a little help from the linker.
 * Since `const' objects end up in the text segment, TEXT_SET is the
 * correct directive to use.
 */
static const struct execsw elf_execsw = {exec_elf_imgact, "ELF"};
TEXT_SET(execsw_set, elf_execsw);

