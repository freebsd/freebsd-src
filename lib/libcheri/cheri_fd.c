/*-
 * Copyright (c) 2014-2015 Robert N. M. Watson
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
#include <sys/stat.h>

#include <machine/cheri.h>
#include <machine/cheric.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "cheri_class.h"
#define CHERI_FD_INTERNAL
#include "cheri_fd.h"
#include "cheri_type.h"

/*
 * This file implements the CHERI 'file descriptor' (fd) class.  Pretty
 * minimalist.
 *
 * XXXRW: This is a slightly risky business, as we're taking capabilities as
 * arguments and casting them back to global pointers that can be passed to
 * the conventional MIPS system-call ABI.  We need to check that the access we
 * then perform on the pointer is authorised by the capability it was derived
 * from (e.g., length, permissions).
 *
 * XXXRW: Right now, no implementation of permission checking narrowing
 * file-descriptor rights, but we will add that once user permissions are
 * supported.  There's some argument we would like to have a larger permission
 * mask than supported by CHERI -- how should we handle that?
 *
 * XXXRW: This raises lots of questions about reference/memory management.
 * For now, define a 'revoke' interface that clears the fd (-1) to prevent new
 * operations from started.  However, this doesn't block waiting on old ones
 * to complete.  Differentiate 'revoke' from 'destroy', the latter of which is
 * safe only once all references have drained.  We rely on the ambient code
 * knowing when it is safe (or perhaps never calling it).
 */

CHERI_CLASS_DECL(cheri_fd);

static __capability void	*cheri_fd_type;
__capability intptr_t		*cheri_fd_vtable;

/*
 * Data segment for a cheri_fd.
 */
struct cheri_fd {
	CHERI_SYSTEM_OBJECT_FIELDS;
	int	cf_fd;	/* Underlying file descriptor. */
};

#define	min(x, y)	((x) < (y) ? (x) : (y))

static __attribute__ ((constructor)) void
cheri_fd_init(void)
{

	cheri_fd_type = cheri_type_alloc();
}

/*
 * Allocate a new cheri_fd object for an already-open file descriptor.
 */
int
cheri_fd_new(int fd, struct cheri_object *cop)
{
	__capability void *codecap, *datacap;
	struct cheri_fd *cfp;

	cfp = calloc(1, sizeof(*cfp));
	if (cfp == NULL) {
		errno = ENOMEM;
		return (-1);
	}
	CHERI_SYSTEM_OBJECT_INIT(cfp, cheri_fd_vtable);
	cfp->cf_fd = fd;

	/*
	 * Construct a sealed code capability for the class.  This is just the
	 * ambient $pcc with the offset set to the entry address.
	 *
	 * XXXRW: For now, when invoked, we install $pcc into $c0, so this
	 * needs a full set of permissions rather than just LOAD/EXECUTE. In
	 * the future, we will want to preserve a copy of cheri_getdefault()
	 * in the struct cheri_fd to be reinstalled by the entry code.
	 *
	 * XXXRW: In the future, use cheri_codeptr() here?
	 */
	codecap = cheri_setoffset(cheri_getpcc(),
	    (register_t)CHERI_CLASS_ENTRY(cheri_fd));
	cop->co_codecap = cheri_seal(codecap, cheri_fd_type);

	/*
	 * Construct a sealed data capability for the class.  This describes
	 * the 'struct cheri_fd' for the specific file descriptor.  The $c0
	 * to reinstall later is the first field in the structure.
	 *
	 * XXXRW: Should we also do an explicit cheri_setoffset()?
	 */
	datacap = cheri_ptrperm(cfp, sizeof(*cfp), CHERI_PERM_GLOBAL |
	    CHERI_PERM_LOAD | CHERI_PERM_LOAD_CAP | CHERI_PERM_STORE |
	    CHERI_PERM_STORE_CAP);
	cop->co_datacap = cheri_seal(datacap, cheri_fd_type);
	return (0);
}

/*
 * Revoke further accesses via the object -- although in-flight accesses
 * continue.  Note: does not close the fd or free memory.  The latter must
 */
void
cheri_fd_revoke(struct cheri_object co)
{
	__capability struct cheri_fd *cfp;

	cfp = cheri_unseal(co.co_datacap, cheri_fd_type);
	cfp->cf_fd = -1;
}

/*
 * Actually free a cheri_fd.  This can only be done if there are no
 * outstanding references in any sandboxes (etc).
 */
void
cheri_fd_destroy(struct cheri_object co)
{
	__capability struct cheri_fd *cfp;

	cfp = cheri_unseal(co.co_datacap, cheri_fd_type);
	CHERI_SYSTEM_OBJECT_FINI(cfp);
	free((void *)cfp);
}

