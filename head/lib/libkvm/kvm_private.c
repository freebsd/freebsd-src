/*-
 * Copyright (c) 1989, 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software developed by the Computer Systems
 * Engineering group at Lawrence Berkeley Laboratory under DARPA contract
 * BG 91-66 and contributed to Berkeley.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/fnv_hash.h>

#define	_WANT_VNET

#include <sys/user.h>
#include <sys/linker.h>
#include <sys/pcpu.h>
#include <sys/stat.h>

#include <net/vnet.h>

#include <assert.h>
#include <fcntl.h>
#include <kvm.h>
#include <limits.h>
#include <paths.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdarg.h>

#include "kvm_private.h"

/*
 * Routines private to libkvm.
 */

/* from src/lib/libc/gen/nlist.c */
int __fdnlist(int, struct nlist *);

/*
 * Report an error using printf style arguments.  "program" is kd->program
 * on hard errors, and 0 on soft errors, so that under sun error emulation,
 * only hard errors are printed out (otherwise, programs like gdb will
 * generate tons of error messages when trying to access bogus pointers).
 */
void
_kvm_err(kvm_t *kd, const char *program, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	if (program != NULL) {
		(void)fprintf(stderr, "%s: ", program);
		(void)vfprintf(stderr, fmt, ap);
		(void)fputc('\n', stderr);
	} else
		(void)vsnprintf(kd->errbuf,
		    sizeof(kd->errbuf), fmt, ap);

	va_end(ap);
}

void
_kvm_syserr(kvm_t *kd, const char *program, const char *fmt, ...)
{
	va_list ap;
	int n;

	va_start(ap, fmt);
	if (program != NULL) {
		(void)fprintf(stderr, "%s: ", program);
		(void)vfprintf(stderr, fmt, ap);
		(void)fprintf(stderr, ": %s\n", strerror(errno));
	} else {
		char *cp = kd->errbuf;

		(void)vsnprintf(cp, sizeof(kd->errbuf), fmt, ap);
		n = strlen(cp);
		(void)snprintf(&cp[n], sizeof(kd->errbuf) - n, ": %s",
		    strerror(errno));
	}
	va_end(ap);
}

void *
_kvm_malloc(kvm_t *kd, size_t n)
{
	void *p;

	if ((p = calloc(n, sizeof(char))) == NULL)
		_kvm_err(kd, kd->program, "can't allocate %zu bytes: %s",
			 n, strerror(errno));
	return (p);
}

int
_kvm_probe_elf_kernel(kvm_t *kd, int class, int machine)
{

	return (kd->nlehdr.e_ident[EI_CLASS] == class &&
	    kd->nlehdr.e_type == ET_EXEC &&
	    kd->nlehdr.e_machine == machine);
}

int
_kvm_is_minidump(kvm_t *kd)
{
	char minihdr[8];

	if (kd->rawdump)
		return (0);
	if (pread(kd->pmfd, &minihdr, 8, 0) == 8 &&
	    memcmp(&minihdr, "minidump", 8) == 0)
		return (1);
	return (0);
}

/*
 * The powerpc backend has a hack to strip a leading kerneldump
 * header from the core before treating it as an ELF header.
 *
 * We can add that here if we can get a change to libelf to support
 * an initial offset into the file.  Alternatively we could patch
 * savecore to extract cores from a regular file instead.
 */
