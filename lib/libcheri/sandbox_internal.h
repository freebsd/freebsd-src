/*-
 * Copyright (c) 2012-2014 Robert N. M. Watson
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

#if !__has_feature(capabilities)
#error "This code requires a CHERI-aware compiler"
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
#include <sandbox_stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "cheri_invoke.h"
#include "libcheri_stat.h"
#include "sandbox.h"
#include "sandboxasm.h"

#define	roundup2(x, y)	(((x)+((y)-1))&(~((y)-1))) /* if y is powers of two */

#define	GUARD_PAGE_SIZE	0x1000
#define	METADATA_SIZE	0x1000
#define	STACK_SIZE	(32*PAGE_SIZE)

/*
 * Control verbose debugging output around sandbox invocation; disabled by
 * default but may be enabled using an environmental variable.
 */
int sb_verbose;

/*
 * Description of a 'sandbox class': an instance of code that may be sandboxed
 * and invoked, along with statistics/monitoring information, etc.
 *
 * NB: For now, support up to 'SANDBOX_CLASS_METHOD_COUNT' sets of method
 * statistics, which will be indexed by method number.  If the requested
 * method number isn't in range, use the catchall entry instead.
 *
 * XXXRW: Ideally, we would load this data from the target ELF rather than
 * letting the caller provide it.
 */
#define	SANDBOX_CLASS_METHOD_COUNT	32
struct sandbox_class {
	char			*sbc_path;
	int			 sbc_fd;
	struct stat		 sbc_stat;
	size_t			 sbc_sandboxlen;
	struct sandbox_class_stat	*sbc_sandbox_class_statp;
	struct sandbox_method_stat	*sbc_sandbox_method_nonamep;
	struct sandbox_method_stat	*sbc_sandbox_methods[
					    SANDBOX_CLASS_METHOD_COUNT];
};

/*-
 * Description of a 'sandbox object' or 'sandbox instance': an in-flight
 * combination of code and data.  Currently, due to compiler limitations, we
 * must conflate 'code', 'heap', and 'stack', but eventually would like to
 * allow one VM mapping of code to serve many object instances.  This would
 * also ease supporting multithreaded objects.
 *
 * TODO:
 * - Add atomically set flag and assertion to ensure single-threaded entry to
 *   the sandbox.
 * - Once the compiler supports it, move the code memory mapping and
 *   capability out of sandbox_object into sandbox_class.
 */
struct sandbox_object {
	struct sandbox_class	*sbo_sandbox_classp;
	void			*sbo_mem;
	register_t		 sbo_sandboxlen;
	register_t		 sbo_heapbase;
	register_t		 sbo_heaplen;
	struct cheri_object	 sbo_cheri_object;
	struct cheri_object	 sbo_cheri_system_object;
	struct sandbox_object_stat	*sbo_sandbox_object_statp;
};

/*
 * A classic 'sandbox' is actually a combination of a sandbox class and a
 * sandbox object.  We continue to support this model as it is used in some
 * CHERI demo and test code.
 */
struct sandbox {
	struct sandbox_class	*sb_sandbox_classp;
	struct sandbox_object	*sb_sandbox_objectp;
};

/*
 * Routines for measuring time -- depends on a later MIPS userspace cycle
 * counter.
 */
static __inline uint64_t
get_cyclecount(void)
{
	uint64_t _time;

	__asm __volatile("rdhwr %0, $2" : "=r" (_time));
	return (_time);
}

__attribute__ ((constructor)) static void
sandbox_init(void)
{

	if (getenv("LIBCHERI_SB_VERBOSE"))
		sb_verbose = 1;
}

int
sandbox_class_new(const char *path, size_t sandboxlen,
    struct sandbox_class **sbcpp)
{
	char sandbox_basename[MAXPATHLEN];
	struct sandbox_class *sbcp;
	int fd, saved_errno;

	fd = open(path, O_RDONLY);
	if (fd == -1) {
		saved_errno = errno;
		warn("%s: open %s", __func__, path);
		errno = saved_errno;
		return (-1);
	}

