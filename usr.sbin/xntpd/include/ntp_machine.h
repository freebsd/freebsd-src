/* ntp_machine.h,v 3.1 1993/07/06 01:06:49 jbj Exp
 * Collect all machine dependent idiosyncrasies in one place.
 */

#ifndef __ntp_machine
#define __ntp_machine

/*
    Various options.
    They can defined with the DEFS directive in the Config file if they
    are not defined here.
 
WHICH NICE

  HAVE_ATT_NICE     - Use att nice(priority_change)
  HAVE_BSD_NICE     - Use bsd setprioirty(which, who, priority)
  HAVE_NO_NICE      - Don't have (or use) either

KERNEL MUCKING - If you porting to a new system see xntpd/ntp_unixclock.c and
		 util/tickadj.c to see what these do. This is very system 
	         dependent stuff!!!
		 
  HAVE_LIBKVM       - Use libkvm to read kernal memory
  HAVE_READKMEM     - Use read to read kernal memory 
  NOKMEM	    - Don't read kmem
  HAVE_N_UN         - Have u_nn nlist struct.

WHICH SETPGRP TO USE - Not needed if NTP_POSIX_SOURCE is defined since you
		       better of setsid!

  HAVE_ATT_SETPGRP  - setpgrp(void) instead of setpgrp(int, int)


Signaled IO -  Signled IO defines. 

  HAVE_SIGNALED_IO  - Enable signaled io. Assumes you are going to use SIGIO
		      for tty and udp io.
  USE_UDP_SIGPOLL   - Use SIGPOLL on socket io. This assumes that the
		      sockets routines are defined on top of streams.
  USE_TTY_SIGPOLL   - Use SIGPOLL on tty io. This assumes streams.
  UDP_BACKWARDS_SETOWN - SunOS 3.5 or Ultirx 2.0 system.
	      

WHICH TERMINAL MODEL TO USE - I would assume HAVE_TERMIOS if 
		      NTP_POSIX_SOURCE was set but can't.  The 
		      posix tty driver is too restrictive on most systems.
		      It is defined if you define STREAMS.

  We do not put these defines in the ntp_machine.h as some systems
  offer multiple interfaces and refclock configuration likes to
  peek into the configuration defines for tty model restrictions.
  Thus all tty definitions should be in the files in the machines directory.

  HAVE_TERMIOS      - Use POSIX termios.h
  HAVE_SYSV_TTYS    - Use SYSV termio.h
  HAVE_BSD_TTYS     - Use BSD stty.h

THIS MAKES PORTS TO NEW SYSTEMS EASY - You only have to wory about
		                       kernel mucking.

  NTP_POSIX_SOURCE  - Use POSIX functions over bsd functions and att functions.
		      This is NOT the same as _POSIX_SOURCE.
		      It is much weaker!


STEP SLEW OR TWO STEP - The Default is to step.

  SLEWALWAYS  	    - setttimeofday can not be used to set the time of day at 
		      all. 
  STEP_SLEW 	    - setttimeofday can not set the seconds part of time
		      time use setttimeofday to set the seconds part of the
		      time and the slew the seconds.
  FORCE_NTPDATE_STEP - even if SLEWALWAYS is defined, force a step of
		      of the systemtime (via settimeofday()). Only takes
		      affect if STEP_SLEW isn't defined.

WHICH TIMEOFDAY()

  SYSV_TIMEOFDAY    - [sg]ettimeofday(struct timeval *) as opposed to BSD
                      [sg]ettimeofday(struct timeval *, struct timezone *)

INFO ON NEW KERNEL PLL SYS CALLS

  NTP_SYSCALLS_STD  - use the "normal" ones
  NTP_SYSCALL_GET   - SYS_ntp_gettime id
  NTP_SYSCALL_ADJ   - SYS_ntp_adjtime id
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

  USE_PROTOTYPES    - Prototype functions
  DOSYNCTODR        - Resync TODR clock  every hour.
  RETSIGTYPE        - Define signal function type.
  NO_SIGNED_CHAR_DECL - No "signed char" see include/ntp.h
  LOCK_PROCESS      - Have plock.
  UDP_WILDCARD_DELIVERY
		    - these systems deliver broadcast packets to the wildcard
		      port instead to a port bound to the interface bound
		      to the correct broadcast address - are these
		      implementations broken or did the spec change ?

DEFINITIONS FOR SYSTEM && PROCESSOR
  STR_SYSTEM        - value of system variable
  STR_PROCESSOR     - value of processor variable

You could just put the defines on the DEFS line in machines/<os> file.
I don't since there are lots of different types of compilers that a system might
have, some that can do proto typing and others that cannot on the same system.
I get a chance to twiddle some of the configuration parameters at compile
time based on compiler/machine combinations by using this include file.
See convex, aix and sun configurations see how complex it get.
  
Note that it _is_ considered reasonable to add some system-specific defines
to the machine/<os> file if it would be too inconvenient to puzzle them out
in this file.
  
*/
  

