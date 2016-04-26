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
#include <sys/queue.h>

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

STAILQ_HEAD(sandbox_map_head, sandbox_map_entry);

struct sandbox_map_entry {
	STAILQ_ENTRY(sandbox_map_entry)	sme_entries;

	size_t	sme_map_offset;		/* Offset to sandbox start */
	size_t	sme_len;		/* Length of mapping */
	int	sme_prot;		/* Page protections */
	int	sme_flags;		/* Mmap flags */
	int	sme_fd;			/* File */
	off_t	sme_file_offset;	/* Offset in file */
	size_t	sme_tailbytes;		/* Bytes to zero on last page */
};

struct sandbox_map {
	struct sandbox_map_head	sm_head;
	size_t			sm_minoffset;
	size_t			sm_maxoffset;
};

static struct sandbox_map_entry *
sandbox_map_entry_new(size_t map_offset, size_t len, int prot, int flags,
    int fd, off_t file_offset, size_t tailbytes)
{
	struct sandbox_map_entry *sme;

	if ((sme = calloc(1, sizeof(*sme))) == NULL) {
		warn("%s: calloc", __func__);
		return (NULL);
	}
	sme->sme_map_offset = map_offset;
	sme->sme_len = roundup2(len, PAGE_SIZE);
	sme->sme_prot = prot;
	sme->sme_flags = flags;
	sme->sme_fd = fd;
	sme->sme_file_offset = file_offset;
	sme->sme_tailbytes = tailbytes;

	return (sme);
}

static void *
sandbox_map_entry_mmap(void *base, struct sandbox_map_entry *sme, int add_prot)
{
	caddr_t taddr;
	void *addr;

	taddr = (caddr_t)base + sme->sme_map_offset;
#ifdef DEBUG
	if (sme->sme_fd > -1)
		printf("mapping 0x%zx bytes at %p, file offset 0x%zx\n",
		    sme->sme_len, taddr, sme->sme_file_offset);
	else
		printf("mapping 0x%zx bytes at 0x%p\n", sme->sme_len, taddr);
#endif
	if ((addr = mmap(taddr, sme->sme_len, sme->sme_prot | add_prot,
	    sme->sme_flags, sme->sme_fd, sme->sme_file_offset)) ==
	    sandbox_map_entry_mmap) {
		warn("%s: mmap", __func__);
		return (addr);
	}
	assert((vaddr_t)addr == (vaddr_t)taddr);

	memset(addr + sme->sme_len - sme->sme_tailbytes, 0, sme->sme_tailbytes);

	return (addr);
}

static int
sandbox_map_entry_mprotect(void *base, struct sandbox_map_entry *sme)
{
	caddr_t taddr;

	taddr = (caddr_t)base + sme->sme_map_offset;
	if (mprotect(taddr, sme->sme_len, sme->sme_prot) != 0) {
		warn("%s: mprotect", __func__);
		return (-1);
	}
	return (0);
}

int
sandbox_map_load(void *base, struct sandbox_map *sm)
{
	struct sandbox_map_entry *sme;

	STAILQ_FOREACH(sme, &sm->sm_head, sme_entries) {
		if (sandbox_map_entry_mmap(base, sme, PROT_WRITE) == MAP_FAILED)
			return (-1);
	}
	return (0);
}

int
sandbox_map_protect(void *base, struct sandbox_map *sm)
{
	struct sandbox_map_entry *sme;

	STAILQ_FOREACH(sme, &sm->sm_head, sme_entries) {
		if (sme->sme_prot & PROT_WRITE)
			continue;
		if (sandbox_map_entry_mprotect(base, sme) == MAP_FAILED)
			return (-1);
	}
	return (0);
}

int
sandbox_map_reload(void *base, struct sandbox_map *sm)
{
	struct sandbox_map_entry *sme;

	STAILQ_FOREACH(sme, &sm->sm_head, sme_entries) {
		if (!(sme->sme_prot & PROT_WRITE))
			continue;
		if (sandbox_map_entry_mmap(base, sme, 0) == MAP_FAILED)
			return (-1);
	}
	return (0);
}

