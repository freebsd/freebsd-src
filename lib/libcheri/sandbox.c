/*-
 * Copyright (c) 2012-2013 Robert N. M. Watson
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
#include <sys/stat.h>

#include <machine/cheri.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "sandbox.h"

#define	roundup2(x, y)	(((x)+((y)-1))&(~((y)-1))) /* if y is powers of two */

#define	GUARD_PAGE_SIZE	0x1000
#define	PAGE_SIZE	0x1000
#define	STACK_SIZE	(32*PAGE_SIZE)

int sb_verbose;

/*
 * Library routine for setting up a sandbox.
 */

register_t	_chsbrt_invoke(register_t, register_t, register_t,
		    register_t, register_t, register_t);

struct sandbox {
	char		*sb_path;
	void		*sb_mem;
	register_t	 sb_sandboxlen;
	register_t	 sb_heapbase;
	register_t	 sb_heaplen;
	struct chericap	 sb_codecap;	/* Sealed code capability for CCall. */
	struct chericap	 sb_datacap;	/* Sealed data capability for CCall. */
	struct stat	 sb_stat;
};

int
sandbox_setup(const char *path, register_t sandboxlen, struct sandbox **sbp)
{
	struct sandbox *sb;
	int fd, saved_errno;
	size_t length;
	uint8_t *base;
	register_t v;

	fd = open(path, O_RDONLY);
	if (fd == -1) {
		saved_errno = errno;
		warn("%s: open %s", __func__, path);
		errno = saved_errno;
		return (-1);
	}

	sb = calloc(1, sizeof(*sb));
	if (sb == NULL) {
		saved_errno = errno;
		warn("%s: malloc", __func__);
		goto error;
	}
	sb->sb_path = strdup(path);
	if (sb->sb_path == NULL) {
		saved_errno = errno;
		warn("%s: fstat %s", __func__, path);
		goto error;
	}

	if (fstat(fd, &sb->sb_stat) < 0) {
		saved_errno = errno;
		warn("%s: fstat %s", __func__, path);
		goto error;
	}

	/* For now, support only "small" sandboxed programs. */
	if (sb->sb_stat.st_size >= sandboxlen/2) {
		saved_errno = EINVAL;
		warnx("%s: %s too large", __func__, path);
		goto error;
	}

	/*
	 * Perform an initial reservation of space for the sandbox, but using
	 * anonymous memory that is neither readable nor writable.  This
	 * ensures there is space for all the various segments we will be
	 * installing later.
	 *
	 * The rough sandbox memory map is as follows:
	 *
	 * [stack]
	 * [guard page]
	 * [heap]
	 * [guard page]
	 * [memory mapped binary]
	 * [guard page]
	 */
	length = sandboxlen;
	base = sb->sb_mem = mmap(NULL, length, 0, MAP_ANON, -1, 0);
	if (sb->sb_mem == MAP_FAILED) {
		saved_errno = errno;
		warn("%s: mmap region", __func__);
		goto error;
	}

	/*
	 * Skip guard page.
	 */
	base += GUARD_PAGE_SIZE;
	length -= GUARD_PAGE_SIZE;

	/*
	 * Map program binary.
	 */
	if (mmap(base, sb->sb_stat.st_size, PROT_READ | PROT_WRITE,
	    MAP_PRIVATE | MAP_FIXED, fd, 0) == MAP_FAILED) {
		saved_errno = errno;
		warn("%s: mmap %s", __func__, path);
		goto error;
	}
	base += roundup2(sb->sb_stat.st_size, PAGE_SIZE);
	length += roundup2(sb->sb_stat.st_size, PAGE_SIZE);

	close(fd);
	fd = -1;

	/*
	 * Skip guard page.
	 */
	base += GUARD_PAGE_SIZE;
	length -= GUARD_PAGE_SIZE;

	/*
	 * Heap.
	 */
	sb->sb_heapbase = (register_t)base - (register_t)sb->sb_mem;
	sb->sb_heaplen = length - (GUARD_PAGE_SIZE + STACK_SIZE);
	if (mmap(base, sb->sb_heaplen, PROT_READ | PROT_WRITE,
	    MAP_ANON | MAP_FIXED, -1, 0) == MAP_FAILED) {
		saved_errno = errno;
		warn("%s: mmap heap", __func__);
		goto error;
	}
	memset(base, 0, sb->sb_heaplen);
	base += sb->sb_heaplen;
	length -= sb->sb_heaplen;

	/*
	 * Skip guard page.
	 */
	base += GUARD_PAGE_SIZE;
	length -= GUARD_PAGE_SIZE;

	/*
	 * Stack.
	 */
	if (mmap(base, length, PROT_READ | PROT_WRITE, MAP_ANON | MAP_FIXED,
	    -1, 0) == MAP_FAILED) {
		saved_errno = errno;
		warn("%s: mmap stack", __func__);
		goto error;
	}
	memset(base, 0, length);
	base += STACK_SIZE;
	length -= STACK_SIZE;

	/*
	 * There should not be too much, nor too little space remaining.  0
	 * is our Goldilocks number.
	 */
	assert(length == 0);

	/*
	 * Construct a generic capability in $c10 that describes the combined
	 * code/data segment that we will seal.
	 *
	 * Derive from $c3 a code capability in $c1, and data capability in
	 * $c2, suitable for use with CCall.  Store in the persistent sandbox
	 * description for later use.
	 *
	 * XXXRW: $c3 is probably not the right thing.
	 */
	CHERI_CINCBASE(3, 0, sb->sb_mem);
	CHERI_CSETTYPE(3, 3, 0x1000);		/* Sandbox start address. */
	CHERI_CSETLEN(3, 3, sandboxlen);

	/*
	 * Construct a code capability in $c1, derived from $c3, suitable for
	 * use with CCall.
	 */
	CHERI_CANDPERM(1, 3, CHERI_PERM_EXECUTE | CHERI_PERM_SEAL);
	CHERI_CSEALCODE(1, 1);

	/*
	 * Construct a data capability in $c2, derived from $c1 and $c3,
	 * suitable for use with CCall.
	 */
	CHERI_CANDPERM(2, 3, CHERI_PERM_LOAD | CHERI_PERM_STORE |
	    CHERI_PERM_LOAD_CAP | CHERI_PERM_STORE_CAP);
	CHERI_CSEALDATA(2, 2, 3);

	CHERI_CSC(1, 0, &sb->sb_codecap, 0);
	CHERI_CSC(2, 0, &sb->sb_datacap, 0);
	sb->sb_sandboxlen = sandboxlen;

	if (sb_verbose) {
		printf("Sandbox configured:\n");
		printf("  Path: %s\n", sb->sb_path);
		printf("  Mem: %p\n", sb->sb_mem);
		printf("  Len: %ju\n", (uintmax_t)sb->sb_sandboxlen);
		printf("  Code capability:\n");
		CHERI_CGETTAG(v, 1);
		printf("    t %u", (u_int)v);
		CHERI_CGETUNSEALED(v, 1);
		printf(" u %u", (u_int)v);
		CHERI_CGETPERM(v, 1);
		printf(" perms %04x", (u_int)v);
		CHERI_CGETTYPE(v, 1);
		printf(" otype %p\n", (void *)v);
		CHERI_CGETBASE(v, 1);
		printf("    base %p", (void *)v);
		CHERI_CGETLEN(v, 1);
		printf(" length %p\n", (void *)v);

		printf("  Data capability:\n");
		CHERI_CGETTAG(v, 2);
		printf("    t %u", (u_int)v);
		CHERI_CGETUNSEALED(v, 2);
		printf(" u %u", (u_int)v);
		CHERI_CGETPERM(v, 2);
		printf(" perms %04x", (u_int)v);
		CHERI_CGETTYPE(v, 2);
		printf(" otype %p\n", (void *)v);
		CHERI_CGETBASE(v, 2);
		printf("    base %p", (void *)v);
		CHERI_CGETLEN(v, 2);
		printf(" length %p\n", (void *)v);
	}

	/*
	 * Clear $c1, $c2, and $c3, which we no longer require.
	 */
	CHERI_CCLEARTAG(1);
	CHERI_CCLEARTAG(2);
	CHERI_CCLEARTAG(3);

	*sbp = sb;
	return (0);

error:
	if (sb != NULL) {
		if (sb->sb_path != NULL)
			free(sb->sb_path);
		if (sb->sb_mem != NULL)
			munmap(sb->sb_mem, sandboxlen);
		free(sb);
	}
	if (fd != -1)
		close(fd);
	errno = saved_errno;
	return (-1);
}