/*
 * RS6000 running AIX.
 */
#if defined(SYS_AIX)
#define HAVE_SIGNALED_IO
#ifndef _BSD
#define NTP_STDC
#define NTP_POSIX_SOURCE
/*
 * Keep USE_PROTOTYPES and _NO_PROTO in step.
 */
#if defined(_NO_PROTO) && defined(USE_PROTOTYPES)
#undef USE_PROTOTYPES
#endif
#if !defined(_NO_PROTO) && !defined(USE_PROTOTYPES)
#define USE_PROTOTYPES
#endif
#endif /*_BSD */
#define	HAVE_BSD_NICE
#ifndef STR_SYSTEM
#define STR_SYSTEM "UNIX/AIX"
#endif
#endif /* RS6000 */

/*
 * SunOS 4.X.X
 * Note: posix version has NTP_POSIX_SOURCE and HAVE_SIGNALED_IO
 */
#if defined(SYS_SUNOS4)
#define NTP_NEED_BOPS
#define NO_SIGNED_CHAR_DECL
#define HAVE_LIBKVM 
#define HAVE_MALLOC_H
#define HAVE_BSD_NICE
#define	RETSIGTYPE	void
#define	NTP_SYSCALL_GET	132
#define	NTP_SYSCALL_ADJ	147
#ifndef STR_SYSTEM
#define STR_SYSTEM "UNIX/SunOS 4.x"
#endif
#endif

/*
 * Sinix-M
 */
#if defined(SYS_SINIXM)
#undef HAVE_SIGNALED_IO
#undef USE_TTY_SIGPOLL
#undef USE_UDP_SIGPOLL
#define STREAMS_TLI
#define NO_SIGNED_CHAR_DECL 
#define STEP_SLEW 		/* TWO step */
#define	RETSIGTYPE void
#define NTP_POSIX_SOURCE
#define HAVE_ATT_SETPGRP
#define HAVE_ATT_NICE
#ifndef STR_SYSTEM
#define STR_SYSTEM "UNIX/SINIX-M"
#endif
#endif

/*
 * SunOS 5.1 or SunOS 5.2 or Solaris 2.1 or Solaris 2.2
 */
#if defined(SYS_SOLARIS)
#define HAVE_SIGNALED_IO
#define USE_TTY_SIGPOLL
#define USE_UDP_SIGPOLL
#define NO_SIGNED_CHAR_DECL 
#define STEP_SLEW 		/* TWO step */
#define	RETSIGTYPE void
#define NTP_POSIX_SOURCE
#define HAVE_ATT_SETPGRP
#define HAVE_ATT_NICE
#define UDP_WILDCARD_DELIVERY
#ifndef STR_SYSTEM
#define STR_SYSTEM "UNIX/Solaris 2.x"
#endif
#endif

/*
 * Convex
 */
#if defined(SYS_CONVEXOS10) || defined(SYS_CONVEXOS9)
#define HAVE_SIGNALED_IO
#define HAVE_N_UN
#define HAVE_READKMEM 
#define HAVE_BSD_NICE
#if defined(convex) 
#define	RETSIGTYPE int
#define NO_SIGNED_CHAR_DECL
#else
#if defined(__stdc__) && !defined(USE_PROTOTYPES)
#define USE_PROTOTYPES
#endif
#if !defined(__stdc__) && defined(USE_PROTOTYPES)
#undef USE_PROTOTYPES
#endif
#define NTP_POSIX_SOURCE
#define HAVE_ATT_SETPGRP
#endif
#ifndef STR_SYSTEM
#define STR_SYSTEM "UNIX/ConvexOS"
#endif
#endif

