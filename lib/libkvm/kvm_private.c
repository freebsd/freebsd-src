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

static void
_kvm_hpt_insert(struct hpt *hpt, uint64_t pa, off_t off)
{
	struct hpte *hpte;
	uint32_t fnv = FNV1_32_INIT;

	fnv = fnv_32_buf(&pa, sizeof(pa), fnv);
	fnv &= (HPT_SIZE - 1);
	hpte = malloc(sizeof(*hpte));
	hpte->pa = pa;
	hpte->off = off;
	hpte->next = hpt->hpt_head[fnv];
	hpt->hpt_head[fnv] = hpte;
}

void
_kvm_hpt_init(kvm_t *kd, struct hpt *hpt, void *base, size_t len, off_t off,
    int page_size, int word_size)
{
	uint64_t bits, idx, pa;
	uint64_t *base64;
	uint32_t *base32;

	base64 = base;
	base32 = base;
	for (idx = 0; idx < len / word_size; idx++) {
		if (word_size == sizeof(uint64_t))
			bits = _kvm64toh(kd, base64[idx]);
		else
			bits = _kvm32toh(kd, base32[idx]);
		pa = idx * word_size * NBBY * page_size;
		for (; bits != 0; bits >>= 1, pa += page_size) {
			if ((bits & 1) == 0)
				continue;
			_kvm_hpt_insert(hpt, pa, off);
			off += page_size;
		}
	}
}

off_t
_kvm_hpt_find(struct hpt *hpt, uint64_t pa)
{
	struct hpte *hpte;
	uint32_t fnv = FNV1_32_INIT;

	fnv = fnv_32_buf(&pa, sizeof(pa), fnv);
	fnv &= (HPT_SIZE - 1);
	for (hpte = hpt->hpt_head[fnv]; hpte != NULL; hpte = hpte->next) {
		if (pa == hpte->pa)
			return (hpte->off);
	}
	return (-1);
}

void
_kvm_hpt_free(struct hpt *hpt)
{
	struct hpte *hpte, *next;
	int i;

	for (i = 0; i < HPT_SIZE; i++) {
		for (hpte = hpt->hpt_head[i]; hpte != NULL; hpte = next) {
			next = hpte->next;
			free(hpte);
		}
	}
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
