/*
 * Collect all machine dependent idiosyncrasies in one place.
 */

#ifndef __ntp_machine
#define __ntp_machine

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#ifdef TIME_WITH_SYS_TIME
# include <sys/time.h>
# include <time.h>
#else
# ifdef HAVE_SYS_TIME_H
#  include <sys/time.h>
# else
#  include <time.h>
# endif
#endif

#include "ntp_proto.h"

/*

			 HEY!  CHECK THIS OUT!

  The first half of this file is obsolete, and is only there to help
  reconcile "what went before" with "current behavior".

  The per-system SYS_* #defins ARE NO LONGER USED, with the temporary
  exception of SYS_WINNT.

  If you find a hunk of code that is bracketed by a SYS_* macro and you
  *know* that it is still needed, please let us know.  In many cases the
  code fragment is now handled somewhere else by autoconf choices.

*/

/*

INFO ON NEW KERNEL PLL SYS CALLS

  NTP_SYSCALLS_STD	- use the "normal" ones
  NTP_SYSCALL_GET	- SYS_ntp_gettime id
  NTP_SYSCALL_ADJ	- SYS_ntp_adjtime id
  NTP_SYSCALLS_LIBC - ntp_adjtime() and ntp_gettime() are in libc.

HOW TO GET IP INTERFACE INFORMATION

  Some UNIX V.4 machines implement a sockets library on top of
  streams. For these systems, you must use send the SIOCGIFCONF down
  the stream in an I_STR ioctl. This ususally also implies
  USE_STREAMS_DEVICE FOR IF_CONFIG. Dell UNIX is a notable exception.

  STREAMS_TLI - use ioctl(I_STR) to implement ioctl(SIOCGIFCONF)

WHAT DOES IOCTL(SIOCGIFCONF) RETURN IN THE BUFFER

  UNIX V.4 machines implement a sockets library on top of streams.
  When requesting the IP interface configuration with an ioctl(2) calll,
  an array of ifreq structures are placed in the provided buffer.  Some
  implementations also place the length of the buffer information in
  the first integer position of the buffer.

  SIZE_RETURNED_IN_BUFFER - size integer is in the buffer

WILL IOCTL(SIOCGIFCONF) WORK ON A SOCKET

  Some UNIX V.4 machines do not appear to support ioctl() requests for the
  IP interface configuration on a socket.  They appear to require the use
  of the streams device instead.

  USE_STREAMS_DEVICE_FOR_IF_CONFIG - use the /dev/ip device for configuration

MISC

  HAVE_PROTOTYPES	- Prototype functions
  DOSYNCTODR		- Resync TODR clock  every hour.
  RETSIGTYPE		- Define signal function type.
  NO_SIGNED_CHAR_DECL - No "signed char" see include/ntp.h
  LOCK_PROCESS		- Have plock.
  UDP_WILDCARD_DELIVERY
			- these systems deliver broadcast packets to the wildcard
			  port instead to a port bound to the interface bound
			  to the correct broadcast address - are these
			  implementations broken or did the spec change ?
*/

/*
 * Set up for prototyping (duplicated from ntp_types.h)
 */
#ifndef P
#if defined(__STDC__) || defined(HAVE_PROTOTYPES)
#define P(x)    x
#else /* not __STDC__ and not HAVE_PROTOTYPES */
#define P(x)    ()
#endif /* not __STDC__ and not HAVE_PROTOTYPES */
#endif /* P */

#if 0

/*
 * IRIX 4.X and IRIX 5.x
 */
#if defined(SYS_IRIX4)||defined(SYS_IRIX5)
# define ADJTIME_IS_ACCURATE
# define LOCK_PROCESS
#endif

/*
 * Ultrix
 * Note: posix version has NTP_POSIX_SOURCE and HAVE_SIGNALED_IO
 */
#if defined(SYS_ULTRIX)
# define S_CHAR_DEFINED
# define NTP_SYSCALLS_STD
# define HAVE_MODEM_CONTROL
#endif

/*
 * AUX
 */
#if defined(SYS_AUX2) || defined(SYS_AUX3)
# define NO_SIGNED_CHAR_DECL
# define LOCK_PROCESS
# define NTP_POSIX_SOURCE
/*
 * This requires that _POSIX_SOURCE be forced on the
 * compiler command flag. We can't do it here since this
 * file is included _after_ the system header files and we
 * need to let _them_ know we're POSIX. We do this in
 * compilers/aux3.gcc...
 */
