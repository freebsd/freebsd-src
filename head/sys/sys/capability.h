/*-
 * Copyright (c) 2008-2010 Robert N. M. Watson
 * Copyright (c) 2012 FreeBSD Foundation
 * All rights reserved.
 *
 * This software was developed at the University of Cambridge Computer
 * Laboratory with support from a grant from Google, Inc.
 *
 * Portions of this software were developed by Pawel Jakub Dawidek under
 * sponsorship from the FreeBSD Foundation.
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
 * Definitions for FreeBSD capabilities facility.
 */
#ifndef _SYS_CAPABILITY_H_
#define	_SYS_CAPABILITY_H_

#include <sys/cdefs.h>
#include <sys/param.h>

#include <sys/file.h>
#include <sys/fcntl.h>

/*
 * Possible rights on capabilities.
 *
 * Notes:
 * Some system calls don't require a capability in order to perform an
 * operation on an fd.  These include: close, dup, dup2.
 *
 * sendfile is authorized using CAP_READ on the file and CAP_WRITE on the
 * socket.
 *
 * mmap() and aio*() system calls will need special attention as they may
 * involve reads or writes depending a great deal on context.
 */

#define	CAP_NONE		0x0000000000000000ULL

/*
 * General file I/O.
 */
/* Allows for openat(O_RDONLY), read(2), readv(2). */
#define	CAP_READ		0x0000000000000001ULL
/* Allows for openat(O_WRONLY | O_APPEND), write(2), writev(2). */
#define	CAP_WRITE		0x0000000000000002ULL
/* Allows for lseek(2). */
#define	CAP_SEEK		0x0000000000000080ULL
/* Allows for pread(2), preadv(2). */
#define	CAP_PREAD		(CAP_SEEK | CAP_READ)
/* Allows for openat(O_WRONLY) (without O_APPEND), pwrite(2), pwritev(2). */
#define	CAP_PWRITE		(CAP_SEEK | CAP_WRITE)
/* Allows for mmap(PROT_NONE). */
#define	CAP_MMAP		0x0000000000000004ULL
/* Allows for mmap(PROT_READ). */
#define	CAP_MMAP_R		(CAP_MMAP | CAP_SEEK | CAP_READ)
/* Allows for mmap(PROT_WRITE). */
#define	CAP_MMAP_W		(CAP_MMAP | CAP_SEEK | CAP_WRITE)
/* Allows for mmap(PROT_EXEC). */
#define	CAP_MMAP_X		(CAP_MMAP | CAP_SEEK | 0x0000000000000008ULL)
/* Allows for mmap(PROT_READ | PROT_WRITE). */
#define	CAP_MMAP_RW		(CAP_MMAP_R | CAP_MMAP_W)
/* Allows for mmap(PROT_READ | PROT_EXEC). */
#define	CAP_MMAP_RX		(CAP_MMAP_R | CAP_MMAP_X)
/* Allows for mmap(PROT_WRITE | PROT_EXEC). */
#define	CAP_MMAP_WX		(CAP_MMAP_W | CAP_MMAP_X)
/* Allows for mmap(PROT_READ | PROT_WRITE | PROT_EXEC). */
#define	CAP_MMAP_RWX		(CAP_MMAP_R | CAP_MMAP_W | CAP_MMAP_X)
/* Allows for openat(O_CREAT). */
#define	CAP_CREATE		0x0000000000080000ULL
/* Allows for openat(O_EXEC) and fexecve(2) in turn. */
#define	CAP_FEXECVE		0x0000000000000010ULL
/* Allows for openat(O_SYNC), openat(O_FSYNC), fsync(2). */
#define	CAP_FSYNC		0x0000000000000020ULL
/* Allows for openat(O_TRUNC), ftruncate(2). */
#define	CAP_FTRUNCATE		0x0000000000000040ULL

/* VFS methods. */
#define	CAP_FCHDIR		0x0000000000000200ULL
#define	CAP_FCHFLAGS		0x0000000000000100ULL
#define	CAP_CHFLAGSAT		CAP_FCHFLAGS
#define	CAP_FCHMOD		0x0000000000000400ULL
#define	CAP_FCHMODAT		CAP_FCHMOD
#define	CAP_FCHOWN		0x0000000000000800ULL
#define	CAP_FCHOWNAT		CAP_FCHOWN
#define	CAP_FCNTL		0x0000000000001000ULL
#define	CAP_FLOCK		0x0000000000004000ULL
#define	CAP_FPATHCONF		0x0000000000002000ULL
#define	CAP_FSCK		0x0000000000008000ULL
#define	CAP_FSTAT		0x0000000000010000ULL
#define	CAP_FSTATAT		CAP_FSTAT
#define	CAP_FSTATFS		0x0000000000020000ULL
#define	CAP_FUTIMES		0x0000000000040000ULL
#define	CAP_FUTIMESAT		CAP_FUTIMES
#define	CAP_LINKAT		0x0000000000400000ULL
#define	CAP_MKDIRAT		0x0000000000200000ULL
#define	CAP_MKFIFOAT		0x0000000000800000ULL
#define	CAP_MKNODAT		0x0080000000000000ULL
#define	CAP_RENAMEAT		0x0200000000000000ULL
#define	CAP_SYMLINKAT		0x0100000000000000ULL
#define	CAP_UNLINKAT		0x0000000000100000ULL

