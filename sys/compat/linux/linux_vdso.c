/*-
 * Copyright (c) 2013-2021 Dmitry Chagin <dchagin@FreeBSD.org>
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

#if defined(__i386__) || (defined(__amd64__) && defined(COMPAT_LINUX32))
#define	__ELF_WORD_SIZE	32
#else
#define	__ELF_WORD_SIZE	64
#endif

#include <sys/param.h>
#include <sys/elf.h>
#include <sys/imgact.h>
#include <sys/lock.h>
#include <sys/proc.h>
#include <sys/rwlock.h>
#include <sys/sysent.h>

#include <vm/pmap.h>
#include <vm/vm_extern.h>
#include <vm/vm_map.h>
#include <vm/vm_object.h>
#include <vm/vm_page.h>
#include <vm/vm_pager.h>
#include <vm/vm_param.h>

#include <compat/linux/linux_vdso.h>

SLIST_HEAD(, linux_vdso_sym) __elfN(linux_vdso_syms) =
    SLIST_HEAD_INITIALIZER(__elfN(linux_vdso_syms));

void
__elfN(linux_vdso_sym_init)(struct linux_vdso_sym *s)
{

	SLIST_INSERT_HEAD(&__elfN(linux_vdso_syms), s, sym);
}

vm_object_t
__elfN(linux_shared_page_init)(char **mapping, vm_size_t size)
{
	vm_page_t m;
	vm_object_t obj;
	vm_offset_t addr;
	size_t n, pages;

	pages = size / PAGE_SIZE;

	addr = kva_alloc(size);
	obj = vm_pager_allocate(OBJT_PHYS, 0, size,
	    VM_PROT_DEFAULT, 0, NULL);
	VM_OBJECT_WLOCK(obj);
	for (n = 0; n < pages; n++) {
		m = vm_page_grab(obj, n,
		    VM_ALLOC_ZERO);
		vm_page_valid(m);
		vm_page_xunbusy(m);
		pmap_qenter(addr + n * PAGE_SIZE, &m, 1);
	}
	VM_OBJECT_WUNLOCK(obj);
	*mapping = (char *)addr;
	return (obj);
}

void
__elfN(linux_shared_page_fini)(vm_object_t obj, void *mapping,
    vm_size_t size)
{
	vm_offset_t va;

	va = (vm_offset_t)mapping;
	pmap_qremove(va, size / PAGE_SIZE);
	kva_free(va, size);
	vm_object_deallocate(obj);
}

void
__elfN(linux_vdso_fixup)(char *base, vm_offset_t offset)
{
	struct linux_vdso_sym *lsym;
	const Elf_Shdr *shdr;
	Elf_Ehdr *ehdr;
	Elf_Sym *dsym, *sym;
	char *strtab, *symname;
	int i, symcnt;

	ehdr = __DECONST(Elf_Ehdr *, base);

	MPASS(IS_ELF(*ehdr));
	MPASS(ehdr->e_ident[EI_CLASS] == ELF_TARG_CLASS);
	MPASS(ehdr->e_ident[EI_DATA] == ELF_TARG_DATA);
	MPASS(ehdr->e_ident[EI_VERSION] == EV_CURRENT);
	MPASS(ehdr->e_shentsize == sizeof(Elf_Shdr));
	MPASS(ehdr->e_shoff != 0);
	MPASS(ehdr->e_type == ET_DYN);

	shdr = (const Elf_Shdr *)(base + ehdr->e_shoff);

	dsym = NULL;
	for (i = 0; i < ehdr->e_shnum; i++) {
		if (shdr[i].sh_size == 0)
			continue;
		if (shdr[i].sh_type == SHT_DYNSYM) {
			dsym = (Elf_Sym *)(base + shdr[i].sh_offset);
			strtab = base + shdr[shdr[i].sh_link].sh_offset;
			symcnt = shdr[i].sh_size / sizeof(*dsym);
			break;
		}
	}
	MPASS(dsym != NULL);

	ehdr->e_ident[EI_OSABI] = ELFOSABI_LINUX;
	/*
	 * VDSO is readonly mapped to the process VA and
	 * can't be relocated by rtld.
	 */
	SLIST_FOREACH(lsym, &__elfN(linux_vdso_syms), sym) {
		for (i = 0, sym = dsym; i < symcnt; i++, sym++) {
			symname = strtab + sym->st_name;
			if (strncmp(lsym->symname, symname, lsym->size) == 0) {
				sym->st_value += offset;
				*lsym->ptr = sym->st_value;
				break;

			}
		}
	}
}

int
linux_map_vdso(struct proc *p, vm_object_t obj, vm_offset_t base,
    vm_offset_t size, struct image_params *imgp)
{
	struct vmspace *vmspace;
	vm_map_t map;
	int error;

	MPASS((imgp->sysent->sv_flags & SV_ABI_MASK) == SV_ABI_LINUX);
	MPASS(obj != NULL);

	vmspace = p->p_vmspace;
	map = &vmspace->vm_map;

	vm_object_reference(obj);
	error = vm_map_fixed(map, obj, 0, base, size,
	    VM_PROT_READ | VM_PROT_EXECUTE,
	    VM_PROT_READ | VM_PROT_EXECUTE,
	    MAP_INHERIT_SHARE | MAP_ACC_NO_CHARGE);
	if (error != KERN_SUCCESS) {
		vm_object_deallocate(obj);
		return (vm_mmap_to_errno(error));
	}
	return (0);
}