# define LOG_NTP LOG_LOCAL1
#endif

/*
 * HPUX
 */
#if defined(SYS_HPUX)
# define getdtablesize() sysconf(_SC_OPEN_MAX)
# define setlinebuf(f) setvbuf(f, NULL, _IOLBF, 0)
# define NO_SIGNED_CHAR_DECL
# define LOCK_PROCESS
#endif

/*
 * BSD/OS 2.0 and above
 */
#if defined(SYS_BSDI)
# define USE_FSETOWNCTTY	/* this funny system demands a CTTY for FSETOWN */
#endif

/*
 * FreeBSD 2.0 and above
 */
#ifdef SYS_FREEBSD
# define KERNEL_PLL
#endif

/*
 * Linux
 */
#if defined(SYS_LINUX)
# define ntp_adjtime __adjtimex
#endif

/*
 * PTX
 */
#if defined(SYS_PTX)
# define LOCK_PROCESS
struct timezone { int __0; };	/* unused placebo */
/*
 * no comment !@!
 */
typedef unsigned int u_int;
# ifndef	_NETINET_IN_SYSTM_INCLUDED	/* i am about to comment... */
typedef unsigned char u_char;
typedef unsigned short u_short;
typedef unsigned long u_long;
# endif
#endif

/*
 * UNIX V.4 on and NCR 3000
 */
#if defined(SYS_SVR4)
# define STREAM
# define LOCK_PROCESS
# define SIZE_RETURNED_IN_BUFFER
#endif

/*
 * (Univel/Novell) Unixware1 SVR4 on intel x86 processor
 */
#if defined(SYS_UNIXWARE1)
/* #define _POSIX_SOURCE */
# define STREAM
# define STREAMS
# undef STEP_SLEW		/* TWO step */
# define LOCK_PROCESS
# define SIZE_RETURNED_IN_BUFFER
# include <sys/sockio.h>
# include <sys/types.h>
# include <netinet/in_systm.h>
#endif

/*
 * DomainOS
 */
#if defined(SYS_DOMAINOS)
# define NTP_SYSCALLS_STD
/* older versions of domain/os don't have class D */
# ifndef IN_CLASSD
#  define IN_CLASSD(i)		(((long)(i) & 0xf0000000) == 0xe0000000)
#  define IN_CLASSD_NET 	0xf0000000
#  define IN_CLASSD_NSHIFT	28
#  define IN_CLASSD_HOST	0xfffffff
#  define IN_MULTICAST(i)	IN_CLASSD(i)
# endif
#endif

/*
 * Fujitsu UXP/V
 */
#if defined(SYS_UXPV)
# define LOCK_PROCESS
# define SIZE_RETURNED_IN_BUFFER
#endif


#endif /* 0 */

/*
 * Windows NT
 */
#if defined(SYS_WINNT)
# if !defined(HAVE_CONFIG_H)  || !defined(__config)
    error "NT requires config.h to be included"
# endif /* HAVE_CONFIG_H) */

# define ifreq _INTERFACE_INFO
# define ifr_flags iiFlags
# define ifr_addr iiAddress.AddressIn
# define ifr_broadaddr iiBroadcastAddress.AddressIn
# define ifr_mask iiNetmask.AddressIn

# define isascii __isascii
# define isatty _isatty
# define mktemp _mktemp
# if 0
#  define getpid GetCurrentProcessId
# endif
# include <windows.h>
# include <ws2tcpip.h>
# undef interface
 typedef char *caddr_t;
#endif /* SYS_WINNT */

int ntp_set_tod P((struct timeval *tvp, void *tzp));

#if defined (SYS_CYGWIN32)
#include <windows.h>
#define __int64 long long
#endif

/*casey Tue May 27 15:45:25 SAT 1997*/
#ifdef SYS_VXWORKS

/* casey's new defines */
#define NO_MAIN_ALLOWED 	1
#define NO_NETDB			1
#define NO_RENAME			1

/* in vxWorks we use FIONBIO, but the others are defined for old systems, so
 * all hell breaks loose if we leave them defined we define USE_FIONBIO to
 * undefine O_NONBLOCK FNDELAY O_NDELAY where necessary.
 */
