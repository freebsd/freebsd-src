/*	$FreeBSD$	*/

/*
 * Copyright (C) 1993-2001, 2003 by Darren Reed.
 *
 * See the IPFILTER.LICENCE file for details on licencing.
 *
 * @(#)ip_compat.h	1.8 1/14/96
 * $FreeBSD$
 * Id: ip_compat.h,v 2.142.2.25 2005/03/28 09:33:36 darrenr Exp
 */

#ifndef	__IP_COMPAT_H__
#define	__IP_COMPAT_H__

#ifndef	__P
# ifdef	__STDC__
#  define	__P(x)  x
# else
#  define	__P(x)  ()
# endif
#endif
#ifndef	__STDC__
# undef		const
# define	const
#endif

#if defined(_KERNEL) || defined(KERNEL) || defined(__KERNEL__)
# undef	KERNEL
# undef	_KERNEL
# undef 	__KERNEL__
# define	KERNEL
# define	_KERNEL
# define 	__KERNEL__
#endif

#ifndef	SOLARIS
#define	SOLARIS	(defined(sun) && (defined(__svr4__) || defined(__SVR4)))
#endif
#if SOLARIS2 >= 8
# ifndef	USE_INET6
#  define	USE_INET6
# endif
#endif
#if defined(__FreeBSD_version) && (__FreeBSD_version >= 400000) && \
    !defined(_KERNEL) && !defined(USE_INET6) && !defined(NOINET6)
# define	USE_INET6
#endif
#if defined(__NetBSD_Version__) && (__NetBSD_Version__ >= 105000000) && \
    !defined(_KERNEL) && !defined(USE_INET6)
# define	USE_INET6
# define	IPFILTER_M_IPFILTER
#endif
#if defined(OpenBSD) && (OpenBSD >= 200206) && \
    !defined(_KERNEL) && !defined(USE_INET6)
# define	USE_INET6
#endif
#if defined(__osf__)
# define	USE_INET6
#endif
#if defined(linux) && (!defined(_KERNEL) || defined(CONFIG_IPV6))
# define	USE_INET6
#endif
#if defined(HPUXREV) && (HPUXREV >= 1111)
# define	USE_INET6
#endif

#if defined(BSD) && (BSD < 199103) && defined(__osf__)
# undef BSD
# define BSD 199103
#endif

#if defined(__SVR4) || defined(__svr4__) || defined(__sgi)
# define index   strchr
# if !defined(_KERNEL)
#  define	bzero(a,b)	memset(a,0,b)
#  define	bcmp		memcmp
#  define	bcopy(a,b,c)	memmove(b,a,c)
# endif
#endif

#ifndef LIFNAMSIZ
# ifdef IF_NAMESIZE
#  define	LIFNAMSIZ	IF_NAMESIZE
# else
#  ifdef	IFNAMSIZ
#   define	LIFNAMSIZ	IFNAMSIZ
#  else
#   define	LIFNAMSIZ	16
#  endif
# endif
#endif

#if defined(__sgi) || defined(bsdi) || defined(__hpux) || defined(hpux)
struct  ether_addr {
        u_char  ether_addr_octet[6];
};
#endif

#if defined(__sgi) && !defined(IPFILTER_LKM)
# ifdef __STDC__
#  define IPL_EXTERN(ep) ipfilter##ep
# else
#  define IPL_EXTERN(ep) ipfilter/**/ep
# endif
#else
# ifdef __STDC__
#  define IPL_EXTERN(ep) ipl##ep
# else
#  define IPL_EXTERN(ep) ipl/**/ep
# endif
#endif

/*
 * This is a workaround for <sys/uio.h> troubles on FreeBSD and OpenBSD.
 */
#ifndef linux
# ifndef _KERNEL
#  define ADD_KERNEL
#  define _KERNEL
#  define KERNEL
# endif
# ifdef __OpenBSD__
struct file;
# endif
# include <sys/uio.h>
# ifdef ADD_KERNEL
#  undef _KERNEL
#  undef KERNEL
# endif
#endif


/* ----------------------------------------------------------------------- */
/*                                  S O L A R I S                          */
/* ----------------------------------------------------------------------- */
#if SOLARIS
# define	MENTAT	1
# include	<sys/cmn_err.h>
# include	<sys/isa_defs.h>
# include	<sys/stream.h>
# include	<sys/ioccom.h>
# include	<sys/sysmacros.h>
# include	<sys/kmem.h>
# if SOLARIS2 >= 10
#  include	<sys/procset.h>
#  include	<sys/proc.h>
#  include	<sys/devops.h>
#  include	<sys/ddi_impldefs.h>
# endif
/*
 * because Solaris 2 defines these in two places :-/
 */
# ifndef	KERNEL
#  define	_KERNEL
#  undef	RES_INIT
# endif /* _KERNEL */

# if SOLARIS2 >= 8
#  include <netinet/ip6.h>
#  include <netinet/icmp6.h>
# endif

# include <inet/common.h>
/* These 5 are defined in <inet/ip.h> and <netinet/ip.h> */
# undef	IPOPT_EOL
# undef	IPOPT_NOP
# undef	IPOPT_LSRR
# undef	IPOPT_RR
# undef	IPOPT_SSRR
# ifdef i386
#  define _SYS_PROMIF_H
# endif
# include <inet/ip.h>
# undef COPYOUT
# include <inet/ip_ire.h>
# ifndef	KERNEL
#  undef	_KERNEL
# endif
# if SOLARIS2 >= 8
#  define SNPRINTF	snprintf

#  include <inet/ip_if.h>
#  define	ipif_local_addr	ipif_lcl_addr
/* Only defined in private include file */
#  ifndef	V4_PART_OF_V6
#   define	V4_PART_OF_V6(v6)	v6.s6_addr32[3]
#  endif
struct ip6_ext {
	u_char	ip6e_nxt;
	u_char	ip6e_len;
};
# endif /* SOLARIS2 >= 8 */

# if SOLARIS2 >= 6
#  include <sys/atomic.h>
typedef	uint32_t	u_32_t;
# else
typedef unsigned int	u_32_t;
# endif
# define	U_32_T	1

# ifdef _KERNEL
#  define	KRWLOCK_T		krwlock_t
#  define	KMUTEX_T		kmutex_t
#  include "qif.h"
#  include "pfil.h"
#  if SOLARIS2 >= 6
#   if SOLARIS2 == 6
#    define	ATOMIC_INCL(x)		atomic_add_long((uint32_t*)&(x), 1)
#    define	ATOMIC_DECL(x)		atomic_add_long((uint32_t*)&(x), -1)
#   else
#    define	ATOMIC_INCL(x)		atomic_add_long(&(x), 1)
#    define	ATOMIC_DECL(x)		atomic_add_long(&(x), -1)
#   endif /* SOLARIS2 == 6 */
#   define	ATOMIC_INC64(x)		atomic_add_64((uint64_t*)&(x), 1)
#   define	ATOMIC_INC32(x)		atomic_add_32((uint32_t*)&(x), 1)
#   define	ATOMIC_INC16(x)		atomic_add_16((uint16_t*)&(x), 1)
#   define	ATOMIC_DEC64(x)		atomic_add_64((uint64_t*)&(x), -1)
#   define	ATOMIC_DEC32(x)		atomic_add_32((uint32_t*)&(x), -1)
#   define	ATOMIC_DEC16(x)		atomic_add_16((uint16_t*)&(x), -1)
#  else
#   define	ATOMIC_INC(x)		{ mutex_enter(&ipf_rw); (x)++; \
					  mutex_exit(&ipf_rw); }
#   define	ATOMIC_DEC(x)		{ mutex_enter(&ipf_rw); (x)--; \
					  mutex_exit(&ipf_rw); }
#  endif /* SOLARIS2 >= 6 */
#  define	USE_MUTEXES
#  define	MUTEX_ENTER(x)		mutex_enter(&(x)->ipf_lk)
#  define	READ_ENTER(x)		rw_enter(&(x)->ipf_lk, RW_READER)
#  define	WRITE_ENTER(x)		rw_enter(&(x)->ipf_lk, RW_WRITER)
#  define	MUTEX_DOWNGRADE(x)	rw_downgrade(&(x)->ipf_lk)
#  define	RWLOCK_INIT(x, y)	rw_init(&(x)->ipf_lk, (y),  \
						RW_DRIVER, NULL)
#  define	RWLOCK_EXIT(x)		rw_exit(&(x)->ipf_lk)
#  define	RW_DESTROY(x)		rw_destroy(&(x)->ipf_lk)
#  define	MUTEX_INIT(x, y)	mutex_init(&(x)->ipf_lk, (y), \
						   MUTEX_DRIVER, NULL)
#  define	MUTEX_DESTROY(x)	mutex_destroy(&(x)->ipf_lk)
#  define	MUTEX_NUKE(x)		bzero((x), sizeof(*(x)))
#  define	MUTEX_EXIT(x)		mutex_exit(&(x)->ipf_lk)
#  define	COPYIN(a,b,c)	copyin((caddr_t)(a), (caddr_t)(b), (c))
#  define	COPYOUT(a,b,c)	copyout((caddr_t)(a), (caddr_t)(b), (c))
#  define	BCOPYIN(a,b,c)	(void) copyin((caddr_t)(a), (caddr_t)(b), (c))
#  define	BCOPYOUT(a,b,c)	(void) copyout((caddr_t)(a), (caddr_t)(b), (c))
#  define	UIOMOVE(a,b,c,d)	uiomove((caddr_t)a,b,c,d)
#  define	KFREE(x)	kmem_free((char *)(x), sizeof(*(x)))
#  define	KFREES(x,s)	kmem_free((char *)(x), (s))
#  define	SPL_NET(x)	;
#  define	SPL_IMP(x)	;
#  undef	SPL_X
#  define	SPL_X(x)	;
#  ifdef sparc
#   define	ntohs(x)	(x)
#   define	ntohl(x)	(x)
#   define	htons(x)	(x)
#   define	htonl(x)	(x)
#  endif /* sparc */
#  define	KMALLOC(a,b)	(a) = (b)kmem_alloc(sizeof(*(a)), KM_NOSLEEP)
#  define	KMALLOCS(a,b,c)	(a) = (b)kmem_alloc((c), KM_NOSLEEP)
#  define	GET_MINOR(x)	getminor(x)
extern	void	*get_unit __P((char *, int));
#  define	GETIFP(n, v)	get_unit(n, v)
#  define	IFNAME(x)	((qif_t *)x)->qf_name
#  define	COPYIFNAME(x, b) \
				(void) strncpy(b, ((qif_t *)x)->qf_name, \
					       LIFNAMSIZ)
#  define	GETKTIME(x)	uniqtime((struct timeval *)x)
#  define	MSGDSIZE(x)	msgdsize(x)
#  define	M_LEN(x)	((x)->b_wptr - (x)->b_rptr)
#  define	M_DUPLICATE(x)	dupmsg((x))
#  define	MTOD(m,t)	((t)((m)->b_rptr))
#  define	MTYPE(m)	((m)->b_datap->db_type)
#  define	FREE_MB_T(m)	freemsg(m)
#  define	m_next		b_cont
#  define	CACHE_HASH(x)	(((qpktinfo_t *)(x)->fin_qpi)->qpi_num & 7)
#  define	IPF_PANIC(x,y)	if (x) { printf y; cmn_err(CE_PANIC, "ipf_panic"); }
typedef mblk_t mb_t;
# endif /* _KERNEL */

# if (SOLARIS2 >= 7)
#  ifdef lint
#   define ALIGN32(ptr)    (ptr ? 0L : 0L)
#   define ALIGN16(ptr)    (ptr ? 0L : 0L)
#  else
#   define ALIGN32(ptr)    (ptr)
#   define ALIGN16(ptr)    (ptr)
#  endif
# endif

# if SOLARIS2 < 6
typedef	struct uio	uio_t;
# endif
typedef	int		ioctlcmd_t;

# define OS_RECOGNISED 1

#endif /* SOLARIS */

/* ----------------------------------------------------------------------- */
/*                                  H P U X                                */
/* ----------------------------------------------------------------------- */
#ifdef __hpux
# define	MENTAT	1
# include	<sys/sysmacros.h>
# include	<sys/spinlock.h>
# include	<sys/lock.h>
# include	<sys/stream.h>
# ifdef USE_INET6
#  include	<netinet/if_ether.h>
#  include	<netinet/ip6.h>
#  include	<netinet/icmp6.h>
typedef	struct	ip6_hdr	ip6_t;
# endif

# ifdef _KERNEL
#  define SNPRINTF	sprintf
#  if (HPUXREV >= 1111)
#   define	IPL_SELECT
#   ifdef	IPL_SELECT
#    include	<machine/sys/user.h>
#    include	<sys/kthread_iface.h>
#    define	READ_COLLISION	0x01

