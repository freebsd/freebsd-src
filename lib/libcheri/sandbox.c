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

#include <sys/cdefs.h>

#if defined(__capability)
#define	USE_C_CAPS
#endif

#include <sys/types.h>
#include <sys/param.h>
#include <sys/mman.h>
#include <sys/stat.h>

#include <machine/cheri.h>
#include <machine/cheric.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "sandbox.h"
#include "sandboxasm.h"
#include "sandbox_stat.h"
#include "sandbox_internal.h"

#define	roundup2(x, y)	(((x)+((y)-1))&(~((y)-1))) /* if y is powers of two */

#define	GUARD_PAGE_SIZE	0x1000
#define	METADATA_SIZE	0x1000
#define	STACK_SIZE	(32*PAGE_SIZE)

int sb_verbose;

/*-
 * Opaque data structure describing each sandbox; returned by sandbox_setup()
 * and destroyed by sandbox_destroy().  Currently, sandboxes are
 * single-threaded.
 *
 * TODO:
 * - Add atomically set flag and assertion to ensure single-threaded entry to
 *   the sandbox.
 */
struct sandbox {
	char		*sb_path;
	void		*sb_mem;
	register_t	 sb_sandboxlen;
	register_t	 sb_heapbase;
	register_t	 sb_heaplen;
#ifdef USE_C_CAPS
	__capability void	*sb_codecap;	/* Sealed code capability. */
	__capability void	*sb_datacap;	/* Sealed data capability. */
#else
	struct chericap	 sb_codecap;	/* Sealed code capability for CCall. */
	struct chericap	 sb_datacap;	/* Sealed data capability for CCall. */
#endif
	struct stat	 sb_stat;
	struct sandbox_class_stat	*sb_sandbox_class_stat;
	struct sandbox_method_stat	*sb_sandbox_method_stat;
	struct sandbox_object_stat	*sb_sandbox_object_stat;
};