#define USE_FIONBIO 		1
/* end my new defines */

#define TIMEOFDAY		0x0 	/* system wide realtime clock */
#define HAVE_GETCLOCK		1	/* configure does not set this ... */
#define HAVE_NO_NICE		1	/* configure does not set this ... */
#define HAVE_RANDOM		1	/* configure does not set this ...  */
#define HAVE_SRANDOM		1	/* configure does not set this ... */

#define NODETACH		1

/* vxWorks specific additions to take care of its
 * unix (non)complicance
 */

#include "vxWorks.h"
#include "ioLib.h"
#include "taskLib.h"
#include "time.h"

extern int sysClkRateGet P(());

/* usrtime.h
 * Bob Herlien's excellent time code find it at:
 * ftp://ftp.atd.ucar.edu/pub/vxworks/vx/usrTime.shar
 * I would recommend this instead of clock_[g|s]ettime() plus you get
 * adjtime() too ... casey
 */
/*
extern int	  gettimeofday P(( struct timeval *tp, struct timezone *tzp ));
extern int	  settimeofday P((struct timeval *, struct timezone *));
extern int	  adjtime P(( struct timeval *delta, struct timeval *olddelta ));
 */

/* in  machines.c */
extern void sleep P((int seconds));
extern void alarm P((int seconds));
/* machines.c */


/*		this is really this 	*/
#define getpid		taskIdSelf
#define getclock	clock_gettime
#define fcntl		ioctl
#define _getch		getchar
#define random 		rand
#define srandom		srand

/* define this away for vxWorks */
#define openlog(x,y)
/* use local defines for these */
#undef min
#undef max

#endif /* SYS_VXWORKS */

#ifdef NO_NETDB
/* These structures are needed for gethostbyname() etc... */
/* structures used by netdb.h */
struct	hostent {
	char	*h_name;				/* official name of host */
	char	**h_aliases;			/* alias list */
	int h_addrtype; 				/* host address type */
	int h_length;					/* length of address */
	char	**h_addr_list;			/* list of addresses from name server */
#define 	h_addr h_addr_list[0]	/* address, for backward compatibility */
};

struct	servent {
	char	*s_name;				/* official service name */
	char	**s_aliases;			/* alias list */
	int s_port; 					/* port # */
	char	*s_proto;				/* protocol to use */
};
extern int h_errno;

#define TRY_AGAIN	2

struct hostent *gethostbyname P((char * netnum));
struct hostent *gethostbyaddr P((char * netnum, int size, int addr_type));
/* type is the protocol */
struct servent *getservbyname P((char *name, char *type));
#endif	/* NO_NETDB */

#ifdef NO_MAIN_ALLOWED
/* we have no main routines so lets make a plan */
#define CALL(callname, progname, callmain) \
	extern int callmain (int,char**); \
	void callname (a0,a1,a2,a3,a4,a5,a6,a7,a8,a9,a10) \
		char *a0;  \
		char *a1;  \
		char *a2;  \
		char *a3;  \
		char *a4;  \
		char *a5;  \
		char *a6;  \
		char *a7;  \
		char *a8;  \
		char *a9;  \
		char *a10; \
	{ \
	  char *x[11]; \
	  int argc; \
	  char *argv[] = {progname,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL}; \
	  int i; \
	  for (i=0;i<11;i++) \
	   x[i] = NULL; \
	  x[0] = a0; \
	  x[1] = a1; \
	  x[2] = a2; \
	  x[3] = a3; \
	  x[4] = a4; \
	  x[5] = a5; \
	  x[6] = a6; \
	  x[7] = a7; \
	  x[8] = a8; \
	  x[9] = a9; \
	  x[10] = a10; \
	  argc=1; \
	  for (i=0; i<11;i++) \
		if (x[i]) \
		{ \
		  argv[argc++] = x[i];	\
		} \
	 callmain(argc,argv);  \
	}
#endif /* NO_MAIN_ALLOWED */
/*casey Tue May 27 15:45:25 SAT 1997*/

/*
 * Here's where autoconfig starts to take over
 */
#ifdef HAVE_SYS_STROPTS_H
# ifdef HAVE_SYS_STREAM_H
#  define STREAM
# endif
#endif

#ifndef RETSIGTYPE
# if defined(NTP_POSIX_SOURCE)
#  define	RETSIGTYPE	void
# else
#  define	RETSIGTYPE	int
# endif
#endif