typedef	struct	iplog_select_s {
	kthread_t	*read_waiter;
	int		state;
} iplog_select_t;
#   endif
#  endif

#  define	GETKTIME(x)	uniqtime((struct timeval *)x)

#  if HPUXREV == 1111
#   include	"kern_svcs.h"
#  else
#   include	<sys/kern_svcs.h>
#  endif
#  undef	ti_flags
#  undef	TCP_NODELAY
#  undef	TCP_MAXSEG
#  include <sys/reg.h>
#  include "../netinet/ip_info.h"
/*
 * According to /usr/include/sys/spinlock.h on HP-UX 11.00, these functions
 * are available.  Attempting to use them actually results in unresolved
 * symbols when it comes time to load the module.
 * This has been fixed!  Yipee!
 */
#  if 1
#   ifdef __LP64__
#    define	ATOMIC_INCL(x)		lock_and_incr_int64(&ipf_rw.ipf_lk, &(x), 1)
#    define	ATOMIC_DECL(x)		lock_and_incr_int64(&ipf_rw.ipf_lk, &(x), -1)
#   else
#    define	ATOMIC_INCL(x)		lock_and_incr_int32(&ipf_rw.ipf_lk, &(x), 1)
#    define	ATOMIC_DECL(x)		lock_and_incr_int32(&ipf_rw.ipf_lk, &(x), -1)
#   endif
#   define	ATOMIC_INC64(x)		lock_and_incr_int64(&ipf_rw.ipf_lk, &(x), 1)
#   define	ATOMIC_INC32(x)		lock_and_incr_int32(&ipf_rw.ipf_lk, &(x), 1)
#   define	ATOMIC_INC16(x)		lock_and_incr_int16(&ipf_rw.ipf_lk, &(x), 1)
#   define	ATOMIC_DEC64(x)		lock_and_incr_int64(&ipf_rw.ipf_lk, &(x), -1)
#   define	ATOMIC_DEC32(x)		lock_and_incr_int32(&ipf_rw.ipf_lk, &(x), -1)
#   define	ATOMIC_DEC16(x)		lock_and_incr_int16(&ipf_rw.ipf_lk, &(x), -1)
#  else /* 0 */
#   define	ATOMIC_INC64(x)		{ MUTEX_ENTER(&ipf_rw); (x)++; \
					  MUTEX_EXIT(&ipf_rw); }
#   define	ATOMIC_DEC64(x)		{ MUTEX_ENTER(&ipf_rw); (x)--; \
					  MUTEX_EXIT(&ipf_rw); }
#   define	ATOMIC_INC32(x)		{ MUTEX_ENTER(&ipf_rw); (x)++; \
					  MUTEX_EXIT(&ipf_rw); }
#   define	ATOMIC_DEC32(x)		{ MUTEX_ENTER(&ipf_rw); (x)--; \
					  MUTEX_EXIT(&ipf_rw); }
#   define	ATOMIC_INCL(x)		{ MUTEX_ENTER(&ipf_rw); (x)++; \
					  MUTEX_EXIT(&ipf_rw); }
#   define	ATOMIC_DECL(x)		{ MUTEX_ENTER(&ipf_rw); (x)--; \
					  MUTEX_EXIT(&ipf_rw); }
#   define	ATOMIC_INC(x)		{ MUTEX_ENTER(&ipf_rw); (x)++; \
					  MUTEX_EXIT(&ipf_rw); }
#   define	ATOMIC_DEC(x)		{ MUTEX_ENTER(&ipf_rw); (x)--; \
					  MUTEX_EXIT(&ipf_rw); }
#  endif
#  define	ip_cksum		ip_csuma
#  define	memcpy(a,b,c)		bcopy((caddr_t)b, (caddr_t)a, c)
#  define	USE_MUTEXES
#  define	MUTEX_INIT(x, y)	initlock(&(x)->ipf_lk, 0, 0, (y))
#  define	MUTEX_ENTER(x)		spinlock(&(x)->ipf_lk)
#  define	MUTEX_EXIT(x)		spinunlock(&(x)->ipf_lk);
#  define	MUTEX_DESTROY(x)
#  define	MUTEX_NUKE(x)		bzero((char *)(x), sizeof(*(x)))
#  define	KMUTEX_T		lock_t
#  define	kmutex_t		lock_t		/* for pfil.h */
#  define	krwlock_t		lock_t		/* for pfil.h */
/*
 * The read-write lock implementation in HP-UX 11.0 is crippled - it can
 * only be used by threads working in a user context!
 * This has been fixed!  Yipee! (Or at least it does in 11.00, not 11.11..)
 */
#  if HPUXREV < 1111
#   define	MUTEX_DOWNGRADE(x)	lock_write_to_read(x)
#   define	KRWLOCK_T		struct rw_lock
#   define	READ_ENTER(x)		lock_read(&(x)->ipf_lk)
#   define	WRITE_ENTER(x)		lock_write(&(x)->ipf_lk)
#   if HPUXREV >= 1111
#    define	RWLOCK_INIT(x, y)	rwlock_init4(&(x)->ipf_lk, 0, RWLCK_CANSLEEP, 0, y)
#   else
#    define	RWLOCK_INIT(x, y)	lock_init3(&(x)->ipf_lk, 0, 1, 0, 0, y)
#   endif
#   define	RWLOCK_EXIT(x)		lock_done(&(x)->ipf_lk)
#  else
#   define	KRWLOCK_T		lock_t
#   define	KMUTEX_T		lock_t
#   define	READ_ENTER(x)		MUTEX_ENTER(x)
#   define	WRITE_ENTER(x)		MUTEX_ENTER(x)
#   define	MUTEX_DOWNGRADE(x)
#   define	RWLOCK_INIT(x, y)	initlock(&(x)->ipf_lk, 0, 0, y)
#   define	RWLOCK_EXIT(x)		MUTEX_EXIT(x)
#  endif
#  define	RW_DESTROY(x)
#  define	COPYIN(a,b,c)	copyin((caddr_t)(a), (caddr_t)(b), (c))
#  define	COPYOUT(a,b,c)	copyout((caddr_t)(a), (caddr_t)(b), (c))
#  if HPUXREV >= 1111
#   define	BCOPYIN(a,b,c)	0; bcopy((caddr_t)(a), (caddr_t)(b), (c))
#   define	BCOPYOUT(a,b,c)	0; bcopy((caddr_t)(a), (caddr_t)(b), (c))
#  else
#   define	BCOPYIN(a,b,c)	bcopy((caddr_t)(a), (caddr_t)(b), (c))
#   define	BCOPYOUT(a,b,c)	bcopy((caddr_t)(a), (caddr_t)(b), (c))
#  endif
#  define	SPL_NET(x)	;
#  define	SPL_IMP(x)	;
#  undef	SPL_X
#  define	SPL_X(x)	;
extern	void	*get_unit __P((char *, int));
#  define	GETIFP(n, v)	get_unit(n, v)
#  define	IFNAME(x, b)	((ill_t *)x)->ill_name
#  define	COPYIFNAME(x, b) \
				(void) strncpy(b, ((qif_t *)x)->qf_name, \
					       LIFNAMSIZ)
#  define	UIOMOVE(a,b,c,d)	uiomove((caddr_t)a,b,c,d)
#  define	SLEEP(id, n)	{ lock_t *_l = get_sleep_lock((caddr_t)id); \
				  sleep(id, PZERO+1); \
				  spinunlock(_l); \
				}
#  define	WAKEUP(id,x)	{ lock_t *_l = get_sleep_lock((caddr_t)id); \
				  wakeup(id + x); \
				  spinunlock(_l); \
				}
#  define	KMALLOC(a, b)	MALLOC((a), b, sizeof(*(a)), M_IOSYS, M_NOWAIT)
#  define	KMALLOCS(a, b, c)	MALLOC((a), b, (c), M_IOSYS, M_NOWAIT)
#  define	KFREE(x)	kmem_free((char *)(x), sizeof(*(x)))
#  define	KFREES(x,s)	kmem_free((char *)(x), (s))
#  define	MSGDSIZE(x)	msgdsize(x)
#  define	M_LEN(x)	((x)->b_wptr - (x)->b_rptr)
#  define	M_DUPLICATE(x)	dupmsg((x))
#  define	MTOD(m,t)	((t)((m)->b_rptr))
#  define	MTYPE(m)	((m)->b_datap->db_type)
#  define	FREE_MB_T(m)	freemsg(m)
#  define	m_next		b_cont
#  define	IPF_PANIC(x,y)	if (x) { printf y; panic("ipf_panic"); }
typedef mblk_t mb_t;

#  define	CACHE_HASH(x)	(((qpktinfo_t *)(x)->fin_qpi)->qpi_num & 7)

#  include "qif.h"
#  include "pfil.h"

# else /* _KERNEL */

typedef	unsigned char uchar_t;

#  ifndef	_SYS_STREAM_INCLUDED
typedef char * mblk_t;
typedef void * queue_t;
typedef	u_long ulong;
#  endif
#  include <netinet/ip_info.h>

# endif /* _KERNEL */

# ifdef lint
#  define ALIGN32(ptr)    (ptr ? 0L : 0L)
#  define ALIGN16(ptr)    (ptr ? 0L : 0L)
# else
#  define ALIGN32(ptr)    (ptr)
#  define ALIGN16(ptr)    (ptr)
# endif

typedef	struct uio	uio_t;
typedef	int		ioctlcmd_t;
typedef	int		minor_t;
typedef unsigned int	u_32_t;
# define	U_32_T	1

# define OS_RECOGNISED 1

#endif /* __hpux */

/* ----------------------------------------------------------------------- */
/*                                  I R I X                                */
/* ----------------------------------------------------------------------- */
#ifdef __sgi
# undef		MENTAT
# if IRIX < 60500
typedef	struct uio	uio_t;
# endif
typedef	int		ioctlcmd_t;
typedef u_int32_t       u_32_t;
# define	U_32_T	1

# ifdef INET6
#  define USE_INET6
# endif

# define  hz HZ
# include <sys/ksynch.h>
# define	IPF_LOCK_PL	plhi
# include <sys/sema.h>
# undef kmutex_t
typedef struct {
	lock_t *l;
	int pl;
} kmutex_t;

# ifdef MUTEX_INIT
#  define	KMUTEX_T		mutex_t
# else
#  define	KMUTEX_T		kmutex_t
#  define	KRWLOCK_T		kmutex_t
# endif

# ifdef _KERNEL
#  define	ATOMIC_INC(x)		{ MUTEX_ENTER(&ipf_rw); \
					  (x)++; MUTEX_EXIT(&ipf_rw); }
#  define	ATOMIC_DEC(x)		{ MUTEX_ENTER(&ipf_rw); \
					  (x)--; MUTEX_EXIT(&ipf_rw); }
#  define	USE_MUTEXES
#  ifdef MUTEX_INIT
#   include <sys/atomic_ops.h>
#   define	ATOMIC_INCL(x)		atomicAddUlong(&(x), 1)
#   define	ATOMIC_INC64(x)		atomicAddUint64(&(x), 1)
#   define	ATOMIC_INC32(x)		atomicAddUint(&(x), 1)
#   define	ATOMIC_INC16		ATOMIC_INC
#   define	ATOMIC_DECL(x)		atomicAddUlong(&(x), -1)
#   define	ATOMIC_DEC64(x)		atomicAddUint64(&(x), -1)
#   define	ATOMIC_DEC32(x)		atomicAddUint(&(x), -1)
#   define	ATOMIC_DEC16		ATOMIC_DEC
#   undef	MUTEX_INIT
#   define	MUTEX_INIT(x, y)	mutex_init(&(x)->ipf_lk,  \
						   MUTEX_DEFAULT, y)
