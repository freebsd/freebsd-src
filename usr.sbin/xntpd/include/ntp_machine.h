/* ntp_compat.h,v 3.1 1993/07/06 01:06:49 jbj Exp
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
	      

WHICH TERMINAL MODEL TO USE - I would assume HAVE_POSIX_TTYS if 
		      NTP_POSIX_SOURCE was set but cann't.  The 
		      posix tty driver is too restrictive on most systems.
		      It defined if you define STREAMS.

  HAVE_SYSV_TTYS    - Use SYSV termio.h
  HAVE_BSD_TTYS     - Use BSD stty.h
  HAVE_POSIX_TTYS   - "struct termios" has c_line defined

THIS MAKES PORTS TO NEW SYSTEMS EASY - You only have to wory about
		                       kernal mucking.

  NTP_POSIX_SOURCE  - Use POSIX functions over bsd functions and att functions.
		      This is NOT the same as _POSIX_SOURCE.
		      It is much weeker!


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

WHAT DOES IOCTL(SIOCGIFCONF) RETURN IN THE BUFFER

  UNIX V.4 machines implement a sockets library on top of streams.
  When requesting the IP interface configuration with an ioctl(2) calll,
  an arrat of ifreq structures are placed in the provided buffer.  Some
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
  LOCK_PROCESS      -  Have plock.
  UDP_WILDCARD_DELIVERY
		    - these systems deliver broadcast pakets to the wildcard
		      port instead to a port bound to the interface bound
		      to the correct broadcast address - are these
		      implementations broken or did the spec change ?

  HAVE_UNISTD_H     - Maybe should be part of NTP_POSIX_SOURCE ?

You could just put the defines on the DEFS line in machines/<os> file.
I don't since there are lost of different types compiler that a systemm might
have, some that can do proto typing and others that cannot on the saem system.
I get a chanse to twiddle some of the configuration paramasters at compile
time based on compler/machine combinatsions by using this include file.
See convex, aix and sun configurations see how complex it get.

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
#if  defined(_NO_PROTO)&&defined(USE_PROTOTYPES)
#undef USE_PROTOTYPES
#endif
#if  !defined(_NO_PROTO)&&!defined(USE_PROTOTYPES)
#define USE_PROTOTYPES
#endif
#endif /*_BSD */
#define	HAVE_BSD_NICE
#endif /* RS6000 */

/*
 * SunOS 4.X.X
 * Note: posix version has NTP_POSIX_SOURCE and HAVE_SIGNALED_IO
 */
#if defined(SYS_SUNOS4)
#define NO_SIGNED_CHAR_DECL
#define HAVE_LIBKVM 
#define HAVE_MALLOC_H
#define HAVE_BSD_NICE
#define	RETSIGTYPE	void
#define	NTP_SYSCALL_GET	132
#define	NTP_SYSCALL_ADJ	147
#endif

/*
 * Sinix-M
 */
#if defined(SYS_SINIXM)
#undef HAVE_SIGNALED_IO
#undef USE_TTY_SIGPOLL
#undef USE_UDP_SIGPOLL
#define NO_SIGNED_CHAR_DECL 
#define STEP_SLEW 		/* TWO step */
#define	RETSIGTYPE void
#define NTP_POSIX_SOURCE
#define HAVE_ATT_SETPGRP
#define HAVE_ATT_NICE
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
#endif

/*
 * Convex
 */
#if  defined(SYS_CONVEXOS10)||defined(SYS_CONVEXOS9)
#define HAVE_SIGNALED_IO
#define HAVE_N_UN
#define HAVE_READKMEM 
#define HAVE_BSD_NICE
#if defined(convex) 
#define	RETSIGTYPE int
#define NO_SIGNED_CHAR_DECL
#else
#if defined(__stdc__)&&!defined(USE_PROTOTYPES)
#define USE_PROTOTYPES
#endif
#if !defined(__stdc__)&&defined(USE_PROTOTYPES)
#undef  USE_PROTOTYPES
#endif
#define NTP_POSIX_SOURCE
#define HAVE_ATT_SETPGRP
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
#endif

/*
 * AUX
 */
#if defined(SYS_AUX2)||defined(SYS_AUX3)
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
#define HAVE_BSD_TTYS
#define LOG_NTP LOG_LOCAL1
#define HAVE_SIGNALED_IO
#endif

/*
 * Next
 */
#if defined(SYS_NEXT)
#define DOSYNCTODR
#define HAVE_READKMEM
#define HAVE_BSD_NICE
#define HAVE_N_UN
#undef NTP_POSIX_SOURCE
#endif

/*
 * HPUX
 */
#if defined(SYS_HPUX)
#define NTP_POSIX_SOURCE
#define HAVE_SIGNALED_IO
#define HAVE_UNISTD_H
#define NO_SIGNED_CHAR_DECL
#define LOCK_PROCESS
#define	HAVE_NO_NICE	/* HPUX uses rtprio instead */
#define RETSIGTYPE      void
#if (SYS_HPUX < 10)
#define NOKMEM
#else
#define HAVE_READKMEM
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
#define ntp_adjtime adjtimex
#define HAVE_BSD_NICE
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
#endif

/*
 * DECOSF1
 */
#if defined(SYS_DECOSF1)
#define HAVE_SIGNALED_IO
#define HAVE_READKMEM
#define NTP_POSIX_SOURCE
#define	NTP_SYSCALLS_STD
#define	HAVE_BSD_NICE
#endif

/*
 * I386
 */
#if defined(SYS_I386)
#define HAVE_READKMEM 
#define S_CHAR_DEFINED 
#define HAVE_BSD_NICE
#endif

/*
 * Mips
 */
#if defined(SYS_MIPS)
#define NOKMEM 
#define HAVE_BSD_NICE
#endif

/*
 * SEQUENT
 */
#if defined(SYS_SEQUENT)
#define HAVE_BSD_NICE
#endif

/*
 * PTX
 */
#if defined(SYS_PTX)
#define NO_SIGNED_CHAR_DECL
#ifndef HAVE_SYSV_TTYS
#define HAVE_SYSV_TTYS
#endif
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
#endif


/*
 * Sony
 */
#if defined(SYS_SONY)
#define NO_SIGNED_CHAR_DECL 
#define HAVE_READKMEM
#define HAVE_BSD_NICE
#endif

/*
 * VAX
 */
#if defined(SYS_VAX)
#define NO_SIGNED_CHAR_DECL 
#define HAVE_READKMEM 
#define HAVE_BSD_NICE
#endif

/* 
 * UNIX V.4 on and NCR 3000
 */
#if defined(SYS_SVR4)
#define HAVE_ATT_SETPGRP
#define USE_PROTOTYPES
#define HAVE_UNISTD_H
#define NTP_POSIX_SOURCE
#define HAVE_ATT_NICE
#define HAVE_READKMEM 
#define HAVE_SIGNALED_IO
#define USE_TTY_SIGPOLL
#define USE_UDP_SIGPOLL
#define STREAM
#define STEP_SLEW 		/* TWO step */
#define LOCK_PROCESS
#define SYSV_TIMEOFDAY
#define SIZE_RETURNED_IN_BUFFER
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

#if	!defined(HAVE_ATT_NICE) && !defined(HAVE_BSD_NICE) && !defined(HAVE_NO_NICE)
	ERROR You_must_define_one_of_the_HAVE_xx_NICE_defines
#endif

#endif /* __ntp_machine */