int
_kvm_read_core_phdrs(kvm_t *kd, size_t *phnump, GElf_Phdr **phdrp)
{
	GElf_Ehdr ehdr;
	GElf_Phdr *phdr;
	Elf *elf;
	size_t i, phnum;

	elf = elf_begin(kd->pmfd, ELF_C_READ, NULL);
	if (elf == NULL) {
		_kvm_err(kd, kd->program, "%s", elf_errmsg(0));
		return (-1);
	}
	if (elf_kind(elf) != ELF_K_ELF) {
		_kvm_err(kd, kd->program, "invalid core");
		goto bad;
	}
	if (gelf_getclass(elf) != kd->nlehdr.e_ident[EI_CLASS]) {
		_kvm_err(kd, kd->program, "invalid core");
		goto bad;
	}
	if (gelf_getehdr(elf, &ehdr) == NULL) {
		_kvm_err(kd, kd->program, "%s", elf_errmsg(0));
		goto bad;
	}
	if (ehdr.e_type != ET_CORE) {
		_kvm_err(kd, kd->program, "invalid core");
		goto bad;
	}
	if (ehdr.e_machine != kd->nlehdr.e_machine) {
		_kvm_err(kd, kd->program, "invalid core");
		goto bad;
	}

	if (elf_getphdrnum(elf, &phnum) == -1) {
		_kvm_err(kd, kd->program, "%s", elf_errmsg(0));
		goto bad;
	}

	phdr = calloc(phnum, sizeof(*phdr));
	if (phdr == NULL) {
		_kvm_err(kd, kd->program, "failed to allocate phdrs");
		goto bad;
	}

	for (i = 0; i < phnum; i++) {
		if (gelf_getphdr(elf, i, &phdr[i]) == NULL) {
			_kvm_err(kd, kd->program, "%s", elf_errmsg(0));
			goto bad;
		}
	}
	elf_end(elf);
	*phnump = phnum;
	*phdrp = phdr;
	return (0);

bad:
	elf_end(elf);
	return (-1);
}

/*
 * Transform v such that only bits [bit0, bitN) may be set.  Generates a
 * bitmask covering the number of bits, then shifts so +bit0+ is the first.
 */
static uint64_t
bitmask_range(uint64_t v, uint64_t bit0, uint64_t bitN)
{
	if (bit0 == 0 && bitN == BITS_IN(v))
		return (v);

	return (v & (((1ULL << (bitN - bit0)) - 1ULL) << bit0));
}

/*
 * Returns the number of bits in a given byte array range starting at a
 * given base, from bit0 to bitN.  bit0 may be non-zero in the case of
 * counting backwards from bitN.
 */
static uint64_t
popcount_bytes(uint64_t *addr, uint32_t bit0, uint32_t bitN)
{
	uint32_t res = bitN - bit0;
	uint64_t count = 0;
	uint32_t bound;

	/* Align to 64-bit boundary on the left side if needed. */
	if ((bit0 % BITS_IN(*addr)) != 0) {
		bound = MIN(bitN, roundup2(bit0, BITS_IN(*addr)));
		count += __bitcount64(bitmask_range(*addr, bit0, bound));
		res -= (bound - bit0);
		addr++;
	}

	while (res > 0) {
		bound = MIN(res, BITS_IN(*addr));
		count += __bitcount64(bitmask_range(*addr, 0, bound));
		res -= bound;
		addr++;
	}

	return (count);
}

int
_kvm_pt_init(kvm_t *kd, size_t map_len, off_t map_off, off_t sparse_off,
    int page_size, int word_size)
{
	uint64_t *addr;
	uint32_t *popcount_bin;
	int bin_popcounts = 0;
	uint64_t pc_bins, res;
	ssize_t rd;

	/*
	 * Map the bitmap specified by the arguments.
	 */
	kd->pt_map = _kvm_malloc(kd, map_len);
	if (kd->pt_map == NULL) {
		_kvm_err(kd, kd->program, "cannot allocate %zu bytes for bitmap",
		    map_len);
		return (-1);
	}
	rd = pread(kd->pmfd, kd->pt_map, map_len, map_off);
	if (rd < 0 || rd != (ssize_t)map_len) {
		_kvm_err(kd, kd->program, "cannot read %zu bytes for bitmap",
		    map_len);
		return (-1);
	}
	kd->pt_map_size = map_len;

	/*
	 * Generate a popcount cache for every POPCOUNT_BITS in the bitmap,
	 * so lookups only have to calculate the number of bits set between
	 * a cache point and their bit.  This reduces lookups to O(1),
	 * without significantly increasing memory requirements.
	 *
	 * Round up the number of bins so that 'upper half' lookups work for
	 * the final bin, if needed.  The first popcount is 0, since no bits
	 * precede bit 0, so add 1 for that also.  Without this, extra work
	 * would be needed to handle the first PTEs in _kvm_pt_find().
	 */
	addr = kd->pt_map;
	res = map_len;
	pc_bins = 1 + (res * NBBY + POPCOUNT_BITS / 2) / POPCOUNT_BITS;
	kd->pt_popcounts = calloc(pc_bins, sizeof(uint32_t));
	if (kd->pt_popcounts == NULL)
		return (-1);

	for (popcount_bin = &kd->pt_popcounts[1]; res > 0;
	    addr++, res -= sizeof(*addr)) {
		*popcount_bin += popcount_bytes(addr, 0,
		    MIN(res * NBBY, BITS_IN(*addr)));
		if (++bin_popcounts == POPCOUNTS_IN(*addr)) {
			popcount_bin++;
			*popcount_bin = *(popcount_bin - 1);
			bin_popcounts = 0;
		}
	}

	assert(pc_bins * sizeof(*popcount_bin) ==
	    ((uintptr_t)popcount_bin - (uintptr_t)kd->pt_popcounts));

	kd->pt_sparse_off = sparse_off;
	kd->pt_sparse_size = (uint64_t)*popcount_bin * PAGE_SIZE;
	kd->pt_page_size = page_size;
	kd->pt_word_size = word_size;
	return (0);
}