#   undef	MUTEX_ENTER
#   define	MUTEX_ENTER(x)		mutex_lock(&(x)->ipf_lk, 0)
#   undef	MUTEX_EXIT
#   define	MUTEX_EXIT(x)		mutex_unlock(&(x)->ipf_lk)
#   undef	MUTEX_DESTROY
#   define	MUTEX_DESTROY(x)	mutex_destroy(&(x)->ipf_lk)
#   define	MUTEX_DOWNGRADE(x)	mrdemote(&(x)->ipf_lk)
#   define	KRWLOCK_T		mrlock_t
#   define	RWLOCK_INIT(x, y)	mrinit(&(x)->ipf_lk, y)
#   undef	RW_DESTROY
#   define	RW_DESTROY(x)		mrfree(&(x)->ipf_lk)
#   define	READ_ENTER(x)		RW_RDLOCK(&(x)->ipf_lk)
#   define	WRITE_ENTER(x)		RW_WRLOCK(&(x)->ipf_lk)
#   define	RWLOCK_EXIT(x)		RW_UNLOCK(&(x)->ipf_lk)
#  else
#   define	READ_ENTER(x)		MUTEX_ENTER(&(x)->ipf_lk)
#   define	WRITE_ENTER(x)		MUTEX_ENTER(&(x)->ipf_lk)
#   define	MUTEX_DOWNGRADE(x)	;
#   define	RWLOCK_EXIT(x)		MUTEX_EXIT(&(x)->ipf_lk)
#   define	MUTEX_EXIT(x)		UNLOCK((x)->ipf_lk.l, (x)->ipf_lk.pl);
#   define	MUTEX_INIT(x,y)		(x)->ipf_lk.l = LOCK_ALLOC((uchar_t)-1, IPF_LOCK_PL, (lkinfo_t *)-1, KM_NOSLEEP)
#   define	MUTEX_DESTROY(x)	LOCK_DEALLOC((x)->ipf_lk.l)
#   define	MUTEX_ENTER(x)		(x)->ipf_lk.pl = LOCK((x)->ipf_lk.l, \
							      IPF_LOCK_PL);
#  endif
#  define	MUTEX_NUKE(x)		bzero((x), sizeof(*(x)))
#  define	FREE_MB_T(m)	m_freem(m)
#  define	MTOD(m,t)	mtod(m,t)
#  define	COPYIN(a,b,c)	(bcopy((caddr_t)(a), (caddr_t)(b), (c)), 0)
#  define	COPYOUT(a,b,c)	(bcopy((caddr_t)(a), (caddr_t)(b), (c)), 0)
#  define	BCOPYIN(a,b,c)	(bcopy((caddr_t)(a), (caddr_t)(b), (c)), 0)
#  define	BCOPYOUT(a,b,c)	(bcopy((caddr_t)(a), (caddr_t)(b), (c)), 0)
#  define	UIOMOVE(a,b,c,d)	uiomove((caddr_t)a,b,c,d)
#  define	SLEEP(id, n)	sleep((id), PZERO+1)
#  define	WAKEUP(id,x)	wakeup(id+x)
#  define	KFREE(x)	kmem_free((char *)(x), sizeof(*(x)))
#  define	KFREES(x,s)	kmem_free((char *)(x), (s))
#  define	GETIFP(n,v)	ifunit(n)
#  include <sys/kmem.h>
#  include <sys/ddi.h>
#  define	KMALLOC(a,b)	(a) = (b)kmem_alloc(sizeof(*(a)), KM_NOSLEEP)
#  define	KMALLOCS(a,b,c)	(a) = (b)kmem_alloc((c), KM_NOSLEEP)
#  define	GET_MINOR(x)	getminor(x)
#  define	USE_SPL		1
#  define	SPL_IMP(x)	(x) = splimp()
#  define	SPL_NET(x)	(x) = splnet()
#  define	SPL_X(x)	(void) splx(x)
extern	void	m_copydata __P((struct mbuf *, int, int, caddr_t));
extern	void	m_copyback __P((struct mbuf *, int, int, caddr_t));
#  define	MSGDSIZE(x)	mbufchainlen(x)
#  define	M_LEN(x)	(x)->m_len
#  define	M_DUPLICATE(x)	m_copy((x), 0, M_COPYALL)
#  define	GETKTIME(x)	microtime((struct timeval *)x)
#  define	CACHE_HASH(x)	((IFNAME(fin->fin_ifp)[0] + \
				  ((struct ifnet *)fin->fin_ifp)->if_unit) & 7)
#  define	IPF_PANIC(x,y)	if (x) { printf y; panic("ipf_panic"); }
typedef struct mbuf mb_t;
# else
#  undef RW_DESTROY
#  undef MUTEX_INIT
#  undef MUTEX_DESTROY
# endif /* _KERNEL */

# define OS_RECOGNISED 1

#endif /* __sgi */

/* ----------------------------------------------------------------------- */
/*                                  T R U 6 4                              */
/* ----------------------------------------------------------------------- */
#ifdef __osf__
# undef		MENTAT

# include <kern/lock.h>
# include <sys/sysmacros.h>

# ifdef _KERNEL
#  define	KMUTEX_T		simple_lock_data_t
#  define	KRWLOCK_T		lock_data_t
#  include <net/net_globals.h>
#  define	USE_MUTEXES
#  define	READ_ENTER(x)		lock_read(&(x)->ipf_lk)
#  define	WRITE_ENTER(x)		lock_write(&(x)->ipf_lk)
#  define	MUTEX_DOWNGRADE(x)	lock_write_to_read(&(x)->ipf_lk)
#  define	RWLOCK_INIT(x, y)	lock_init(&(x)->ipf_lk, TRUE)
#  define	RWLOCK_EXIT(x)		lock_done(&(x)->ipf_lk)
#  define	RW_DESTROY(x)		lock_terminate(&(x)->ipf_lk)
#  define	MUTEX_ENTER(x)		simple_lock(&(x)->ipf_lk)
#  define	MUTEX_INIT(x, y)	simple_lock_init(&(x)->ipf_lk)
#  define	MUTEX_DESTROY(x)	simple_lock_terminate(&(x)->ipf_lk)
#  define	MUTEX_EXIT(x)		simple_unlock(&(x)->ipf_lk)
#  define	MUTEX_NUKE(x)		bzero(x, sizeof(*(x)))
#  define	ATOMIC_INC64(x)		atomic_incq((uint64_t*)&(x))
#  define	ATOMIC_DEC64(x)		atomic_decq((uint64_t*)&(x))
#  define	ATOMIC_INC32(x)		atomic_incl((uint32_t*)&(x))
#  define	ATOMIC_DEC32(x)		atomic_decl((uint32_t*)&(x))
#  define	ATOMIC_INC16(x)		{ simple_lock(&ipf_rw); (x)++; \
					  simple_unlock(&ipf_rw); }
#  define	ATOMIC_DEC16(x)		{ simple_lock(&ipf_rw); (x)--; \
					  simple_unlock(&ipf_rw); }
#  define	ATOMIC_INCL(x)		atomic_incl((uint32_t*)&(x))
#  define	ATOMIC_DECL(x)		atomic_decl((uint32_t*)&(x))
#  define	ATOMIC_INC(x)		{ simple_lock(&ipf_rw); (x)++; \
					  simple_unlock(&ipf_rw); }
#  define	ATOMIC_DEC(x)		{ simple_lock(&ipf_rw); (x)--; \
					  simple_unlock(&ipf_rw); }
#  define	SPL_NET(x)		;
#  define	SPL_IMP(x)		;
#  undef	SPL_X
#  define	SPL_X(x)		;
#  define	UIOMOVE(a,b,c,d)	uiomove((caddr_t)a, b, d)
#  define	FREE_MB_T(m)		m_freem(m)
#  define	MTOD(m,t)		mtod(m,t)
#  define	GETIFP(n, v)		ifunit(n)
#  define	GET_MINOR		getminor
#  define	WAKEUP(id,x)		wakeup(id + x)
#  define	COPYIN(a,b,c)	copyin((caddr_t)(a), (caddr_t)(b), (c))
#  define	COPYOUT(a,b,c)	copyout((caddr_t)(a), (caddr_t)(b), (c))
#  define	BCOPYIN(a,b,c)	bcopy((caddr_t)(a), (caddr_t)(b), (c))
#  define	BCOPYOUT(a,b,c)	bcopy((caddr_t)(a), (caddr_t)(b), (c))
#  define	KMALLOC(a, b)	MALLOC((a), b, sizeof(*(a)), M_PFILT, M_NOWAIT)
#  define	KMALLOCS(a, b, c)	MALLOC((a), b, (c), M_PFILT, \
					    ((c) > 4096) ? M_WAITOK : M_NOWAIT)
#  define	KFREE(x)	FREE((x), M_PFILT)
#  define	KFREES(x,s)	FREE((x), M_PFILT)
#  define	MSGDSIZE(x)	mbufchainlen(x)
#  define	M_LEN(x)	(x)->m_len
#  define	M_DUPLICATE(x)	m_copy((x), 0, M_COPYALL)
#  define	GETKTIME(x)	microtime((struct timeval *)x)
#  define	CACHE_HASH(x)	((IFNAME(fin->fin_ifp)[0] + \
				  ((struct ifnet *)fin->fin_ifp)->if_unit) & 7)
#  define	IPF_PANIC(x,y)	if (x) { printf y; panic("ipf_panic"); }
typedef struct mbuf mb_t;
# endif /* _KERNEL */

# if (defined(_KERNEL) || defined(_NO_BITFIELDS) || (__STDC__ == 1))
#  define	IP_V(x)		((x)->ip_vhl >> 4)
#  define	IP_HL(x)	((x)->ip_vhl & 0xf)
#  define	IP_V_A(x,y)	(x)->ip_vhl |= (((y) << 4) & 0xf0)
#  define	IP_HL_A(x,y)	(x)->ip_vhl |= ((y) & 0xf)
#  define	TCP_X2(x)	((x)->th_xoff & 0xf)
#  define	TCP_X2_A(x,y)	(x)->th_xoff |= ((y) & 0xf)
#  define	TCP_OFF(x)	((x)->th_xoff >> 4)
#  define	TCP_OFF_A(x,y)	(x)->th_xoff |= (((y) << 4) & 0xf0)
# endif

/*
 * These are from's Solaris' #defines for little endian.
 */
#define	IP6F_MORE_FRAG		0x0100
#define	IP6F_RESERVED_MASK	0x0600
#define	IP6F_OFF_MASK		0xf8ff

struct ip6_ext {
	u_char	ip6e_nxt;
	u_char	ip6e_len;
};

typedef	int		ioctlcmd_t;  
/*
 * Really, any arch where sizeof(long) != sizeof(int).
 */
typedef unsigned int    u_32_t;
# define	U_32_T	1

# define OS_RECOGNISED 1
#endif /* __osf__ */

/* ----------------------------------------------------------------------- */
/*                                  N E T B S D                            */
/* ----------------------------------------------------------------------- */
#ifdef __NetBSD__
# if defined(_KERNEL) && !defined(IPFILTER_LKM)
#  include "bpfilter.h"
#  if defined(__NetBSD_Version__) && (__NetBSD_Version__ >= 104110000)
#   include "opt_inet.h"
#  endif
#  ifdef INET6
#   define USE_INET6
#  endif
#  if (__NetBSD_Version__ >= 105000000)
#   define HAVE_M_PULLDOWN 1
#  endif
# endif

# ifdef _KERNEL
#  define	MSGDSIZE(x)	mbufchainlen(x)
#  define	M_LEN(x)	(x)->m_len
#  define	M_DUPLICATE(x)	m_copy((x), 0, M_COPYALL)
#  define	GETKTIME(x)	microtime((struct timeval *)x)
#  define	IPF_PANIC(x,y)	if (x) { printf y; panic("ipf_panic"); }
#  define	COPYIN(a,b,c)	copyin((caddr_t)(a), (caddr_t)(b), (c))
#  define	COPYOUT(a,b,c)	copyout((caddr_t)(a), (caddr_t)(b), (c))
#  define	BCOPYIN(a,b,c)	bcopy((caddr_t)(a), (caddr_t)(b), (c))
#  define	BCOPYOUT(a,b,c)	bcopy((caddr_t)(a), (caddr_t)(b), (c))
typedef struct mbuf mb_t;
# endif /* _KERNEL */
# if (NetBSD <= 1991011) && (NetBSD >= 199606)
#  define	IFNAME(x)	((struct ifnet *)x)->if_xname
#  define	COPYIFNAME(x, b) \
				(void) strncpy(b, \
					       ((struct ifnet *)x)->if_xname, \
					       LIFNAMSIZ)
#  define	CACHE_HASH(x)	((((struct ifnet *)fin->fin_ifp)->if_index)&7)
# else
#  define	CACHE_HASH(x)	((IFNAME(fin->fin_ifp)[0] + \
				  ((struct ifnet *)fin->fin_ifp)->if_unit) & 7)
# endif

typedef	struct uio	uio_t;
typedef	u_long		ioctlcmd_t;  
typedef	int		minor_t;
typedef	u_int32_t	u_32_t;
# define	U_32_T	1

# define OS_RECOGNISED 1
#endif /* __NetBSD__ */


