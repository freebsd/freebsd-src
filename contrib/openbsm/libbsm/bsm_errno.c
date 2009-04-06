/*-
 * Copyright (c) 2008 Apple Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE. 
 *
 * $P4: //depot/projects/trustedbsd/openbsm/libbsm/bsm_errno.c#12 $
 */

#include <sys/types.h>

#include <config/config.h>

#include <bsm/audit_errno.h>
#include <bsm/libbsm.h>

#include <errno.h>
#include <string.h>

/*
 * Different operating systems use different numeric constants for different
 * error numbers, and sometimes error numbers don't exist in more than one
 * operating system.  These routines convert between BSM and local error
 * number spaces, subject to the above realities.  BSM error numbers are
 * stored in a single 8-bit character, so don't have a byte order.
 */

struct bsm_errors {
	int		 be_bsm_error;
	int		 be_os_error;
	const char	*be_strerror;
};

#define	ERRNO_NO_LOCAL_MAPPING	-600

/*
 * Mapping table -- please maintain in numeric sorted order with respect to
 * the BSM constant.  Today we do a linear lookup, but could switch to a
 * binary search if it makes sense.  We only ifdef errors that aren't
 * generally available, but it does make the table a lot more ugly.
 *
 * XXXRW: It would be nice to have a similar ordered table mapping to BSM
 * constant from local constant, but the order of local constants varies by
 * OS.  Really we need to build that table at compile-time but don't do that
 * yet.
 *
 * XXXRW: We currently embed English-language error strings here, but should
 * support catalogues; these are only used if the OS doesn't have an error
 * string using strerror(3).
 */
