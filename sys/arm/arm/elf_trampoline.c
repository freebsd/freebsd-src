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
#include <sys/inflate.h>
#include <machine/elf.h>
#include <machine/pte.h>

#include <stdlib.h>

#include "opt_global.h"
#include "opt_kernname.h"

extern char kernel_start[];
extern char kernel_end[];

void __start(void);

#define GZ_HEAD	0xa

static __inline void *
memcpy(void *dst, const void *src, int len)
{
	const char *s = src;
    	char *d = dst;

	while (len) {
		if (0 && len >= 4 && !((vm_offset_t)d & 3) &&
		    !((vm_offset_t)s & 3)) {
			*(uint32_t *)d = *(uint32_t *)s;
			s += 4;
			d += 4;
			len -= 4;
		} else {
			*d++ = *s++;
			len--;
		}
	}
	return (dst);
}

static __inline void
bzero(void *addr, int count)
{
	char *tmp = (char *)addr;

	while (count > 0) {
		if (count >= 4 && !((vm_offset_t)tmp & 3)) {
			*(uint32_t *)tmp = 0;
			tmp += 4;
			count -= 4;
		} else {
			*tmp = 0;
			tmp++;
			count--;
		}
	}
}

void
_start(void)
{
	int physaddr = KERNPHYSADDR;
	int tmp1;
	
	__asm __volatile("adr %0, 2f\n"
	    		 "bic %0, %0, #0xff000000\n"
			 "bic sp, sp, #0xff000000\n"
			 "and %1, %1, #0xff000000\n"
			 "orr %0, %0, %1\n"
			 "orr sp, sp, %1\n"
			 "mrc p15, 0, %1, c1, c0, 0\n"
			 "bic %1, %1, #1\n" /* Disable MMU */
			 "orr %1, %1, #(4 | 8)\n" /* Add DC enable, 
						     WBUF enable */
			 "orr %1, %1, #0x1000\n" /* Add IC enable */
			 "orr %1, %1, #(0x800)\n" /* BPRD enable */

			 "mcr p15, 0, %1, c1, c0, 0\n"
			 "nop\n"
			 "nop\n"
			 "nop\n"
			 "mov pc, %0\n"
			 "2: nop\n"
			 : "=r" (tmp1), "+r" (physaddr));
	__start();
}

#ifdef KZIP
static  unsigned char *orig_input, *i_input, *i_output;


static u_int memcnt;		/* Memory allocated: blocks */
static size_t memtot;		/* Memory allocated: bytes */
/*
 * Library functions required by inflate().
 */

#define MEMSIZ 0x8000

/*
 * Allocate memory block.
 */
unsigned char *
kzipmalloc(int size)
{
	void *ptr;
	static u_char mem[MEMSIZ];

	if (memtot + size > MEMSIZ)
		return NULL;
	ptr = mem + memtot;
	memtot += size;
	memcnt++;
	return ptr;
}

/*
 * Free allocated memory block.
 */
void
kzipfree(void *ptr)
{
	memcnt--;
	if (!memcnt)
		memtot = 0;
}

void
putstr(char *dummy)
{
}

static int
input(void *dummy)
{
	if ((size_t)(i_input - orig_input) >= KERNSIZE) {
		return (GZ_EOF);
	}
	return *i_input++;
}

static int
output(void *dummy, unsigned char *ptr, unsigned long len)
{

	memcpy(i_output, ptr, len);
	i_output += len;
	return (0);
}

static void *
inflate_kernel(void *kernel, void *startaddr)
{
	struct inflate infl;
	char slide[GZ_WSIZE];

	orig_input = kernel;
	memcnt = memtot = 0;
	i_input = (char *)kernel + GZ_HEAD;
	if (((char *)kernel)[3] & 0x18) {
		while (*i_input)
			i_input++;
		i_input++;
	}
	i_output = startaddr;
	bzero(&infl, sizeof(infl));
	infl.gz_input = input;
	infl.gz_output = output;
	infl.gz_slide = slide;
	inflate(&infl);
	return ((char *)(((vm_offset_t)i_output & ~3) + 4));
}

#endif