/* ----------------------------------------------------------------------- */
/*                                F R E E B S D                            */
/* ----------------------------------------------------------------------- */
#ifdef __FreeBSD__
# if defined(_KERNEL) && !defined(IPFILTER_LKM) && !defined(KLD_MODULE)
#  if (__FreeBSD_version >= 500000)                          
#   include "opt_bpf.h"
#  else
#   include "bpf.h"    
#  endif
#  if defined(__FreeBSD_version) && (__FreeBSD_version >= 400000)
#   include "opt_inet6.h"
#  endif
#  if defined(INET6) && !defined(USE_INET6)
#   define USE_INET6
#  endif
# endif

# if defined(_KERNEL)
#  if (__FreeBSD_version >= 400000)
/*
 * When #define'd, the 5.2.1 kernel panics when used with the ftp proxy.
 * There may be other, safe, kernels but this is not extensively tested yet.
 */
#   define HAVE_M_PULLDOWN
#  endif
#  if !defined(IPFILTER_LKM) && (__FreeBSD_version >= 300000)
#   include "opt_ipfilter.h"
#  endif
#  define	COPYIN(a,b,c)	copyin((caddr_t)(a), (caddr_t)(b), (c))
#  define	COPYOUT(a,b,c)	copyout((caddr_t)(a), (caddr_t)(b), (c))
#  define	BCOPYIN(a,b,c)	bcopy((caddr_t)(a), (caddr_t)(b), (c))
#  define	BCOPYOUT(a,b,c)	bcopy((caddr_t)(a), (caddr_t)(b), (c))

#  if (__FreeBSD_version >= 500043)
#   define NETBSD_PF
#  endif
# endif /* _KERNEL */

# if (__FreeBSD_version >= 500043)
#  include <sys/mutex.h>
#  include <sys/sx.h>
/*
 * Whilst the sx(9) locks on FreeBSD have the right semantics and interface
 * for what we want to use them for, despite testing showing they work -
 * with a WITNESS kernel, it generates LOR messages.
 */
#  define	KMUTEX_T		struct mtx
#  if 1
#   define	KRWLOCK_T		struct mtx
#  else
#   define	KRWLOCK_T		struct sx
#  endif
# endif

# if (__FreeBSD_version >= 501113)
#  include <net/if_var.h>
#  define	IFNAME(x)	((struct ifnet *)x)->if_xname
#  define	COPYIFNAME(x, b) \
				(void) strncpy(b, \
					       ((struct ifnet *)x)->if_xname, \
					       LIFNAMSIZ)
# endif
# if (__FreeBSD_version >= 500043)
#  define	CACHE_HASH(x)	((((struct ifnet *)fin->fin_ifp)->if_index) & 7)
# else
#  define	CACHE_HASH(x)	((IFNAME(fin->fin_ifp)[0] + \
				  ((struct ifnet *)fin->fin_ifp)->if_unit) & 7)
# endif

# ifdef _KERNEL
#  define	GETKTIME(x)	microtime((struct timeval *)x)

#  if (__FreeBSD_version >= 500002)
#   include <netinet/in_systm.h>
#   include <netinet/ip.h>
#   include <machine/in_cksum.h>
#  endif

#  if (__FreeBSD_version >= 500043)
#   define	USE_MUTEXES
#   define	MUTEX_ENTER(x)		mtx_lock(&(x)->ipf_lk)
#   define	MUTEX_EXIT(x)		mtx_unlock(&(x)->ipf_lk)
#   define	MUTEX_INIT(x,y)		mtx_init(&(x)->ipf_lk, (y), NULL,\
						 MTX_DEF)
#   define	MUTEX_DESTROY(x)	mtx_destroy(&(x)->ipf_lk)
#   define	MUTEX_NUKE(x)		bzero((x), sizeof(*(x)))
/*
 * Whilst the sx(9) locks on FreeBSD have the right semantics and interface
 * for what we want to use them for, despite testing showing they work -
 * with a WITNESS kernel, it generates LOR messages.
 */
#   if 1
#    define	READ_ENTER(x)		mtx_lock(&(x)->ipf_lk)
#    define	WRITE_ENTER(x)		mtx_lock(&(x)->ipf_lk)
#    define	RWLOCK_EXIT(x)		mtx_unlock(&(x)->ipf_lk)
#    define	MUTEX_DOWNGRADE(x)	;
#    define	RWLOCK_INIT(x,y)	mtx_init(&(x)->ipf_lk, (y), NULL,\
						 MTX_DEF)
#    define	RW_DESTROY(x)		mtx_destroy(&(x)->ipf_lk)
#   else
#    define	READ_ENTER(x)		sx_slock(&(x)->ipf_lk)
#    define	WRITE_ENTER(x)		sx_xlock(&(x)->ipf_lk)
#    define	MUTEX_DOWNGRADE(x)	sx_downgrade(&(x)->ipf_lk)
#    define	RWLOCK_INIT(x, y)	sx_init(&(x)->ipf_lk, (y))
#    define	RW_DESTROY(x)		sx_destroy(&(x)->ipf_lk)
#    ifdef sx_unlock
#     define	RWLOCK_EXIT(x)		sx_unlock(x)
#    else
#     define	RWLOCK_EXIT(x)		do { \
					    if ((x)->ipf_lk.sx_cnt < 0) \
						sx_xunlock(&(x)->ipf_lk); \
					    else \
						sx_sunlock(&(x)->ipf_lk); \
					} while (0)
#    endif
#   endif
#   include <machine/atomic.h>
#   define	ATOMIC_INC(x)		{ mtx_lock(&ipf_rw.ipf_lk); (x)++; \
					  mtx_unlock(&ipf_rw.ipf_lk); }
#   define	ATOMIC_DEC(x)		{ mtx_lock(&ipf_rw.ipf_lk); (x)--; \
					  mtx_unlock(&ipf_rw.ipf_lk); }
#   define	ATOMIC_INCL(x)		atomic_add_long(&(x), 1)
#   define	ATOMIC_INC64(x)		ATOMIC_INC(x)
#   define	ATOMIC_INC32(x)		atomic_add_32(&(x), 1)
#   define	ATOMIC_INC16(x)		atomic_add_16(&(x), 1)
#   define	ATOMIC_DECL(x)		atomic_add_long(&(x), -1)
#   define	ATOMIC_DEC64(x)		ATOMIC_DEC(x)
#   define	ATOMIC_DEC32(x)		atomic_add_32(&(x), -1)
#   define	ATOMIC_DEC16(x)		atomic_add_16(&(x), -1)
#   define	SPL_X(x)	;
#   define	SPL_NET(x)	;
#   define	SPL_IMP(x)	;
extern	int	in_cksum __P((struct mbuf *, int));
#  endif /* __FreeBSD_version >= 500043 */
#  define	MSGDSIZE(x)	mbufchainlen(x)
#  define	M_LEN(x)	(x)->m_len
#  define	M_DUPLICATE(x)	m_copy((x), 0, M_COPYALL)
#  define	IPF_PANIC(x,y)	if (x) { printf y; panic("ipf_panic"); }
typedef struct mbuf mb_t;
# endif /* _KERNEL */

# if __FreeBSD__ < 3
#  include <machine/spl.h>
# else
#  if __FreeBSD__ == 3
#   if defined(IPFILTER_LKM) && !defined(ACTUALLY_LKM_NOT_KERNEL)
#    define	ACTUALLY_LKM_NOT_KERNEL
#   endif
#  endif
# endif

# if (__FreeBSD_version >= 300000)
typedef	u_long		ioctlcmd_t;
# else
typedef	int		ioctlcmd_t;
# endif
typedef	struct uio	uio_t;
typedef	int		minor_t;
typedef	u_int32_t	u_32_t;
# define	U_32_T	1

# define OS_RECOGNISED 1
#endif /* __FreeBSD__ */


/* ----------------------------------------------------------------------- */
/*                                O P E N B S D                            */
/* ----------------------------------------------------------------------- */
#ifdef __OpenBSD__
# ifdef INET6
#  define USE_INET6
# endif

# ifdef _KERNEL
#  if !defined(IPFILTER_LKM)
#   include "bpfilter.h"
#  endif
#  if (OpenBSD >= 200311)
#   define SNPRINTF	snprintf
#   if defined(USE_INET6)
#    include "netinet6/in6_var.h"
#    include "netinet6/nd6.h"
#   endif
#  endif
#  if (OpenBSD >= 200012)
#   define HAVE_M_PULLDOWN 1
#  endif
#  define	COPYIN(a,b,c)	copyin((caddr_t)(a), (caddr_t)(b), (c))
#  define	COPYOUT(a,b,c)	copyout((caddr_t)(a), (caddr_t)(b), (c))
#  define	BCOPYIN(a,b,c)	bcopy((caddr_t)(a), (caddr_t)(b), (c))
#  define	BCOPYOUT(a,b,c)	bcopy((caddr_t)(a), (caddr_t)(b), (c))
#  define	GETKTIME(x)	microtime((struct timeval *)x)
#  define	MSGDSIZE(x)	mbufchainlen(x)
#  define	M_LEN(x)	(x)->m_len
#  define	M_DUPLICATE(x)	m_copy((x), 0, M_COPYALL)
#  define	IPF_PANIC(x,y)	if (x) { printf y; panic("ipf_panic"); }
typedef struct mbuf mb_t;
# endif /* _KERNEL */
# if (OpenBSD >= 199603)
#  define	IFNAME(x, b)	((struct ifnet *)x)->if_xname
#  define	COPYIFNAME(x, b) \
				(void) strncpy(b, \
					       ((struct ifnet *)x)->if_xname, \
					       LIFNAMSIZ)
#  define	CACHE_HASH(x)	((((struct ifnet *)fin->fin_ifp)->if_index)&7)
# else
#  define	CACHE_HASH(x)	((IFNAME(fin->fin_ifp)[0] + \
				  ((struct ifnet *)fin->fin_ifp)->if_unit) & 7)
# endif

typedef	struct uio	uio_t;
typedef	u_long		ioctlcmd_t;  
typedef	int		minor_t;
typedef	u_int32_t	u_32_t;
# define	U_32_T	1

# define OS_RECOGNISED 1
#endif /* __OpenBSD__ */


/* ----------------------------------------------------------------------- */
/*                                B S D O S                                */
/* ----------------------------------------------------------------------- */
#ifdef _BSDI_VERSION
# ifdef INET6
#  define USE_INET6
# endif

# ifdef _KERNEL
#  define	GETKTIME(x)	microtime((struct timeval *)x)
#  define	MSGDSIZE(x)	mbufchainlen(x)
#  define	M_LEN(x)	(x)->m_len
#  define	M_DUPLICATE(x)	m_copy((x), 0, M_COPYALL)
#  define	CACHE_HASH(x)	((IFNAME(fin->fin_ifp)[0] + \
				  ((struct ifnet *)fin->fin_ifp)->if_unit) & 7)
typedef struct mbuf mb_t;
# endif /* _KERNEL */

# if (_BSDI_VERSION >= 199701)
typedef	u_long		ioctlcmd_t;
# else
typedef	int		ioctlcmd_t;
# endif
typedef	u_int32_t	u_32_t;
# define	U_32_T	1

#endif /* _BSDI_VERSION */


/* ----------------------------------------------------------------------- */
/*                                  S U N O S 4                            */
/* ----------------------------------------------------------------------- */
#if defined(sun) && !defined(OS_RECOGNISED) /* SunOS4 */
# ifdef _KERNEL
#  include	<sys/kmem_alloc.h>
#  define	GETKTIME(x)	uniqtime((struct timeval *)x)
#  define	MSGDSIZE(x)	mbufchainlen(x)
#  define	M_LEN(x)	(x)->m_len
#  define	M_DUPLICATE(x)	m_copy((x), 0, M_COPYALL)
#  define	CACHE_HASH(x)	((IFNAME(fin->fin_ifp)[0] + \
				  ((struct ifnet *)fin->fin_ifp)->if_unit) & 7)
#  define	GETIFP(n, v)	ifunit(n, IFNAMSIZ)
#  define	KFREE(x)	kmem_free((char *)(x), sizeof(*(x)))
#  define	KFREES(x,s)	kmem_free((char *)(x), (s))
#  define	SLEEP(id, n)	sleep((id), PZERO+1)
#  define	WAKEUP(id,x)	wakeup(id + x)
#  define	UIOMOVE(a,b,c,d)	uiomove((caddr_t)a,b,c,d)
#  define	IPF_PANIC(x,y)	if (x) { printf y; panic("ipf_panic"); }

extern	void	m_copydata __P((struct mbuf *, int, int, caddr_t));
extern	void	m_copyback __P((struct mbuf *, int, int, caddr_t));

typedef struct mbuf mb_t;
# endif

typedef	struct uio	uio_t;
typedef	int		ioctlcmd_t;  
typedef	int		minor_t;
typedef	unsigned int	u_32_t;
# define	U_32_T	1