/*
 * IRIX 4.X and IRIX 5.x
 */
#if defined(SYS_IRIX4)||defined(SYS_IRIX5)
#define HAVE_SIGNALED_IO
#define USE_TTY_SIGPOLL
#define ADJTIME_IS_ACCURATE
#define LOCK_PROCESS
#define USE_PROTOTYPES
#define HAVE_ATT_SETPGRP
#define HAVE_BSD_NICE
#define NTP_POSIX_SOURCE
#ifndef STR_SYSTEM
#define STR_SYSTEM "UNIX/IRIX"
#endif
#endif

/*
 * Ultrix
 * Note: posix version has NTP_POSIX_SOURCE and HAVE_SIGNALED_IO
 */
#if defined(SYS_ULTRIX)
#define S_CHAR_DEFINED
#define HAVE_READKMEM 
#define HAVE_BSD_NICE
#define	RETSIGTYPE	void
#define	NTP_SYSCALLS_STD
#ifndef STR_SYSTEM
#define STR_SYSTEM "UNIX/Ultrix"
#endif
#endif

/*
 * AUX
 */
#if defined(SYS_AUX2) || defined(SYS_AUX3)
#define NO_SIGNED_CHAR_DECL
#define HAVE_READKMEM
#define HAVE_ATT_NICE
#define LOCK_PROCESS
#define NTP_POSIX_SOURCE
/*
 * This requires that _POSIX_SOURCE be forced on the
 * compiler command flag. We can't do it here since this
 * file is included _after_ the system header files and we
 * need to let _them_ know we're POSIX. We do this in
 * compilers/aux3.gcc...
 */
#define SLEWALWAYS
#define FORCE_NTPDATE_STEP
#define	RETSIGTYPE	void
#define HAVE_ATT_SETPGRP
#define LOG_NTP LOG_LOCAL1
#define HAVE_SIGNALED_IO
#define NTP_NEED_BOPS
#ifndef STR_SYSTEM
#define STR_SYSTEM "UNIX/AUX"
#endif
#endif

/*
 * Next
 */
#if defined(SYS_NEXT)
#define RETSIGTYPE void
#define DOSYNCTODR
#define HAVE_READKMEM
#define HAVE_BSD_NICE
#define HAVE_N_UN
#undef NTP_POSIX_SOURCE
#ifndef STR_SYSTEM
#define STR_SYSTEM "UNIX/Next"
#endif
#endif

/*
 * HPUX
 */
#if defined(SYS_HPUX)
#define NTP_POSIX_SOURCE
#define HAVE_SIGNALED_IO
#define getdtablesize() sysconf(_SC_OPEN_MAX)
#define setlinebuf(f) setvbuf(f, NULL, _IOLBF, 0)
#define NO_SIGNED_CHAR_DECL
#define LOCK_PROCESS
#define RETSIGTYPE      void
#if (SYS_HPUX < 9)
#define	HAVE_NO_NICE	/* HPUX uses rtprio instead */
#else
#define HAVE_BSD_NICE	/* new at 9.X */
#endif
#if (SYS_HPUX < 10)
#define NOKMEM
#else
#define HAVE_READKMEM
#endif
#ifndef STR_SYSTEM
#define STR_SYSTEM "UNIX/HPUX"
#endif
#endif

/*
 * bsdi
 */
#if defined(SYS_BSDI)
#define HAVE_SIGNALED_IO
#define HAVE_LIBKVM
#define NTP_POSIX_SOURCE
#define HAVE_BSD_NICE
#ifndef STR_SYSTEM
#define STR_SYSTEM "UNIX/BSDI"
#endif
#endif

/*
 * Linux
 */
#if defined(SYS_LINUX)
#undef HAVE_SIGNALED_IO
#define	RETSIGTYPE void
#define NTP_POSIX_SOURCE
#define ADJTIME_IS_ACCURATE
#define HAVE_SYS_TIMEX_H
/* hope there will be a standard interface 
 * along with a standard name one day ! */
#define ntp_adjtime __adjtimex
#define HAVE_BSD_NICE
#ifndef STR_SYSTEM
#define STR_SYSTEM "UNIX/Linux"
#endif
#endif