/* Lookups - used to constrain *at() calls. */
#define	CAP_LOOKUP		0x0000000001000000ULL

/* Extended attributes. */
#define	CAP_EXTATTR_DELETE	0x0000000002000000ULL
#define	CAP_EXTATTR_GET		0x0000000004000000ULL
#define	CAP_EXTATTR_LIST	0x0000000008000000ULL
#define	CAP_EXTATTR_SET		0x0000000010000000ULL

/* Access Control Lists. */
#define	CAP_ACL_CHECK		0x0000000020000000ULL
#define	CAP_ACL_DELETE		0x0000000040000000ULL
#define	CAP_ACL_GET		0x0000000080000000ULL
#define	CAP_ACL_SET		0x0000000100000000ULL

/* Socket operations. */
#define	CAP_ACCEPT		0x0000000200000000ULL
#define	CAP_BIND		0x0000000400000000ULL
#define	CAP_CONNECT		0x0000000800000000ULL
#define	CAP_GETPEERNAME		0x0000001000000000ULL
#define	CAP_GETSOCKNAME		0x0000002000000000ULL
#define	CAP_GETSOCKOPT		0x0000004000000000ULL
#define	CAP_LISTEN		0x0000008000000000ULL
#define	CAP_PEELOFF		0x0000010000000000ULL
#define	CAP_RECV		CAP_READ
#define	CAP_SEND		CAP_WRITE
#define	CAP_SETSOCKOPT		0x0000020000000000ULL
#define	CAP_SHUTDOWN		0x0000040000000000ULL

#define	CAP_SOCK_CLIENT \
	(CAP_CONNECT | CAP_GETPEERNAME | CAP_GETSOCKNAME | CAP_GETSOCKOPT | \
	 CAP_PEELOFF | CAP_RECV | CAP_SEND | CAP_SETSOCKOPT | CAP_SHUTDOWN)
#define	CAP_SOCK_SERVER \
	(CAP_ACCEPT | CAP_BIND | CAP_GETPEERNAME | CAP_GETSOCKNAME | \
	 CAP_GETSOCKOPT | CAP_LISTEN | CAP_PEELOFF | CAP_RECV | CAP_SEND | \
	 CAP_SETSOCKOPT | CAP_SHUTDOWN)

/* Mandatory Access Control. */
#define	CAP_MAC_GET		0x0000080000000000ULL
#define	CAP_MAC_SET		0x0000100000000000ULL

/* Methods on semaphores. */
#define	CAP_SEM_GETVALUE	0x0000200000000000ULL
#define	CAP_SEM_POST		0x0000400000000000ULL
#define	CAP_SEM_WAIT		0x0000800000000000ULL

/* kqueue events. */
#define	CAP_POLL_EVENT		0x0001000000000000ULL
#define	CAP_POST_EVENT		0x0002000000000000ULL

/* Strange and powerful rights that should not be given lightly. */
#define	CAP_IOCTL		0x0004000000000000ULL
#define	CAP_TTYHOOK		0x0008000000000000ULL

/* Process management via process descriptors. */
#define	CAP_PDGETPID		0x0010000000000000ULL
#define	CAP_PDWAIT		0x0020000000000000ULL
#define	CAP_PDKILL		0x0040000000000000ULL

/*
 * Rights that allow to use bindat(2) and connectat(2) syscalls on a
 * directory descriptor.
 */
#define	CAP_BINDAT		0x0400000000000000ULL
#define	CAP_CONNECTAT		0x0800000000000000ULL

/* The mask of all valid method rights. */
#define	CAP_MASK_VALID		0x0fffffffffffffffULL
#define	CAP_ALL			CAP_MASK_VALID

/* Available bits. */
#define	CAP_UNUSED3		0x1000000000000000ULL
#define	CAP_UNUSED2		0x2000000000000000ULL
#define	CAP_UNUSED1		0x4000000000000000ULL
#define	CAP_UNUSED0		0x8000000000000000ULL