# define OS_RECOGNISED 1

#endif /* SunOS 4 */

/* ----------------------------------------------------------------------- */
/*                            L I N U X                                    */
/* ----------------------------------------------------------------------- */
#if defined(linux) && !defined(OS_RECOGNISED)
#include <linux/config.h>
#include <linux/version.h>
# if LINUX >= 20600
#  define	 HDR_T_PRIVATE	1
# endif
# undef USE_INET6
# ifdef USE_INET6
struct ip6_ext {
	u_char	ip6e_nxt;
	u_char	ip6e_len;
};
# endif

# ifdef _KERNEL
#  define	IPF_PANIC(x,y)	if (x) { printf y; panic("ipf_panic"); }
#  define	BCOPYIN(a,b,c)	bcopy((caddr_t)(a), (caddr_t)(b), (c))
#  define	BCOPYOUT(a,b,c)	bcopy((caddr_t)(a), (caddr_t)(b), (c))
#  define	COPYIN(a,b,c)	copy_from_user((caddr_t)(b), (caddr_t)(a), (c))
#  define	COPYOUT(a,b,c)	copy_to_user((caddr_t)(b), (caddr_t)(a), (c))
#  define	FREE_MB_T(m)	kfree_skb(m)
#  define	GETKTIME(x)	do_gettimeofday((struct timeval *)x)
#  define	SLEEP(x,s)	0, interruptible_sleep_on(x##_linux)
#  define	WAKEUP(x,y)	wake_up(x##_linux + y)
#  define	UIOMOVE(a,b,c,d)	uiomove(a,b,c,d)
#  define	USE_MUTEXES
#  define	KRWLOCK_T		rwlock_t
#  define	KMUTEX_T		spinlock_t
#  define	MUTEX_INIT(x,y)		spin_lock_init(&(x)->ipf_lk)
#  define	MUTEX_ENTER(x)		spin_lock(&(x)->ipf_lk)
#  define	MUTEX_EXIT(x)		spin_unlock(&(x)->ipf_lk)
#  define	MUTEX_DESTROY(x)	do { } while (0)
#  define	MUTEX_NUKE(x)		bzero(&(x)->ipf_lk, sizeof((x)->ipf_lk))
#  define	READ_ENTER(x)		ipf_read_enter(x)
#  define	WRITE_ENTER(x)		ipf_write_enter(x)
#  define	RWLOCK_INIT(x,y)	rwlock_init(&(x)->ipf_lk)
#  define	RW_DESTROY(x)		do { } while (0)
#  define	RWLOCK_EXIT(x)		ipf_rw_exit(x)
#  define	MUTEX_DOWNGRADE(x)	ipf_rw_downgrade(x)
#  define	ATOMIC_INCL(x)		MUTEX_ENTER(&ipf_rw); (x)++; \
					MUTEX_EXIT(&ipf_rw)
#  define	ATOMIC_DECL(x)		MUTEX_ENTER(&ipf_rw); (x)--; \
					MUTEX_EXIT(&ipf_rw)
#  define	ATOMIC_INC64(x)		MUTEX_ENTER(&ipf_rw); (x)++; \
					MUTEX_EXIT(&ipf_rw)
#  define	ATOMIC_INC32(x)		MUTEX_ENTER(&ipf_rw); (x)++; \
					MUTEX_EXIT(&ipf_rw)
#  define	ATOMIC_INC16(x)		MUTEX_ENTER(&ipf_rw); (x)++; \
					MUTEX_EXIT(&ipf_rw)
#  define	ATOMIC_DEC64(x)		MUTEX_ENTER(&ipf_rw); (x)--; \
					MUTEX_EXIT(&ipf_rw)
#  define	ATOMIC_DEC32(x)		MUTEX_ENTER(&ipf_rw); (x)--; \
					MUTEX_EXIT(&ipf_rw)
#  define	ATOMIC_DEC16(x)		MUTEX_ENTER(&ipf_rw); (x)--; \
					MUTEX_EXIT(&ipf_rw)
#  define	SPL_IMP(x)		do { } while (0)
#  define	SPL_NET(x)		do { } while (0)
#  define	SPL_X(x)		do { } while (0)
#  define	IFNAME(x)		((struct net_device*)x)->name
#  define	CACHE_HASH(x)	((IFNAME(fin->fin_ifp)[0] + \
			  ((struct net_device *)fin->fin_ifp)->ifindex) & 7)
typedef	struct	sk_buff	mb_t;
extern	void	m_copydata __P((mb_t *, int, int, caddr_t));
extern	void	m_copyback __P((mb_t *, int, int, caddr_t));
extern	void	m_adj __P((mb_t *, int));
extern	mb_t	*m_pullup __P((mb_t *, int));
#  define	mbuf	sk_buff

#  define	mtod(m, t)	((t)(m)->data)
#  define	m_len		len
#  define	m_next		next
#  define	M_DUPLICATE(m)	skb_clone((m), in_interrupt() ? GFP_ATOMIC : \
								GFP_KERNEL)
#  define	MSGDSIZE(m)	(m)->len
#  define	M_LEN(m)	(m)->len

#  define	splnet(x)	;
#  define	printf		printk
#  define	bcopy(s,d,z)	memmove(d, s, z)
#  define	bzero(s,z)	memset(s, 0, z)
#  define	bcmp(a,b,z)	memcmp(a, b, z)

#  define	ifnet		net_device
#  define	if_xname	name
#  define	if_unit		ifindex 

#  define	KMALLOC(x,t)	(x) = (t)kmalloc(sizeof(*(x)), \
				    in_interrupt() ? GFP_ATOMIC : GFP_KERNEL)
#  define	KFREE(x)	kfree(x)
#  define	KMALLOCS(x,t,s)	(x) = (t)kmalloc((s), \
				    in_interrupt() ? GFP_ATOMIC : GFP_KERNEL)
#  define	KFREES(x,s)	kfree(x)

#  define GETIFP(n,v)	dev_get_by_name(n)

# else
#  include <net/ethernet.h>

struct mbuf {
};

#  ifndef _NET_ROUTE_H
struct rtentry {
};
#  endif

struct ifnet {
	char	if_xname[IFNAMSIZ];
	int	if_unit;
	int	(* if_output) __P((struct ifnet *, struct mbuf *, struct sockaddr *, struct rtentry *));
	struct	ifaddr	*if_addrlist;
};
# define	IFNAME(x)	((struct ifnet *)x)->if_xname

# endif	/* _KERNEL */

# define	COPYIFNAME(x, b) \
				(void) strncpy(b, \
					       ((struct ifnet *)x)->if_xname, \
					       LIFNAMSIZ)

# include <linux/fs.h>
# define	FWRITE	FMODE_WRITE
# define	FREAD	FMODE_READ

# define	__USE_MISC	1
# define	__FAVOR_BSD	1

typedef	struct uio {
	struct iovec	*uio_iov;
	void	*uio_file;
	char	*uio_buf;
	int	uio_iovcnt;
	int	uio_offset;
	size_t	uio_resid;
	int	uio_rw;
} uio_t;

extern	int	uiomove __P((caddr_t, size_t, int, struct uio *));

# define	UIO_READ	1
# define	UIO_WRITE	2

typedef	u_long		ioctlcmd_t;
typedef	int		minor_t;
typedef u_int32_t 	u_32_t;
# define	U_32_T	1

# define OS_RECOGNISED 1

#endif


#ifndef	OS_RECOGNISED
#error	ip_compat.h does not recognise this platform/OS.
#endif


/* ----------------------------------------------------------------------- */
/*                           G E N E R I C                                 */
/* ----------------------------------------------------------------------- */
#ifndef OS_RECOGNISED
#endif

/*
 * For BSD kernels, if bpf is in the kernel, enable ipfilter to use bpf in
 * filter rules.
 */
#if !defined(IPFILTER_BPF) && ((NBPF > 0) || (NBPFILTER > 0))
# define IPFILTER_BPF
#endif

/*
 * Userland locking primitives
 */
typedef	struct	{
	char	*eMm_owner;
	char	*eMm_heldin;
	u_int	eMm_magic;
	int	eMm_held;
	int	eMm_heldat;
#ifdef __hpux
	char	eMm_fill[8];
#endif
} eMmutex_t;

typedef	struct	{
	char	*eMrw_owner;
	char	*eMrw_heldin;
	u_int	eMrw_magic;
	short	eMrw_read;
	short	eMrw_write;
	int	eMrw_heldat;
#ifdef __hpux
	char	eMm_fill[24];
#endif
} eMrwlock_t;

typedef union {
#ifdef KMUTEX_T
	struct	{
		KMUTEX_T	ipf_slk;
		char		*ipf_lname;
	} ipf_lkun_s;
#endif
	eMmutex_t	ipf_emu;
} ipfmutex_t;

typedef union {
#ifdef KRWLOCK_T
	struct	{
		KRWLOCK_T	ipf_slk;
		char		*ipf_lname;
		int		ipf_sr;
		int		ipf_sw;
		u_int		ipf_magic;
	} ipf_lkun_s;
#endif
	eMrwlock_t	ipf_emu;
} ipfrwlock_t;

#define	ipf_lk		ipf_lkun_s.ipf_slk
#define	ipf_lname	ipf_lkun_s.ipf_lname
#define	ipf_isr		ipf_lkun_s.ipf_sr
#define	ipf_isw		ipf_lkun_s.ipf_sw
#define	ipf_magic	ipf_lkun_s.ipf_magic

#if !defined(__GNUC__) || \
    (defined(__FreeBSD_version) && (__FreeBSD_version >= 503000))
# ifndef	INLINE
#  define	INLINE
# endif
#else
# define	INLINE	__inline__
#endif

#if defined(linux) && defined(_KERNEL)
extern	INLINE	void	ipf_read_enter __P((ipfrwlock_t *));
extern	INLINE	void	ipf_write_enter __P((ipfrwlock_t *));
extern	INLINE	void	ipf_rw_exit __P((ipfrwlock_t *));
extern	INLINE	void	ipf_rw_downgrade __P((ipfrwlock_t *));
#endif

/*
 * In a non-kernel environment, there are a lot of macros that need to be
 * filled in to be null-ops or to point to some compatibility function,
 * somewhere in userland.
 */
#ifndef _KERNEL
typedef	struct	mb_s	{
	struct	mb_s	*mb_next;
	int		mb_len;
	u_long		mb_buf[2048];
} mb_t;
# undef		m_next
# define	m_next		mb_next
# define	MSGDSIZE(x)	(x)->mb_len	/* XXX - from ipt.c */
# define	M_LEN(x)	(x)->mb_len
# define	M_DUPLICATE(x)	(x)
# define	GETKTIME(x)	gettimeofday((struct timeval *)(x), NULL)
# define	MTOD(m, t)	((t)(m)->mb_buf)
# define	FREE_MB_T(x)
# define	SLEEP(x,y)	1;
# define	WAKEUP(x,y)	;
# define	IPF_PANIC(x,y)	;
# define	PANIC(x,y)	;
# define	SPL_NET(x)	;
# define	SPL_IMP(x)	;
# define	SPL_X(x)	;
# define	KMALLOC(a,b)	(a) = (b)malloc(sizeof(*a))
# define	KMALLOCS(a,b,c)	(a) = (b)malloc(c)
# define	KFREE(x)	free(x)
# define	KFREES(x,s)	free(x)
# define	GETIFP(x, v)	get_unit(x,v)
# define	COPYIN(a,b,c)	(bcopy((a), (b), (c)), 0)
# define	COPYOUT(a,b,c)	(bcopy((a), (b), (c)), 0)
# define	BCOPYIN(a,b,c)	(bcopy((a), (b), (c)), 0)
# define	BCOPYOUT(a,b,c)	(bcopy((a), (b), (c)), 0)
# define	COPYDATA(m, o, l, b)	bcopy(MTOD((mb_t *)m, char *) + (o), \
					      (b), (l))
# define	COPYBACK(m, o, l, b)	bcopy((b), \
					      MTOD((mb_t *)m, char *) + (o), \
					      (l))
# define	UIOMOVE(a,b,c,d)	ipfuiomove(a,b,c,d)
extern	void	m_copydata __P((mb_t *, int, int, caddr_t));
extern	int	ipfuiomove __P((caddr_t, int, int, struct uio *));
# ifndef CACHE_HASH
#  define	CACHE_HASH(x)	((IFNAME(fin->fin_ifp)[0] + \
				  ((struct ifnet *)fin->fin_ifp)->if_unit) & 7)
# endif

# define	MUTEX_DESTROY(x)	eMmutex_destroy(&(x)->ipf_emu)
# define	MUTEX_ENTER(x)		eMmutex_enter(&(x)->ipf_emu, \
						      __FILE__, __LINE__)
