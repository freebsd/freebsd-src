/* $FreeBSD$ */
#ifndef	PORT_AFTER_H
#define	PORT_AFTER_H
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

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/param.h>
#if (!defined(BSD)) || (BSD < 199306)
#include <sys/bitypes.h>
#endif
#include <netinet/in.h>
#ifdef __KAME__
#define	HAS_INET6_STRUCTS
#endif
#include <sys/time.h>

/*
 * We need to know the IPv6 address family number even on IPv4-only systems.
 * Note that this is NOT a protocol constant, and that if the system has its
 * own AF_INET6, different from ours below, all of BIND's libraries and
 * executables will need to be recompiled after the system <sys/socket.h>
 * has had this type added.  The type number below is correct on most BSD-
 * derived systems for which AF_INET6 is defined.
 */
#ifndef AF_INET6
#define AF_INET6	28
#endif

#ifdef SIN6_LEN
#define HAS_INET6_STRUCTS
#define HAVE_SA_LEN
#endif

#ifndef	PF_INET6
#define PF_INET6	AF_INET6
#endif

#ifndef HAS_INET6_STRUCTS
/* Replace with structure from later rev of O/S if known. */
struct in6_addr {
	u_int8_t	s6_addr[16];
};

/* Replace with structure from later rev of O/S if known. */
struct sockaddr_in6 {
#ifdef	HAVE_SA_LEN
	u_int8_t	sin6_len;	/* length of this struct */
	u_int8_t	sin6_family;	/* AF_INET6 */
#else
	u_int16_t	sin6_family;	/* AF_INET6 */
#endif
	u_int16_t	sin6_port;	/* transport layer port # */
	u_int32_t	sin6_flowinfo;	/* IPv6 flow information */
	struct in6_addr	sin6_addr;	/* IPv6 address */
	u_int32_t	sin6_scope_id;	/* set of interfaces for a scope */
};

#ifndef IN6ADDR_ANY_INIT
#define	IN6ADDR_ANY_INIT	{{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}}
#endif
#ifndef IN6ADDR_LOOPBACK_INIT
#define	IN6ADDR_LOOPBACK_INIT	{{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1}}
#endif
#endif	/* HAS_INET6_STRUCTS */

#if defined(NEED_SOCKADDR_STORAGE) || !defined(HAS_INET6_STRUCTS)
#define __SS_MAXSIZE 128
#define __SS_ALLIGSIZE (sizeof (long))

struct sockaddr_storage {
#ifdef  HAVE_SA_LEN
        u_int8_t        ss_len;       /* address length */
        u_int8_t        ss_family;    /* address family */
        char            __ss_pad1[__SS_ALLIGSIZE - 2 * sizeof(u_int8_t)];
        long            __ss_align;
        char            __ss_pad2[__SS_MAXSIZE - 2 * __SS_ALLIGSIZE];
#else
        u_int16_t       ss_family;    /* address family */
        char            __ss_pad1[__SS_ALLIGSIZE - sizeof(u_int16_t)];
        long            __ss_align;
        char            __ss_pad2[__SS_MAXSIZE - 2 * __SS_ALLIGSIZE];
#endif
};
#endif


#if !defined(HAS_INET6_STRUCTS) || defined(NEED_IN6ADDR_ANY)
#define in6addr_any isc_in6addr_any
extern const struct in6_addr in6addr_any;
#endif

#ifndef IN6_ARE_ADDR_EQUAL
#define IN6_ARE_ADDR_EQUAL(a,b) \
   (memcmp(&(a)->s6_addr[0], &(b)->s6_addr[0], sizeof(struct in6_addr)) == 0)
#endif

#ifndef IN6_IS_ADDR_UNSPECIFIED
#define IN6_IS_ADDR_UNSPECIFIED(a)      \
	IN6_ARE_ADDR_EQUAL(a, &in6addr_any)
#endif

#ifndef IN6_IS_ADDR_LOOPBACK
extern const struct in6_addr isc_in6addr_loopback;
#define IN6_IS_ADDR_LOOPBACK(a)		\
	IN6_ARE_ADDR_EQUAL(a, &isc_in6addr_loopback)
#endif

