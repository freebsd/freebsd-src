/*-
 * Copyright (c) 2014 Roger Pau Monné <royger@FreeBSD.org>.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS AS IS'' AND
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

#ifndef __XEN_ERROR_H__
#define __XEN_ERROR_H__

/* List of Xen error codes */
#define	XEN_EPERM		 1	/* Operation not permitted */
#define	XEN_ENOENT		 2	/* No such file or directory */
#define	XEN_ESRCH		 3	/* No such process */
#define	XEN_EINTR		 4	/* Interrupted system call */
#define	XEN_EIO			 5	/* I/O error */
#define	XEN_ENXIO		 6	/* No such device or address */
#define	XEN_E2BIG		 7	/* Arg list too long */
#define	XEN_ENOEXEC		 8	/* Exec format error */
#define	XEN_EBADF		 9	/* Bad file number */
#define	XEN_ECHILD		10	/* No child processes */
#define	XEN_EAGAIN		11	/* Try again */
#define	XEN_ENOMEM		12	/* Out of memory */
#define	XEN_EACCES		13	/* Permission denied */
#define	XEN_EFAULT		14	/* Bad address */
#define	XEN_ENOTBLK		15	/* Block device required */
#define	XEN_EBUSY		16	/* Device or resource busy */
#define	XEN_EEXIST		17	/* File exists */
#define	XEN_EXDEV		18	/* Cross-device link */
#define	XEN_ENODEV		19	/* No such device */
#define	XEN_ENOTDIR		20	/* Not a directory */
#define	XEN_EISDIR		21	/* Is a directory */
#define	XEN_EINVAL		22	/* Invalid argument */
#define	XEN_ENFILE		23	/* File table overflow */
#define	XEN_EMFILE		24	/* Too many open files */
#define	XEN_ENOTTY		25	/* Not a typewriter */
#define	XEN_ETXTBSY		26	/* Text file busy */
#define	XEN_EFBIG		27	/* File too large */
#define	XEN_ENOSPC		28	/* No space left on device */
#define	XEN_ESPIPE		29	/* Illegal seek */
#define	XEN_EROFS		30	/* Read-only file system */
#define	XEN_EMLINK		31	/* Too many links */
#define	XEN_EPIPE		32	/* Broken pipe */
#define	XEN_EDOM		33	/* Math argument out of domain of func */
#define	XEN_ERANGE		34	/* Math result not representable */
#define	XEN_EDEADLK		35	/* Resource deadlock would occur */
#define	XEN_ENAMETOOLONG	36	/* File name too long */
#define	XEN_ENOLCK		37	/* No record locks available */
#define	XEN_ENOSYS		38	/* Function not implemented */
#define	XEN_ENOTEMPTY	 	39	/* Directory not empty */
#define	XEN_ELOOP		40	/* Too many symbolic links encountered */
#define	XEN_ENOMSG		42	/* No message of desired type */
#define	XEN_EIDRM		43	/* Identifier removed */
#define	XEN_ECHRNG		44	/* Channel number out of range */
#define	XEN_EL2NSYNC		45	/* Level 2 not synchronized */
#define	XEN_EL3HLT		46	/* Level 3 halted */
#define	XEN_EL3RST		47	/* Level 3 reset */
#define	XEN_ELNRNG		48	/* Link number out of range */
#define	XEN_EUNATCH		49	/* Protocol driver not attached */
#define	XEN_ENOCSI		50	/* No CSI structure available */
#define	XEN_EL2HLT		51	/* Level 2 halted */
#define	XEN_EBADE		52	/* Invalid exchange */
#define	XEN_EBADR		53	/* Invalid request descriptor */
#define	XEN_EXFULL		54	/* Exchange full */
#define	XEN_ENOANO		55	/* No anode */
#define	XEN_EBADRQC		56	/* Invalid request code */
#define	XEN_EBADSLT		57	/* Invalid slot */
#define	XEN_EBFONT		59	/* Bad font file format */
#define	XEN_ENOSTR		60	/* Device not a stream */
#define	XEN_ENODATA		61	/* No data available */
#define	XEN_ETIME		62	/* Timer expired */
#define	XEN_ENOSR		63	/* Out of streams resources */
#define	XEN_ENONET		64	/* Machine is not on the network */
#define	XEN_ENOPKG		65	/* Package not installed */
#define	XEN_EREMOTE		66	/* Object is remote */
#define	XEN_ENOLINK		67	/* Link has been severed */
#define	XEN_EADV		68	/* Advertise error */
#define	XEN_ESRMNT		69	/* Srmount error */
#define	XEN_ECOMM		70	/* Communication error on send */
#define	XEN_EPROTO		71	/* Protocol error */
#define	XEN_EMULTIHOP		72	/* Multihop attempted */
#define	XEN_EDOTDOT		73	/* RFS specific error */
#define	XEN_EBADMSG		74	/* Not a data message */
#define	XEN_EOVERFLOW		75	/* Value too large for defined data type */
#define	XEN_ENOTUNIQ		76	/* Name not unique on network */
#define	XEN_EBADFD		77	/* File descriptor in bad state */
#define	XEN_EREMCHG		78	/* Remote address changed */
#define	XEN_ELIBACC		79	/* Can not access a needed shared library */
#define	XEN_ELIBBAD		80	/* Accessing a corrupted shared library */
#define	XEN_ELIBSCN		81	/* .lib section in a.out corrupted */
#define	XEN_ELIBMAX		82	/* Attempting to link in too many shared libraries */
#define	XEN_ELIBEXEC		83	/* Cannot exec a shared library directly */
#define	XEN_EILSEQ		84	/* Illegal byte sequence */
#define	XEN_ERESTART		85	/* Interrupted system call should be restarted */
#define	XEN_ESTRPIPE		86	/* Streams pipe error */
#define	XEN_EUSERS		87	/* Too many users */
#define	XEN_ENOTSOCK		88	/* Socket operation on non-socket */
#define	XEN_EDESTADDRREQ	89	/* Destination address required */
#define	XEN_EMSGSIZE		90	/* Message too long */
#define	XEN_EPROTOTYPE		91	/* Protocol wrong type for socket */
#define	XEN_ENOPROTOOPT		92	/* Protocol not available */
#define	XEN_EPROTONOSUPPORT	93	/* Protocol not supported */
#define	XEN_ESOCKTNOSUPPORT	94	/* Socket type not supported */
#define	XEN_EOPNOTSUPP		95	/* Operation not supported on transport endpoint */
#define	XEN_EPFNOSUPPORT	96	/* Protocol family not supported */
#define	XEN_EAFNOSUPPORT	97	/* Address family not supported by protocol */
#define	XEN_EADDRINUSE		98	/* Address already in use */
#define	XEN_EADDRNOTAVAIL	99	/* Cannot assign requested address */
#define	XEN_ENETDOWN		100	/* Network is down */
#define	XEN_ENETUNREACH		101	/* Network is unreachable */
#define	XEN_ENETRESET		102	/* Network dropped connection because of reset */
#define	XEN_ECONNABORTED	103	/* Software caused connection abort */
#define	XEN_ECONNRESET		104	/* Connection reset by peer */
#define	XEN_ENOBUFS		105	/* No buffer space available */
#define	XEN_EISCONN		106	/* Transport endpoint is already connected */
#define	XEN_ENOTCONN		107	/* Transport endpoint is not connected */
#define	XEN_ESHUTDOWN		108	/* Cannot send after transport endpoint shutdown */
#define	XEN_ETOOMANYREFS	109	/* Too many references: cannot splice */
#define	XEN_ETIMEDOUT		110	/* Connection timed out */
#define	XEN_ECONNREFUSED	111	/* Connection refused */
#define	XEN_EHOSTDOWN		112	/* Host is down */
#define	XEN_EHOSTUNREACH	113	/* No route to host */
#define	XEN_EALREADY		114	/* Operation already in progress */
#define	XEN_EINPROGRESS		115	/* Operation now in progress */
#define	XEN_ESTALE		116	/* Stale NFS file handle */
#define	XEN_EUCLEAN		117	/* Structure needs cleaning */
#define	XEN_ENOTNAM		118	/* Not a XENIX named type file */
#define	XEN_ENAVAIL		119	/* No XENIX semaphores available */
#define	XEN_EISNAM		120	/* Is a named type file */
#define	XEN_EREMOTEIO		121	/* Remote I/O error */
#define	XEN_EDQUOT		122	/* Quota exceeded */

