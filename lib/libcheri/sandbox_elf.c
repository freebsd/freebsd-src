/*-
 * Copyright (c) 2014-2015 SRI International
 * Copyright (c) 2015 Robert N. M. Watson
 * All rights reserved.
 *
 * This software was developed by SRI International and the University of
 * Cambridge Computer Laboratory under DARPA/AFRL contract (FA8750-10-C-0237)
 * ("CTSRD"), as part of the DARPA CRASH research programme.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/types.h>
#include <sys/mman.h>
#include <sys/param.h>

#include <assert.h>
#include <elf.h>
#include <err.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "sandbox_elf.h"

#define	CHERI_CALLEE_SYM_PREFIX	"__cheri_callee_method."
#define	CHERI_CALLER_SYM_PREFIX	"__cheri_method."

ssize_t
sandbox_loadelf64(int fd, void *location, size_t maxsize, u_int flags)
{
	int i, prot;
	char *addr, *taddr;
	int maxoffset = 0;
	ssize_t rlen;
	size_t maplen, mappedbytes, offset, zerobytes;
	Elf64_Ehdr ehdr;
	Elf64_Phdr phdr;

	assert((intptr_t)location % PAGE_SIZE == 0);

	if ((rlen = pread(fd, &ehdr, sizeof(ehdr), 0)) != sizeof(ehdr)) {
		warn("%s: read ELF header", __func__);
		return (-1);
	}

	/* XXX: check for magic number */

#ifdef DEBUG
	printf("type %d\n", ehdr.e_type);
	printf("version %d\n", ehdr.e_version);
	printf("entry %p\n", (void *)ehdr.e_entry);
	printf("elf header size %jd (read %jd)\n", (intmax_t)ehdr.e_ehsize,
	    rlen);
	printf("program header offset %jd\n", (intmax_t)ehdr.e_phoff);
	printf("program header size %jd\n", (intmax_t)ehdr.e_phentsize);
	printf("program header number %jd\n", (intmax_t)ehdr.e_phnum);
	printf("section header offset %jd\n", (intmax_t)ehdr.e_shoff);
	printf("section header size %jd\n", (intmax_t)ehdr.e_shentsize);
	printf("section header number %jd\n", (intmax_t)ehdr.e_shnum);
	printf("section name strings section %jd\n", (intmax_t)ehdr.e_shstrndx);
#endif

	for (i = 0; i < ehdr.e_phnum; i++) {
		if ((rlen = pread(fd, &phdr, sizeof(phdr), ehdr.e_phoff +
		    ehdr.e_phentsize * i)) != sizeof(phdr)) {
			warn("%s: reading %d program header", __func__, i+1);
			return (-1);
		}
#ifdef DEBUG
		printf("phdr[%d] type        %jx\n", i, (intmax_t)phdr.p_type);
		printf("phdr[%d] flags       %jx (%c%c%c)\n", i,
		   (intmax_t)phdr.p_flags,
		   phdr.p_flags & PF_R ? 'r' : '-',
		   phdr.p_flags & PF_W ? 'w' : '-',
		   phdr.p_flags & PF_X ? 'x' : '-');
		printf("phdr[%d] offset      0x%0.16jx\n", i,
		    (intmax_t)phdr.p_offset);
		printf("phdr[%d] vaddr       0x%0.16jx\n", i,
		    (intmax_t)phdr.p_vaddr);
		printf("phdr[%d] file size   0x%0.16jx\n", i,
		    (intmax_t)phdr.p_filesz);
		printf("phdr[%d] memory size 0x%0.16jx\n", i,
		    (intmax_t)phdr.p_memsz);
#endif

		if (phdr.p_type != PT_LOAD) {
#ifdef DEBUG
			/* XXXBD: should we handled GNU_STACK? */
			printf("skipping program segment %d\n", i+1);
#endif
			continue;
		}

		/*
		 * Consider something 'data' if PF_X is unset; otherwise,
		 * consider it code.  Either way, load it only if requested by
		 * a suitable flag.
		 */
		if (phdr.p_flags & PF_X) {
#ifdef NOTYET
			/*
			 * XXXRW: Our current linker script will sometimes
			 * place data and code in the same page.  For now,
			 * map code into object instances.
			 */
			if (!(flags & SANDBOX_LOADELF_CODE))
				continue;
#endif
		} else {
			if (!(flags & SANDBOX_LOADELF_DATA))
				continue;
		}
		prot = (
		    (phdr.p_flags & PF_R ? PROT_READ : 0) |
		    (phdr.p_flags & PF_W ? PROT_WRITE : 0) |
		    (phdr.p_flags & PF_X ? PROT_EXEC : 0));
		taddr = (char *)rounddown2((phdr.p_vaddr + (intptr_t)location),
		    PAGE_SIZE);
		offset = rounddown2(phdr.p_offset, PAGE_SIZE);
		maplen = phdr.p_offset - rounddown2(phdr.p_offset, PAGE_SIZE)
		    + phdr.p_filesz;
		/* XXX-BD: rtld handles this, but I don't see why you would. */
		if (phdr.p_filesz != phdr.p_memsz && !(phdr.p_flags & PF_W)) {
			warnx("%s: segment %d expects 0 fill, but is not "
			    "writable, skipping", __func__, i+1);
			continue;
		}
#ifdef DEBUG
		printf("mapping 0x%zx bytes at %p, file offset 0x%zx\n",
		    maplen, taddr, offset);
#endif
		if((addr = mmap(taddr, maplen, prot,
		    MAP_FIXED | MAP_PRIVATE, fd, offset)) == MAP_FAILED) {
			warn("%s: mmap", __func__);
			return (-1);
		}
		maxoffset = MAX((size_t)maxoffset, phdr.p_vaddr + phdr.p_memsz);

		/* If we've mapped everything directly we're done. */
		if (phdr.p_filesz == phdr.p_memsz)
			continue;

		/* Zero any remaining bits of the last page */
		mappedbytes = roundup2(maplen, PAGE_SIZE);
		zerobytes = mappedbytes - maplen;
		memset(taddr + maplen, 0, zerobytes);

		/* If everything fit it the mapped range we're done */
		if (phdr.p_memsz <= mappedbytes)
			continue;

		taddr = taddr + mappedbytes;
		maplen = (phdr.p_offset - rounddown2(phdr.p_offset, PAGE_SIZE)) +
		    phdr.p_memsz - mappedbytes;
#ifdef DEBUG
		printf("mapping 0x%zx bytes at 0x%p\n", maplen, taddr);
#endif
		if (mmap(taddr, maplen, prot,
		    MAP_FIXED | MAP_ANON, -1, 0) == MAP_FAILED) {
			warn("%s: mmap (anon)", __func__);
			return (-1);
		}

	}

	return (maxoffset);
}

#ifdef TEST_LOADELF64
int
main(int argc, char **argv)
{
	void *base;
	ssize_t len;
	size_t maxlen;
	int fd;

	if (argc != 2)
		errx(1, "usage: elf_loader <file>");

	maxlen = 10 * 1024 * 1024;
	base = mmap(NULL, maxlen, 0, MAP_ANON, -1, 0);
	if (base == MAP_FAILED)
		err(1, "%s: mmap region", __func__);

	if ((fd = open(argv[1], O_RDONLY)) == -1)
		err(1, "%s: open(%s)", __func__, argv[1]);

	if ((len = sandbox_loadelf64(fd, base, maxlen, SANDBOX_LOADELF_CODE))
	    == -1)
		err(1, "%s: sandbox_loadelf64", __func__);
	printf("mapped %jd bytes from %s\n", len, argv[1]);

	return (0);
}
#endif