void *
load_kernel(unsigned int kstart, unsigned int curaddr,unsigned int func_end, 
    int d)
{
	Elf32_Ehdr *eh;
	Elf32_Phdr phdr[512] /* XXX */, *php;
	Elf32_Shdr shdr[512] /* XXX */;
	int i,j;
	void *entry_point;
	int symtabindex = -1;
	int symstrindex = -1;
	vm_offset_t lastaddr = 0;
	Elf_Addr ssym = 0, esym = 0;
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
	
	/* Save the symbol tables, as there're about to be scratched. */
	memcpy(shdr, (void *)(kstart + eh->e_shoff),
	    sizeof(*shdr) * eh->e_shnum);
	if (eh->e_shnum * eh->e_shentsize != 0 &&
	    eh->e_shoff != 0) {
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
			} else {
				lastaddr += shdr[symtabindex].sh_size;
				lastaddr = roundup(lastaddr,
				    sizeof(shdr[symtabindex].sh_size));
				lastaddr += sizeof(shdr[symstrindex].sh_size);
				lastaddr += shdr[symstrindex].sh_size;
				lastaddr = roundup(lastaddr, 
				    sizeof(shdr[symstrindex].sh_size));
			}
			
		}
	}
	if (!d)
		return ((void *)lastaddr);
	
	j = eh->e_phnum;
	for (i = 0; i < j; i++) {
		volatile char c;

		if (phdr[i].p_type != PT_LOAD)
			continue;
		memcpy((void *)(phdr[i].p_vaddr - KERNVIRTADDR + curaddr),
		    (void*)(kstart + phdr[i].p_offset), phdr[i].p_filesz);
		/* Clean space from oversized segments, eg: bss. */
		if (phdr[i].p_filesz < phdr[i].p_memsz)
			bzero((void *)(phdr[i].p_vaddr - KERNVIRTADDR + 
			    curaddr + phdr[i].p_filesz), phdr[i].p_memsz -
			    phdr[i].p_filesz);
	}
	/* Now grab the symbol tables. */
	if (symtabindex >= 0 && symstrindex >= 0) {
		*(Elf_Size *)lastaddr = 
		    shdr[symtabindex].sh_size;
		lastaddr += sizeof(shdr[symtabindex].sh_size);
		memcpy((void*)lastaddr,
		    (void *)func_end,
		    shdr[symtabindex].sh_size);
		lastaddr += shdr[symtabindex].sh_size;
		lastaddr = roundup(lastaddr,
		    sizeof(shdr[symtabindex].sh_size));
		*(Elf_Size *)lastaddr =
		    shdr[symstrindex].sh_size;
		lastaddr += sizeof(shdr[symstrindex].sh_size);
		memcpy((void*)lastaddr,
		    (void*)(func_end +
			    shdr[symtabindex].sh_size),
		    shdr[symstrindex].sh_size);
		lastaddr += shdr[symstrindex].sh_size;
		lastaddr = roundup(lastaddr, 
   		    sizeof(shdr[symstrindex].sh_size));
		*(Elf_Addr *)curaddr = MAGIC_TRAMP_NUMBER;
		*((Elf_Addr *)curaddr + 1) = ssym - curaddr + KERNVIRTADDR;
		*((Elf_Addr *)curaddr + 2) = lastaddr - curaddr + KERNVIRTADDR;
	} else
		*(Elf_Addr *)curaddr = 0;
	/* Invalidate the instruction cache. */
	__asm __volatile("mcr p15, 0, %0, c7, c5, 0\n"
	    		 "mcr p15, 0, %0, c7, c10, 4\n"
			 : : "r" (curaddr));
	/* Jump to the entry point. */
	((void(*)(void))(entry_point - KERNVIRTADDR + curaddr))();
	__asm __volatile(".globl func_end\n"
	    "func_end:");
	
}

extern char func_end[];

extern void *_end;

#define PMAP_DOMAIN_KERNEL	15 /*
				    * Just define it instead of including the
				    * whole VM headers set.
				    */
int __hack;
static __inline void
setup_pagetables(unsigned int pt_addr, vm_paddr_t physstart, vm_paddr_t physend)
{
	unsigned int *pd = (unsigned int *)pt_addr;
	vm_paddr_t addr;
	int domain = (DOMAIN_CLIENT << (PMAP_DOMAIN_KERNEL * 2)) | DOMAIN_CLIENT;
	int tmp;

	bzero(pd, L1_TABLE_SIZE);
	for (addr = physstart; addr < physend; addr += L1_S_SIZE)
		pd[addr >> L1_S_SHIFT] = L1_TYPE_S|L1_S_C|L1_S_AP(AP_KRW)|
		    L1_S_DOM(PMAP_DOMAIN_KERNEL) | addr;
	/* XXX: See below */
	if (0xfff00000 < physstart || 0xfff00000 > physend)
		pd[0xfff00000 >> L1_S_SHIFT] = L1_TYPE_S|L1_S_AP(AP_KRW)|
		    L1_S_DOM(PMAP_DOMAIN_KERNEL)|physstart;
	__asm __volatile("mcr p15, 0, %1, c2, c0, 0\n" /* set TTB */
	    		 "mcr p15, 0, %1, c8, c7, 0\n" /* Flush TTB */
			 "mcr p15, 0, %2, c3, c0, 0\n" /* Set DAR */
			 "mrc p15, 0, %0, c1, c0, 0\n"
			 "orr %0, %0, #1\n" /* MMU_ENABLE */
			 "mcr p15, 0, %0, c1, c0, 0\n"
			 "mrc p15, 0, %0, c2, c0, 0\n" /* CPWAIT */
			 "mov r0, r0\n"
			 "sub pc, pc, #4\n" :
			 "=r" (tmp) : "r" (pd), "r" (domain));
	
	/* 
	 * XXX: This is the most stupid workaround I've ever wrote.
	 * For some reason, the KB9202 won't boot the kernel unless
	 * we access an address which is not in the 
	 * 0x20000000 - 0x20ffffff range. I hope I'll understand
	 * what's going on later.
	 */
	__hack = *(volatile int *)0xfffff21c;
}

void
__start(void)
{
	void *curaddr;
	void *dst;
	char *kernel = (char *)&kernel_start;

	__asm __volatile("mov %0, pc"  :
	    "=r" (curaddr));
	curaddr = (void*)((unsigned int)curaddr & 0xfff00000);
#ifdef KZIP
	if (*kernel == 0x1f && kernel[1] == 0x8b) {
		int pt_addr = (((int)&_end + KERNSIZE + 0x100) & 
		    ~(L1_TABLE_SIZE - 1)) + L1_TABLE_SIZE;
		setup_pagetables(pt_addr, (vm_paddr_t)curaddr,
		    (vm_paddr_t)curaddr + 0x10000000);
		/* Gzipped kernel */
		dst = inflate_kernel(kernel, &_end);
		kernel = (char *)&_end;
	} else
#endif
		dst = 4 + load_kernel((unsigned int)&kernel_start, 
	    (unsigned int)curaddr, 
	    (unsigned int)&func_end, 0);
	memcpy((void *)dst, (void *)&load_kernel, (unsigned int)&func_end - 
	    (unsigned int)&load_kernel);
	((void (*)())dst)((unsigned int)kernel, 
			  (unsigned int)curaddr,
			  dst + (unsigned int)(&func_end) - 
			  (unsigned int)(&load_kernel), 1);
}
