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
 *      $Id: map_object.c,v 1.2 1998/03/06 22:14:53 jdp Exp $
 */

#include <sys/param.h>
#include <sys/mman.h>

#include <assert.h>
#include <errno.h>
#include <stddef.h>
#include <string.h>
#include <unistd.h>

#include "rtld.h"

static int protflags(int);	/* Elf flags -> mmap protection */

/*
 * Map a shared object into memory.  The argument is a file descriptor,
 * which must be open on the object and positioned at its beginning.
 *
 * The return value is a pointer to a newly-allocated Obj_Entry structure
 * for the shared object.  Returns NULL on failure.
 */
Obj_Entry *
map_object(int fd)
{
    Obj_Entry *obj;
    union {
	Elf32_Ehdr hdr;
	char buf[PAGE_SIZE];
    } u;
    int nbytes;
    Elf32_Phdr *phdr;
    Elf32_Phdr *phlimit;
    Elf32_Phdr *segs[2];
    int nsegs;
    Elf32_Phdr *phdyn;
    Elf32_Phdr *phphdr;
    caddr_t mapbase;
    size_t mapsize;
    Elf32_Off base_offset;
    Elf32_Addr base_vaddr;
    Elf32_Addr base_vlimit;
    caddr_t base_addr;
    Elf32_Off data_offset;
    Elf32_Addr data_vaddr;
    Elf32_Addr data_vlimit;
    caddr_t data_addr;
    Elf32_Addr clear_vaddr;
    caddr_t clear_addr;
    size_t nclear;
    Elf32_Addr bss_vaddr;
    Elf32_Addr bss_vlimit;
    caddr_t bss_addr;

    if ((nbytes = read(fd, u.buf, PAGE_SIZE)) == -1) {
	_rtld_error("Read error: %s", strerror(errno));
	return NULL;
    }

    /* Make sure the file is valid */
    if (nbytes < sizeof(Elf32_Ehdr)
      || u.hdr.e_ident[EI_MAG0] != ELFMAG0
      || u.hdr.e_ident[EI_MAG1] != ELFMAG1
      || u.hdr.e_ident[EI_MAG2] != ELFMAG2
      || u.hdr.e_ident[EI_MAG3] != ELFMAG3) {
	_rtld_error("Invalid file format");
	return NULL;
    }
    if (u.hdr.e_ident[EI_CLASS] != ELFCLASS32
      || u.hdr.e_ident[EI_DATA] != ELFDATA2LSB) {
	_rtld_error("Unsupported file layout");
	return NULL;
    }
    if (u.hdr.e_ident[EI_VERSION] != EV_CURRENT
      || u.hdr.e_version != EV_CURRENT) {
	_rtld_error("Unsupported file version");
	return NULL;
    }
    if (u.hdr.e_type != ET_EXEC && u.hdr.e_type != ET_DYN) {
	_rtld_error("Unsupported file type");
	return NULL;
    }
    if (u.hdr.e_machine != EM_386) {
	_rtld_error("Unsupported machine");
	return NULL;
    }

    /*
     * We rely on the program header being in the first page.  This is
     * not strictly required by the ABI specification, but it seems to
     * always true in practice.  And, it simplifies things considerably.
     */
    assert(u.hdr.e_phentsize == sizeof(Elf32_Phdr));
    assert(u.hdr.e_phoff + u.hdr.e_phnum*sizeof(Elf32_Phdr) <= PAGE_SIZE);
    assert(u.hdr.e_phoff + u.hdr.e_phnum*sizeof(Elf32_Phdr) <= nbytes);

    /*
     * Scan the program header entries, and save key information.
     *
     * We rely on there being exactly two load segments, text and data,
     * in that order.
     */
    phdr = (Elf32_Phdr *) (u.buf + u.hdr.e_phoff);
    phlimit = phdr + u.hdr.e_phnum;
    nsegs = 0;
    phdyn = NULL;
    phphdr = NULL;
    while (phdr < phlimit) {
	switch (phdr->p_type) {

	case PT_LOAD:
	    assert(nsegs < 2);
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
	_rtld_error("Object is not dynamically-linked");
	return NULL;
    }

    assert(nsegs == 2);
    assert(segs[0]->p_align <= PAGE_SIZE);
    assert(segs[1]->p_align <= PAGE_SIZE);

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
	_rtld_error("mmap of entire address space failed: %s",
	  strerror(errno));
	return NULL;
    }
    if (base_addr != NULL && mapbase != base_addr) {
	_rtld_error("mmap returned wrong address: wanted %p, got %p",
	  base_addr, mapbase);
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
	_rtld_error("mmap of data failed: %s", strerror(errno));
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
	    _rtld_error("mmap of bss failed: %s", strerror(errno));
	    return NULL;
	}
    }

    obj = CNEW(Obj_Entry);
    obj->mapbase = mapbase;
    obj->mapsize = mapsize;
    obj->textsize = round_page(segs[0]->p_vaddr + segs[0]->p_memsz) -
      base_vaddr;
    obj->vaddrbase = base_vaddr;
    obj->relocbase = mapbase - base_vaddr;
    obj->dynamic = (const Elf32_Dyn *)
      (mapbase + (phdyn->p_vaddr - base_vaddr));
    if (u.hdr.e_entry != 0)
	obj->entry = (caddr_t) (mapbase + (u.hdr.e_entry - base_vaddr));
    if (phphdr != NULL) {
	obj->phdr = (const Elf32_Phdr *)
	  (mapbase + (phphdr->p_vaddr - base_vaddr));
	obj->phsize = phphdr->p_memsz;
    }

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