void
sandbox_map_free(struct sandbox_map *sm)
{
	struct sandbox_map_entry *sme, *sme_temp;

	if (sm == NULL)
		return;

	STAILQ_FOREACH_SAFE(sme, &sm->sm_head, sme_entries, sme_temp) {
		STAILQ_REMOVE(&sm->sm_head, sme, sandbox_map_entry,
		    sme_entries);
		free(sme);
	}
	free(sm);
}

size_t
sandbox_map_maxoffset(struct sandbox_map *sm)
{

	return(sm->sm_maxoffset);
}

size_t
sandbox_map_minoffset(struct sandbox_map *sm)
{

	return(sm->sm_minoffset);
}

static int
sandbox_map_optimize(struct sandbox_map *sm)
{
	size_t delta, entry_end, newlen;
	struct sandbox_map_entry *sme, *next_sme, *tmp_sme;

	/*
	 * Search for any mapping that start below sm->sm_minoffset (the
	 * offset of the first real program data in a section) and shift
	 * them up to match.  This eliminates the problem of the reserved
	 * first 0x8000 bytes being mapped from the ELF file due to linker
	 * script bugs.
	 */
	STAILQ_FOREACH(sme, &sm->sm_head, sme_entries) {
		if (sme->sme_map_offset < sm->sm_minoffset) {
			delta = sm->sm_minoffset - sme->sme_map_offset;
#ifdef DEBUG
			printf("shifting map up by 0x%zx from 0x%zx\n", delta,
			    sme->sme_map_offset);
#endif
			assert(sme->sme_len > delta);
			sme->sme_map_offset += delta;
			sme->sme_len -= delta;
			sme->sme_file_offset += delta;
		}
	}

	/*
	 * Search for mapping with segments that will be overwritten by
	 * the next mapping and truncate the mappings appropriately.
	 */
	STAILQ_FOREACH_SAFE(sme, &sm->sm_head, sme_entries, tmp_sme) {
		next_sme = STAILQ_NEXT(sme, sme_entries);
		if (next_sme == NULL)
			break;

		/*
		 * Normal elf files have their sections sorted.  We can't
		 * do anything useful if they aren't.  Complain loudly
		 * if this happens so we know we need to handle this case.
		 */
		if (next_sme->sme_map_offset < sme->sme_map_offset) {
			warnx("%s: unsorted mappings, most optimizations are "
			    "disabled!", __func__);
			continue;
		}

		if (sme->sme_map_offset + sme->sme_len <=
		    next_sme->sme_map_offset)
			continue;
		if (sme->sme_map_offset + sme->sme_len >
		    next_sme->sme_map_offset + next_sme->sme_len) {
			/* This should not happen in normal ELF files... */
			warnx("%s: mapping 0x%zx of length 0x%zx surrounds "
			    "next mapping 0x%zx of length 0x%zx!", __func__,
			    sme->sme_map_offset, sme->sme_len,
			    next_sme->sme_map_offset, next_sme->sme_len);
			continue;
		}
		newlen = next_sme->sme_map_offset - sme->sme_map_offset;
#ifdef DEBUG
		printf("truncating mapping at 0x%zx from 0x%zx to 0x%zx\n",
		    sme->sme_map_offset, sme->sme_len, newlen);
#endif
		sme->sme_len = newlen;
		sme->sme_tailbytes = 0;
		if (sme->sme_len == 0) {
			STAILQ_REMOVE(&sm->sm_head, sme, sandbox_map_entry,
			    sme_entries);
			free(sme);
		}
	}

	/*
	 * Search for mappings containing tailbytes and map the last page to
	 * check if the bytes actually need to be zeroed.
	 */
	STAILQ_FOREACH(sme, &sm->sm_head, sme_entries) {
		char *lastpage;
		if (sme->sme_tailbytes == 0)
			continue;
#ifdef DEBUG
		printf("Testing %zu tailbytes for mapping at 0x%zx\n",
		    sme->sme_tailbytes, sme->sme_map_offset);
#endif
		if ((lastpage = mmap(0, PAGE_SIZE, PROT_READ, MAP_PRIVATE,
		    sme->sme_fd, sme->sme_file_offset + sme->sme_len -
		    PAGE_SIZE)) == MAP_FAILED) {
			warn("%s: mmap\n", __func__);
			return (-1);
		}
		for (/**/; sme->sme_tailbytes > 0; sme->sme_tailbytes--)
			if (lastpage[PAGE_SIZE - sme->sme_tailbytes] != 0)
				break;
#ifdef DEBUG
		printf("%zu actual tailbytes\n", sme->sme_tailbytes);
#endif
		if (munmap(lastpage, PAGE_SIZE) == -1) {
			warn("%s: munmap", __func__);
			return (-1);
		}
	}

	/*
	 * Search for adjacent mappings of the same file with the same
	 * permissions and combine them.
	 */
	STAILQ_FOREACH_SAFE(sme, &sm->sm_head, sme_entries, tmp_sme) {
#if defined(DEBUG) && DEBUG > 1
		printf("sme_map_offset  = 0x%zx\n", sme->sme_map_offset);
		printf("sme_len         = 0x%zx\n", sme->sme_len);
		printf("sme_prot        = 0x%x\n", (uint)sme->sme_prot);
		printf("sme_flags       = 0x%x\n", (uint)sme->sme_flags);
		printf("sme_fd          = 0x%d\n", sme->sme_fd);
		printf("sme_file_offset = 0x%zx\n", (size_t)sme->sme_file_offset);
		printf("sme_tailbytes   = 0x%zx\n", sme->sme_tailbytes);
#endif
		next_sme = STAILQ_NEXT(sme, sme_entries);
		if (next_sme == NULL)
			continue;

		/*
		 * Normal elf files have their sections sorted.  We can't
		 * do anything useful if they aren't.
		 */
		if (next_sme->sme_map_offset < sme->sme_map_offset ||
		    next_sme->sme_file_offset < sme->sme_file_offset)
			continue;

		/*
		 * Note: the truncation pass above ensures that eligable
		 * entries run precisely to the page proceeding the next one.
		 */
		entry_end = sme->sme_map_offset + sme->sme_len;
		delta = next_sme->sme_map_offset - sme->sme_map_offset;
		if (sme->sme_tailbytes > 0 ||
		    sme->sme_prot != next_sme->sme_prot ||
		    sme->sme_flags != next_sme->sme_flags ||
		    sme->sme_fd != next_sme->sme_fd ||
		    entry_end != next_sme->sme_map_offset ||
		    (size_t)next_sme->sme_file_offset - sme->sme_file_offset
		    != delta)
			continue;

#ifdef DEBUG
		printf("combining mapping at 0x%zx with mapping at 0x%zx\n",
		    sme->sme_map_offset, next_sme->sme_map_offset);
#endif
		next_sme->sme_map_offset -= delta;
		next_sme->sme_len += delta;
		next_sme->sme_file_offset -= delta;

		STAILQ_REMOVE(&sm->sm_head, sme, sandbox_map_entry, sme_entries);
		free(sme);
	}

	return (0);
}