	sbcp = calloc(1, sizeof(*sbcp));
	if (sbcp == NULL) {
		saved_errno = errno;
		warn("%s: malloc", __func__);
		close(fd);
		errno = saved_errno;
		return (-1);
	}
	sbcp->sbc_sandboxlen = sandboxlen;
	sbcp->sbc_fd = fd;
	sbcp->sbc_path = strdup(path);
	if (sbcp->sbc_path == NULL) {
		saved_errno = errno;
		warn("%s: fstat %s", __func__, path);
		goto error;
	}

	if (fstat(sbcp->sbc_fd, &sbcp->sbc_stat) < 0) {
		saved_errno = errno;
		warn("%s: fstat %s", __func__, path);
		goto error;
	}

	/* For now, support only "small" sandboxed programs. */
	if (sbcp->sbc_stat.st_size >= (off_t)sbcp->sbc_sandboxlen/2) {
		saved_errno = EINVAL;
		warnx("%s: %s too large", __func__, path);
		goto error;
	}

	/*
	 * Register the class/object for statistics; also register a single
	 * "noname" method to catch statistics for unnamed or overflow
	 * methods.
	 *
	 * NB: We use the base address of the sandbox's $c0 as the 'name' of
	 * the object, since this is most useful for comparison to capability
	 * values.  However, you could also see an argument for using 'sb'
	 * itself here.
	 */
	(void)sandbox_stat_class_register(&sbcp->sbc_sandbox_class_statp,
	    basename_r(path, sandbox_basename));
	(void)sandbox_stat_method_register(&sbcp->sbc_sandbox_method_nonamep,
	    sbcp->sbc_sandbox_class_statp, "<noname>");
	*sbcpp = sbcp;
	return (0);

error:
	if (sbcp->sbc_path != NULL)
		free(sbcp->sbc_path);
	close(sbcp->sbc_fd);
	free(sbcp);
	errno = saved_errno;
	return (-1);
}

int
sandbox_class_method_declare(struct sandbox_class *sbcp, u_int methodnum,
    const char *methodname)
{

	if (methodnum >= SANDBOX_CLASS_METHOD_COUNT) {
		errno = E2BIG;
		return (-1);
	}
	if (sbcp->sbc_sandbox_methods[methodnum] != NULL) {
		errno = EEXIST;
		return (-1);
	}
	return (sandbox_stat_method_register(
	    &sbcp->sbc_sandbox_methods[methodnum],
	    sbcp->sbc_sandbox_class_statp, methodname));
}

void
sandbox_class_destroy(struct sandbox_class *sbcp)
{
	u_int i;

	for (i = 0; i < SANDBOX_CLASS_METHOD_COUNT; i++) {
		if (sbcp->sbc_sandbox_methods[i] != NULL)
			(void)sandbox_stat_method_deregister(
			    sbcp->sbc_sandbox_methods[i]);
	}
	if (sbcp->sbc_sandbox_method_nonamep != NULL)
		(void)sandbox_stat_method_deregister(
		    sbcp->sbc_sandbox_method_nonamep);
	if (sbcp->sbc_sandbox_class_statp != NULL)
		(void)sandbox_stat_class_deregister(
		    sbcp->sbc_sandbox_class_statp);
	close(sbcp->sbc_fd);
	free(sbcp->sbc_path);
	free(sbcp);
}

extern void __cheri_enter;