# define	MUTEX_EXIT(x)		eMmutex_exit(&(x)->ipf_emu)
# define	MUTEX_INIT(x,y)		eMmutex_init(&(x)->ipf_emu, y)
# define	MUTEX_NUKE(x)		bzero((x), sizeof(*(x)))

# define	MUTEX_DOWNGRADE(x)	eMrwlock_downgrade(&(x)->ipf_emu, \
							   __FILE__, __LINE__)
# define	READ_ENTER(x)		eMrwlock_read_enter(&(x)->ipf_emu, \
							    __FILE__, __LINE__)
# define	RWLOCK_INIT(x, y)	eMrwlock_init(&(x)->ipf_emu, y)
# define	RWLOCK_EXIT(x)		eMrwlock_exit(&(x)->ipf_emu)
# define	RW_DESTROY(x)		eMrwlock_destroy(&(x)->ipf_emu)
# define	WRITE_ENTER(x)		eMrwlock_write_enter(&(x)->ipf_emu, \
							     __FILE__, \
							     __LINE__)

# define	USE_MUTEXES		1

extern void eMmutex_destroy __P((eMmutex_t *));
extern void eMmutex_enter __P((eMmutex_t *, char *, int));
extern void eMmutex_exit __P((eMmutex_t *));
extern void eMmutex_init __P((eMmutex_t *, char *));
extern void eMrwlock_destroy __P((eMrwlock_t *));
extern void eMrwlock_exit __P((eMrwlock_t *));
extern void eMrwlock_init __P((eMrwlock_t *, char *));
extern void eMrwlock_read_enter __P((eMrwlock_t *, char *, int));
extern void eMrwlock_write_enter __P((eMrwlock_t *, char *, int));
extern void eMrwlock_downgrade __P((eMrwlock_t *, char *, int));

#endif

#define	MAX_IPV4HDR	((0xf << 2) + sizeof(struct icmp) + sizeof(ip_t) + 8)

#ifndef	IP_OFFMASK
# define	IP_OFFMASK	0x1fff
#endif


/*
 * On BSD's use quad_t as a guarantee for getting at least a 64bit sized
 * object.
 */
#if	BSD > 199306
# define	USE_QUAD_T
# define	U_QUAD_T	u_quad_t
# define	QUAD_T		quad_t
#else /* BSD > 199306 */
# define	U_QUAD_T	u_long
# define	QUAD_T		long
#endif /* BSD > 199306 */


#ifdef	USE_INET6
# if defined(__NetBSD__) || defined(__OpenBSD__) || defined(__FreeBSD__) || \
     defined(__osf__) || defined(linux)
#  include <netinet/ip6.h>
#  include <netinet/icmp6.h>
#  if !defined(linux)
#   if defined(_KERNEL) && !defined(__osf__)
#    include <netinet6/ip6_var.h>
#   endif
#  endif
typedef	struct ip6_hdr	ip6_t;
# endif
#endif

#ifndef	MAX
# define	MAX(a,b)	(((a) > (b)) ? (a) : (b))
#endif

#if defined(_KERNEL)
# ifdef MENTAT
#  define	COPYDATA	mb_copydata
#  define	COPYBACK	mb_copyback
# else
#  define	COPYDATA	m_copydata
#  define	COPYBACK	m_copyback
# endif
# if (BSD >= 199306) || defined(__FreeBSD__)
#  if (defined(__NetBSD_Version__) && (__NetBSD_Version__ < 105180000)) || \
       defined(__FreeBSD__) || (defined(OpenBSD) && (OpenBSD < 200206)) || \
       defined(_BSDI_VERSION)
#   include <vm/vm.h>
#  endif
#  if !defined(__FreeBSD__) || (defined (__FreeBSD_version) && \
      (__FreeBSD_version >= 300000))
#   if (defined(__NetBSD_Version__) && (__NetBSD_Version__ >= 105180000)) || \
       (defined(OpenBSD) && (OpenBSD >= 200111))
#    include <uvm/uvm_extern.h>
#   else
#    include <vm/vm_extern.h>
extern  vm_map_t        kmem_map;
#   endif
#   include <sys/proc.h>
#  else /* !__FreeBSD__ || (__FreeBSD__ && __FreeBSD_version >= 300000) */
#   include <vm/vm_kern.h>
#  endif /* !__FreeBSD__ || (__FreeBSD__ && __FreeBSD_version >= 300000) */

#  ifdef IPFILTER_M_IPFILTER
#    include <sys/malloc.h>
MALLOC_DECLARE(M_IPFILTER);
#    define	_M_IPF		M_IPFILTER
#  else /* IPFILTER_M_IPFILTER */
#   ifdef M_PFIL
#    define	_M_IPF		M_PFIL
#   else
#    ifdef M_IPFILTER
#     define	_M_IPF		M_IPFILTER
#    else
#     define	_M_IPF		M_TEMP
#    endif /* M_IPFILTER */
#   endif /* M_PFIL */
#  endif /* IPFILTER_M_IPFILTER */
#  define	KMALLOC(a, b)	MALLOC((a), b, sizeof(*(a)), _M_IPF, M_NOWAIT)
#  define	KMALLOCS(a, b, c)	MALLOC((a), b, (c), _M_IPF, M_NOWAIT)
#  define	KFREE(x)	FREE((x), _M_IPF)
#  define	KFREES(x,s)	FREE((x), _M_IPF)
#  define	UIOMOVE(a,b,c,d)	uiomove(a,b,d)
#  define	SLEEP(id, n)	tsleep((id), PPAUSE|PCATCH, n, 0)
#  define	WAKEUP(id,x)	wakeup(id+x)
#  define	GETIFP(n, v)	ifunit(n)
# endif /* (Free)BSD */

# if !defined(USE_MUTEXES) && !defined(SPL_NET)
#  if (defined(NetBSD) && (NetBSD <= 1991011) && (NetBSD >= 199407)) || \
      (defined(OpenBSD) && (OpenBSD >= 200006))
#   define	SPL_NET(x)	x = splsoftnet()
#  else
#   define	SPL_IMP(x)	x = splimp()
#   define	SPL_NET(x)	x = splnet()
#  endif /* NetBSD && (NetBSD <= 1991011) && (NetBSD >= 199407) */
#  define	SPL_X(x)	(void) splx(x)
# endif /* !USE_MUTEXES */

# ifndef FREE_MB_T
#  define	FREE_MB_T(m)	m_freem(m)
# endif

# ifndef MTOD
#  define	MTOD(m,t)	mtod(m,t)
# endif

# ifndef COPYIN
#  define	COPYIN(a,b,c)	(bcopy((caddr_t)(a), (caddr_t)(b), (c)), 0)
#  define	COPYOUT(a,b,c)	(bcopy((caddr_t)(a), (caddr_t)(b), (c)), 0)
#  define	BCOPYIN(a,b,c)	(bcopy((caddr_t)(a), (caddr_t)(b), (c)), 0)
#  define	BCOPYOUT(a,b,c)	(bcopy((caddr_t)(a), (caddr_t)(b), (c)), 0)
# endif

# ifndef KMALLOC
#  define	KMALLOC(a,b)	(a) = (b)new_kmem_alloc(sizeof(*(a)), \
							KMEM_NOSLEEP)
#  define	KMALLOCS(a,b,c)	(a) = (b)new_kmem_alloc((c), KMEM_NOSLEEP)
# endif

# ifndef	GET_MINOR
#  define	GET_MINOR(x)	minor(x)
# endif
# define	PANIC(x,y)	if (x) panic y
#endif /* _KERNEL */

#ifndef	IFNAME
# define	IFNAME(x)	((struct ifnet *)x)->if_name
#endif
#ifndef	COPYIFNAME
# define	NEED_FRGETIFNAME
extern	char	*fr_getifname __P((struct ifnet *, char *));
# define	COPYIFNAME(x, b) \
				fr_getifname((struct ifnet *)x, b)
#endif

#ifndef ASSERT
# define	ASSERT(x)
#endif

/*
 * Because the ctype(3) posix definition, if used "safely" in code everywhere,
 * would mean all normal code that walks through strings needed casts.  Yuck.
 */
#define	ISALNUM(x)	isalnum((u_char)(x))
#define	ISALPHA(x)	isalpha((u_char)(x))
#define	ISASCII(x)	isascii((u_char)(x))
#define	ISDIGIT(x)	isdigit((u_char)(x))
#define	ISPRINT(x)	isprint((u_char)(x))
#define	ISSPACE(x)	isspace((u_char)(x))
#define	ISUPPER(x)	isupper((u_char)(x))
#define	ISXDIGIT(x)	isxdigit((u_char)(x))
#define	ISLOWER(x)	islower((u_char)(x))
#define	TOUPPER(x)	toupper((u_char)(x))
#define	TOLOWER(x)	tolower((u_char)(x))

/*
 * If mutexes aren't being used, turn all the mutex functions into null-ops.
 */
#if !defined(USE_MUTEXES)
# define	USE_SPL			1
# undef		RW_DESTROY
# undef		MUTEX_INIT
# undef		MUTEX_NUKE
# undef		MUTEX_DESTROY
# define	MUTEX_ENTER(x)		;
# define	READ_ENTER(x)		;
# define	WRITE_ENTER(x)		;
# define	MUTEX_DOWNGRADE(x)	;
# define	RWLOCK_INIT(x, y)	;
# define	RWLOCK_EXIT(x)		;
# define	RW_DESTROY(x)		;
# define	MUTEX_EXIT(x)		;
# define	MUTEX_INIT(x,y)		;
# define	MUTEX_DESTROY(x)	;
# define	MUTEX_NUKE(x)		;
#endif /* !USE_MUTEXES */
#ifndef	ATOMIC_INC
# define	ATOMIC_INC(x)		(x)++
# define	ATOMIC_DEC(x)		(x)--
#endif

/*
 * If there are no atomic operations for bit sizes defined, define them to all
 * use a generic one that works for all sizes.
 */
#ifndef	ATOMIC_INCL
# define	ATOMIC_INCL		ATOMIC_INC
# define	ATOMIC_INC64		ATOMIC_INC
# define	ATOMIC_INC32		ATOMIC_INC
# define	ATOMIC_INC16		ATOMIC_INC
# define	ATOMIC_DECL		ATOMIC_DEC
# define	ATOMIC_DEC64		ATOMIC_DEC
# define	ATOMIC_DEC32		ATOMIC_DEC
# define	ATOMIC_DEC16		ATOMIC_DEC
#endif

#ifndef HDR_T_PRIVATE
typedef	struct	tcphdr	tcphdr_t;
typedef	struct	udphdr	udphdr_t;
#endif
typedef	struct	icmp	icmphdr_t;
typedef	struct	ip	ip_t;
typedef	struct	ether_header	ether_header_t;
typedef	struct	tcpiphdr	tcpiphdr_t;

#ifndef	FR_GROUPLEN
# define	FR_GROUPLEN	16
#endif

#ifdef offsetof
# undef	offsetof
#endif
#ifndef offsetof
# define offsetof(t,m) (int)((&((t *)0L)->m))
#endif

/*
 * This set of macros has been brought about because on Tru64 it is not
 * possible to easily assign or examine values in a structure that are
 * bit fields.
 */
#ifndef IP_V
# define	IP_V(x)		(x)->ip_v
#endif
#ifndef	IP_V_A
# define	IP_V_A(x,y)	(x)->ip_v = (y)
#endif
#ifndef	IP_HL
# define	IP_HL(x)	(x)->ip_hl
#endif
#ifndef	IP_HL_A
# define	IP_HL_A(x,y)	(x)->ip_hl = (y)
#endif
#ifndef	TCP_X2
# define	TCP_X2(x)	(x)->th_x2
#endif
#ifndef	TCP_X2_A
# define	TCP_X2_A(x,y)	(x)->th_x2 = (y)
#endif
#ifndef	TCP_OFF
# define	TCP_OFF(x)	(x)->th_off
#endif
#ifndef	TCP_OFF_A
# define	TCP_OFF_A(x,y)	(x)->th_off = (y)
#endif
#define	IPMINLEN(i, h)	((i)->ip_len >= (IP_HL(i) * 4 + sizeof(struct h)))


/*
 * XXX - This is one of those *awful* hacks which nobody likes
 */
#ifdef	ultrix
#define	A_A
#else
#define	A_A	&
#endif

#define	TCPF_ALL	(TH_FIN|TH_SYN|TH_RST|TH_PUSH|TH_ACK|TH_URG|\
			 TH_ECN|TH_CWR)

#if (BSD >= 199306) && !defined(m_act)
# define	m_act	m_nextpkt
#endif  