#ifdef	NTP_SYSCALLS_STD
# ifndef	NTP_SYSCALL_GET
#  define	NTP_SYSCALL_GET 235
# endif
# ifndef	NTP_SYSCALL_ADJ
#  define	NTP_SYSCALL_ADJ 236
# endif
#endif	/* NTP_SYSCALLS_STD */

#ifdef MPE
# include <sys/types.h>
# include <netinet/in.h>
# include <stdio.h>
# include <time.h>

/* missing functions that are easily renamed */

# define _getch getchar

/* special functions that require MPE-specific wrappers */

# define bind	__ntp_mpe_bind
# define fcntl	__ntp_mpe_fcntl

/* standard macros missing from MPE include files */

# define IN_CLASSD(i)	((((long)(i))&0xf0000000)==0xe0000000)
# define IN_MULTICAST IN_CLASSD
# define ITIMER_REAL 0
# define MAXHOSTNAMELEN 64

/* standard structures missing from MPE include files */

struct itimerval { 
        struct timeval it_interval;    /* timer interval */
        struct timeval it_value;       /* current value */
};

/* various declarations to make gcc stop complaining */

extern int __filbuf(FILE *);
extern int __flsbuf(int, FILE *);
extern int gethostname(char *, int);
extern unsigned long inet_addr(char *);
extern char *strdup(const char *);

/* miscellaneous NTP macros */

# define HAVE_NO_NICE
#endif /* MPE */

#ifdef HAVE_RTPRIO
# define HAVE_NO_NICE
#else
# ifdef HAVE_SETPRIORITY
#  define HAVE_BSD_NICE
# else
#  ifdef HAVE_NICE
#	define HAVE_ATT_NICE
#  endif
# endif
#endif

#if !defined(HAVE_ATT_NICE) \
	&& !defined(HAVE_BSD_NICE) \
	&& !defined(HAVE_NO_NICE) \
	&& !defined(SYS_WINNT)
#include "ERROR: You must define one of the HAVE_xx_NICE defines!"
#endif

/*
 * use only one tty model - no use in initialising
 * a tty in three ways
 * HAVE_TERMIOS is preferred over HAVE_SYSV_TTYS over HAVE_BSD_TTYS
 */

#ifdef HAVE_TERMIOS_H
# define HAVE_TERMIOS
#else
# ifdef HAVE_TERMIO_H
#  define HAVE_SYSV_TTYS
# else
#  ifdef HAVE_SGTTY_H
#	define HAVE_BSD_TTYS
#  endif
# endif
#endif

#ifdef HAVE_TERMIOS
# undef HAVE_BSD_TTYS
# undef HAVE_SYSV_TTYS
#endif

#ifdef HAVE_SYSV_TTYS
# undef HAVE_BSD_TTYS
#endif

#if !defined(SYS_WINNT) && !defined(VMS) && !defined(SYS_VXWORKS)
# if	!defined(HAVE_SYSV_TTYS) \
	&& !defined(HAVE_BSD_TTYS) \
	&& !defined(HAVE_TERMIOS)
#include "ERROR: no tty type defined!"
# endif
#endif /* SYS_WINNT || VMS	|| SYS_VXWORKS*/

#ifdef	WORDS_BIGENDIAN
# define	XNTP_BIG_ENDIAN 1
#else
# define	XNTP_LITTLE_ENDIAN	1
#endif

/*
 * Byte order woes.  The DES code is sensitive to byte order.  This
 * used to be resolved by calling ntohl() and htonl() to swap things
 * around, but this turned out to be quite costly on Vaxes where those
 * things are actual functions.  The code now straightens out byte
 * order troubles on its own, with no performance penalty for little
 * end first machines, but at great expense to cleanliness.
 */
#if !defined(XNTP_BIG_ENDIAN) && !defined(XNTP_LITTLE_ENDIAN)
	/*
	 * Pick one or the other.
	 */
	BYTE_ORDER_NOT_DEFINED_FOR_AUTHENTICATION
#endif

#if defined(XNTP_BIG_ENDIAN) && defined(XNTP_LITTLE_ENDIAN)
	/*
	 * Pick one or the other.
	 */
	BYTE_ORDER_NOT_DEFINED_FOR_AUTHENTICATION
#endif

#endif /* __ntp_machine */