static const struct bsm_errors bsm_errors[] = {
	{ BSM_ESUCCESS, 0, "Success" },
	{ BSM_EPERM, EPERM, "Operation not permitted" },
	{ BSM_ENOENT, ENOENT, "No such file or directory" },
	{ BSM_ESRCH, ESRCH, "No such process" },
	{ BSM_EINTR, EINTR, "Interrupted system call" },
	{ BSM_EIO, EIO, "Input/output error" },
	{ BSM_ENXIO, ENXIO, "Device not configured" },
	{ BSM_E2BIG, E2BIG, "Argument list too long" },
	{ BSM_ENOEXEC, ENOEXEC, "Exec format error" },
	{ BSM_EBADF, EBADF, "BAd file descriptor" },
	{ BSM_ECHILD, ECHILD, "No child processes" },
	{ BSM_EAGAIN, EAGAIN, "Resource temporarily unavailable" },
	{ BSM_ENOMEM, ENOMEM, "Cannot allocate memory" },
	{ BSM_EACCES, EACCES, "Permission denied" },
	{ BSM_EFAULT, EFAULT, "Bad address" },
	{ BSM_ENOTBLK, ENOTBLK, "Block device required" },
	{ BSM_EBUSY, EBUSY, "Device busy" },
	{ BSM_EEXIST, EEXIST, "File exists" },
	{ BSM_EXDEV, EXDEV, "Cross-device link" },
	{ BSM_ENODEV, ENODEV, "Operation not supported by device" },
	{ BSM_ENOTDIR, ENOTDIR, "Not a directory" },
	{ BSM_EISDIR, EISDIR, "Is a directory" },
	{ BSM_EINVAL, EINVAL, "Invalid argument" },
	{ BSM_ENFILE, ENFILE, "Too many open files in system" },
	{ BSM_EMFILE, EMFILE, "Too many open files" },
	{ BSM_ENOTTY, ENOTTY, "Inappropriate ioctl for device" },
	{ BSM_ETXTBSY, ETXTBSY, "Text file busy" },
	{ BSM_EFBIG, EFBIG, "File too large" },
	{ BSM_ENOSPC, ENOSPC, "No space left on device" },
	{ BSM_ESPIPE, ESPIPE, "Illegal seek" },
	{ BSM_EROFS, EROFS, "Read-only file system" },
	{ BSM_EMLINK, EMLINK, "Too many links" },
	{ BSM_EPIPE, EPIPE, "Broken pipe" },
	{ BSM_EDOM, EDOM, "Numerical argument out of domain" },
	{ BSM_ERANGE, ERANGE, "Result too large" },
	{ BSM_ENOMSG, ENOMSG, "No message of desired type" },
	{ BSM_EIDRM, EIDRM, "Identifier removed" },
	{ BSM_ECHRNG,
#ifdef ECHRNG
	ECHRNG,
#else
	ERRNO_NO_LOCAL_MAPPING,
#endif
	"Channel number out of range" },
	{ BSM_EL2NSYNC,
#ifdef EL2NSYNC
	EL2NSYNC,
#else
	ERRNO_NO_LOCAL_MAPPING,
#endif
	"Level 2 not synchronized" },
	{ BSM_EL3HLT,
#ifdef EL3HLT
	EL3HLT,
#else
	ERRNO_NO_LOCAL_MAPPING,
#endif
	"Level 3 halted" },
	{ BSM_EL3RST,
#ifdef EL3RST
	EL3RST,
#else
	ERRNO_NO_LOCAL_MAPPING,
#endif
	"Level 3 reset" },
	{ BSM_ELNRNG,
#ifdef ELNRNG
	ELNRNG,
#else
	ERRNO_NO_LOCAL_MAPPING,
#endif
	"Link number out of range" },
	{ BSM_EUNATCH,
#ifdef EUNATCH
	EUNATCH,
#else
	ERRNO_NO_LOCAL_MAPPING,
#endif
	"Protocol driver not attached" },
	{ BSM_ENOCSI,
#ifdef ENOCSI
	ENOCSI,
#else
	ERRNO_NO_LOCAL_MAPPING,
#endif
	"No CSI structure available" },
	{ BSM_EL2HLT,
#ifdef EL2HLT
	EL2HLT,
#else
	ERRNO_NO_LOCAL_MAPPING,
#endif
	"Level 2 halted" },
	{ BSM_EDEADLK, EDEADLK, "Resource deadlock avoided" },
	{ BSM_ENOLCK, ENOLCK, "No locks available" },
	{ BSM_ECANCELED, ECANCELED, "Operation canceled" },
	{ BSM_ENOTSUP, ENOTSUP, "Operation not supported" },
	{ BSM_EDQUOT, EDQUOT, "Disc quota exceeded" },
	{ BSM_EBADE,
#ifdef EBADE
	EBADE,
#else
	ERRNO_NO_LOCAL_MAPPING,
#endif
	"Invalid exchange" },
	{ BSM_EBADR,
#ifdef EBADR
	EBADR,
#else
	ERRNO_NO_LOCAL_MAPPING,
#endif
	"Invalid request descriptor" },
	{ BSM_EXFULL,
#ifdef EXFULL
	EXFULL,
#else
	ERRNO_NO_LOCAL_MAPPING,
#endif
	"Exchange full" },
	{ BSM_ENOANO,
#ifdef ENOANO
	ENOANO,
#else
	ERRNO_NO_LOCAL_MAPPING,
#endif
	"No anode" },
	{ BSM_EBADRQC,
#ifdef EBADRQC
	EBADRQC,
#else
	ERRNO_NO_LOCAL_MAPPING,
#endif
	"Invalid request descriptor" },
	{ BSM_EBADSLT,
#ifdef EBADSLT
	EBADSLT,
#else
	ERRNO_NO_LOCAL_MAPPING,
#endif
	"Invalid slot" },
	{ BSM_EDEADLOCK,
#ifdef EDEADLOCK
	EDEADLOCK,
#else
	ERRNO_NO_LOCAL_MAPPING,
#endif
	"Resource deadlock avoided" },
	{ BSM_EBFONT,
#ifdef EBFONT
	EBFONT,
#else
	ERRNO_NO_LOCAL_MAPPING,
#endif
	"Bad font file format" },
	{ BSM_EOWNERDEAD,
#ifdef EOWNERDEAD
	EOWNERDEAD,
#else
	ERRNO_NO_LOCAL_MAPPING,
#endif
	"Process died with the lock" },
	{ BSM_ENOTRECOVERABLE,
#ifdef ENOTRECOVERABLE
	ENOTRECOVERABLE,
#else
	ERRNO_NO_LOCAL_MAPPING,
#endif
	"Lock is not recoverable" },
	{ BSM_ENOSTR,
#ifdef ENOSTR
	ENOSTR,
#else
	ERRNO_NO_LOCAL_MAPPING,
#endif
	"Device not a stream" },
	{ BSM_ENONET,
#ifdef ENONET
	ENONET,
#else
	ERRNO_NO_LOCAL_MAPPING,
#endif
	"Machine is not on the network" },
	{ BSM_ENOPKG,
#ifdef ENOPKG
	ENOPKG,
#else
	ERRNO_NO_LOCAL_MAPPING,
#endif
	"Package not installed" },
	{ BSM_EREMOTE, EREMOTE, "Too many levels of remote in path" },
	{ BSM_ENOLINK,
#ifdef ENOLINK
	ENOLINK,
#else
	ERRNO_NO_LOCAL_MAPPING,
#endif
	"Link has been severed" },
	{ BSM_EADV,
#ifdef EADV
	EADV,
#else
	ERRNO_NO_LOCAL_MAPPING,
#endif
	"Advertise error" },
	{ BSM_ESRMNT,
#ifdef ESRMNT
	ESRMNT,
#else
	ERRNO_NO_LOCAL_MAPPING,
#endif
	"srmount error" },
	{ BSM_ECOMM,
#ifdef ECOMM
	ECOMM,
#else
	ERRNO_NO_LOCAL_MAPPING,
#endif
	"Communication error on send" },
	{ BSM_EPROTO,
#ifdef EPROTO
	EPROTO,
#else
	ERRNO_NO_LOCAL_MAPPING,
#endif
	"Protocol error" },
	{ BSM_ELOCKUNMAPPED,
#ifdef ELOCKUNMAPPED
	ELOCKUNMAPPED,
#else
	ERRNO_NO_LOCAL_MAPPING,
#endif
	"Locked lock was unmapped" },
	{ BSM_ENOTACTIVE,
#ifdef ENOTACTIVE
	ENOTACTIVE,
#else
	ERRNO_NO_LOCAL_MAPPING,
#endif
	"Facility is not active" },
	{ BSM_EMULTIHOP,
#ifdef EMULTIHOP
	EMULTIHOP,
#else
	ERRNO_NO_LOCAL_MAPPING,
#endif
	"Multihop attempted" },
	{ BSM_EBADMSG,
#ifdef EBADMSG
	EBADMSG,
#else
	ERRNO_NO_LOCAL_MAPPING,
#endif
	"Bad message" },
	{ BSM_ENAMETOOLONG, ENAMETOOLONG, "File name too long" },
	{ BSM_EOVERFLOW, EOVERFLOW, "Value too large to be stored in data type" },
	{ BSM_ENOTUNIQ,
#ifdef ENOTUNIQ
	ENOTUNIQ,
#else
	ERRNO_NO_LOCAL_MAPPING,
#endif
	"Given log name not unique" },
	{ BSM_EBADFD,
#ifdef EBADFD
	EBADFD,
#else
	ERRNO_NO_LOCAL_MAPPING,
#endif
	"Given f.d. invalid for this operation" },
	{ BSM_EREMCHG,
#ifdef EREMCHG
	EREMCHG,
#else
	ERRNO_NO_LOCAL_MAPPING,
#endif
	"Remote address changed" },
	{ BSM_ELIBACC,
#ifdef ELIBACC
	ELIBACC,
#else
	ERRNO_NO_LOCAL_MAPPING,
#endif
	"Can't access a needed shared lib" },
	{ BSM_ELIBBAD,
#ifdef ELIBBAD
	ELIBBAD,
#else
	ERRNO_NO_LOCAL_MAPPING,
#endif
	"Accessing a corrupted shared lib" },
	{ BSM_ELIBSCN,
#ifdef ELIBSCN
	ELIBSCN,
#else
	ERRNO_NO_LOCAL_MAPPING,
#endif
	".lib section in a.out corrupted" },
	{ BSM_ELIBMAX,
#ifdef ELIBMAX
	ELIBMAX,
#else
	ERRNO_NO_LOCAL_MAPPING,
#endif
	"Attempting to link in too many libs" },
	{ BSM_ELIBEXEC,
#ifdef ELIBEXEC
	ELIBEXEC,
#else
	ERRNO_NO_LOCAL_MAPPING,
#endif
	"Attempting to exec a shared library" },
	{ BSM_EILSEQ, EILSEQ, "Illegal byte sequence" },
	{ BSM_ENOSYS, ENOSYS, "Function not implemented" },
	{ BSM_ELOOP, ELOOP, "Too many levels of symbolic links" },
	{ BSM_ERESTART,
#ifdef ERESTART
	ERESTART,
#else
	ERRNO_NO_LOCAL_MAPPING,
#endif
	"Restart syscall" },
	{ BSM_ESTRPIPE,
#ifdef ESTRPIPE
	ESTRPIPE,
#else
	ERRNO_NO_LOCAL_MAPPING,
#endif
	"If pipe/FIFO, don't sleep in stream head" },
	{ BSM_ENOTEMPTY, ENOTEMPTY, "Directory not empty" },
	{ BSM_EUSERS, EUSERS, "Too many users" },
	{ BSM_ENOTSOCK, ENOTSOCK, "Socket operation on non-socket" },
	{ BSM_EDESTADDRREQ, EDESTADDRREQ, "Destination address required" },
	{ BSM_EMSGSIZE, EMSGSIZE, "Message too long" },
	{ BSM_EPROTOTYPE, EPROTOTYPE, "Protocol wrong type for socket" },
	{ BSM_ENOPROTOOPT, ENOPROTOOPT, "Protocol not available" },
	{ BSM_EPROTONOSUPPORT, EPROTONOSUPPORT, "Protocol not supported" },
	{ BSM_ESOCKTNOSUPPORT, ESOCKTNOSUPPORT, "Socket type not supported" },
	{ BSM_EOPNOTSUPP, EOPNOTSUPP, "Operation not supported" },
	{ BSM_EPFNOSUPPORT, EPFNOSUPPORT, "Protocol family not supported" },
	{ BSM_EAFNOSUPPORT, EAFNOSUPPORT, "Address family not supported by protocol family" },
	{ BSM_EADDRINUSE, EADDRINUSE, "Address already in use" },
	{ BSM_EADDRNOTAVAIL, EADDRNOTAVAIL, "Can't assign requested address" },
	{ BSM_ENETDOWN, ENETDOWN, "Network is down" },
	{ BSM_ENETRESET, ENETRESET, "Network dropped connection on reset" },
	{ BSM_ECONNABORTED, ECONNABORTED, "Software caused connection abort" },
	{ BSM_ECONNRESET, ECONNRESET, "Connection reset by peer" },
	{ BSM_ENOBUFS, ENOBUFS, "No buffer space available" },
	{ BSM_EISCONN, EISCONN, "Socket is already connected" },
	{ BSM_ENOTCONN, ENOTCONN, "Socket is not connected" },
	{ BSM_ESHUTDOWN, ESHUTDOWN, "Can't send after socket shutdown" },
	{ BSM_ETOOMANYREFS, ETOOMANYREFS, "Too many references: can't splice" },
	{ BSM_ETIMEDOUT, ETIMEDOUT, "Operation timed out" },
	{ BSM_ECONNREFUSED, ECONNREFUSED, "Connection refused" },
	{ BSM_EHOSTDOWN, EHOSTDOWN, "Host is down" },
	{ BSM_EHOSTUNREACH, EHOSTUNREACH, "No route to host" },
	{ BSM_EALREADY, EALREADY, "Operation already in progress" },
	{ BSM_EINPROGRESS, EINPROGRESS, "Operation now in progress" },
	{ BSM_ESTALE, ESTALE, "Stale NFS file handle" },
	{ BSM_EPWROFF,
#ifdef EPWROFF
	EPWROFF,
#else
	ERRNO_NO_LOCAL_MAPPING,
#endif
	"Device power is off" },
	{ BSM_EDEVERR,
#ifdef EDEVERR
	EDEVERR,
#else
	ERRNO_NO_LOCAL_MAPPING,
#endif
	"Device error" },
	{ BSM_EBADEXEC,
#ifdef EBADEXEC
	EBADEXEC,
#else
	ERRNO_NO_LOCAL_MAPPING,
#endif
	"Bad executable" },
	{ BSM_EBADARCH,
#ifdef EBADARCH
	EBADARCH,
#else
	ERRNO_NO_LOCAL_MAPPING,
#endif
	"Bad CPU type in executable" },
	{ BSM_ESHLIBVERS,
#ifdef ESHLIBVERS
	ESHLIBVERS,
#else
	ERRNO_NO_LOCAL_MAPPING,
#endif
	"Shared library version mismatch" },
	{ BSM_EBADMACHO,
#ifdef EBADMACHO
	EBADMACHO,
#else
	ERRNO_NO_LOCAL_MAPPING,
#endif
	"Malfored Macho file" },
	{ BSM_EPOLICY,
#ifdef EPOLICY
	EPOLICY,
#else
	ERRNO_NO_LOCAL_MAPPING,
#endif
	"Operation failed by policy" },
	{ BSM_EDOTDOT,
#ifdef EDOTDOT
	EDOTDOT,
#else
	ERRNO_NO_LOCAL_MAPPING,
#endif
	"RFS specific error" },
	{ BSM_EUCLEAN,
#ifdef EUCLEAN
	EUCLEAN,
#else
	ERRNO_NO_LOCAL_MAPPING,
#endif
	"Structure needs cleaning" },
	{ BSM_ENOTNAM,
#ifdef ENOTNAM
	ENOTNAM,
#else
	ERRNO_NO_LOCAL_MAPPING,
#endif
	"Not a XENIX named type file" },
	{ BSM_ENAVAIL,
#ifdef ENAVAIL
	ENAVAIL,
#else
	ERRNO_NO_LOCAL_MAPPING,
#endif
	"No XENIX semaphores available" },
	{ BSM_EISNAM,
#ifdef EISNAM
	EISNAM,
#else
	ERRNO_NO_LOCAL_MAPPING,
#endif
	"Is a named type file" },
	{ BSM_EREMOTEIO,
#ifdef EREMOTEIO
	EREMOTEIO,
#else
	ERRNO_NO_LOCAL_MAPPING,
#endif
	"Remote I/O error" },
	{ BSM_ENOMEDIUM,
#ifdef ENOMEDIUM
	ENOMEDIUM,
#else
	ERRNO_NO_LOCAL_MAPPING,
#endif
	"No medium found" },
	{ BSM_EMEDIUMTYPE,
#ifdef EMEDIUMTYPE
	EMEDIUMTYPE,
#else
	ERRNO_NO_LOCAL_MAPPING,
#endif
	"Wrong medium type" },
	{ BSM_ENOKEY,
#ifdef ENOKEY
	ENOKEY,
#else
	ERRNO_NO_LOCAL_MAPPING,
#endif
	"Required key not available" },
	{ BSM_EKEYEXPIRED,
#ifdef EKEEXPIRED
	EKEYEXPIRED,
#else
	ERRNO_NO_LOCAL_MAPPING,
#endif
	"Key has expired" },
	{ BSM_EKEYREVOKED,
#ifdef EKEYREVOKED
	EKEYREVOKED,
#else
	ERRNO_NO_LOCAL_MAPPING,
#endif
	"Key has been revoked" },
	{ BSM_EKEYREJECTED,
#ifdef EKEREJECTED
	EKEYREJECTED,
#else
	ERRNO_NO_LOCAL_MAPPING,
#endif
	"Key was rejected by service" },
};
static const int bsm_errors_count = sizeof(bsm_errors) / sizeof(bsm_errors[0]);