/*
 * Security Options for Intenet Protocol (IPSO) as defined in RFC 1108.
 *
 * Basic Option
 *
 * 00000001   -   (Reserved 4)
 * 00111101   -   Top Secret
 * 01011010   -   Secret
 * 10010110   -   Confidential
 * 01100110   -   (Reserved 3)
 * 11001100   -   (Reserved 2)
 * 10101011   -   Unclassified
 * 11110001   -   (Reserved 1)
 */
#define	IPSO_CLASS_RES4		0x01
#define	IPSO_CLASS_TOPS		0x3d
#define	IPSO_CLASS_SECR		0x5a
#define	IPSO_CLASS_CONF		0x96
#define	IPSO_CLASS_RES3		0x66
#define	IPSO_CLASS_RES2		0xcc
#define	IPSO_CLASS_UNCL		0xab
#define	IPSO_CLASS_RES1		0xf1

#define	IPSO_AUTH_GENSER	0x80
#define	IPSO_AUTH_ESI		0x40
#define	IPSO_AUTH_SCI		0x20
#define	IPSO_AUTH_NSA		0x10
#define	IPSO_AUTH_DOE		0x08
#define	IPSO_AUTH_UN		0x06
#define	IPSO_AUTH_FTE		0x01

/*
 * IP option #defines
 */
#undef	IPOPT_RR
#define	IPOPT_RR	7 
#undef	IPOPT_ZSU
#define	IPOPT_ZSU	10	/* ZSU */
#undef	IPOPT_MTUP
#define	IPOPT_MTUP	11	/* MTUP */
#undef	IPOPT_MTUR
#define	IPOPT_MTUR	12	/* MTUR */
#undef	IPOPT_ENCODE
#define	IPOPT_ENCODE	15	/* ENCODE */
#undef	IPOPT_TS
#define	IPOPT_TS	68
#undef	IPOPT_TR
#define	IPOPT_TR	82	/* TR */
#undef	IPOPT_SECURITY
#define	IPOPT_SECURITY	130
#undef	IPOPT_LSRR
#define	IPOPT_LSRR	131
#undef	IPOPT_E_SEC
#define	IPOPT_E_SEC	133	/* E-SEC */
#undef	IPOPT_CIPSO
#define	IPOPT_CIPSO	134	/* CIPSO */
#undef	IPOPT_SATID
#define	IPOPT_SATID	136
#ifndef	IPOPT_SID
# define	IPOPT_SID	IPOPT_SATID
#endif
#undef	IPOPT_SSRR
#define	IPOPT_SSRR	137
#undef	IPOPT_ADDEXT
#define	IPOPT_ADDEXT	147	/* ADDEXT */
#undef	IPOPT_VISA
#define	IPOPT_VISA	142	/* VISA */
#undef	IPOPT_IMITD
#define	IPOPT_IMITD	144	/* IMITD */
#undef	IPOPT_EIP
#define	IPOPT_EIP	145	/* EIP */
#undef	IPOPT_RTRALRT
#define	IPOPT_RTRALRT	148	/* RTRALRT */
#undef	IPOPT_SDB
#define	IPOPT_SDB	149
#undef	IPOPT_NSAPA
#define	IPOPT_NSAPA	150
#undef	IPOPT_DPS
#define	IPOPT_DPS	151
#undef	IPOPT_UMP
#define	IPOPT_UMP	152
#undef	IPOPT_FINN
#define	IPOPT_FINN	205	/* FINN */

#ifndef TCPOPT_EOL
# define TCPOPT_EOL		0
#endif
#ifndef TCPOPT_NOP
# define TCPOPT_NOP		1
#endif
#ifndef TCPOPT_MAXSEG
# define TCPOPT_MAXSEG		2
#endif
#ifndef TCPOLEN_MAXSEG
# define TCPOLEN_MAXSEG		4
#endif
#ifndef TCPOPT_WINDOW
# define TCPOPT_WINDOW		3
#endif
#ifndef TCPOLEN_WINDOW
# define TCPOLEN_WINDOW		3
#endif
#ifndef TCPOPT_SACK_PERMITTED
# define TCPOPT_SACK_PERMITTED	4
#endif
#ifndef TCPOLEN_SACK_PERMITTED
# define TCPOLEN_SACK_PERMITTED	2
#endif
#ifndef TCPOPT_SACK
# define TCPOPT_SACK		5
#endif
#ifndef TCPOPT_TIMESTAMP
# define TCPOPT_TIMESTAMP	8
#endif

#ifndef	ICMP_MINLEN
# define	ICMP_MINLEN	8
#endif
#ifndef	ICMP_ECHOREPLY
# define	ICMP_ECHOREPLY	0
#endif
#ifndef	ICMP_UNREACH
# define	ICMP_UNREACH	3
#endif
#ifndef	ICMP_UNREACH_NET
# define	ICMP_UNREACH_NET	0
#endif
#ifndef	ICMP_UNREACH_HOST
# define	ICMP_UNREACH_HOST	1
#endif
#ifndef	ICMP_UNREACH_PROTOCOL
# define	ICMP_UNREACH_PROTOCOL	2
#endif
#ifndef	ICMP_UNREACH_PORT
# define	ICMP_UNREACH_PORT	3
#endif
#ifndef	ICMP_UNREACH_NEEDFRAG
# define	ICMP_UNREACH_NEEDFRAG	4
#endif
#ifndef	ICMP_UNREACH_SRCFAIL
# define	ICMP_UNREACH_SRCFAIL	5
#endif
#ifndef	ICMP_UNREACH_NET_UNKNOWN
# define	ICMP_UNREACH_NET_UNKNOWN	6
#endif
#ifndef	ICMP_UNREACH_HOST_UNKNOWN
# define	ICMP_UNREACH_HOST_UNKNOWN	7
#endif
#ifndef	ICMP_UNREACH_ISOLATED
# define	ICMP_UNREACH_ISOLATED	8
#endif
#ifndef	ICMP_UNREACH_NET_PROHIB
# define	ICMP_UNREACH_NET_PROHIB	9
#endif
#ifndef	ICMP_UNREACH_HOST_PROHIB
# define	ICMP_UNREACH_HOST_PROHIB	10
#endif
#ifndef	ICMP_UNREACH_TOSNET
# define	ICMP_UNREACH_TOSNET	11
#endif
#ifndef	ICMP_UNREACH_TOSHOST
# define	ICMP_UNREACH_TOSHOST	12
#endif
#ifndef	ICMP_UNREACH_ADMIN_PROHIBIT
# define	ICMP_UNREACH_ADMIN_PROHIBIT	13
#endif
#ifndef	ICMP_UNREACH_FILTER
# define	ICMP_UNREACH_FILTER	13
#endif
#ifndef	ICMP_UNREACH_HOST_PRECEDENCE
# define	ICMP_UNREACH_HOST_PRECEDENCE	14
#endif
#ifndef	ICMP_UNREACH_PRECEDENCE_CUTOFF
# define	ICMP_UNREACH_PRECEDENCE_CUTOFF	15
#endif
#ifndef	ICMP_SOURCEQUENCH
# define	ICMP_SOURCEQUENCH	4
#endif
#ifndef	ICMP_REDIRECT_NET
# define	ICMP_REDIRECT_NET	0
#endif
#ifndef	ICMP_REDIRECT_HOST
# define	ICMP_REDIRECT_HOST	1
#endif
#ifndef	ICMP_REDIRECT_TOSNET
# define	ICMP_REDIRECT_TOSNET	2
#endif
#ifndef	ICMP_REDIRECT_TOSHOST
# define	ICMP_REDIRECT_TOSHOST	3
#endif
#ifndef	ICMP_ALTHOSTADDR
# define	ICMP_ALTHOSTADDR	6
#endif
#ifndef	ICMP_TIMXCEED
# define	ICMP_TIMXCEED	11
#endif
#ifndef	ICMP_TIMXCEED_INTRANS
# define	ICMP_TIMXCEED_INTRANS	0
#endif
#ifndef	ICMP_TIMXCEED_REASS
# define		ICMP_TIMXCEED_REASS	1
#endif
#ifndef	ICMP_PARAMPROB
# define	ICMP_PARAMPROB	12
#endif
#ifndef	ICMP_PARAMPROB_ERRATPTR
# define	ICMP_PARAMPROB_ERRATPTR	0
#endif
#ifndef	ICMP_PARAMPROB_OPTABSENT
# define	ICMP_PARAMPROB_OPTABSENT	1
#endif
#ifndef	ICMP_PARAMPROB_LENGTH
# define	ICMP_PARAMPROB_LENGTH	2
#endif
#ifndef ICMP_TSTAMP
# define	ICMP_TSTAMP	13
#endif
#ifndef ICMP_TSTAMPREPLY
# define	ICMP_TSTAMPREPLY	14
#endif
#ifndef ICMP_IREQ
# define	ICMP_IREQ	15
#endif
#ifndef ICMP_IREQREPLY
# define	ICMP_IREQREPLY	16
#endif
#ifndef	ICMP_MASKREQ
# define	ICMP_MASKREQ	17
#endif
#ifndef ICMP_MASKREPLY
# define	ICMP_MASKREPLY	18
#endif
#ifndef	ICMP_TRACEROUTE
# define	ICMP_TRACEROUTE	30
#endif
#ifndef	ICMP_DATACONVERR
# define	ICMP_DATACONVERR	31
#endif
#ifndef	ICMP_MOBILE_REDIRECT
# define	ICMP_MOBILE_REDIRECT	32
#endif
#ifndef	ICMP_IPV6_WHEREAREYOU
# define	ICMP_IPV6_WHEREAREYOU	33
#endif
#ifndef	ICMP_IPV6_IAMHERE
# define	ICMP_IPV6_IAMHERE	34
#endif
#ifndef	ICMP_MOBILE_REGREQUEST
# define	ICMP_MOBILE_REGREQUEST	35
#endif
#ifndef	ICMP_MOBILE_REGREPLY
# define	ICMP_MOBILE_REGREPLY	36
#endif
#ifndef	ICMP_SKIP
# define	ICMP_SKIP	39
#endif
#ifndef	ICMP_PHOTURIS
# define	ICMP_PHOTURIS	40
#endif
#ifndef	ICMP_PHOTURIS_UNKNOWN_INDEX
# define	ICMP_PHOTURIS_UNKNOWN_INDEX	1
#endif
#ifndef	ICMP_PHOTURIS_AUTH_FAILED
# define	ICMP_PHOTURIS_AUTH_FAILED	2
#endif
#ifndef	ICMP_PHOTURIS_DECRYPT_FAILED
# define	ICMP_PHOTURIS_DECRYPT_FAILED	3
#endif
#ifndef	IPVERSION
# define	IPVERSION	4
#endif
#ifndef	IPOPT_MINOFF
# define	IPOPT_MINOFF	4
#endif
#ifndef	IPOPT_COPIED
# define	IPOPT_COPIED(x)	((x)&0x80)
#endif
#ifndef	IPOPT_EOL
# define	IPOPT_EOL	0
#endif
#ifndef	IPOPT_NOP
# define	IPOPT_NOP	1
#endif
#ifndef	IP_MF
# define	IP_MF	((u_short)0x2000)
#endif
#ifndef	ETHERTYPE_IP
# define	ETHERTYPE_IP	((u_short)0x0800)
#endif
#ifndef	TH_FIN
# define	TH_FIN	0x01
#endif
#ifndef	TH_SYN
# define	TH_SYN	0x02
#endif
#ifndef	TH_RST
# define	TH_RST	0x04
#endif
#ifndef	TH_PUSH
# define	TH_PUSH	0x08
#endif
#ifndef	TH_ACK
# define	TH_ACK	0x10
#endif
#ifndef	TH_URG
# define	TH_URG	0x20
#endif
#undef	TH_ACKMASK
#define	TH_ACKMASK	(TH_FIN|TH_SYN|TH_RST|TH_ACK)

