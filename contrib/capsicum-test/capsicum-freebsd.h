#ifndef __CAPSICUM_FREEBSD_H__
#define __CAPSICUM_FREEBSD_H__
#ifdef __FreeBSD__
/************************************************************
 * FreeBSD Capsicum Functionality.
 ************************************************************/

#ifdef __cplusplus
extern "C" {
#endif

/* FreeBSD definitions. */
#include <errno.h>
#include <sys/param.h>
#if __FreeBSD_version >= 1100014 || \
    (__FreeBSD_version >= 1001511 && __FreeBSD_version < 1100000)
#include <sys/capsicum.h>
#else
#include <sys/capability.h>
#endif
#include <sys/procdesc.h>

#if __FreeBSD_version >= 1000000
#define AT_SYSCALLS_IN_CAPMODE
#define HAVE_CAP_RIGHTS_GET
#define HAVE_CAP_RIGHTS_LIMIT
#define HAVE_PROCDESC_FSTAT
#define HAVE_CAP_FCNTLS_LIMIT
// fcntl(2) takes int, cap_fcntls_limit(2) takes uint32_t.
typedef uint32_t cap_fcntl_t;
#define HAVE_CAP_IOCTLS_LIMIT
// ioctl(2) and cap_ioctls_limit(2) take unsigned long.
typedef unsigned long cap_ioctl_t;

#if __FreeBSD_version >= 1101000
#define HAVE_OPENAT_INTERMEDIATE_DOTDOT
#endif

#endif

#ifdef __cplusplus
}
#endif

// Use fexecve_() in tests to allow Linux variant to bypass glibc version.
#define fexecve_(F, A, E) fexecve(F, A, E)

#ifdef ENOTBENEATH
#define E_NO_TRAVERSE_CAPABILITY ENOTBENEATH
#define E_NO_TRAVERSE_O_BENEATH ENOTBENEATH
#else
#define E_NO_TRAVERSE_CAPABILITY ENOTCAPABLE
#define E_NO_TRAVERSE_O_BENEATH ENOTCAPABLE
#endif

// FreeBSD limits the number of ioctls in cap_ioctls_limit to 256
#define CAP_IOCTLS_LIMIT_MAX 256

// Too many links
#define E_TOO_MANY_LINKS EMLINK

// TODO(FreeBSD): uncomment if/when FreeBSD propagates rights on accept.
// FreeBSD does not generate a capability from accept(cap_fd,...).
// https://bugs.freebsd.org/201052
// #define CAP_FROM_ACCEPT
// TODO(FreeBSD): uncomment if/when FreeBSD propagates rights on sctp_peeloff.
// FreeBSD does not generate a capability from sctp_peeloff(cap_fd,...).
// https://bugs.freebsd.org/201052
// #define CAP_FROM_PEELOFF

#endif  /* __FreeBSD__ */

#endif /*__CAPSICUM_FREEBSD_H__*/