/*
 * Find the offset for the given physical page address; returns -1 otherwise.
 *
 * A page's offset is represented by the sparse page base offset plus the
 * number of bits set before its bit multiplied by PAGE_SIZE.  This means
 * that if a page exists in the dump, it's necessary to know how many pages
 * in the dump precede it.  Reduce this O(n) counting to O(1) by caching the
 * number of bits set at POPCOUNT_BITS intervals.
 *
 * Then to find the number of pages before the requested address, simply
 * index into the cache and count the number of bits set between that cache
 * bin and the page's bit.  Halve the number of bytes that have to be
 * checked by also counting down from the next higher bin if it's closer.
 */
off_t
_kvm_pt_find(kvm_t *kd, uint64_t pa)
{
	uint64_t *bitmap = kd->pt_map;
	uint64_t pte_bit_id = pa / PAGE_SIZE;
	uint64_t pte_u64 = pte_bit_id / BITS_IN(*bitmap);
	uint64_t popcount_id = pte_bit_id / POPCOUNT_BITS;
	uint64_t pte_mask = 1ULL << (pte_bit_id % BITS_IN(*bitmap));
	uint64_t bitN;
	uint32_t count;

	/* Check whether the page address requested is in the dump. */
	if (pte_bit_id >= (kd->pt_map_size * NBBY) ||
	    (bitmap[pte_u64] & pte_mask) == 0)
		return (-1);

	/*
	 * Add/sub popcounts from the bitmap until the PTE's bit is reached.
	 * For bits that are in the upper half between the calculated
	 * popcount id and the next one, use the next one and subtract to
	 * minimize the number of popcounts required.
	 */
	if ((pte_bit_id % POPCOUNT_BITS) < (POPCOUNT_BITS / 2)) {
		count = kd->pt_popcounts[popcount_id] + popcount_bytes(
		    bitmap + popcount_id * POPCOUNTS_IN(*bitmap),
		    0, pte_bit_id - popcount_id * POPCOUNT_BITS);
	} else {
		/*
		 * Counting in reverse is trickier, since we must avoid
		 * reading from bytes that are not in range, and invert.
		 */
		uint64_t pte_u64_bit_off = pte_u64 * BITS_IN(*bitmap);

		popcount_id++;
		bitN = MIN(popcount_id * POPCOUNT_BITS,
		    kd->pt_map_size * BITS_IN(uint8_t));
		count = kd->pt_popcounts[popcount_id] - popcount_bytes(
		    bitmap + pte_u64,
		    pte_bit_id - pte_u64_bit_off, bitN - pte_u64_bit_off);
	}

	/*
	 * This can only happen if the core is truncated.  Treat these
	 * entries as if they don't exist, since their backing doesn't.
	 */
	if (count >= (kd->pt_sparse_size / PAGE_SIZE))
		return (-1);

	return (kd->pt_sparse_off + (uint64_t)count * PAGE_SIZE);
}