#define	XEN_ENOMEDIUM		123	/* No medium found */
#define	XEN_EMEDIUMTYPE		124	/* Wrong medium type */

/* Translation table */
static int xen_errors[] =
{
	[XEN_EPERM]		= EPERM,
	[XEN_ENOENT]		= ENOENT,
	[XEN_ESRCH]		= ESRCH,
	[XEN_EINTR]		= EINTR,
	[XEN_EIO]		= EIO,
	[XEN_ENXIO]		= ENXIO,
	[XEN_E2BIG]		= E2BIG,
	[XEN_ENOEXEC]		= ENOEXEC,
	[XEN_EBADF]		= EBADF,
	[XEN_ECHILD]		= ECHILD,
	[XEN_EAGAIN]		= EAGAIN,
	[XEN_ENOMEM]		= ENOMEM,
	[XEN_EACCES]		= EACCES,
	[XEN_EFAULT]		= EFAULT,
	[XEN_ENOTBLK]		= ENOTBLK,
	[XEN_EBUSY]		= EBUSY,
	[XEN_EEXIST]		= EEXIST,
	[XEN_EXDEV]		= EXDEV,
	[XEN_ENODEV]		= ENODEV,
	[XEN_ENOTDIR]		= ENOTDIR,
	[XEN_EISDIR]		= EISDIR,
	[XEN_EINVAL]		= EINVAL,
	[XEN_ENFILE]		= ENFILE,
	[XEN_EMFILE]		= EMFILE,
	[XEN_ENOTTY]		= ENOTTY,
	[XEN_ETXTBSY]		= ETXTBSY,
	[XEN_EFBIG]		= EFBIG,
	[XEN_ENOSPC]		= ENOSPC,
	[XEN_ESPIPE]		= ESPIPE,
	[XEN_EROFS]		= EROFS,
	[XEN_EMLINK]		= EMLINK,
	[XEN_EPIPE]		= EPIPE,
	[XEN_EDOM]		= EDOM,
	[XEN_ERANGE]		= ERANGE,
	[XEN_EDEADLK]		= EDEADLK,
	[XEN_ENAMETOOLONG]	= ENAMETOOLONG,
	[XEN_ENOLCK]		= ENOLCK,
	[XEN_ENOSYS]		= ENOSYS,
	[XEN_ENOTEMPTY]		= ENOTEMPTY,
	[XEN_ELOOP]		= ELOOP,
	[XEN_ENOMSG]		= ENOMSG,
	[XEN_EIDRM]		= EIDRM,
	[XEN_ECHRNG]		= ERANGE,
	[XEN_EL2NSYNC]		= EFAULT,
	[XEN_EL3HLT]		= EFAULT,
	[XEN_EL3RST]		= EFAULT,
	[XEN_ELNRNG]		= ERANGE,
	[XEN_EUNATCH]		= ENODEV,
	[XEN_ENOCSI]		= ENODEV,
	[XEN_EL2HLT]		= EFAULT,
	[XEN_EBADE]		= ERANGE,
	[XEN_EBADR]		= EINVAL,
	[XEN_EXFULL]		= ENOBUFS,
	[XEN_ENOANO]		= EINVAL,
	[XEN_EBADRQC]		= EINVAL,
	[XEN_EBADSLT]		= EINVAL,
	[XEN_EBFONT]		= EFAULT,
	[XEN_ENOSTR]		= EINVAL,
	[XEN_ENODATA]		= ENOENT,
	[XEN_ETIME]		= ETIMEDOUT,
	[XEN_ENOSR]		= EFAULT,
	[XEN_ENONET]		= ENETDOWN,
	[XEN_ENOPKG]		= EINVAL,
	[XEN_EREMOTE]		= EREMOTE,
	[XEN_ENOLINK]		= ENOLINK,
	[XEN_EADV]		= EFAULT,
	[XEN_ESRMNT]		= EFAULT,
	[XEN_ECOMM]		= EFAULT,
	[XEN_EPROTO]		= EPROTO,
	[XEN_EMULTIHOP]		= EMULTIHOP,
	[XEN_EDOTDOT]		= EFAULT,
	[XEN_EBADMSG]		= EBADMSG,
	[XEN_EOVERFLOW]		= EOVERFLOW,
	[XEN_ENOTUNIQ]		= EADDRINUSE,
	[XEN_EBADFD]		= EBADF,
	[XEN_EREMCHG]		= EHOSTDOWN,
	[XEN_ELIBACC]		= EFAULT,
	[XEN_ELIBBAD]		= EFAULT,
	[XEN_ELIBSCN]		= EFAULT,
	[XEN_ELIBMAX]		= EFAULT,
	[XEN_ELIBEXEC]		= EFAULT,
	[XEN_EILSEQ]		= EILSEQ,
	[XEN_ERESTART]		= EAGAIN,
	[XEN_ESTRPIPE]		= EPIPE,
	[XEN_EUSERS]		= EUSERS,
	[XEN_ENOTSOCK]		= ENOTSOCK,
	[XEN_EDESTADDRREQ]	= EDESTADDRREQ,
	[XEN_EMSGSIZE]		= EMSGSIZE,
	[XEN_EPROTOTYPE]	= EPROTOTYPE,
	[XEN_ENOPROTOOPT]	= ENOPROTOOPT,
	[XEN_EPROTONOSUPPORT]	= EPROTONOSUPPORT,
	[XEN_ESOCKTNOSUPPORT]	= ESOCKTNOSUPPORT,
	[XEN_EOPNOTSUPP]	= EOPNOTSUPP,
	[XEN_EPFNOSUPPORT]	= EPFNOSUPPORT,
	[XEN_EAFNOSUPPORT]	= EAFNOSUPPORT,
	[XEN_EADDRINUSE]	= EADDRINUSE,
	[XEN_EADDRNOTAVAIL]	= EADDRNOTAVAIL,
	[XEN_ENETDOWN]		= ENETDOWN,
	[XEN_ENETUNREACH]	= ENETUNREACH,
	[XEN_ENETRESET]		= ENETRESET,
	[XEN_ECONNABORTED]	= ECONNABORTED,
	[XEN_ECONNRESET]	= ECONNRESET,
	[XEN_ENOBUFS]		= ENOBUFS,
	[XEN_EISCONN]		= EISCONN,
	[XEN_ENOTCONN]		= ENOTCONN,
	[XEN_ESHUTDOWN]		= ESHUTDOWN,
	[XEN_ETOOMANYREFS]	= ETOOMANYREFS,
	[XEN_ETIMEDOUT]		= ETIMEDOUT,
	[XEN_ECONNREFUSED]	= ECONNREFUSED,
	[XEN_EHOSTDOWN]		= EHOSTDOWN,
	[XEN_EHOSTUNREACH]	= EHOSTUNREACH,
	[XEN_EALREADY]		= EALREADY,
	[XEN_EINPROGRESS]	= EINPROGRESS,
	[XEN_ESTALE]		= ESTALE,
	[XEN_EUCLEAN]		= EFAULT,
	[XEN_ENOTNAM]		= EFAULT,
	[XEN_ENAVAIL]		= EFAULT,
	[XEN_EISNAM]		= EFAULT,
	[XEN_EREMOTEIO]		= EIO,
	[XEN_EDQUOT]		= EDQUOT,
	[XEN_ENOMEDIUM]		= ENOENT,
	[XEN_EMEDIUMTYPE]	= ENOENT,
};

static inline int
xen_translate_error(int error)
{
	int bsd_error;

	KASSERT((error < 0), ("Value is not a valid Xen error code"));

	if (-error >= nitems(xen_errors)) {
		/*
		 * We received an error value that cannot be translated,
		 * return EINVAL.
		 */
		return (EINVAL);
	}

	bsd_error = xen_errors[-error];
	KASSERT((bsd_error != 0), ("Unknown Xen error code"));

	return (bsd_error);
}

#endif /* !__XEN_ERROR_H__ */