/*
 * 386BSD and any variants 8-) - should really have only ONE define
 * for this bunch.
 */
#if defined(SYS_386BSD) || defined(SYS_FREEBSD) || defined(SYS_NETBSD)
#define HAVE_SIGNALED_IO
#define HAVE_READKMEM
#define NTP_POSIX_SOURCE
#define HAVE_BSD_NICE
#ifndef STR_SYSTEM
#define STR_SYSTEM "UNIX/*BSD"
#endif
#endif
#ifdef SYS_FREEBSD
#define HAVE_TERMIOS
#define HAVE_UNAME
#define NTP_SYSCALLS_LIBC
#endif

/*
 * DEC AXP OSF/1
 */
#if defined(SYS_DECOSF1)
#define HAVE_SIGNALED_IO
#define HAVE_READKMEM
#define NTP_POSIX_SOURCE
#define	NTP_SYSCALLS_STD
#define	HAVE_BSD_NICE
#ifndef STR_SYSTEM
#define STR_SYSTEM "UNIX/DECOSF1"
#endif
#endif

/*
 * I386
 * XXX - what OS?
 */
#if defined(SYS_I386)
#define HAVE_READKMEM 
#define S_CHAR_DEFINED 
#define HAVE_BSD_NICE
#ifndef STR_SYSTEM
#define STR_SYSTEM "UNIX/I386"
#endif
#endif

/*
 * Mips
 */
#if defined(SYS_MIPS)
#define NOKMEM 
#define HAVE_BSD_NICE
#ifndef STR_SYSTEM
#define STR_SYSTEM "UNIX/Mips"
#endif
#endif

/*
 * SEQUENT
 */
#if defined(SYS_SEQUENT)
#define HAVE_BSD_NICE
#ifndef STR_SYSTEM
#define STR_SYSTEM "UNIX/Sequent Dynix 3"
#endif
#endif

/*
 * PTX
 */
#if defined(SYS_PTX)
#define NO_SIGNED_CHAR_DECL
#define STREAMS_TLI
#define HAVE_ATT_SETPGRP 
#define HAVE_SIGNALED_IO
#define USE_UDP_SIGPOLL
#define USE_TTY_SIGPOLL
#undef ADJTIME_IS_ACCURATE	/* not checked yet */
#define LOCK_PROCESS
#define HAVE_ATT_SETPGRP
#define HAVE_ATT_NICE
#define STEP_SLEW 		/* TWO step */
#define SYSV_GETTIMEOFDAY
#define HAVE_READKMEM
#define UDP_WILDCARD_DELIVERY
#define NTP_POSIX_SOURCE
#define memmove(x, y, z) memcpy(x, y, z)
struct timezone { int __0; };   /* unused placebo */
/*
 * no comment !@!
 */
typedef unsigned int u_int;
#ifndef	_NETINET_IN_SYSTM_INCLUDED	/* i am about to comment... */
typedef unsigned char u_char;
typedef unsigned short u_short;
typedef unsigned long u_long;
#endif
#ifndef STR_SYSTEM
#define STR_SYSTEM "UNIX/Sequent PTX"
#endif
#endif


/*
 * Sony NEWS
 */
#if defined(SYS_SONY)
#define NO_SIGNED_CHAR_DECL 
#define HAVE_READKMEM
#define HAVE_BSD_NICE
#ifndef STR_SYSTEM
#define STR_SYSTEM "UNIX/Sony"
#endif
#endif

/*
 * VAX
 * XXX - VMS?
 */
#if defined(SYS_VAX)
#define NO_SIGNED_CHAR_DECL 
#define HAVE_READKMEM 
#define HAVE_BSD_NICE
#ifndef STR_SYSTEM
#define STR_SYSTEM "UNIX/VAX"
#endif
#endif

/* 
 * UNIX V.4 on and NCR 3000
 */
#if defined(SYS_SVR4)
#define HAVE_ATT_SETPGRP
#define USE_PROTOTYPES
#define NTP_POSIX_SOURCE
#define HAVE_ATT_NICE
#define HAVE_READKMEM 
#define USE_TTY_SIGPOLL
#define USE_UDP_SIGPOLL
#define STREAM
#define STEP_SLEW 		/* TWO step */
#define LOCK_PROCESS
#define SYSV_TIMEOFDAY
#define SIZE_RETURNED_IN_BUFFER
#ifndef STR_SYSTEM
#define STR_SYSTEM "UNIX/SysVR4"
#endif
#endif

