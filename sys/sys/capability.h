/*-
 * Copyright (c) 2008-2009 Robert N. M. Watson
 * All rights reserved.
 *
 * WARNING: THIS IS EXPERIMENTAL SECURITY SOFTWARE THAT MUST NOT BE RELIED
 * ON IN PRODUCTION SYSTEMS.  IT WILL BREAK YOUR SOFTWARE IN NEW AND
 * UNEXPECTED WAYS.
 * 
 * This software was developed at the University of Cambridge Computer
 * Laboratory with support from a grant from Google, Inc. 
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
 * $P4: //depot/projects/trustedbsd/capabilities/src/sys/sys/capability.h#25 $
 */

/*
 * Definitions for FreeBSD capabilities facility.
 */
#ifndef _SYS_CAPABILITY_H_
#define	_SYS_CAPABILITY_H_

#include <sys/cdefs.h>
#include <sys/types.h>

/*
 * Possibly rights on capabilities.
 */
#define	CAP_READ		0x0000000000000001ULL	/* read/recv */
#define	CAP_WRITE		0x0000000000000002ULL	/* write/send */
#define	CAP_SEEK		0x0000000000000004ULL	/* lseek, various io */
#define	CAP_GETPEERNAME		0x0000000000000008ULL	/* getpeername */
#define	CAP_GETSOCKNAME		0x0000000000000010ULL	/* getsockname */
#define	CAP_FCHFLAGS		0x0000000000000020ULL	/* fchflags */
#define	CAP_IOCTL		0x0000000000000040ULL	/* ioctl */
#define	CAP_FSTAT		0x0000000000000080ULL	/* fstat */
#define	CAP_MMAP		0x0000000000000100ULL	/* mmap */
#define	CAP_FCNTL		0x0000000000000200ULL	/* fcntl */
#define	CAP_EVENT		0x0000000000000400ULL	/* select/poll */
#define	CAP_FSYNC		0x0000000000000800ULL	/* fsync */
#define	CAP_FCHOWN		0x0000000000001000ULL	/* fchown */
#define	CAP_FCHMOD		0x0000000000002000ULL	/* fchmod */
#define	CAP_FTRUNCATE		0x0000000000004000ULL	/* ftruncate */
#define	CAP_FLOCK		0x0000000000008000ULL	/* flock */
#define	CAP_FSTATFS		0x0000000000010000ULL	/* fstatfs */
#define	CAP_REVOKE		0x0000000000020000ULL	/* revoke */
#define	CAP_FEXECVE		0x0000000000040000ULL	/* fexecve */
#define	CAP_FPATHCONF		0x0000000000080000ULL	/* fpathconf */
#define	CAP_FUTIMES		0x0000000000100000ULL	/* futimes */
#define	CAP_ACL_GET		0x0000000000200000ULL	/* acl_get_fd */
#define	CAP_ACL_SET		0x0000000000400000ULL	/* acl_set_fd */
#define	CAP_ACL_DELETE		0x0000000000800000ULL	/* acl_delete_fd */
#define	CAP_ACL_CHECK		0x0000000001000000ULL	/* acl_list_fd */
#define	CAP_EXTATTR_GET		0x0000000002000000ULL	/* extattr_get_fd */
#define	CAP_EXTATTR_SET		0x0000000004000000ULL	/* extattr_set_fd */
#define	CAP_EXTATTR_DELETE	0x0000000008000000ULL	/* extattr_delete_fd */
#define	CAP_EXTATTR_LIST	0x0000000010000000ULL	/* extattr_list_fd */
#define	CAP_MAC_GET		0x0000000020000000ULL	/* mac_get_fd */
#define	CAP_MAC_SET		0x0000000040000000ULL	/* mac_set_fd */
#define	CAP_ACCEPT		0x0000000080000000ULL	/* accept */
#define	CAP_CONNECT		0x0000000100000000ULL	/* connect/sendto */
#define	CAP_BIND		0x0000000200000000ULL	/* bind */
#define	CAP_GETSOCKOPT		0x0000000400000000ULL	/* getsockopt */
#define	CAP_SETSOCKOPT		0x0000000800000000ULL	/* setsockopt */
#define	CAP_LISTEN		0x0000001000000000ULL	/* listen */
#define	CAP_SHUTDOWN		0x0000002000000000ULL	/* shutdown */
#define	CAP_PEELOFF		0x0000004000000000ULL	/* sctp_peeloff */
#define	CAP_LOOKUP		0x0000008000000000ULL	/* _at(2) lookup */
#define	CAP_SEM_POST		0x0000010000000000ULL	/* ksem_post */
#define	CAP_SEM_WAIT		0x0000020000000000ULL	/* ksem_wait */
#define	CAP_SEM_GETVALUE	0x0000040000000000ULL	/* ksem_getvalue */
#define	CAP_KEVENT		0x0000080000000000ULL	/* kevent(2) */
#define	CAP_PDGETPID		0x0000100000000000ULL	/* pdgetpid(2) */
#define	CAP_PDWAIT		0x0000200000000000ULL	/* pdwait(2) */
#define	CAP_PDKILL		0x0000400000000000ULL	/* pdkill(2) */
#define	CAP_MAPEXEC		0x0000800000000000ULL	/* mmap(2) as exec */
#define	CAP_TTYHOOK		0x0001000000000000ULL	/* register tty hook */
#define	CAP_FCHDIR		0x0002000000000000ULL	/* fchdir(2) */
#define	CAP_FSCK		0x0004000000000000ULL	/* sysctl_ffs_fsck */
#define	CAP_MASK_VALID		0x0007ffffffffffffULL