int
sandbox_setup(const char *path, register_t sandboxlen, struct sandbox **sbp)
{
	char sandbox_basename[MAXPATHLEN];
#ifdef USE_C_CAPS
	__capability void *sbcap;
#endif
	struct sandbox_metadata *sbm;
	struct sandbox *sb;
	int fd, saved_errno;
	size_t length;
	uint8_t *base;
	register_t v;

	if (getenv("LIBCHERI_SB_VERBOSE"))
		sb_verbose = 1;

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
	 * K + 0x1000 [stack]
	 * K          [guard page]
	 * J + 0x1000 [heap]
	 * J          [guard page]
	 * 0x8000     [memory mapped binary] (SANDBOX_ENTRY)
	 * 0x2000     [guard page]
	 * 0x1000     [read-only sandbox metadata page]
	 * 0x0000     [guard page]
	 *
	 * Address constants in sandbox.h must be synchronised with the layout
	 * implemented here.  Location and contents of sandbox metadata is
	 * part of the ABI.
	 */
	length = sandboxlen;
	base = sb->sb_mem = mmap(NULL, length, 0, MAP_ANON, -1, 0);
	if (sb->sb_mem == MAP_FAILED) {
		saved_errno = errno;
		warn("%s: mmap region", __func__);
		goto error;
	}

	/*
	 * Skip guard page(s) to the base of the metadata structure.
	 */
	base += SANDBOX_METADATA_BASE;
	length -= SANDBOX_METADATA_BASE;

	/*
	 * Map metadata structure -- but can't fill it out until we have
	 * calculated all the other addresses involved.
	 */
	if ((sbm = mmap(base, METADATA_SIZE, PROT_READ | PROT_WRITE,
	    MAP_ANON | MAP_FIXED, -1, 0)) == MAP_FAILED) {
		saved_errno = errno;
		warn("%s: mmap metadata", __func__);
		goto error;
	}

	/*
	 * Skip forward to the mapping location for the binary -- in case we
	 * add more metadata in the future.  Assert that we didn't bump into
	 * the sandbox entry address.  This address is hard to change as it is
	 * the address used in static linking for sandboxed code.
	 */
	assert((register_t)base - (register_t)sb->sb_mem < SANDBOX_ENTRY);
	base = (void *)((register_t)sb->sb_mem + SANDBOX_ENTRY);
	length = sandboxlen - SANDBOX_ENTRY;

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
	 * Now that addresses are known, write out metadata for in-sandbox
	 * use; then mprotect() so that it can't be modified by the sandbox.
	 */
	sbm->sbm_heapbase = sb->sb_heapbase;
	sbm->sbm_heaplen = sb->sb_heaplen;
	if (mprotect(base, METADATA_SIZE, PROT_READ) < 0) {
		saved_errno = errno;
		warn("%s: mprotect metadata", __func__);
		goto error;
	}
	
	/*
	 * Register the class/object for statistics; also register a single
	 * "default" method.
	 *
	 * NB: We use the base address of the sandbox's $c0 as the 'name' of
	 * the object, since this is most useful for comparison to capability
	 * values.  However, you could also see an argument for using 'sb'
	 * itself here.
	 */
	(void)sandbox_stat_class_register(&sb->sb_sandbox_class_stat,
	    basename_r(path, sandbox_basename));
	(void)sandbox_stat_method_register(&sb->sb_sandbox_method_stat,
	    sb->sb_sandbox_class_stat, "default");
	(void)sandbox_stat_object_register(&sb->sb_sandbox_object_stat,
	    sb->sb_sandbox_class_stat, SANDBOX_OBJECT_TYPE_POINTER,
	    (uintptr_t)sb->sb_mem);
	SANDBOX_CLASS_ALLOC(sb->sb_sandbox_class_stat);

#ifdef USE_C_CAPS
	/*
	 * Construct a generic capability that describes the combined
	 * data/code segment that we will seal.
	 */
	sbcap = cheri_ptrtype(sb->sb_mem, sandboxlen, SANDBOX_ENTRY);

	/* Construct sealed code capability. */
	sb->sb_codecap = cheri_andperm(sbcap, CHERI_PERM_EXECUTE |
	    CHERI_PERM_SEAL);
	sb->sb_codecap = cheri_sealcode(sb->sb_codecap);

	/* Construct sealed data capability. */
	sb->sb_datacap = cheri_andperm(sbcap, CHERI_PERM_LOAD |
	    CHERI_PERM_STORE | CHERI_PERM_LOAD_CAP | CHERI_PERM_STORE_CAP |
	    CHERI_PERM_STORE_EPHEM_CAP);
	sb->sb_datacap = cheri_sealdata(sb->sb_datacap, sbcap);
#else
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
	CHERI_CSETTYPE(3, 3, SANDBOX_ENTRY);
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
	    CHERI_PERM_LOAD_CAP | CHERI_PERM_STORE_CAP |
	    CHERI_PERM_STORE_EPHEM_CAP);
	CHERI_CSEALDATA(2, 2, 3);

	CHERI_CSC(1, 0, &sb->sb_codecap, 0);
	CHERI_CSC(2, 0, &sb->sb_datacap, 0);
#endif

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

#ifndef USE_C_CAPS
	/*
	 * Clear $c1, $c2, and $c3, which we no longer require.
	 */
	CHERI_CCLEARTAG(1);
	CHERI_CCLEARTAG(2);
	CHERI_CCLEARTAG(3);
#endif

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

#ifdef USE_C_CAPS
register_t
sandbox_cinvoke(struct sandbox *sb, register_t a0, register_t a1,
    register_t a2, register_t a3, register_t a4, register_t a5, register_t a6,
    register_t a7, __capability void *c3, __capability void *c4,
    __capability void *c5, __capability void *c6, __capability void *c7,
    __capability void *c8, __capability void *c9, __capability void *c10)
{

	/*
	 * XXXRW: TODO:
	 *
	 * 1. What about $v1, capability return values?
	 * 2. Does the right thing happen with $a0..$a7, $c3..$c10?
	 */
	return (_chsbrt_invoke(sb->sb_codecap, sb->sb_datacap, a0, a1, a2, a3,
	    a4, a5, a6, a7, c3, c4, c5, c6, c7, c8, c9, c10));
}
#endif