#ifndef IN6_IS_ADDR_V4COMPAT
#define IN6_IS_ADDR_V4COMPAT(a)		\
	((a)->s6_addr[0] == 0x00 && (a)->s6_addr[1] == 0x00 && \
	 (a)->s6_addr[2] == 0x00 && (a)->s6_addr[3] == 0x00 && \
	 (a)->s6_addr[4] == 0x00 && (a)->s6_addr[5] == 0x00 && \
	 (a)->s6_addr[6] == 0x00 && (a)->s6_addr[7] == 0x00 && \
	 (a)->s6_addr[8] == 0x00 && (a)->s6_addr[9] == 0x00 && \
	 (a)->s6_addr[10] == 0x00 && (a)->s6_addr[11] == 0x00 && \
	 ((a)->s6_addr[12] != 0x00 || (a)->s6_addr[13] != 0x00 || \
	  (a)->s6_addr[14] != 0x00 || \
	  ((a)->s6_addr[15] != 0x00 && (a)->s6_addr[15] != 1)))
#endif

#ifndef IN6_IS_ADDR_V4MAPPED
#define IN6_IS_ADDR_V4MAPPED(a)		\
	((a)->s6_addr[0] == 0x00 && (a)->s6_addr[1] == 0x00 && \
	 (a)->s6_addr[2] == 0x00 && (a)->s6_addr[3] == 0x00 && \
	 (a)->s6_addr[4] == 0x00 && (a)->s6_addr[5] == 0x00 && \
	 (a)->s6_addr[6] == 0x00 && (a)->s6_addr[7] == 0x00 && \
	 (a)->s6_addr[8] == 0x00 && (a)->s6_addr[9] == 0x00 && \
	 (a)->s6_addr[10] == 0xff && (a)->s6_addr[11] == 0xff)
#endif

#ifndef IN6_IS_ADDR_SITELOCAL
#define IN6_IS_ADDR_SITELOCAL(a)        \
	(((a)->s6_addr[0] == 0xfe) && (((a)->s6_addr[1] & 0xc0) == 0xc0))
#endif

#ifndef IN6_IS_ADDR_LINKLOCAL
#define IN6_IS_ADDR_LINKLOCAL(a)	\
	(((a)->s6_addr[0] == 0xfe) && (((a)->s6_addr[1] & 0xc0) == 0x80))
#endif

#ifndef IN6_IS_ADDR_MULTICAST
#define IN6_IS_ADDR_MULTICAST(a)        ((a)->s6_addr[0] == 0xff)
#endif

#ifndef __IPV6_ADDR_MC_SCOPE
#define __IPV6_ADDR_MC_SCOPE(a)         ((a)->s6_addr[1] & 0x0f)
#endif

#ifndef __IPV6_ADDR_SCOPE_SITELOCAL
#define __IPV6_ADDR_SCOPE_SITELOCAL 0x05
#endif

#ifndef __IPV6_ADDR_SCOPE_ORGLOCAL
#define __IPV6_ADDR_SCOPE_ORGLOCAL  0x08
#endif

#ifndef IN6_IS_ADDR_MC_SITELOCAL
#define IN6_IS_ADDR_MC_SITELOCAL(a)     \
	(IN6_IS_ADDR_MULTICAST(a) &&    \
	 (__IPV6_ADDR_MC_SCOPE(a) == __IPV6_ADDR_SCOPE_SITELOCAL))
#endif

#ifndef IN6_IS_ADDR_MC_ORGLOCAL
#define IN6_IS_ADDR_MC_ORGLOCAL(a)      \
	(IN6_IS_ADDR_MULTICAST(a) &&    \
	 (__IPV6_ADDR_MC_SCOPE(a) == __IPV6_ADDR_SCOPE_ORGLOCAL))
#endif

#ifndef INET6_ADDRSTRLEN
#define INET6_ADDRSTRLEN 46
#endif

#ifndef ISC_FACILITY
#define ISC_FACILITY LOG_DAEMON
#endif

#define UNUSED(x) (x) = (x)
#define DE_CONST(konst, var) \
	do { \
		union { const void *k; void *v; } _u; \
		_u.k = konst; \
		var = _u.v; \
	} while (0) 
#endif /* ! PORT_AFTER_H */
