/*-
 * Copyright 1996-1998 John D. Polstra.
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
#include <sys/mman.h>
#include <sys/stat.h>

#include <errno.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "rtld.h"

static int protflags(int);	/* Elf flags -> mmap protection */

/*
 * Map a shared object into memory.  The "fd" argument is a file descriptor,
 * which must be open on the object and positioned at its beginning.
 * The "path" argument is a pathname that is used only for error messages.
 *
 * The return value is a pointer to a newly-allocated Obj_Entry structure
 * for the shared object.  Returns NULL on failure.
 */
Obj_Entry *
map_object(int fd, const char *path, const struct stat *sb)
{
    Obj_Entry *obj;
    union {
	Elf_Ehdr hdr;
	char buf[PAGE_SIZE];
    } u;
    int nbytes;
    Elf_Phdr *phdr;
    Elf_Phdr *phlimit;
    Elf_Phdr *segs[2];
    int nsegs;
    Elf_Phdr *phdyn;
    Elf_Phdr *phphdr;
    Elf_Phdr *phinterp;
    caddr_t mapbase;
    size_t mapsize;
    Elf_Off base_offset;
    Elf_Addr base_vaddr;
    Elf_Addr base_vlimit;
    caddr_t base_addr;
    Elf_Off data_offset;
    Elf_Addr data_vaddr;
    Elf_Addr data_vlimit;
    caddr_t data_addr;
    Elf_Addr clear_vaddr;
    caddr_t clear_addr;
    size_t nclear;
    Elf_Addr bss_vaddr;
    Elf_Addr bss_vlimit;
    caddr_t bss_addr;

    if ((nbytes = read(fd, u.buf, PAGE_SIZE)) == -1) {
	_rtld_error("%s: read error: %s", path, strerror(errno));
	return NULL;
    }

    /* Make sure the file is valid */
    if (nbytes < sizeof(Elf_Ehdr)
      || u.hdr.e_ident[EI_MAG0] != ELFMAG0
      || u.hdr.e_ident[EI_MAG1] != ELFMAG1
      || u.hdr.e_ident[EI_MAG2] != ELFMAG2
      || u.hdr.e_ident[EI_MAG3] != ELFMAG3) {
	_rtld_error("%s: invalid file format", path);
	return NULL;
    }
    if (u.hdr.e_ident[EI_CLASS] != ELF_TARG_CLASS
      || u.hdr.e_ident[EI_DATA] != ELF_TARG_DATA) {
	_rtld_error("%s: unsupported file layout", path);
	return NULL;
    }
    if (u.hdr.e_ident[EI_VERSION] != EV_CURRENT
      || u.hdr.e_version != EV_CURRENT) {
	_rtld_error("%s: unsupported file version", path);
	return NULL;
    }
    if (u.hdr.e_type != ET_EXEC && u.hdr.e_type != ET_DYN) {
	_rtld_error("%s: unsupported file type", path);
	return NULL;
    }
    if (u.hdr.e_machine != ELF_TARG_MACH) {
	_rtld_error("%s: unsupported machine", path);
	return NULL;
    }

    /*
     * We rely on the program header being in the first page.  This is
     * not strictly required by the ABI specification, but it seems to
     * always true in practice.  And, it simplifies things considerably.
     */
    if (u.hdr.e_phentsize != sizeof(Elf_Phdr)) {
	_rtld_error(
	  "%s: invalid shared object: e_phentsize != sizeof(Elf_Phdr)", path);
	return NULL;
    }
    if (u.hdr.e_phoff + u.hdr.e_phnum*sizeof(Elf_Phdr) > nbytes) {
	_rtld_error("%s: program header too large", path);
	return NULL;
    }

    /*
     * Scan the program header entries, and save key information.
     *
     * We rely on there being exactly two load segments, text and data,
     * in that order.
     */
    phdr = (Elf_Phdr *) (u.buf + u.hdr.e_phoff);
    phlimit = phdr + u.hdr.e_phnum;
    nsegs = 0;
    phdyn = phphdr = phinterp = NULL;
    while (phdr < phlimit) {
	switch (phdr->p_type) {

	case PT_INTERP:
	    phinterp = phdr;
	    break;

	case PT_LOAD:
	    if (nsegs >= 2) {
		_rtld_error("%s: too many PT_LOAD segments", path);
		return NULL;
	    }
	    segs[nsegs] = phdr;
	    ++nsegs;
	    break;

	case PT_PHDR:
	    phphdr = phdr;
	    break;

	case PT_DYNAMIC:
	    phdyn = phdr;
	    break;
	}

	++phdr;
    }
    if (phdyn == NULL) {
	_rtld_error("%s: object is not dynamically-linked", path);
	return NULL;
    }

    if (nsegs < 2) {
	_rtld_error("%s: too few PT_LOAD segments", path);
	return NULL;
    }
    if (segs[0]->p_align < PAGE_SIZE || segs[1]->p_align < PAGE_SIZE) {
	_rtld_error("%s: PT_LOAD segments not page-aligned", path);
	return NULL;
    }

    /*
     * Map the entire address space of the object, to stake out our
     * contiguous region, and to establish the base address for relocation.
     */
    base_offset = trunc_page(segs[0]->p_offset);
    base_vaddr = trunc_page(segs[0]->p_vaddr);
    base_vlimit = round_page(segs[1]->p_vaddr + segs[1]->p_memsz);
    mapsize = base_vlimit - base_vaddr;
    base_addr = u.hdr.e_type == ET_EXEC ? (caddr_t) base_vaddr : NULL;

    mapbase = mmap(base_addr, mapsize, protflags(segs[0]->p_flags),
      MAP_PRIVATE, fd, base_offset);
    if (mapbase == (caddr_t) -1) {
	_rtld_error("%s: mmap of entire address space failed: %s",
	  path, strerror(errno));
	return NULL;
    }
    if (base_addr != NULL && mapbase != base_addr) {
	_rtld_error("%s: mmap returned wrong address: wanted %p, got %p",
	  path, base_addr, mapbase);
	munmap(mapbase, mapsize);
	return NULL;
    }

    /* Overlay the data segment onto the proper region. */
    data_offset = trunc_page(segs[1]->p_offset);
    data_vaddr = trunc_page(segs[1]->p_vaddr);
    data_vlimit = round_page(segs[1]->p_vaddr + segs[1]->p_filesz);
    data_addr = mapbase + (data_vaddr - base_vaddr);
    if (mmap(data_addr, data_vlimit - data_vaddr, protflags(segs[1]->p_flags),
      MAP_PRIVATE|MAP_FIXED, fd, data_offset) == (caddr_t) -1) {
	_rtld_error("%s: mmap of data failed: %s", path, strerror(errno));
	return NULL;
    }

    /* Clear any BSS in the last page of the data segment. */
    clear_vaddr = segs[1]->p_vaddr + segs[1]->p_filesz;
    clear_addr = mapbase + (clear_vaddr - base_vaddr);
    if ((nclear = data_vlimit - clear_vaddr) > 0)
	memset(clear_addr, 0, nclear);

    /* Overlay the BSS segment onto the proper region. */
    bss_vaddr = data_vlimit;
    bss_vlimit = round_page(segs[1]->p_vaddr + segs[1]->p_memsz);
    bss_addr = mapbase +  (bss_vaddr - base_vaddr);
    if (bss_vlimit > bss_vaddr) {	/* There is something to do */
	if (mmap(bss_addr, bss_vlimit - bss_vaddr, protflags(segs[1]->p_flags),
	  MAP_PRIVATE|MAP_FIXED|MAP_ANON, -1, 0) == (caddr_t) -1) {
	    _rtld_error("%s: mmap of bss failed: %s", path, strerror(errno));
	    return NULL;
	}
    }

    obj = obj_new();
    if (sb != NULL) {
	obj->dev = sb->st_dev;
	obj->ino = sb->st_ino;
    }
    obj->mapbase = mapbase;
    obj->mapsize = mapsize;
    obj->textsize = round_page(segs[0]->p_vaddr + segs[0]->p_memsz) -
      base_vaddr;
    obj->vaddrbase = base_vaddr;
    obj->relocbase = mapbase - base_vaddr;
    obj->dynamic = (const Elf_Dyn *) (obj->relocbase + phdyn->p_vaddr);
    if (u.hdr.e_entry != 0)
	obj->entry = (caddr_t) (obj->relocbase + u.hdr.e_entry);
    if (phphdr != NULL) {
	obj->phdr = (const Elf_Phdr *) (obj->relocbase + phphdr->p_vaddr);
	obj->phsize = phphdr->p_memsz;
    }
    if (phinterp != NULL)
	obj->interp = (const char *) (obj->relocbase + phinterp->p_vaddr);

    return obj;
}

