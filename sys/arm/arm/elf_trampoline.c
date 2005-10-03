/*-
 * Copyright (c) 2005 Olivier Houchard.  All rights reserved.
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");
#include <machine/asm.h>
#include <sys/types.h>
#include <sys/elf32.h>
#include <sys/param.h>
#include <machine/elf.h>
#include <stdlib.h>

#include "opt_global.h"

extern char kernel_start[];
extern char kernel_end[];

static __inline void *
memcpy(void *dst, const void *src, int len)
{
	const char *s = src;
    	char *d = dst;
	
	while (len--) {
		*d++ = *s++;
	}
	return (dst);
}

static __inline void
bzero(char *addr, int count)
{
	while (count > 0) {
		*addr = 0;
		addr++;
		count--;
	}
}

void *
load_kernel(unsigned int kstart, unsigned int kend, unsigned int curaddr,unsigned int func_end, int d)
{
	Elf32_Ehdr *eh;
	Elf32_Phdr phdr[512] /* XXX */, *php;
	Elf32_Shdr *shdr;
	int i,j;
	void *entry_point;
	int symtabindex;
	int symstrindex;
	vm_offset_t lastaddr = 0;
	Elf_Addr ssym, esym;
	Elf_Dyn *dp;
	
	eh = (Elf32_Ehdr *)kstart;
	ssym = esym = 0;
	entry_point = (void*)eh->e_entry;
	memcpy(phdr, (void *)(kstart + eh->e_phoff ),
	    eh->e_phnum * sizeof(phdr[0]));
	/* Determine lastaddr. */
	for (i = 0; i < eh->e_phnum; i++) {
		if (lastaddr < (phdr[i].p_vaddr - KERNVIRTADDR + curaddr
		    + phdr[i].p_memsz))
			lastaddr = phdr[i].p_vaddr - KERNVIRTADDR +
			    curaddr + phdr[i].p_memsz;
	}
	
	/* Now grab the symbol tables. */
	lastaddr = roundup(lastaddr, sizeof(long));
	if (eh->e_shnum * eh->e_shentsize != 0 &&
	    eh->e_shoff != 0) {
		symtabindex = symstrindex = -1;
		shdr = (Elf_Shdr *)(kstart + eh->e_shoff);
		for (i = 0; i < eh->e_shnum; i++) {
			if (shdr[i].sh_type == SHT_SYMTAB) {
				for (j = 0; j < eh->e_phnum; j++) {
					if (phdr[j].p_type == PT_LOAD &&
					    shdr[i].sh_offset >=
					    phdr[j].p_offset &&
					    (shdr[i].sh_offset + 
					     shdr[i].sh_size <=
					     phdr[j].p_offset +
					     phdr[j].p_filesz)) {
						shdr[i].sh_offset = 0;
						shdr[i].sh_size = 0;
						j = eh->e_phnum;
					}
				}
				if (shdr[i].sh_offset != 0 && 
				    shdr[i].sh_size != 0) {
					symtabindex = i;
					symstrindex = shdr[i].sh_link;
				}
			}
		}
		func_end = roundup(func_end, sizeof(long));
		if (symtabindex >= 0 && symstrindex >= 0) {
			ssym = lastaddr;
			if (d) {
				memcpy((void *)func_end, (void *)(
				    shdr[symtabindex].sh_offset + kstart), 
				    shdr[symtabindex].sh_size);
				memcpy((void *)(func_end +
				    shdr[symtabindex].sh_size),
				    (void *)(shdr[symstrindex].sh_offset +
				    kstart), shdr[symstrindex].sh_size);
				*(Elf_Size *)lastaddr = 
				    shdr[symtabindex].sh_size;
			}
			lastaddr += sizeof(shdr[symtabindex].sh_size);
			if (d)
				memcpy((void*)lastaddr,
				    (void *)func_end,
 				    shdr[symtabindex].sh_size);
			lastaddr += shdr[symtabindex].sh_size;
			lastaddr = roundup(lastaddr,
			    sizeof(shdr[symtabindex].sh_size));
			if (d)
				*(Elf_Size *)lastaddr =
				    shdr[symstrindex].sh_size;
			lastaddr += sizeof(shdr[symstrindex].sh_size);
			if (d)
				memcpy((void*)lastaddr,
				    (void*)(func_end +
					    shdr[symtabindex].sh_size),
				    shdr[symstrindex].sh_size);
			lastaddr += shdr[symstrindex].sh_size;
			lastaddr = roundup(lastaddr, 
			    sizeof(shdr[symstrindex].sh_size));
			
		}
	}
	if (!d)
		return ((void *)lastaddr);
	
	j = eh->e_phnum;
	for (i = 0; i < j; i++) {
		volatile char c;
		if (phdr[i].p_type != PT_LOAD) {
			continue;
		}
		memcpy((void *)(phdr[i].p_vaddr - KERNVIRTADDR + curaddr),
		    (void*)(kstart + phdr[i].p_offset), phdr[i].p_filesz);
		/* Clean space from oversized segments, eg: bss. */
		if (phdr[i].p_filesz < phdr[i].p_memsz)
			bzero((void *)(phdr[i].p_vaddr - KERNVIRTADDR + 
			    curaddr + phdr[i].p_filesz), phdr[i].p_memsz -
			    phdr[i].p_filesz);
	}
	/* Now grab the symbol tables. */
	*(Elf_Addr *)curaddr = ssym - curaddr + KERNVIRTADDR;
	*((Elf_Addr *)curaddr + 1) = lastaddr - curaddr + KERNVIRTADDR;
	/* Jump to the entry point. */
	((void(*)(void))(entry_point - KERNVIRTADDR + curaddr))();
	__asm __volatile(".globl func_end\n"
	    "func_end:");
	
}

extern char func_end[];

int _start(void)
{
	void *curaddr;

	__asm __volatile("mov %0, pc"  :
	    "=r" (curaddr));
	curaddr = (void*)((unsigned int)curaddr & 0xfff00000);
	void *dst = 4 + load_kernel((unsigned int)&kernel_start, 
	    (unsigned int)&kernel_end, (unsigned int)curaddr, 
	    (unsigned int)&func_end, 0);
	memcpy((void *)dst, (void *)&load_kernel, (unsigned int)&func_end - 
	    (unsigned int)&load_kernel);
	((void (*)())dst)((unsigned int)&kernel_start, 
			  (unsigned int)&kernel_end, (unsigned int)curaddr,
			  dst + (unsigned int)&func_end - 
			  (unsigned int)(&load_kernel),1);
}