#ifndef	IPOPT_EOL
# define	IPOPT_EOL	0
#endif
#ifndef	IPOPT_NOP
# define	IPOPT_NOP	1
#endif
#ifndef	IPOPT_RR
# define	IPOPT_RR	7
#endif
#ifndef	IPOPT_TS
# define	IPOPT_TS	68
#endif
#ifndef	IPOPT_SECURITY
# define	IPOPT_SECURITY	130
#endif
#ifndef	IPOPT_LSRR
# define	IPOPT_LSRR	131
#endif
#ifndef	IPOPT_SATID
# define	IPOPT_SATID	136
#endif
#ifndef	IPOPT_SSRR
# define	IPOPT_SSRR	137
#endif
#ifndef	IPOPT_SECUR_UNCLASS
# define	IPOPT_SECUR_UNCLASS	((u_short)0x0000)
#endif
#ifndef	IPOPT_SECUR_CONFID
# define	IPOPT_SECUR_CONFID	((u_short)0xf135)
#endif
#ifndef	IPOPT_SECUR_EFTO
# define	IPOPT_SECUR_EFTO	((u_short)0x789a)
#endif
#ifndef	IPOPT_SECUR_MMMM
# define	IPOPT_SECUR_MMMM	((u_short)0xbc4d)
#endif
#ifndef	IPOPT_SECUR_RESTR
# define	IPOPT_SECUR_RESTR	((u_short)0xaf13)
#endif
#ifndef	IPOPT_SECUR_SECRET
# define	IPOPT_SECUR_SECRET	((u_short)0xd788)
#endif
#ifndef IPOPT_SECUR_TOPSECRET
# define	IPOPT_SECUR_TOPSECRET	((u_short)0x6bc5)
#endif
#ifndef IPOPT_OLEN
# define	IPOPT_OLEN	1
#endif
#ifndef	IPPROTO_HOPOPTS
# define	IPPROTO_HOPOPTS	0
#endif
#ifndef	IPPROTO_ENCAP
# define	IPPROTO_ENCAP	4
#endif
#ifndef	IPPROTO_IPV6
# define	IPPROTO_IPV6	41
#endif
#ifndef	IPPROTO_ROUTING
# define	IPPROTO_ROUTING	43
#endif
#ifndef	IPPROTO_FRAGMENT
# define	IPPROTO_FRAGMENT	44
#endif
#ifndef	IPPROTO_GRE
# define	IPPROTO_GRE	47	/* GRE encaps RFC 1701 */
#endif
#ifndef	IPPROTO_ESP
# define	IPPROTO_ESP	50
#endif
#ifndef	IPPROTO_AH
# define	IPPROTO_AH	51
#endif
#ifndef	IPPROTO_ICMPV6
# define	IPPROTO_ICMPV6	58
#endif
#ifndef	IPPROTO_NONE
# define	IPPROTO_NONE	59
#endif
#ifndef	IPPROTO_DSTOPTS
# define	IPPROTO_DSTOPTS	60
#endif
#ifndef	IPPROTO_FRAGMENT
# define	IPPROTO_FRAGMENT	44
#endif
#ifndef	ICMP_ROUTERADVERT
# define	ICMP_ROUTERADVERT	9
#endif
#ifndef	ICMP_ROUTERSOLICIT
# define	ICMP_ROUTERSOLICIT	10
#endif
#ifndef	ICMP6_DST_UNREACH
# define	ICMP6_DST_UNREACH	1
#endif
#ifndef	ICMP6_PACKET_TOO_BIG
# define	ICMP6_PACKET_TOO_BIG	2
#endif
#ifndef	ICMP6_TIME_EXCEEDED
# define	ICMP6_TIME_EXCEEDED	3
#endif
#ifndef	ICMP6_PARAM_PROB
# define	ICMP6_PARAM_PROB	4
#endif

#ifndef	ICMP6_ECHO_REQUEST
# define	ICMP6_ECHO_REQUEST	128
#endif
#ifndef	ICMP6_ECHO_REPLY
# define	ICMP6_ECHO_REPLY	129
#endif
#ifndef	ICMP6_MEMBERSHIP_QUERY
# define	ICMP6_MEMBERSHIP_QUERY	130
#endif
#ifndef	MLD6_LISTENER_QUERY
# define	MLD6_LISTENER_QUERY	130
#endif
#ifndef	ICMP6_MEMBERSHIP_REPORT
# define	ICMP6_MEMBERSHIP_REPORT	131
#endif
#ifndef	MLD6_LISTENER_REPORT
# define	MLD6_LISTENER_REPORT	131
#endif
#ifndef	ICMP6_MEMBERSHIP_REDUCTION
# define	ICMP6_MEMBERSHIP_REDUCTION	132
#endif
#ifndef	MLD6_LISTENER_DONE
# define	MLD6_LISTENER_DONE	132
#endif
#ifndef	ND_ROUTER_SOLICIT
# define	ND_ROUTER_SOLICIT	133
#endif
#ifndef	ND_ROUTER_ADVERT
# define	ND_ROUTER_ADVERT	134
#endif
#ifndef	ND_NEIGHBOR_SOLICIT
# define	ND_NEIGHBOR_SOLICIT	135
#endif
#ifndef	ND_NEIGHBOR_ADVERT
# define	ND_NEIGHBOR_ADVERT	136
#endif
#ifndef	ND_REDIRECT
# define	ND_REDIRECT	137
#endif
#ifndef	ICMP6_ROUTER_RENUMBERING
# define	ICMP6_ROUTER_RENUMBERING	138
#endif
#ifndef	ICMP6_WRUREQUEST
# define	ICMP6_WRUREQUEST	139
#endif
#ifndef	ICMP6_WRUREPLY
# define	ICMP6_WRUREPLY		140
#endif
#ifndef	ICMP6_FQDN_QUERY
# define	ICMP6_FQDN_QUERY	139
#endif
#ifndef	ICMP6_FQDN_REPLY
# define	ICMP6_FQDN_REPLY	140
#endif
#ifndef	ICMP6_NI_QUERY
# define	ICMP6_NI_QUERY		139
#endif
#ifndef	ICMP6_NI_REPLY
# define	ICMP6_NI_REPLY		140
#endif
#ifndef	MLD6_MTRACE_RESP
# define	MLD6_MTRACE_RESP	200
#endif
#ifndef	MLD6_MTRACE
# define	MLD6_MTRACE		201
#endif
#ifndef	ICMP6_HADISCOV_REQUEST
# define	ICMP6_HADISCOV_REQUEST	202
#endif
#ifndef	ICMP6_HADISCOV_REPLY
# define	ICMP6_HADISCOV_REPLY	203
#endif
#ifndef	ICMP6_MOBILEPREFIX_SOLICIT
# define	ICMP6_MOBILEPREFIX_SOLICIT	204
#endif
#ifndef	ICMP6_MOBILEPREFIX_ADVERT
# define	ICMP6_MOBILEPREFIX_ADVERT	205
#endif
#ifndef	ICMP6_MAXTYPE
# define	ICMP6_MAXTYPE		205
#endif

#ifndef	ICMP6_DST_UNREACH_NOROUTE
# define	ICMP6_DST_UNREACH_NOROUTE	0
#endif
#ifndef	ICMP6_DST_UNREACH_ADMIN
# define	ICMP6_DST_UNREACH_ADMIN		1
#endif
#ifndef	ICMP6_DST_UNREACH_NOTNEIGHBOR
# define	ICMP6_DST_UNREACH_NOTNEIGHBOR	2
#endif
#ifndef	ICMP6_DST_UNREACH_BEYONDSCOPE
# define	ICMP6_DST_UNREACH_BEYONDSCOPE	2
#endif
#ifndef	ICMP6_DST_UNREACH_ADDR
# define	ICMP6_DST_UNREACH_ADDR		3
#endif
#ifndef	ICMP6_DST_UNREACH_NOPORT
# define	ICMP6_DST_UNREACH_NOPORT	4
#endif
#ifndef	ICMP6_TIME_EXCEED_TRANSIT
# define	ICMP6_TIME_EXCEED_TRANSIT	0
#endif
#ifndef	ICMP6_TIME_EXCEED_REASSEMBLY
# define	ICMP6_TIME_EXCEED_REASSEMBLY	1
#endif

#ifndef	ICMP6_NI_SUCCESS
# define	ICMP6_NI_SUCCESS	0
#endif
#ifndef	ICMP6_NI_REFUSED
# define	ICMP6_NI_REFUSED	1
#endif
#ifndef	ICMP6_NI_UNKNOWN
# define	ICMP6_NI_UNKNOWN	2
#endif

#ifndef	ICMP6_ROUTER_RENUMBERING_COMMAND
# define	ICMP6_ROUTER_RENUMBERING_COMMAND	0
#endif
#ifndef	ICMP6_ROUTER_RENUMBERING_RESULT
# define	ICMP6_ROUTER_RENUMBERING_RESULT	1
#endif
#ifndef	ICMP6_ROUTER_RENUMBERING_SEQNUM_RESET
# define	ICMP6_ROUTER_RENUMBERING_SEQNUM_RESET	255
#endif

#ifndef	ICMP6_PARAMPROB_HEADER
# define	ICMP6_PARAMPROB_HEADER	0
#endif
#ifndef	ICMP6_PARAMPROB_NEXTHEADER
# define	ICMP6_PARAMPROB_NEXTHEADER	1
#endif
#ifndef	ICMP6_PARAMPROB_OPTION
# define	ICMP6_PARAMPROB_OPTION	2
#endif

#ifndef	ICMP6_NI_SUBJ_IPV6
# define	ICMP6_NI_SUBJ_IPV6	0
#endif
#ifndef	ICMP6_NI_SUBJ_FQDN
# define	ICMP6_NI_SUBJ_FQDN	1
#endif
#ifndef	ICMP6_NI_SUBJ_IPV4
# define	ICMP6_NI_SUBJ_IPV4	2
#endif

/*
 * ECN is a new addition to TCP - RFC 2481
 */
#ifndef TH_ECN
# define	TH_ECN	0x40
#endif
#ifndef TH_CWR
# define	TH_CWR	0x80
#endif
#define	TH_ECNALL	(TH_ECN|TH_CWR)

/*
 * TCP States
 */
#define IPF_TCPS_CLOSED		0	/* closed */
#define IPF_TCPS_LISTEN		1	/* listening for connection */
#define IPF_TCPS_SYN_SENT	2	/* active, have sent syn */
#define IPF_TCPS_SYN_RECEIVED	3	/* have send and received syn */
#define IPF_TCPS_HALF_ESTAB	4	/* for connections not fully "up" */
/* states < IPF_TCPS_ESTABLISHED are those where connections not established */
#define IPF_TCPS_ESTABLISHED	5	/* established */
#define IPF_TCPS_CLOSE_WAIT	6	/* rcvd fin, waiting for close */
/* states > IPF_TCPS_CLOSE_WAIT are those where user has closed */
#define IPF_TCPS_FIN_WAIT_1	7	/* have closed, sent fin */
#define IPF_TCPS_CLOSING	8	/* closed xchd FIN; await FIN ACK */
#define IPF_TCPS_LAST_ACK	9	/* had fin and close; await FIN ACK */
/* states > IPF_TCPS_CLOSE_WAIT && < IPF_TCPS_FIN_WAIT_2 await ACK of FIN */
#define IPF_TCPS_FIN_WAIT_2	10	/* have closed, fin is acked */
#define IPF_TCPS_TIME_WAIT	11	/* in 2*msl quiet wait after close */
#define IPF_TCP_NSTATES		12

#define	TCP_MSL			120

#undef	ICMP_MAX_UNREACH
#define	ICMP_MAX_UNREACH	14
#undef	ICMP_MAXTYPE
#define	ICMP_MAXTYPE		18

#ifndef	IFNAMSIZ
#define	IFNAMSIZ		16
#endif

#ifndef	LOG_FTP
# define	LOG_FTP		(11<<3)
#endif
#ifndef	LOG_AUTHPRIV
# define	LOG_AUTHPRIV	(10<<3)
#endif
#ifndef	LOG_AUDIT
# define	LOG_AUDIT	(13<<3)
#endif
#ifndef	LOG_NTP
# define	LOG_NTP		(12<<3)
#endif
#ifndef	LOG_SECURITY
# define	LOG_SECURITY	(13<<3)
#endif
#ifndef	LOG_LFMT
# define	LOG_LFMT	(14<<3)
#endif
#ifndef	LOG_CONSOLE
# define	LOG_CONSOLE	(14<<3)
#endif

/*
 * ICMP error replies have an IP header (20 bytes), 8 bytes of ICMP data,
 * another IP header and then 64 bits of data, totalling 56.  Of course,
 * the last 64 bits is dependant on that being available.
 */
#define	ICMPERR_ICMPHLEN	8
#define	ICMPERR_IPICMPHLEN	(20 + 8)
#define	ICMPERR_MINPKTLEN	(20 + 8 + 20)
#define	ICMPERR_MAXPKTLEN	(20 + 8 + 20 + 8)
#define ICMP6ERR_MINPKTLEN	(40 + 8)
#define ICMP6ERR_IPICMPHLEN	(40 + 8 + 40)

#ifndef MIN
# define	MIN(a,b)	(((a)<(b))?(a):(b))
#endif

#ifdef IPF_DEBUG
# define	DPRINT(x)	printf x
#else
# define	DPRINT(x)
#endif

#endif	/* __IP_COMPAT_H__ */