static int
kvm_fdnlist(kvm_t *kd, struct kvm_nlist *list)
{
	kvaddr_t addr;
	int error, nfail;

	if (kd->resolve_symbol == NULL) {
		struct nlist *nl;
		int count, i;

		for (count = 0; list[count].n_name != NULL &&
		     list[count].n_name[0] != '\0'; count++)
			;
		nl = calloc(count + 1, sizeof(*nl));
		for (i = 0; i < count; i++)
			nl[i].n_name = list[i].n_name;
		nfail = __fdnlist(kd->nlfd, nl);
		for (i = 0; i < count; i++) {
			list[i].n_type = nl[i].n_type;
			list[i].n_value = nl[i].n_value;
		}
		free(nl);
		return (nfail);
	}

	nfail = 0;
	while (list->n_name != NULL && list->n_name[0] != '\0') {
		error = kd->resolve_symbol(list->n_name, &addr);
		if (error != 0) {
			nfail++;
			list->n_value = 0;
			list->n_type = 0;
		} else {
			list->n_value = addr;
			list->n_type = N_DATA | N_EXT;
		}
		list++;
	}
	return (nfail);
}

/*
 * Walk the list of unresolved symbols, generate a new list and prefix the
 * symbol names, try again, and merge back what we could resolve.
 */
static int
kvm_fdnlist_prefix(kvm_t *kd, struct kvm_nlist *nl, int missing,
    const char *prefix, kvaddr_t (*validate_fn)(kvm_t *, kvaddr_t))
{
	struct kvm_nlist *n, *np, *p;
	char *cp, *ce;
	const char *ccp;
	size_t len;
	int slen, unresolved;

	/*
	 * Calculate the space we need to malloc for nlist and names.
	 * We are going to store the name twice for later lookups: once
	 * with the prefix and once the unmodified name delmited by \0.
	 */
	len = 0;
	unresolved = 0;
	for (p = nl; p->n_name && p->n_name[0]; ++p) {
		if (p->n_type != N_UNDF)
			continue;
		len += sizeof(struct kvm_nlist) + strlen(prefix) +
		    2 * (strlen(p->n_name) + 1);
		unresolved++;
	}
	if (unresolved == 0)
		return (unresolved);
	/* Add space for the terminating nlist entry. */
	len += sizeof(struct kvm_nlist);
	unresolved++;

	/* Alloc one chunk for (nlist, [names]) and setup pointers. */
	n = np = malloc(len);
	bzero(n, len);
	if (n == NULL)
		return (missing);
	cp = ce = (char *)np;
	cp += unresolved * sizeof(struct kvm_nlist);
	ce += len;

	/* Generate shortened nlist with special prefix. */
	unresolved = 0;
	for (p = nl; p->n_name && p->n_name[0]; ++p) {
		if (p->n_type != N_UNDF)
			continue;
		*np = *p;
		/* Save the new\0orig. name so we can later match it again. */
		slen = snprintf(cp, ce - cp, "%s%s%c%s", prefix,
		    (prefix[0] != '\0' && p->n_name[0] == '_') ?
			(p->n_name + 1) : p->n_name, '\0', p->n_name);
		if (slen < 0 || slen >= ce - cp)
			continue;
		np->n_name = cp;
		cp += slen + 1;
		np++;
		unresolved++;
	}

	/* Do lookup on the reduced list. */
	np = n;
	unresolved = kvm_fdnlist(kd, np);

	/* Check if we could resolve further symbols and update the list. */
	if (unresolved >= 0 && unresolved < missing) {
		/* Find the first freshly resolved entry. */
		for (; np->n_name && np->n_name[0]; np++)
			if (np->n_type != N_UNDF)
				break;
		/*
		 * The lists are both in the same order,
		 * so we can walk them in parallel.
		 */
		for (p = nl; np->n_name && np->n_name[0] &&
		    p->n_name && p->n_name[0]; ++p) {
			if (p->n_type != N_UNDF)
				continue;
			/* Skip expanded name and compare to orig. one. */
			ccp = np->n_name + strlen(np->n_name) + 1;
			if (strcmp(ccp, p->n_name) != 0)
				continue;
			/* Update nlist with new, translated results. */
			p->n_type = np->n_type;
			if (validate_fn)
				p->n_value = (*validate_fn)(kd, np->n_value);
			else
				p->n_value = np->n_value;
			missing--;
			/* Find next freshly resolved entry. */
			for (np++; np->n_name && np->n_name[0]; np++)
				if (np->n_type != N_UNDF)
					break;
		}
	}
	/* We could assert missing = unresolved here. */

	free(n);
	return (unresolved);
}

