/*-
 * Copyright (c) 1999 Robert N. M. Watson
 * All rights reserved.
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
 *
 * $FreeBSD$
 */
/*
 * Userland/kernel interface for Extended File System Attributes
 *
 * This code from the FreeBSD POSIX.1e implementation.  While the syscalls
 * are fully implemented, invoking the VFS vnops and VFS calls as necessary,
 * no file systems shipped with this version of FreeBSD implement these
 * calls.  Extensions to UFS/FFS to support extended attributes are
 * available from the POSIX.1e implementation page, or possibly in a more
 * recent version of FreeBSD.
 *
 * The POSIX.1e implementation page may be reached at:
 *   http://www.watson.org/fbsd-hardening/posix1e/
 */

#ifndef _SYS_EXTATTR_H_
#define _SYS_EXTATTR_H_

#define EXTATTR_MAXNAMELEN	NAME_MAX

#ifndef KERNEL

int	extattrctl(char *path, int cmd, char *attrname, caddr_t arg);
int	extattr_set_file(char *path, char *attrname, struct iovec *iovp,
	u_int iovcnt);
int	extattr_get_file(char *path, char *attrname, struct iovec *iovp,
	u_int iovcnt);
int	extattr_delete_file(char *path, char *attrname);

#endif /* KERNEL */
#endif /* _SYS_EXTATTR_H_ */