int
sandbox_object_new(struct sandbox_class *sbcp, struct sandbox_object **sbopp)
{
	__capability void *basecap, *sbcap;
	struct sandbox_object *sbop;
	struct sandbox_metadata *sbm;
	size_t length;
	int saved_errno;
	uint8_t *base;
	register_t v;

	sbop = calloc(1, sizeof(*sbop));
	if (sbop == NULL)
		return (-1);
	sbop->sbo_sandbox_classp = sbcp;

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
	length = sbcp->sbc_sandboxlen;
	base = sbop->sbo_mem = mmap(NULL, length, 0, MAP_ANON, -1, 0);
	if (sbop->sbo_mem == MAP_FAILED) {
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
	assert((register_t)base - (register_t)sbop->sbo_mem < SANDBOX_ENTRY);
	base = (void *)((register_t)sbop->sbo_mem + SANDBOX_ENTRY);
	length = sbcp->sbc_sandboxlen - SANDBOX_ENTRY;

	/*
	 * Map program binary.
	 */
	if (mmap(base, sbcp->sbc_stat.st_size, PROT_READ | PROT_WRITE,
	    MAP_PRIVATE | MAP_FIXED, sbcp->sbc_fd, 0) == MAP_FAILED) {
		saved_errno = errno;
		warn("%s: mmap %s", __func__, sbcp->sbc_path);
		goto error;
	}
	base += roundup2(sbcp->sbc_stat.st_size, PAGE_SIZE);
	length += roundup2(sbcp->sbc_stat.st_size, PAGE_SIZE);

	/*
	 * Skip guard page.
	 */
	base += GUARD_PAGE_SIZE;
	length -= GUARD_PAGE_SIZE;

	/*
	 * Heap.
	 */
	sbop->sbo_heapbase = (register_t)base - (register_t)sbop->sbo_mem;
	sbop->sbo_heaplen = length - (GUARD_PAGE_SIZE + STACK_SIZE);
	if (mmap(base, sbop->sbo_heaplen, PROT_READ | PROT_WRITE,
	    MAP_ANON | MAP_FIXED, -1, 0) == MAP_FAILED) {
		saved_errno = errno;
		warn("%s: mmap heap", __func__);
		goto error;
	}
	memset(base, 0, sbop->sbo_heaplen);
	base += sbop->sbo_heaplen;
	length -= sbop->sbo_heaplen;

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
	sbm->sbm_heapbase = sbop->sbo_heapbase;
	sbm->sbm_heaplen = sbop->sbo_heaplen;
	if (mprotect(base, METADATA_SIZE, PROT_READ) < 0) {
		saved_errno = errno;
		warn("%s: mprotect metadata", __func__);
		goto error;
	}

	if (sbcp->sbc_sandbox_class_statp != NULL) {
		(void)sandbox_stat_object_register(
		    &sbop->sbo_sandbox_object_statp,
		    sbcp->sbc_sandbox_class_statp,
		    SANDBOX_OBJECT_TYPE_POINTER, (uintptr_t)sbop->sbo_mem);
		SANDBOX_CLASS_ALLOC(sbcp->sbc_sandbox_class_statp);
	}

	/*
	 * Construct a generic capability that describes the combined
	 * data/code segment that we will seal.
	 */
	basecap = cheri_ptrtype(sbop->sbo_mem, sbcp->sbc_sandboxlen,
	    SANDBOX_ENTRY);

	/* Construct sealed code capability. */
	sbcap = cheri_andperm(basecap, CHERI_PERM_EXECUTE | CHERI_PERM_LOAD |
	    CHERI_PERM_SEAL);
	sbop->sbo_cheri_object.co_codecap =
	    cheri_sealcode(sbcap);

	/* Construct sealed data capability. */
	sbcap = cheri_andperm(basecap, CHERI_PERM_LOAD | CHERI_PERM_STORE |
	    CHERI_PERM_LOAD_CAP | CHERI_PERM_STORE_CAP |
	    CHERI_PERM_STORE_EPHEM_CAP);
	sbop->sbo_cheri_object.co_datacap = cheri_sealdata(sbcap, basecap);

	/*
	 * Construct an object capability for the system class instance that
	 * will be passed into the sandbox.  Its code capability is just our
	 * $c0; the data capability is to the sandbox structure itself, which
	 * allows the system class to identify which sandbox a request is
	 * being issued from.
	 *
	 * Note that $c0 in the 'sandbox' will be set from $pcc, so leave a
	 * full set of write/etc permissions on the code capability.
	 */
	basecap = cheri_settype(cheri_getreg(0), (register_t)&__cheri_enter);
	sbop->sbo_cheri_system_object.co_codecap = cheri_sealcode(basecap);

	sbcap = cheri_ptr(sbop, sizeof(*sbop));
	sbcap = cheri_andperm(sbcap,
	    CHERI_PERM_LOAD | CHERI_PERM_STORE | CHERI_PERM_LOAD_CAP |
	    CHERI_PERM_STORE_CAP | CHERI_PERM_STORE_EPHEM_CAP);
	sbop->sbo_cheri_system_object.co_datacap = cheri_sealdata(sbcap,
	    basecap);

	/* XXXRW: This is not right for USE_C_CAPS. */
	if (sb_verbose) {
		printf("Sandbox configured:\n");
		printf("  Path: %s\n", sbcp->sbc_path);
		printf("  Mem: %p\n", sbop->sbo_mem);
		printf("  Len: %ju\n", (uintmax_t)sbcp->sbc_sandboxlen);
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

	*sbopp = sbop;
	return (0);

error:
	if (sbop != NULL) {
		if (sbop->sbo_mem != NULL)
			munmap(sbop->sbo_mem, sbcp->sbc_sandboxlen);
		free(sbop);
	}
	errno = saved_errno;
	return (-1);
}

register_t
sandbox_object_cinvoke(struct sandbox_object *sbop, u_int methodnum,
    register_t a1, register_t a2, register_t a3, register_t a4, register_t a5,
    register_t a6, register_t a7, __capability void *c3,
    __capability void *c4, __capability void *c5, __capability void *c6,
    __capability void *c7, __capability void *c8, __capability void *c9,
    __capability void *c10)
{
	struct sandbox_class *sbcp;
	uint64_t sample, start;
	register_t v0;

	/*
	 * XXXRW: TODO:
	 *
	 * 1. What about $v1, capability return values?
	 * 2. Does the right thing happen with $a0..$a7, $c3..$c10?
	 */
	sbcp = sbop->sbo_sandbox_classp;
	if (methodnum < SANDBOX_CLASS_METHOD_COUNT)
		SANDBOX_METHOD_INVOKE(sbcp->sbc_sandbox_methods[methodnum]);
	else
		SANDBOX_METHOD_INVOKE(sbcp->sbc_sandbox_method_nonamep);
	SANDBOX_OBJECT_INVOKE(sbop->sbo_sandbox_object_statp);
	start = get_cyclecount();
	v0 = cheri_invoke(sbop->sbo_cheri_object, methodnum, a1, a2, a3, a4,
	    a5, a6, a7, c3, c4, c5, c6, c7, c8, c9, c10);
	sample = get_cyclecount() - start;
	SANDBOX_METHOD_TIME_SAMPLE(sbcp->sbc_sandbox_methods[methodnum],
	    sample);
	SANDBOX_OBJECT_TIME_SAMPLE(sbop->sbo_sandbox_object_statp, sample);
	if (v0 < 0) {
		if (methodnum < SANDBOX_CLASS_METHOD_COUNT)
			SANDBOX_METHOD_FAULT(
			    sbcp->sbc_sandbox_methods[methodnum]);
		else
			SANDBOX_METHOD_FAULT(
			    sbcp->sbc_sandbox_method_nonamep);
		SANDBOX_OBJECT_FAULT(sbop->sbo_sandbox_object_statp);
	}
	return (v0);
}

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
sandbox_object_invoke(struct sandbox_object *sbop, register_t methodnum,
    register_t a1, register_t a2, register_t a3, register_t a4, register_t a5,
    register_t a6, register_t a7, struct chericap *c3p, struct chericap *c4p,
    struct chericap *c5p, struct chericap *c6p, struct chericap *c7p,
    struct chericap *c8p, struct chericap *c9p, struct chericap *c10p)
{
	struct sandbox_class *sbcp;
	__capability void *c3, *c4, *c5, *c6, *c7, *c8, *c9, *c10;
	__capability void *cclear;
	register_t v0;

	sbcp = sbop->sbo_sandbox_classp;
	if (methodnum < SANDBOX_CLASS_METHOD_COUNT)
		SANDBOX_METHOD_INVOKE(sbcp->sbc_sandbox_methods[methodnum]);
	else
		SANDBOX_METHOD_INVOKE(sbcp->sbc_sandbox_method_nonamep);
	SANDBOX_OBJECT_INVOKE(sbop->sbo_sandbox_object_statp);
	cclear = cheri_zerocap();
	c3 = (c3p != NULL ? *(__capability void **)c3p : cclear);
	c4 = (c4p != NULL ? *(__capability void **)c4p : cclear);
	c5 = (c5p != NULL ? *(__capability void **)c5p : cclear);
	c6 = (c6p != NULL ? *(__capability void **)c6p : cclear);
	c7 = (c7p != NULL ? *(__capability void **)c7p : cclear);
	c8 = (c8p != NULL ? *(__capability void **)c8p : cclear);
	c9 = (c9p != NULL ? *(__capability void **)c9p : cclear);
	c10 = (c10p != NULL ? (__capability void *)c10p : cclear);

	v0 = sandbox_object_cinvoke(sbop, methodnum, a1, a2, a3, a4, a5, a6,
	    a7, c3, c4, c5, c6, c7, c8, c9, c10);
	if (v0 < 0) {
		if (methodnum < SANDBOX_CLASS_METHOD_COUNT)
			SANDBOX_METHOD_FAULT(
			    sbcp->sbc_sandbox_methods[methodnum]);
		else
			SANDBOX_METHOD_FAULT(
			    sbcp->sbc_sandbox_method_nonamep);
		SANDBOX_OBJECT_FAULT(sbop->sbo_sandbox_object_statp);
	}
	return (v0);
}

void
sandbox_object_destroy(struct sandbox_object *sbop)
{
	struct sandbox_class *sbcp;

	sbcp = sbop->sbo_sandbox_classp;
	SANDBOX_CLASS_FREE(sbcp->sbc_sandbox_class_statp);
	if (sbop->sbo_sandbox_object_statp != NULL)
		(void)sandbox_stat_object_deregister(
		    sbop->sbo_sandbox_object_statp);
	munmap(sbop->sbo_mem, sbcp->sbc_sandboxlen);
	bzero(sbop, sizeof(*sbop));		/* Clears tags. */
	free(sbop);
}

struct cheri_object
sandbox_object_getsystemobject(struct sandbox_object *sbop)
{

	return (sbop->sbo_cheri_system_object);
}

int
sandbox_setup(const char *path, register_t sandboxlen, struct sandbox **sbpp)
{
	struct sandbox *sbp;

	sbp = calloc(1, sizeof(*sbp));
	if (sbp == NULL)
		return (-1);
	if (sandbox_class_new(path, sandboxlen, &sbp->sb_sandbox_classp) !=
	    0) {
		free(sbp);
		return (-1);
	}
	if (sandbox_object_new(sbp->sb_sandbox_classp,
	    &sbp->sb_sandbox_objectp) != 0) {
		sandbox_class_destroy(sbp->sb_sandbox_classp);
		free(sbp);
		return (-1);
	}
	*sbpp = sbp;
	return (0);
}

void
sandbox_destroy(struct sandbox *sb)
{

	sandbox_object_destroy(sb->sb_sandbox_objectp);
	sandbox_class_destroy(sb->sb_sandbox_classp);
	free(sb);
}

register_t
sandbox_cinvoke(struct sandbox *sb, register_t a0, register_t a1,
    register_t a2, register_t a3, register_t a4, register_t a5, register_t a6,
    register_t a7, __capability void *c3, __capability void *c4,
    __capability void *c5, __capability void *c6, __capability void *c7,
    __capability void *c8, __capability void *c9, __capability void *c10)
{

	return (sandbox_object_cinvoke(sb->sb_sandbox_objectp, a0, a1, a2, a3,
	    a4, a5, a6, a7, c3, c4, c5, c6, c7, c8, c9, c10));
}

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

	return (sandbox_object_invoke(sb->sb_sandbox_objectp, a0, a1, a2, a3,
	    0, 0, 0, 0, c3p, c4p, c5p, c6p, c7p, c8p, c9p, c10p));
}