#define	CHERI_CLOADORCLEAR(cnum, cptr) do {				\
	if (cptr != NULL)						\
		CHERI_CLC(cnum, 0, cptr, 0);				\
	else								\
		CHERI_CCLEARTAG(cnum);					\
} while (0)

/*
 * This version of invoke() is intended for callers not implementing CHERI
 * compiler support -- but internally, it can be implemented either way.
 *
 * XXXRW: Zeroing the capability pointer will clear the tag, but it seems a
 * bit ugly.  It would be nice to have a pretty way to do this.  Note that C
 * NULL != an untagged capability pointer, and we would benefit from having a
 * canonical 'NULL' for the capability space (connoting no rights).
 */
register_t
sandbox_invoke(struct sandbox *sb, register_t a0, register_t a1,
    register_t a2, register_t a3, struct chericap *c3p, struct chericap *c4p,
    struct chericap *c5p, struct chericap *c6p, struct chericap *c7p,
    struct chericap *c8p, struct chericap *c9p, struct chericap *c10p)
{
#ifdef USE_C_CAPS
	__capability void *c3, *c4, *c5, *c6, *c7, *c8, *c9, *c10;
	__capability void *cclear;
#endif
	register_t v0;

	SANDBOX_METHOD_INVOKE(sb->sb_sandbox_method_stat);
	SANDBOX_OBJECT_INVOKE(sb->sb_sandbox_object_stat);
#ifdef USE_C_CAPS
	cclear = cheri_zerocap();
	c3 = (c3p != NULL ? *(__capability void **)c3p : cclear);
	c4 = (c4p != NULL ? *(__capability void **)c4p : cclear);
	c5 = (c5p != NULL ? *(__capability void **)c5p : cclear);
	c6 = (c6p != NULL ? *(__capability void **)c6p : cclear);
	c7 = (c7p != NULL ? *(__capability void **)c7p : cclear);
	c8 = (c8p != NULL ? *(__capability void **)c8p : cclear);
	c9 = (c9p != NULL ? *(__capability void **)c9p : cclear);
	c10 = (c10p != NULL ? (__capability void *)c10p : cclear);

	v0 = sandbox_cinvoke(sb, a0, a1, a2, a3, 0, 0, 0, 0, c3, c4, c5, c6,
	    c7, c8, c9, c10);
#else
	CHERI_CLC(1, 0, &sb->sb_codecap, 0);
	CHERI_CLC(2, 0, &sb->sb_datacap, 0);
	CHERI_CLOADORCLEAR(3, c3p);
	CHERI_CLOADORCLEAR(4, c4p);
	CHERI_CLOADORCLEAR(5, c5p);
	CHERI_CLOADORCLEAR(6, c6p);
	CHERI_CLOADORCLEAR(7, c7p);
	CHERI_CLOADORCLEAR(8, c8p);
	CHERI_CLOADORCLEAR(9, c9p);
	CHERI_CLOADORCLEAR(10, c10p);
	v0 = _chsbrt_invoke(a0, a1, a2, a3, 0, 0, 0, 0);
#endif
	if (v0 < 0) {
		SANDBOX_METHOD_FAULT(sb->sb_sandbox_method_stat);
		SANDBOX_OBJECT_FAULT(sb->sb_sandbox_object_stat);
	}
	return (v0);
}

void
sandbox_destroy(struct sandbox *sb)
{

	SANDBOX_CLASS_FREE(sb->sb_sandbox_class_stat);
	if (sb->sb_sandbox_object_stat != NULL)
		(void)sandbox_stat_object_deregister(
		    sb->sb_sandbox_object_stat);
	if (sb->sb_sandbox_method_stat != NULL)
		(void)sandbox_stat_method_deregister(
		    sb->sb_sandbox_method_stat);
	if (sb->sb_sandbox_class_stat != NULL)
		(void)sandbox_stat_class_deregister(
		    sb->sb_sandbox_class_stat);
	munmap(sb->sb_mem, sb->sb_sandboxlen);
	bzero(sb, sizeof(*sb));		/* Clears tags. */
	free(sb);
}