/*
 * The following defines are provided for backward API compatibility and
 * should not be used in new code.
 */
#define	CAP_MAPEXEC		CAP_MMAP_X
#define	CAP_DELETE		CAP_UNLINKAT
#define	CAP_MKDIR		CAP_MKDIRAT
#define	CAP_RMDIR		CAP_UNLINKAT
#define	CAP_MKFIFO		CAP_MKFIFOAT
#define	CAP_MKNOD		CAP_MKNODAT
#define	CAP_SOCK_ALL		(CAP_SOCK_CLIENT | CAP_SOCK_SERVER)

/*
 * Allowed fcntl(2) commands.
 */
#define	CAP_FCNTL_GETFL		(1 << F_GETFL)
#define	CAP_FCNTL_SETFL		(1 << F_SETFL)
#if __BSD_VISIBLE || __XSI_VISIBLE || __POSIX_VISIBLE >= 200112
#define	CAP_FCNTL_GETOWN	(1 << F_GETOWN)
#define	CAP_FCNTL_SETOWN	(1 << F_SETOWN)
#endif
#if __BSD_VISIBLE || __XSI_VISIBLE || __POSIX_VISIBLE >= 200112
#define	CAP_FCNTL_ALL		(CAP_FCNTL_GETFL | CAP_FCNTL_SETFL | \
				 CAP_FCNTL_GETOWN | CAP_FCNTL_SETOWN)
#else
#define	CAP_FCNTL_ALL		(CAP_FCNTL_GETFL | CAP_FCNTL_SETFL)
#endif

#define	CAP_IOCTLS_ALL	SSIZE_MAX

#ifdef _KERNEL

#include <sys/systm.h>

#define IN_CAPABILITY_MODE(td) ((td->td_ucred->cr_flags & CRED_FLAG_CAPMODE) != 0)

struct filedesc;

/*
 * Test whether a capability grants the requested rights.
 */
int	cap_check(cap_rights_t have, cap_rights_t need);
/*
 * Convert capability rights into VM access flags.
 */
u_char	cap_rights_to_vmprot(cap_rights_t have);

/*
 * For the purposes of procstat(1) and similar tools, allow kern_descrip.c to
 * extract the rights from a capability.
 */
cap_rights_t	cap_rights(struct filedesc *fdp, int fd);

int	cap_ioctl_check(struct filedesc *fdp, int fd, u_long cmd);
int	cap_fcntl_check(struct filedesc *fdp, int fd, int cmd);

#else /* !_KERNEL */

__BEGIN_DECLS
#include <stdbool.h>

/*
 * cap_enter(): Cause the process to enter capability mode, which will
 * prevent it from directly accessing global namespaces.  System calls will
 * be limited to process-local, process-inherited, or file descriptor
 * operations.  If already in capability mode, a no-op.
 *
 * Currently, process-inherited operations are not properly handled -- in
 * particular, we're interested in things like waitpid(2), kill(2), etc,
 * being properly constrained.  One possible solution is to introduce process
 * descriptors.
 */
int	cap_enter(void);

/*
 * Are we sandboxed (in capability mode)?
 * This is a libc wrapper around the cap_getmode(2) system call.
 */
bool	cap_sandboxed(void);

/*
 * cap_getmode(): Are we in capability mode?
 */
int	cap_getmode(u_int *modep);

/*
 * Limits capability rights for the given descriptor (CAP_*).
 */
int cap_rights_limit(int fd, cap_rights_t rights);
/*
 * Returns bitmask of capability rights for the given descriptor.
 */
int cap_rights_get(int fd, cap_rights_t *rightsp);
/*
 * Limits allowed ioctls for the given descriptor.
 */
int cap_ioctls_limit(int fd, const unsigned long *cmds, size_t ncmds);
/*
 * Returns array of allowed ioctls for the given descriptor.
 * If all ioctls are allowed, the cmds array is not populated and
 * the function returns CAP_IOCTLS_ALL.
 */
ssize_t cap_ioctls_get(int fd, unsigned long *cmds, size_t maxcmds);
/*
 * Limits allowed fcntls for the given descriptor (CAP_FCNTL_*).
 */
int cap_fcntls_limit(int fd, uint32_t fcntlrights);
/*
 * Returns bitmask of allowed fcntls for the given descriptor.
 */
int cap_fcntls_get(int fd, uint32_t *fcntlrightsp);

/* For backward compatibility. */
int cap_new(int fd, cap_rights_t rights);
#define	cap_getrights(fd, rightsp)	cap_rights_get((fd), (rightsp))

__END_DECLS

#endif /* !_KERNEL */

#endif /* !_SYS_CAPABILITY_H_ */