/*
 * (Univel/Novell) Unixware1 SVR4 on intel x86 processor
 */
#if defined(SYS_UNIXWARE1)
/* #define _POSIX_SOURCE */
#undef HAVE_ATT_SETPGRP
#define USE_PROTOTYPES
#define NTP_POSIX_SOURCE
#define HAVE_ATT_NICE
#define HAVE_READKMEM 
#define USE_TTY_SIGPOLL
#define USE_UDP_SIGPOLL
#define UDP_WILDCARD_DELIVERY
#undef HAVE_SIGNALED_IO
#define STREAM
#define STREAMS
#ifndef STREAMS_TLI
/*#define STREAMS_TLI*/
#endif
/* #define USE_STREAMS_DEVICE_FOR_IF_CONFIG */
#undef STEP_SLEW 		/* TWO step */
#define LOCK_PROCESS
#define NO_SIGNED_CHAR_DECL 
#undef SYSV_TIMEOFDAY
#define SIZE_RETURNED_IN_BUFFER
#define	RETSIGTYPE void
#include <sys/sockio.h>
#include <sys/types.h>
#include <netinet/in_systm.h>
#ifndef STR_SYSTEM
#define STR_SYSTEM "UNIX/Unixware1"
#endif
#endif

/*
 * DomainOS
 */
#if defined(SYS_DOMAINOS)
#define HAVE_BSD_NICE
#define NOKMEM
#define HAVE_SIGNALED_IO
#define NTP_SYSCALLS_STD
#define USE_PROTOTYPES
#define UDP_WILDCARD_DELIVERY
#ifndef STR_SYSTEM
#define STR_SYSTEM "UNIX/DOMAINOS"
#endif
#endif

#ifdef STREAM			/* STREAM implies TERMIOS */
#ifndef HAVE_TERMIOS
#define HAVE_TERMIOS
#endif
#endif

#ifndef	RETSIGTYPE
#if defined(NTP_POSIX_SOURCE)
#define	RETSIGTYPE	void
#else
#define	RETSIGTYPE	int
#endif
#endif

#ifdef	NTP_SYSCALLS_STD
#ifndef	NTP_SYSCALL_GET
#define	NTP_SYSCALL_GET	235
#endif
#ifndef	NTP_SYSCALL_ADJ
#define	NTP_SYSCALL_ADJ	236
#endif
#endif	/* NTP_SYSCALLS_STD */

#if	!defined(HAVE_ATT_NICE) \
	&& !defined(HAVE_BSD_NICE) \
	&& !defined(HAVE_NO_NICE)
	ERROR You_must_define_one_of_the_HAVE_xx_NICE_defines
#endif

/*
 * use only one tty model - no use in initialising
 * a tty in three ways
 * HAVE_TERMIOS is preferred over HAVE_SYSV_TTYS over HAVE_BSD_TTYS
 */
#ifdef HAVE_TERMIOS
#undef HAVE_BSD_TTYS
#undef HAVE_SYSV_TTYS
#endif

#ifdef HAVE_SYSV_TTYS
#undef HAVE_BSD_TTYS
#endif

#if	!defined(HAVE_SYSV_TTYS) \
	&& !defined(HAVE_BSD_TTYS) \
	&& !defined(HAVE_TERMIOS)
   	ERROR no_tty_type_defined
#endif


#if !defined(XNTP_BIG_ENDIAN) && !defined(XNTP_LITTLE_ENDIAN)

# if defined(XNTP_AUTO_ENDIAN)
#  include <netinet/in.h>

#  if BYTE_ORDER == BIG_ENDIAN
#   define XNTP_BIG_ENDIAN
#  endif
#  if BYTE_ORDER == LITTLE_ENDIAN
#   define XNTP_LITTLE_ENDIAN
#  endif

# else	/* AUTO */

#  ifdef	WORDS_BIGENDIAN
#   define	XNTP_BIG_ENDIAN	1
#  else
#   define	XNTP_LITTLE_ENDIAN	1
#  endif

# endif	/* AUTO */

#endif	/* !BIG && !LITTLE */

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