int
_kvm_nlist(kvm_t *kd, struct kvm_nlist *nl, int initialize)
{
	struct kvm_nlist *p;
	int nvalid;
	struct kld_sym_lookup lookup;
	int error;
	const char *prefix = "";
	char symname[1024]; /* XXX-BZ symbol name length limit? */
	int tried_vnet, tried_dpcpu;

	/*
	 * If we can't use the kld symbol lookup, revert to the
	 * slow library call.
	 */
	if (!ISALIVE(kd)) {
		error = kvm_fdnlist(kd, nl);
		if (error <= 0)			/* Hard error or success. */
			return (error);

		if (_kvm_vnet_initialized(kd, initialize))
			error = kvm_fdnlist_prefix(kd, nl, error,
			    VNET_SYMPREFIX, _kvm_vnet_validaddr);

		if (error > 0 && _kvm_dpcpu_initialized(kd, initialize))
			error = kvm_fdnlist_prefix(kd, nl, error,
			    DPCPU_SYMPREFIX, _kvm_dpcpu_validaddr);

		return (error);
	}

	/*
	 * We can use the kld lookup syscall.  Go through each nlist entry
	 * and look it up with a kldsym(2) syscall.
	 */
	nvalid = 0;
	tried_vnet = 0;
	tried_dpcpu = 0;
again:
	for (p = nl; p->n_name && p->n_name[0]; ++p) {
		if (p->n_type != N_UNDF)
			continue;

		lookup.version = sizeof(lookup);
		lookup.symvalue = 0;
		lookup.symsize = 0;

		error = snprintf(symname, sizeof(symname), "%s%s", prefix,
		    (prefix[0] != '\0' && p->n_name[0] == '_') ?
			(p->n_name + 1) : p->n_name);
		if (error < 0 || error >= (int)sizeof(symname))
			continue;
		lookup.symname = symname;
		if (lookup.symname[0] == '_')
			lookup.symname++;

		if (kldsym(0, KLDSYM_LOOKUP, &lookup) != -1) {
			p->n_type = N_TEXT;
			if (_kvm_vnet_initialized(kd, initialize) &&
			    strcmp(prefix, VNET_SYMPREFIX) == 0)
				p->n_value =
				    _kvm_vnet_validaddr(kd, lookup.symvalue);
			else if (_kvm_dpcpu_initialized(kd, initialize) &&
			    strcmp(prefix, DPCPU_SYMPREFIX) == 0)
				p->n_value =
				    _kvm_dpcpu_validaddr(kd, lookup.symvalue);
			else
				p->n_value = lookup.symvalue;
			++nvalid;
			/* lookup.symsize */
		}
	}

	/*
	 * Check the number of entries that weren't found. If they exist,
	 * try again with a prefix for virtualized or DPCPU symbol names.
	 */
	error = ((p - nl) - nvalid);
	if (error && _kvm_vnet_initialized(kd, initialize) && !tried_vnet) {
		tried_vnet = 1;
		prefix = VNET_SYMPREFIX;
		goto again;
	}
	if (error && _kvm_dpcpu_initialized(kd, initialize) && !tried_dpcpu) {
		tried_dpcpu = 1;
		prefix = DPCPU_SYMPREFIX;
		goto again;
	}

	/*
	 * Return the number of entries that weren't found. If they exist,
	 * also fill internal error buffer.
	 */
	error = ((p - nl) - nvalid);
	if (error)
		_kvm_syserr(kd, kd->program, "kvm_nlist");
	return (error);
}