void
obj_free(Obj_Entry *obj)
{
    Objlist_Entry *elm;

    free(obj->path);
    while (obj->needed != NULL) {
	Needed_Entry *needed = obj->needed;
	obj->needed = needed->next;
	free(needed);
    }
    while (!STAILQ_EMPTY(&obj->dldags)) {
	elm = STAILQ_FIRST(&obj->dldags);
	STAILQ_REMOVE_HEAD(&obj->dldags, link);
	free(elm);
    }
    while (!STAILQ_EMPTY(&obj->dagmembers)) {
	elm = STAILQ_FIRST(&obj->dagmembers);
	STAILQ_REMOVE_HEAD(&obj->dagmembers, link);
	free(elm);
    }
    free(obj);
}

Obj_Entry *
obj_new(void)
{
    Obj_Entry *obj;

    obj = CNEW(Obj_Entry);
    STAILQ_INIT(&obj->dldags);
    STAILQ_INIT(&obj->dagmembers);
    return obj;
}

/*
 * Given a set of ELF protection flags, return the corresponding protection
 * flags for MMAP.
 */
static int
protflags(int elfflags)
{
    int prot = 0;
    if (elfflags & PF_R)
	prot |= PROT_READ;
    if (elfflags & PF_W)
	prot |= PROT_WRITE;
    if (elfflags & PF_X)
	prot |= PROT_EXEC;
    return prot;
}