struct sandbox_map *
sandbox_parse_elf64(int fd, u_int flags)
{
	int i, prot;
	size_t taddr;
	ssize_t rlen;
	size_t maplen, mappedbytes, offset, tailbytes;
	size_t min_section_addr = (size_t)-1;
	Elf64_Ehdr ehdr;
	Elf64_Phdr phdr;
	Elf64_Shdr shdr;
	struct sandbox_map *sm;
	struct sandbox_map_entry *sme;

	if ((sm = calloc(1, sizeof(*sm))) == NULL) {
		warn("%s: malloc sandbox_map", __func__);
		return (NULL);
	}
	STAILQ_INIT(&sm->sm_head);

	if ((rlen = pread(fd, &ehdr, sizeof(ehdr), 0)) != sizeof(ehdr)) {
		warn("%s: read ELF header", __func__);
		return (NULL);
	}

	/* XXX: check for magic number */

#ifdef DEBUG
	printf("type %d\n", ehdr.e_type);
	printf("version %d\n", ehdr.e_version);
	printf("entry %zx\n", (size_t)ehdr.e_entry);
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
			return (NULL);
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
		/* XXXBD: write should not be required for code! */
		/* XXXBD: ideally read would not be required for code. */
		if (flags & SANDBOX_LOADELF_CODE)
			prot &= PROT_READ | PROT_WRITE | PROT_EXEC;
		if (flags & SANDBOX_LOADELF_DATA)
			prot &= PROT_READ | PROT_WRITE;

		taddr = rounddown2((phdr.p_vaddr), PAGE_SIZE);
		offset = rounddown2(phdr.p_offset, PAGE_SIZE);
		maplen = phdr.p_offset - rounddown2(phdr.p_offset, PAGE_SIZE)
		    + phdr.p_filesz;
		/* XXX-BD: rtld handles this, but I don't see why you would. */
		if (phdr.p_filesz != phdr.p_memsz && !(phdr.p_flags & PF_W)) {
			warnx("%s: segment %d expects 0 fill, but is not "
			    "writable, skipping", __func__, i+1);
			continue;
		}

		/* Calculate bytes to be zeroed in last page */
		mappedbytes = roundup2(maplen, PAGE_SIZE);
		tailbytes = mappedbytes - maplen;

		if ((sme = sandbox_map_entry_new(taddr, maplen, prot,
		    MAP_FIXED | MAP_PRIVATE | MAP_PREFAULT_READ,
		    fd, offset, tailbytes)) == NULL)
			goto error;
		STAILQ_INSERT_TAIL(&sm->sm_head, sme, sme_entries);

		sm->sm_maxoffset = MAX(sm->sm_maxoffset, phdr.p_vaddr +
		    phdr.p_memsz);

		/*
		 * If we would map everything directly or everything fit
		 * in the mapped range we're done.
		 */
		if (phdr.p_filesz == phdr.p_memsz || phdr.p_memsz <= mappedbytes)
			continue;

		taddr = taddr + mappedbytes;
		maplen = (phdr.p_offset - rounddown2(phdr.p_offset, PAGE_SIZE)) +
		    phdr.p_memsz - mappedbytes;

		if ((sme = sandbox_map_entry_new(taddr, maplen, prot,
		    MAP_FIXED | MAP_ANON, -1, 0, 0)) == NULL)
			goto error;
		STAILQ_INSERT_TAIL(&sm->sm_head, sme, sme_entries);
	}

	for (i = 1; i < ehdr.e_shnum; i++) {	/* Skip section 0 */
		if ((rlen = pread(fd, &shdr, sizeof(shdr), ehdr.e_shoff +
		    ehdr.e_shentsize * i)) != sizeof(shdr)) {
			warn("%s: reading %d section header", __func__, i+1);
			goto error;
		}

		/*
		 * Find the "real" section with the lowest address.
		 * Exclude everything in the first page as those would
		 * either introduce potential NULL related bugs or
		 * treat various ELF stables as valid.
		 */
		if (shdr.sh_addr >= 0x1000 &&
		    min_section_addr > shdr.sh_addr)
			min_section_addr = shdr.sh_addr;
	}
#ifdef DEBUG
	printf("minimum section address 0x%lx\n", min_section_addr);
#endif
	sm->sm_minoffset = rounddown2(min_section_addr, PAGE_SIZE);

	sandbox_map_optimize(sm);

	return (sm);
error:
	sandbox_map_free(sm);
	return (NULL);
}

#ifdef TEST_LOADELF64
static ssize_t
sandbox_loadelf64(int fd, void *base, size_t maxsize __unused, u_int flags)
{
	struct sandbox_map *sm;
	ssize_t maxoffset;

	assert((intptr_t)base % PAGE_SIZE == 0);

	if ((sm = sandbox_parse_elf64(fd, flags)) == NULL) {
		warnx("%s: sandbox_parse_elf64", __func__);
		return (-1);
	}
	if (sandbox_map_load(base, sm) == -1) {
		warnx("%s: sandbox_map_load", __func__);
		return (-1);
	}
	maxoffset = sm->sm_maxoffset;
	sandbox_map_free(sm);

	return (maxoffset);
}

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