static const struct bsm_errors *
au_bsm_error_lookup_errno(int error)
{
	int i;

	if (error == ERRNO_NO_LOCAL_MAPPING)
		return (NULL);
	for (i = 0; i < bsm_errors_count; i++) {
		if (bsm_errors[i].be_os_error == error)
			return (&bsm_errors[i]);
	}
	return (NULL);
}

static const struct bsm_errors *
au_bsm_error_lookup_bsm(u_char bsm_error)
{
	int i;

	for (i = 0; i < bsm_errors_count; i++) {
		if (bsm_errors[i].be_bsm_error == bsm_error)
			return (&bsm_errors[i]);
	}
	return (NULL);
}

/*
 * Converstion from a BSM error to a local error number may fail if either
 * OpenBSM doesn't recognize the error on the wire, or because there is no
 * appropriate local mapping.  However, we don't allow conversion to BSM to
 * fail, we just convert to BSM_UKNOWNERR.
 */
int
au_bsm_to_errno(u_char bsm_error, int *errorp)
{
	const struct bsm_errors *bsme;

	bsme = au_bsm_error_lookup_bsm(bsm_error);
	if (bsme == NULL || bsme->be_os_error == ERRNO_NO_LOCAL_MAPPING)
		return (-1);
	*errorp = bsme->be_os_error;
	return (0);
}

u_char
au_errno_to_bsm(int error)
{
	const struct bsm_errors *bsme;

	/*
	 * We should never be passed this libbsm-internal constant, and
	 * because it is ambiguous we just return an error.
	 */
	if (error == ERRNO_NO_LOCAL_MAPPING)
		return (BSM_UNKNOWNERR);
	bsme = au_bsm_error_lookup_errno(error);
	if (bsme == NULL)
		return (BSM_UNKNOWNERR);
	return (bsme->be_bsm_error);
}

#if !defined(KERNEL) && !defined(_KERNEL)
const char *
au_strerror(u_char bsm_error)
{
	const struct bsm_errors *bsme;

	bsme = au_bsm_error_lookup_bsm(bsm_error);
	if (bsme == NULL)
		return ("Unrecognized BSM error");
	if (bsme->be_os_error != ERRNO_NO_LOCAL_MAPPING)
		return (strerror(bsme->be_os_error));
	return (bsme->be_strerror);
}
#endif
