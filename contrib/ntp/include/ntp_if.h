/*
 * Sockets are not standard.
 * So hide uglyness in include file.
 */
/* was: defined(SYS_CONVEXOS9) */
#if defined(HAVE__SYS_SYNC_QUEUE_H) && defined(HAVE__SYS_SYNC_SEMA_H)
# include "/sys/sync/queue.h"
# include "/sys/sync/sema.h"
#endif

/* was: (defined(SYS_SOLARIS) && !defined(bsd)) || defined(SYS_SUNOS4) */
/* was: defined(SYS_UNIXWARE1) */
#ifdef HAVE_SYS_SOCKIO_H
# include <sys/sockio.h>
#endif

/* was: #if defined(SYS_PTX) || defined(SYS_SINIXM) */
#ifdef HAVE_SYS_STREAM_H
# include <sys/stream.h>
#endif
#ifdef HAVE_SYS_STROPTS_H
# include <sys/stropts.h>
#endif

/* Was: #if defined(SYS_SVR4) */
#if defined(USE_STREAMS_DEVICE_FOR_IF_CONFIG)
# include <netinet/ip.h>
# undef SIOCGIFCONF
# undef SIOCGIFFLAGS
# undef SIOCGIFADDR
# undef SIOCGIFBRDADDR
# undef SIOCGIFNETMASK
# define SIOCGIFCONF	IPIOC_GETIFCONF
# define SIOCGIFFLAGS	IPIOC_GETIFFLAGS
# define SIOCGIFADDR	IPIOC_GETIFADDR
# define SIOCGIFBRDADDR IPIOC_GETIFBRDADDR
# define SIOCGIFNETMASK IPIOC_GETIFNETMASK
#if 0	/* We don't need this now that sys/sockio.h is handled above */
# else /* USE_STREAMS_DEVICE_FOR_IF_CONFIG */
#  include <sys/sockio.h>
#endif
# endif /* USE_STREAMS_DEVICE_FOR_IF_CONFIG */
/* was #endif SYS_SVR4 */


#ifdef HAVE_NET_IF_H
# include <net/if.h>
#endif /* HAVE_NET_IF_H */
