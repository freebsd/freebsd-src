#ifndef __CAPSICUM_LINUX_H__
#define __CAPSICUM_LINUX_H__

#ifdef __linux__
/************************************************************
 * Linux Capsicum Functionality.
 ************************************************************/
#include <errno.h>
#include <sys/procdesc.h>
#include <sys/capsicum.h>

#define HAVE_CAP_RIGHTS_LIMIT
#define HAVE_CAP_RIGHTS_GET
#define HAVE_CAP_FCNTLS_LIMIT
#define HAVE_CAP_IOCTLS_LIMIT
#define HAVE_PROC_FDINFO
#define HAVE_PDWAIT4
#define CAP_FROM_ACCEPT
// TODO(drysdale): uncomment if/when Linux propagates rights on sctp_peeloff.
// Linux does not generate a capability from sctp_peeloff(cap_fd,...).
// #define CAP_FROM_PEELOFF
// TODO(drysdale): uncomment if/when Linux allows intermediate .. path segments
// for openat()-like operations.
// #define HAVE_OPENAT_INTERMEDIATE_DOTDOT

// Failure to open file due to path traversal generates EPERM
#ifdef ENOTBENEATH
#define E_NO_TRAVERSE_CAPABILITY ENOTBENEATH
#define E_NO_TRAVERSE_O_BENEATH ENOTBENEATH
#else
#define E_NO_TRAVERSE_CAPABILITY EPERM
#define E_NO_TRAVERSE_O_BENEATH EPERM
#endif

// Too many links
#define E_TOO_MANY_LINKS ELOOP

#endif /* __linux__ */

#endif /*__CAPSICUM_LINUX_H__*/