/*
 * Notes:
 *
 * Some system calls don't require a capability in order to perform an
 * operation on an fd.  These include: close, dup, dup2.
 *
 * CAP_SEEK is used alone for lseek, but along-side CAP_READ and CAP_WRITE
 * for various I/O calls, such as read/write/send/receive.
 *
 * pread and pwrite will not use CAP_SEEK.
 *
 * CAP_EVENT covers select, poll, and kqueue registration for a capability;
 * CAP_KEVENT controls the use of a kqueue(2) description.
 *
 * sendfile is authorized using CAP_READ on the file and CAP_WRITE on the
 * socket.
 *
 * sendto should check CAP_CONNECT as well as CAP_WRITE if an address is
 * specified.
 *
 * mmap() and aio*() system calls will need special attention as they may
 * involve reads or writes depending a great deal on context.
 *
 * Socket checks don't generally pass CAP_SEEK but perhaps should?
 */

/*
 * A mask of multiple capabilities useful for situation where a socket will
 * be used in a general-purpose way by the kernel, such as a socket used by
 * the NFS server.
 */
#define	CAP_SOCK_ALL	(CAP_READ | CAP_WRITE | CAP_SEEK | CAP_GETPEERNAME | \
			    CAP_GETSOCKNAME | CAP_IOCTL | CAP_FSTAT | \
			    CAP_FCNTL | CAP_EVENT | CAP_ACCEPT | \
			    CAP_CONNECT | CAP_BIND | CAP_GETSOCKOPT | \
			    CAP_SETSOCKOPT | CAP_LISTEN | CAP_SHUTDOWN | \
			    CAP_PEELOFF)

#ifdef _KERNEL
struct file;

/*
 * Given a file descriptor that may be a capability, check the requested
 * rights and extract the underlying object.  Assumes a valid reference is
 * held to fp_cap, and returns a pointer via fpp under that assumption.  The
 * caller invokes fhold(*fpp) if required.
 */
int	cap_fextract(struct file *fp_cap, cap_rights_t rights,
	    struct file **fpp);
int	cap_fextract_mmap(struct file *fp_cap, cap_rights_t rights,
	    u_char *maxprotp, struct file **fpp);

/*
 * For the purposes of procstat(1) and similar tools, allow kern_descrip.c to
 * extract the rights from a capability.  However, this should not be used by
 * kernel code generally, instead cap_fextract() should be used in order to
 * keep all access control in one place.
 */
cap_rights_t	cap_rights(struct file *fp_cap);

#else /* !_KERNEL */

__BEGIN_DECLS
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
 * cap_new(): Create a new capability derived from an existing file
 * descriptor with the specified rights.  If the existing file descriptor is
 * a capability, then the new rights must be a subset of the existing rights.
 */
int	cap_new(int fd, cap_rights_t rights);

/*
 * cap_getrights(): Query the rights on a capability.
 */
int	cap_getrights(int fd, cap_rights_t *rightsp);
__END_DECLS

#endif /* !_KERNEL */

#endif /* !_SYS_CAPABILITY_H_ */