#define	CHERI_CLOADORCLEAR(cnum, cptr) do {				\
	if (c ## cnum != NULL)						\
		CHERI_CLC(cnum, 0, cptr, 0);				\
	else								\
		CHERI_CCLEARTAG(cnum);					\
} while (0)

register_t
sandbox_invoke(struct sandbox *sb, register_t a0, register_t a1,
    register_t a2, register_t a3, struct chericap *c3, struct chericap *c4,
    struct chericap *c5, struct chericap *c6, struct chericap *c7,
    struct chericap *c8, struct chericap *c9, struct chericap *c10)
{

	CHERI_CLC(1, 0, &sb->sb_codecap, 0);
	CHERI_CLC(2, 0, &sb->sb_datacap, 0);
	CHERI_CLOADORCLEAR(3, c3);
	CHERI_CLOADORCLEAR(4, c4);
	CHERI_CLOADORCLEAR(5, c5);
	CHERI_CLOADORCLEAR(6, c6);
	CHERI_CLOADORCLEAR(7, c7);
	CHERI_CLOADORCLEAR(8, c8);
	CHERI_CLOADORCLEAR(9, c9);
	CHERI_CLOADORCLEAR(10, c10);
#ifndef SPEEDY_BUT_SLOPPY
	CHERI_CCLEARTAG(11);
	CHERI_CCLEARTAG(12);
	CHERI_CCLEARTAG(13);
	CHERI_CCLEARTAG(14);
	CHERI_CCLEARTAG(15);
	CHERI_CCLEARTAG(16);
	CHERI_CCLEARTAG(17);
	CHERI_CCLEARTAG(18);
	CHERI_CCLEARTAG(19);
	CHERI_CCLEARTAG(20);
	CHERI_CCLEARTAG(21);
	CHERI_CCLEARTAG(22);
	CHERI_CCLEARTAG(23);
	CHERI_CCLEARTAG(24);
	CHERI_CCLEARTAG(25);
	CHERI_CCLEARTAG(26);
#endif
	return (_chsbrt_invoke(a0, a1, a2, a3, sb->sb_heapbase,
	    sb->sb_heaplen));
}

void
sandbox_destroy(struct sandbox *sb)
{

	munmap(sb->sb_mem, sb->sb_sandboxlen);
	bzero(sb, sizeof(*sb));
	free(sb);
}
