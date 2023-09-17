/*
 * There are two expected ways of including this header.
 *
 * 1) The "default" case (expected from tools etc).
 *
 * Simply #include <public/errno.h>
 *
 * In this circumstance, normal header guards apply and the includer shall get
 * an enumeration in the XEN_xxx namespace, appropriate for C or assembly.
 *
 * 2) The special case where the includer provides a XEN_ERRNO() in scope.
 *
 * In this case, no inclusion guards apply and the caller is responsible for
 * their XEN_ERRNO() being appropriate in the included context.  The header
 * will unilaterally #undef XEN_ERRNO().
 */

#ifndef XEN_ERRNO

/*
 * Includer has not provided a custom XEN_ERRNO().  Arrange for normal header
 * guards, an automatic enum (for C code) and constants in the XEN_xxx
 * namespace.
 */
#ifndef __XEN_PUBLIC_ERRNO_H__
#define __XEN_PUBLIC_ERRNO_H__

#define XEN_ERRNO_DEFAULT_INCLUDE

#ifndef __ASSEMBLY__

#define XEN_ERRNO(name, value) XEN_##name = value,
enum xen_errno {

#elif __XEN_INTERFACE_VERSION__ < 0x00040700

#define XEN_ERRNO(name, value) .equ XEN_##name, value

#endif /* __ASSEMBLY__ */

#endif /* __XEN_PUBLIC_ERRNO_H__ */
#endif /* !XEN_ERRNO */

/* ` enum neg_errnoval {  [ -Efoo for each Efoo in the list below ]  } */
/* ` enum errnoval { */

#ifdef XEN_ERRNO

/*
 * Values originating from x86 Linux. Please consider using respective
 * values when adding new definitions here.
 *
 * The set of identifiers to be added here shouldn't extend beyond what
 * POSIX mandates (see e.g.
 * http://pubs.opengroup.org/onlinepubs/9699919799/basedefs/errno.h.html)
 * with the exception that we support some optional (XSR) values
 * specified there (but no new ones should be added).
 */

XEN_ERRNO(EPERM,	 1)	/* Operation not permitted */
XEN_ERRNO(ENOENT,	 2)	/* No such file or directory */
XEN_ERRNO(ESRCH,	 3)	/* No such process */
#ifdef __XEN__ /* Internal only, should never be exposed to the guest. */
XEN_ERRNO(EINTR,	 4)	/* Interrupted system call */
#endif
XEN_ERRNO(EIO,		 5)	/* I/O error */
XEN_ERRNO(ENXIO,	 6)	/* No such device or address */
XEN_ERRNO(E2BIG,	 7)	/* Arg list too long */
XEN_ERRNO(ENOEXEC,	 8)	/* Exec format error */
XEN_ERRNO(EBADF,	 9)	/* Bad file number */
XEN_ERRNO(ECHILD,	10)	/* No child processes */
XEN_ERRNO(EAGAIN,	11)	/* Try again */
XEN_ERRNO(EWOULDBLOCK,	11)	/* Operation would block.  Aliases EAGAIN */
XEN_ERRNO(ENOMEM,	12)	/* Out of memory */
XEN_ERRNO(EACCES,	13)	/* Permission denied */
XEN_ERRNO(EFAULT,	14)	/* Bad address */
XEN_ERRNO(EBUSY,	16)	/* Device or resource busy */
XEN_ERRNO(EEXIST,	17)	/* File exists */
XEN_ERRNO(EXDEV,	18)	/* Cross-device link */
XEN_ERRNO(ENODEV,	19)	/* No such device */
XEN_ERRNO(ENOTDIR,	20)	/* Not a directory */
XEN_ERRNO(EISDIR,	21)	/* Is a directory */
XEN_ERRNO(EINVAL,	22)	/* Invalid argument */
XEN_ERRNO(ENFILE,	23)	/* File table overflow */
XEN_ERRNO(EMFILE,	24)	/* Too many open files */
XEN_ERRNO(ENOSPC,	28)	/* No space left on device */
XEN_ERRNO(EROFS,	30)	/* Read-only file system */
XEN_ERRNO(EMLINK,	31)	/* Too many links */
XEN_ERRNO(EDOM,		33)	/* Math argument out of domain of func */
XEN_ERRNO(ERANGE,	34)	/* Math result not representable */
XEN_ERRNO(EDEADLK,	35)	/* Resource deadlock would occur */
XEN_ERRNO(EDEADLOCK,	35)	/* Resource deadlock would occur. Aliases EDEADLK */
XEN_ERRNO(ENAMETOOLONG,	36)	/* File name too long */
XEN_ERRNO(ENOLCK,	37)	/* No record locks available */
XEN_ERRNO(ENOSYS,	38)	/* Function not implemented */
XEN_ERRNO(ENOTEMPTY,	39)	/* Directory not empty */
XEN_ERRNO(ENODATA,	61)	/* No data available */
XEN_ERRNO(ETIME,	62)	/* Timer expired */
XEN_ERRNO(EBADMSG,	74)	/* Not a data message */
XEN_ERRNO(EOVERFLOW,	75)	/* Value too large for defined data type */
XEN_ERRNO(EILSEQ,	84)	/* Illegal byte sequence */
#ifdef __XEN__ /* Internal only, should never be exposed to the guest. */
XEN_ERRNO(ERESTART,	85)	/* Interrupted system call should be restarted */
#endif
XEN_ERRNO(ENOTSOCK,	88)	/* Socket operation on non-socket */
XEN_ERRNO(EMSGSIZE,	90)	/* Message too large. */
XEN_ERRNO(EOPNOTSUPP,	95)	/* Operation not supported on transport endpoint */
XEN_ERRNO(EADDRINUSE,	98)	/* Address already in use */
XEN_ERRNO(EADDRNOTAVAIL, 99)	/* Cannot assign requested address */
XEN_ERRNO(ENOBUFS,	105)	/* No buffer space available */
XEN_ERRNO(EISCONN,	106)	/* Transport endpoint is already connected */
XEN_ERRNO(ENOTCONN,	107)	/* Transport endpoint is not connected */
XEN_ERRNO(ETIMEDOUT,	110)	/* Connection timed out */
XEN_ERRNO(ECONNREFUSED,	111)	/* Connection refused */

#undef XEN_ERRNO
#endif /* XEN_ERRNO */
/* ` } */

/* Clean up from a default include.  Close the enum (for C). */
#ifdef XEN_ERRNO_DEFAULT_INCLUDE
#undef XEN_ERRNO_DEFAULT_INCLUDE
#ifndef __ASSEMBLY__
};
#endif

#endif /* XEN_ERRNO_DEFAULT_INCLUDE */
