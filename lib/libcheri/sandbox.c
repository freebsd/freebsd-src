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

#include "cheri_enter.h"
#include "cheri_invoke.h"
#include "libcheri_stat.h"
#include "sandbox.h"
#include "sandbox_internal.h"
#include "sandboxasm.h"

/*
 * Control verbose debugging output around sandbox invocation; disabled by
 * default but may be enabled using an environmental variable.
 */
int sb_verbose;

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

int
sandbox_object_new(struct sandbox_class *sbcp, struct sandbox_object **sbopp)
{
	struct sandbox_object *sbop;
	int error;

	sbop = calloc(1, sizeof(*sbop));
	if (sbop == NULL)
		return (-1);
	sbop->sbo_sandbox_classp = sbcp;

	error = sandbox_object_load(sbcp, sbop);
	if (error) {
		free(sbop);
		return (-1);
	}

	/*
	 * Invoke object instance's constructors.  Note that, given the tight
	 * binding of class and object in the sandbox library currently, this
	 * will need to change in the future.  We also need to think more
	 * carefully about the mechanism here.
	 *
	 * NB: Should we be passing in a system-class reference...?
	 */
	(void)cheri_invoke(sbop->sbo_cheri_object,
	    SANDBOX_RUNTIME_CONSTRUCTORS, 0, 0, 0, 0, 0, 0, 0,
	    cheri_zerocap(), cheri_zerocap(), cheri_zerocap(),
	    cheri_zerocap(), cheri_zerocap(), cheri_zerocap(),
	    cheri_zerocap(), cheri_zerocap());

	/*
	 * Now that constructors have completed, return object.
	 */
	*sbopp = sbop;
	return (0);
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
	start = cheri_get_cyclecount();
	v0 = cheri_invoke(sbop->sbo_cheri_object, methodnum, a1, a2, a3, a4,
	    a5, a6, a7, c3, c4, c5, c6, c7, c8, c9, c10);
	sample = cheri_get_cyclecount() - start;
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
	sandbox_object_unload(sbcp, sbop);	/* Unmap memory. */
	bzero(sbop, sizeof(*sbop));		/* Clears tags. */
	free(sbop);
}

struct cheri_object
sandbox_object_getobject(struct sandbox_object *sbop)
{

	return (sbop->sbo_cheri_object);
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
