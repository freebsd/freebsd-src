#define CAN_RECONNECT
#define USE_POSIX
#define POSIX_SIGNALS
#define USE_UTIME
#define USE_WAITPID
#define HAVE_GETRUSAGE
#define HAVE_FCHMOD
#define NEED_PSELECT
#define HAVE_SA_LEN
#define SETPWENT_VOID
#define RLIMIT_TYPE rlim_t
#define RLIMIT_LONGLONG
#define RLIMIT_FILE_INFINITY
#define HAVE_MINIMUM_IFREQ
#define HAVE_CHROOT
#define CAN_CHANGE_ID

#define _TIMEZONE timezone

#define PORT_NONBLOCK	O_NONBLOCK
#define PORT_WOULDBLK	EWOULDBLOCK
#define WAIT_T		int
#define KSYMS		"/kernel"
#define KMEM		"/dev/kmem"
#define UDPSUM		"udpcksum"

/*
 * We need to know the IPv6 address family number even on IPv4-only systems.
 * Note that this is NOT a protocol constant, and that if the system has its
 * own AF_INET6, different from ours below, all of BIND's libraries and
 * executables will need to be recompiled after the system <sys/socket.h>
 * has had this type added.  The type number below is correct on most BSD-
 * derived systems for which AF_INET6 is defined.
 */
#ifndef AF_INET6
#define AF_INET6	24
#endif
