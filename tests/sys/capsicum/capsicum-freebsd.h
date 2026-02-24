#ifndef __CAPSICUM_FREEBSD_H__
#define __CAPSICUM_FREEBSD_H__
/************************************************************
 * FreeBSD Capsicum Functionality.
 ************************************************************/

#ifdef __cplusplus
extern "C" {
#endif

/* FreeBSD definitions. */
#include <errno.h>
#include <sys/param.h>
#include <sys/capsicum.h>
#include <sys/procdesc.h>

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

#define HAVE_OPENAT_INTERMEDIATE_DOTDOT

#ifdef __cplusplus
}
#endif

// Use fexecve_() in tests to allow Linux variant to bypass glibc version.
#define fexecve_(F, A, E) fexecve(F, A, E)

#define E_NO_TRAVERSE_CAPABILITY ENOTCAPABLE
#define E_NO_TRAVERSE_O_BENEATH ENOTCAPABLE

// FreeBSD limits the number of ioctls in cap_ioctls_limit to 256
#define CAP_IOCTLS_LIMIT_MAX 256

// Too many links
#define E_TOO_MANY_LINKS EMLINK

// As of commit 85b0f9de11c3 ("capsicum: propagate rights on accept(2)")
// FreeBSD generates a capability from accept(cap_fd,...).
#define CAP_FROM_ACCEPT
// As of commit 91a9e4e01dab ("capsicum: propagate rights on sctp_peeloff")
// FreeBSD generates a capability from sctp_peeloff(cap_fd,...).
#define CAP_FROM_PEELOFF

#endif /*__CAPSICUM_FREEBSD_H__*/