/*
 * Forward fstat() on a cheri_fd to the underlying file descriptor.
 */
struct cheri_fd_ret
cheri_fd_fstat(__capability struct stat *sb_c)
{
	struct cheri_fd_ret ret;
	__capability struct cheri_fd *cfp;
	struct stat *sb;

	/* XXXRW: Object-capability user permission check on idc. */

	/* XXXRW: Change to check permissions directly and throw exception. */
	if (!(cheri_getperm(sb_c) & CHERI_PERM_STORE) ||
	    !(cheri_getlen(sb_c) >= sizeof(*sb))) {
		ret.cfr_retval0 = -1;
		ret.cfr_retval1 = EPROT;
		return (ret);
	}
	sb = (void *)sb_c;

	/* Check that the cheri_fd hasn't been revoked. */
	cfp = cheri_getidc();
	if (cfp->cf_fd == -1) {
		ret.cfr_retval0 = -1;
		ret.cfr_retval1 = EBADF;
		return (ret);
	}

	/* Forward to operating system. */
	ret.cfr_retval0 = fstat(cfp->cf_fd, sb);
	ret.cfr_retval1 = (ret.cfr_retval0 < 0 ? errno : 0);
	return (ret);
}

/*
 * Forward lseek() on a cheri_fd to the underlying file descriptor.
 */
struct cheri_fd_ret
cheri_fd_lseek(off_t offset, int whence)
{
	struct cheri_fd_ret ret;
	__capability struct cheri_fd *cfp;

	/* XXXRW: Object-capability user permission check on idc. */

	/* Check that the cheri_fd hasn't been revoked. */
	cfp = cheri_getidc();
	if (cfp->cf_fd == -1) {
		ret.cfr_retval0 = -1;
		ret.cfr_retval1 = EBADF;
		return (ret);
	}

	/* Forward to operating system. */
	ret.cfr_retval0 = lseek(cfp->cf_fd, offset, whence);
	ret.cfr_retval1 = (ret.cfr_retval0 < 0 ? errno : 0);
	return (ret);
}

/*
 * Forward read() on a cheri_fd to the underlying file descriptor.
 */
struct cheri_fd_ret
cheri_fd_read(__capability void *buf_c, size_t nbytes)
{
	struct cheri_fd_ret ret;
	__capability struct cheri_fd *cfp;
	void *buf;

	/* XXXRW: Object-capability user permission check on idc. */

	/* XXXRW: Change to check permissions directly and throw exception. */
	if (!(cheri_getperm(buf_c) & CHERI_PERM_STORE)) {
		ret.cfr_retval0 = -1;
		ret.cfr_retval1 = EPROT;
		return (ret);
	}
	buf = (void *)buf_c;

	/* Check that the cheri_fd hasn't been revoked. */
	cfp = cheri_getidc();
	if (cfp->cf_fd == -1) {
		ret.cfr_retval0 = -1;
		ret.cfr_retval1 = EBADF;
		return (ret);
	}

	/* Forward to operating system. */
	ret.cfr_retval0 = read(cfp->cf_fd, buf,
	    min(nbytes, cheri_getlen(buf_c) - cheri_getoffset(buf_c)));
	ret.cfr_retval1 = (ret.cfr_retval0 < 0 ? errno : 0);
	return (ret);
}

/*
 * Forward write_c() on a cheri_fd to the underlying file descriptor.
 */
struct cheri_fd_ret
cheri_fd_write(__capability const void *buf_c, size_t nbytes)
{
	struct cheri_fd_ret ret;
	__capability struct cheri_fd *cfp;
	void *buf;

	/* XXXRW: Object-capability user permission check on idc. */

	/* XXXRW: Change to check permissions directly and throw exception. */
	if (!(cheri_getperm(buf_c) & CHERI_PERM_LOAD)) {
		ret.cfr_retval0 = -1;
		ret.cfr_retval1 = EPROT;
		return (ret);
	}
	buf = (void *)buf_c;

	/* Check that cheri_fd hasn't been revoked. */
	cfp = cheri_getidc();
	if (cfp->cf_fd == -1) {
		ret.cfr_retval0 = -1;
		ret.cfr_retval1 = EBADF;
		return (ret);
	}

	/* Forward to operating system. */
	ret.cfr_retval0 = write(cfp->cf_fd, buf,
	    min(nbytes, cheri_getlen(buf_c) - cheri_getoffset(buf_c)));
	ret.cfr_retval1 = (ret.cfr_retval0 < 0 ? errno : 0);
	return (ret);
}
