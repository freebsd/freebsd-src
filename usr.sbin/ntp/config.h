/* config.h.  Generated automatically by configure.  */
/* config.h.in.  Generated automatically from configure.in by autoheader.  */
/* $FreeBSD: src/usr.sbin/ntp/config.h,v 1.4 2000/03/12 13:25:14 dufault Exp $ */

/* Define if on AIX 3.
   System headers sometimes define this.
   We just want to avoid a redefinition error message.  */
#ifndef _ALL_SOURCE
/* #undef _ALL_SOURCE */
#endif

/* Define if type char is unsigned and you are not using gcc.  */
#ifndef __CHAR_UNSIGNED__
/* #undef __CHAR_UNSIGNED__ */
#endif

/* Define to empty if the keyword does not work.  */
/* #undef const */

/* Define to `int' if <sys/types.h> doesn't define.  */
/* #undef gid_t */

/* Define as __inline if that's what the C compiler calls it.  */
/* #undef inline */

/* Define if on MINIX.  */
/* #undef _MINIX */

/* Define if your struct nlist has an n_un member.  */
/* #undef NLIST_NAME_UNION */

/* Define if you have <nlist.h>.  */
#define NLIST_STRUCT 1

/* Define to `long' if <sys/types.h> doesn't define.  */
/* #undef off_t */

/* Define if the system does not provide POSIX.1 features except
   with this defined.  */
/* #undef _POSIX_1_SOURCE */

/* Define if you need to in order for stat and other things to work.  */
/* #undef _POSIX_SOURCE */

/* Define as the return type of signal handlers (int or void).  */
#define RETSIGTYPE void

/* Define to `unsigned' if <sys/types.h> doesn't define.  */
/* #undef size_t */

/* Define if you have the ANSI C header files.  */
#define STDC_HEADERS 1

/* Define if you can safely include both <sys/time.h> and <time.h>.  */
#define TIME_WITH_SYS_TIME 1

/* Define if your <sys/time.h> declares struct tm.  */
/* #undef TM_IN_SYS_TIME */

/* Define to `int' if <sys/types.h> doesn't define.  */
/* #undef uid_t */

/* Define if your processor stores words with the most significant
   byte first (like Motorola and SPARC, unlike Intel and VAX).  */
/* #undef WORDS_BIGENDIAN */

/* debugging code */
#define DEBUG 1

/* Minutes per DST adjustment */
#define DSTMINUTES 60

/* MD5 authentication */
#define MD5 1

/* DES authentication (COCOM only) */
/* #undef DES */

/* time_t */
/* #undef time_t */

/* reference clock interface */
#define REFCLOCK 1

/* Audio CHU? */
/* #undef AUDIO_CHU */

/* PARSE kernel PLL PPS support */
/* #undef PPS_SYNC */

/* ACTS modem service */
/* #undef CLOCK_ACTS */

/* Arbiter 1088A/B GPS receiver */
/* #undef CLOCK_ARBITER */

/* DHD19970505: ARCRON support. */
/* #undef CLOCK_ARCRON_MSF */

/* Austron 2200A/2201A GPS receiver */
/* #undef CLOCK_AS2201 */

/* PPS interface */
#define CLOCK_ATOM 1

/* PPS auxiliary interface for ATOM */
#define PPS_SAMPLE 1

/* Datum/Bancomm bc635/VME interface */
/* #undef CLOCK_BANC */

/* Diems Computime Radio Clock */
/* #undef CLOCK_COMPUTIME */

/* Chronolog K-series WWVB receiver */
/* #undef CLOCK_CHRONOLOG */

/* Datum Programmable Time System */
/* #undef CLOCK_DATUM */

/* ELV/DCF7000 clock */
/* #undef CLOCK_DCF7000 */

/* Dumb generic hh:mm:ss local clock */
#define CLOCK_DUMBCLOCK 1

/* Forum Graphic GPS datating station driver */
/* #undef CLOCK_FG */

/* TrueTime GPS receiver/VME interface */
/* #undef CLOCK_GPSVME */

/* Heath GC-1000 WWV/WWVH receiver */
/* #undef CLOCK_HEATH */

/* HOPF 6021 clock */
/* #undef CLOCK_HOPF6021 */

/* HP 58503A GPS receiver */
/* #undef CLOCK_HPGPS */

/* Sun IRIG audio decoder */
/* #undef CLOCK_IRIG */

/* Rockwell Jupiter GPS clock */
/* #undef CLOCK_JUPITER */

/* Leitch CSD 5300 Master Clock System Driver */
/* #undef CLOCK_LEITCH */

/* local clock reference */
#define CLOCK_LOCAL 1

/* Meinberg clocks */
/* #undef CLOCK_MEINBERG */

/* EES M201 MSF receiver */
/* #undef CLOCK_MSFEES */

/* Magnavox MX4200 GPS receiver */
/* #undef CLOCK_MX4200 */

/* NMEA GPS receiver */
#define CLOCK_NMEA 1

/* Motorola UT Oncore GPS */
#define CLOCK_ONCORE 1

/* Palisade clock */
/* #undef CLOCK_PALISADE */

/* PARSE driver interface */
#define CLOCK_PARSE 1

/* Conrad parallel port radio clock */
/* #undef CLOCK_PCF */

/* PCL 720 clock support */
/* #undef CLOCK_PPS720 */

/* PST/Traconex 1020 WWV/WWVH receiver */
/* #undef CLOCK_PST */

/* PTB modem service */
/* #undef CLOCK_PTBACTS */

/* DCF77 raw time code */
#define CLOCK_RAWDCF 1

/* RCC 8000 clock */
/* #undef CLOCK_RCC8000 */

/* Schmid DCF77 clock */
/* #undef CLOCK_SCHMID */

/* clock thru shared memory */
/* #undef CLOCK_SHM */

/* Spectracom 8170/Netclock/2 WWVB receiver */
/* #undef CLOCK_SPECTRACOM */

/* KSI/Odetics TPRO/S GPS receiver/IRIG interface */
/* #undef CLOCK_TPRO */

/* TRAK 8810 GPS receiver */
/* #undef CLOCK_TRAK */

/* Trimble GPS receiver/TAIP protocol */
/* #undef CLOCK_TRIMTAIP */

/* Trimble GPS receiver/TSIP protocol */
/* #undef CLOCK_TRIMTSIP */

/* Kinemetrics/TrueTime receivers */
/* #undef CLOCK_TRUETIME */

/* Ultralink M320 WWVB receiver */
/* #undef CLOCK_ULINK */

/* USNO modem service */
/* #undef CLOCK_USNO */

/* WHARTON 400A Series protocol */
/* #undef CLOCK_WHARTON_400A */

/* WWV audio driver */
/* #undef CLOCK_WWV */

/* VARITEXT protocol */
/* #undef CLOCK_VARITEXT */

/* define if we need to declare int errno; */
/* #undef DECL_ERRNO */

/* define if we may declare int h_errno; */
#define DECL_H_ERRNO 1

/* define if it's OK to declare char *sys_errlist[]; */
/* #undef CHAR_SYS_ERRLIST */

/* define if it's OK to declare int syscall P((int, struct timeval *, struct timeval *)); */
#define DECL_SYSCALL 1

/* define if we have syscall is buggy (Solaris 2.4) */
/* #undef SYSCALL_BUG */

/* Do we need extra room for SO_RCVBUF? (HPUX <8) */
/* #undef NEED_RCVBUF_SLOP */

/* Should we open the broadcast socket? */
#define OPEN_BCAST_SOCKET 1

/* Do we want the HPUX FindConfig()? */
/* #undef NEED_HPUX_FINDCONFIG */

/* canonical system (cpu-vendor-os) string */
#ifdef __alpha__
#define STR_SYSTEM "alpha-unknown-freebsd"
#else
#define STR_SYSTEM "i386-unknown-freebsd"
#endif

/* define if NetInfo support is available */
/* #undef HAVE_NETINFO */

/* define if [gs]ettimeofday() only takes 1 argument */
/* #undef SYSV_TIMEOFDAY */

/* define if struct sockaddr has sa_len */
#define HAVE_SA_LEN_IN_STRUCT_SOCKADDR 1

/* define if struct clockinfo has hz */
#define HAVE_HZ_IN_STRUCT_CLOCKINFO 1

/* define if struct sigaction has sa_sigaction */
#define HAVE_SA_SIGACTION_IN_STRUCT_SIGACTION 1

/* define if struct clockinfo has tickadj */
#define HAVE_TICKADJ_IN_STRUCT_CLOCKINFO 1

/* define if struct ntptimeval uses time.tv_nsec instead of time.tv_usec */ 
#define HAVE_TV_NSEC_IN_NTPTIMEVAL 1 

/* Does a system header defind struct ppsclockev? */
/* #undef HAVE_STRUCT_PPSCLOCKEV */

/* define if function prototypes are OK */
#define HAVE_PROTOTYPES 1

/* define if setpgrp takes 0 arguments */
/* #undef HAVE_SETPGRP_0 */

/* hardwire a value for tick? */
#define PRESET_TICK 1000000L/hz

/* hardwire a value for tickadj? */
#define PRESET_TICKADJ 500/hz

/* is adjtime() accurate? */
/* #undef ADJTIME_IS_ACCURATE */

/* should we NOT read /dev/kmem? */
/* #undef NOKMEM */

/* use UDP Wildcard Delivery? */
#define UDP_WILDCARD_DELIVERY 1

/* always slew the clock? */
/* #undef SLEWALWAYS */

/* step, then slew the clock? */
/* #undef STEP_SLEW */

/* force ntpdate to step the clock if !defined(STEP_SLEW) ? */
/* #undef FORCE_NTPDATE_STEP */

/* synch TODR hourly? */
/* #undef DOSYNCTODR */

/* do we set process groups with -pid? */
/* #undef UDP_BACKWARDS_SETOWN */

/* must we have a CTTY for fsetown? */
#define USE_FSETOWNCTTY 1

/* can we use SIGIO for tcp and udp IO? */
/* #undef HAVE_SIGNALED_IO */

/* can we use SIGPOLL for UDP? */
/* #undef USE_UDP_SIGPOLL */

/* can we use SIGPOLL for tty IO? */
/* #undef USE_TTY_SIGPOLL */

/* do we want the CHU driver? */
/* #undef CLOCK_CHU */

/* do we have the ppsclock streams module? */
/* #undef PPS */

/* do we have the tty_clk line discipline/streams module? */
/* #undef TTYCLK */

/* does the kernel support precision time discipline? */
#define KERNEL_PLL 1

/* does the kernel support multicasting IP? */
#define MCAST 1

/* do we have ntp_{adj,get}time in libc? */
#define NTP_SYSCALLS_LIBC 1

/* do we have ntp_{adj,get}time in the kernel? */
/* #undef NTP_SYSCALLS_STD */

/* do we have STREAMS/TLI? (Can we replace this with HAVE_SYS_STROPTS_H? */
/* #undef STREAMS_TLI */

/* do we need an s_char typedef? */
#define NEED_S_CHAR_TYPEDEF 1

/* does SIOCGIFCONF return size in the buffer? */
/* #undef SIZE_RETURNED_IN_BUFFER */

/* what is the name of TICK in the kernel? */
#define K_TICK_NAME "_tick"

/* Is K_TICK_NAME (nsec_per_tick, for example) in nanoseconds? */
/* #undef TICK_NANO */

/* what is the name of TICKADJ in the kernel? */
#define K_TICKADJ_NAME "_tickadj"

/* Is K_TICKADJ_NAME (hrestime_adj, for example) in nanoseconds? */
/* #undef TICKADJ_NANO */

/* what is (probably) the name of DOSYNCTODR in the kernel? */
#define K_DOSYNCTODR_NAME "_dosynctodr"

/* what is (probably) the name of NOPRINTF in the kernel? */
#define K_NOPRINTF_NAME "_noprintf"

/* do we need HPUX adjtime() library support? */
/* #undef NEED_HPUX_ADJTIME */

/* Might nlist() values require an extra level of indirection (AIX)? */
/* #undef NLIST_EXTRA_INDIRECTION */

/* Other needed NLIST stuff */
#define NLIST_STRUCT 1
/* #undef NLIST_NAME_UNION */

/* Should we recommend a minimum value for tickadj? */
/* #undef MIN_REC_TICKADJ */

/* Is there a problem using PARENB and IGNPAR (IRIX)? */
#define NO_PARENB_IGNPAR 1

/* Should we not IGNPAR (Linux)? */
/* #undef RAWDCF_NO_IGNPAR */

/* Does the compiler like "volatile"? */
/* #undef volatile */

/* Does qsort expect to work on "void *" stuff? */
#define QSORT_USES_VOID_P 1

/* What is the fallback value for HZ? */
#define DEFAULT_HZ 100

/* Do we need to override the system's idea of HZ? */
#define OVERRIDE_HZ 1

/* Do we want the SCO clock hacks? */
/* #undef SCO5_CLOCK */

/* Do we want the ReliantUNIX clock hacks? */
/* #undef RELIANTUNIX_CLOCK */

/* Does the kernel have an FLL bug? */
/* #undef KERNEL_FLL_BUG */

/* Define if you have the TIOCGPPSEV ioctl (Solaris) */
/* #undef HAVE_TIOCGPPSEV */

/* Define if you have the TIOCSPPS ioctl (Solaris) */
/* #undef HAVE_TIOCSPPS */

/* Define if you have the CIOGETEV ioctl (SunOS, Linux) */
/* #undef HAVE_CIOGETEV */

/* Define if you have the TIOCGSERIAL, TIOCSSERIAL, ASYNC_PPS_CD_POS, and ASYNC_PPS_CD_NEG ioctls (linux) */
/* #undef HAVE_TIO_SERIAL_STUFF */

/* Define if you use struct timespec rather than struct timeval (time in ns rather than us) */
#define HAVE_TIMESPEC 1

/* Define if you have the interface in the Draft RFC */
#define HAVE_PPSAPI 1

/* Do we need to #define _SVID3 when we #include <termios.h>? */
/* #undef TERMIOS_NEEDS__SVID3 */

/* Do we have support for SHMEM_STATUS? */
#define ONCORE_SHMEM_STATUS 1

/* adjtime()? */
/* #undef DECL_ADJTIME_0 */

/* bcopy()? */
/* #undef DECL_BCOPY_0 */

/* bzero()? */
/* #undef DECL_BZERO_0 */

/* cfset[io]speed()? */
/* #undef DECL_CFSETISPEED_0 */

/* ioctl()? */
/* #undef DECL_IOCTL_0 */

/* IPC? (bind, connect, recvfrom, sendto, setsockopt, socket) */
/* #undef DECL_IPC_0 */

/* memmove()? */
/* #undef DECL_MEMMOVE_0 */

/* mkstemp()? */
/* #undef DECL_MKSTEMP_0 */

/* mktemp()? */
/* #undef DECL_MKTEMP_0 */

/* mrand48()? */
/* #undef DECL_MRAND48_0 */

/* nlist()? */
/* #undef DECL_NLIST_0 */

/* plock()? */
/* #undef DECL_PLOCK_0 */

/* rename()? */
/* #undef DECL_RENAME_0 */

/* select()? */
/* #undef DECL_SELECT_0 */

/* setitimer()? */
/* #undef DECL_SETITIMER_0 */

/* setpriority()? */
/* #undef DECL_SETPRIORITY_0 */
/* #undef DECL_SETPRIORITY_1 */

/* sigvec()? */
/* #undef DECL_SIGVEC_0 */

/* srand48()? */
/* #undef DECL_SRAND48_0 */

/* stdio stuff? */
/* #undef DECL_STDIO_0 */

/* stime()? */
/* #undef DECL_STIME_0 */
/* #undef DECL_STIME_1 */

/* strtol()? */
/* #undef DECL_STRTOL_0 */

/* syslog() stuff? */
/* #undef DECL_SYSLOG_0 */

/* time()? */
/* #undef DECL_TIME_0 */

/* [gs]ettimeofday()? */
/* #undef DECL_TIMEOFDAY_0 */

/* tolower()? */
/* #undef DECL_TOLOWER_0 */

/* toupper()? */
/* #undef DECL_TOUPPER_0 */

/* The number of bytes in a int.  */
#define SIZEOF_INT 4

/* The number of bytes in a long.  */
#ifdef __alpha__
#define SIZEOF_LONG 8
#else
#define SIZEOF_LONG 4
#endif

/* The number of bytes in a signed char.  */
#define SIZEOF_SIGNED_CHAR 1

/* Define if you have the K_open function.  */
/* #undef HAVE_K_OPEN */

/* Define if you have the __adjtimex function.  */
/* #undef HAVE___ADJTIMEX */

/* Define if you have the __ntp_gettime function.  */
/* #undef HAVE___NTP_GETTIME */

/* Define if you have the clock_settime function.  */
#define HAVE_CLOCK_SETTIME 1

/* Define if you have the daemon function.  */
#define HAVE_DAEMON 1

/* Define if you have the getbootfile function.  */
#define HAVE_GETBOOTFILE 1

/* Define if you have the getdtablesize function.  */
#define HAVE_GETDTABLESIZE 1

/* Define if you have the getrusage function.  */
#define HAVE_GETRUSAGE 1

/* Define if you have the gettimeofday function.  */
#define HAVE_GETTIMEOFDAY 1

/* Define if you have the getuid function.  */
#define HAVE_GETUID 1

/* Define if you have the kvm_open function.  */
#define HAVE_KVM_OPEN 1

/* Define if you have the memcpy function.  */
#define HAVE_MEMCPY 1

/* Define if you have the memlk function.  */
/* #undef HAVE_MEMLK */

/* Define if you have the memmove function.  */
#define HAVE_MEMMOVE 1

/* Define if you have the memset function.  */
#define HAVE_MEMSET 1

/* Define if you have the mkstemp function.  */
#define HAVE_MKSTEMP 1

/* Define if you have the mktime function.  */
#define HAVE_MKTIME 1

/* Define if you have the mlockall function.  */
/* #undef HAVE_MLOCKALL */

/* Define if you have the mrand48 function.  */
#define HAVE_MRAND48 1

/* Define if you have the nice function.  */
#define HAVE_NICE 1

/* Define if you have the nlist function.  */
#define HAVE_NLIST 1

/* Define if you have the ntp_adjtime function.  */
#define HAVE_NTP_ADJTIME 1

/* Define if you have the ntp_gettime function.  */
#define HAVE_NTP_GETTIME 1

/* Define if you have the plock function.  */
/* #undef HAVE_PLOCK */

/* Define if you have the pututline function.  */
/* #undef HAVE_PUTUTLINE */

/* Define if you have the pututxline function.  */
/* #undef HAVE_PUTUTXLINE */

/* Define if you have the random function.  */
#define HAVE_RANDOM 1

/* Define if you have the rtprio function.  */
/* #undef HAVE_RTPRIO 1 */

/* Define if you have the sched_setscheduler function.  */
/* #undef HAVE_SCHED_SETSCHEDULER 1 */

/* Define if you have the setlinebuf function.  */
#define HAVE_SETLINEBUF 1

/* Define if you have the setpgid function.  */
#define HAVE_SETPGID 1

/* Define if you have the setpriority function.  */
#define HAVE_SETPRIORITY 1

/* Define if you have the setsid function.  */
#define HAVE_SETSID 1

/* Define if you have the settimeofday function.  */
#define HAVE_SETTIMEOFDAY 1

/* Define if you have the setvbuf function.  */
#define HAVE_SETVBUF 1

/* Define if you have the sigaction function.  */
#define HAVE_SIGACTION 1

/* Define if you have the sigset function.  */
/* #undef HAVE_SIGSET */

/* Define if you have the sigsuspend function.  */
#define HAVE_SIGSUSPEND 1

/* Define if you have the sigvec function.  */
#define HAVE_SIGVEC 1

/* Define if you have the srand48 function.  */
#define HAVE_SRAND48 1

/* Define if you have the srandom function.  */
#define HAVE_SRANDOM 1

/* Define if you have the stime function.  */
/* #undef HAVE_STIME */

/* Define if you have the strchr function.  */
#define HAVE_STRCHR 1

/* Define if you have the strerror function.  */
#define HAVE_STRERROR 1

/* Define if you have the sysconf function.  */
#define HAVE_SYSCONF 1

/* Define if you have the sysctl function.  */
#define HAVE_SYSCTL 1

/* Define if you have the timer_create function.  */
/* #undef HAVE_TIMER_CREATE */

/* Define if you have the timer_settime function.  */
/* #undef HAVE_TIMER_SETTIME */

/* Define if you have the umask function.  */
#define HAVE_UMASK 1

/* Define if you have the uname function.  */
#define HAVE_UNAME 1

/* Define if you have the updwtmp function.  */
/* #undef HAVE_UPDWTMP */

/* Define if you have the updwtmpx function.  */
/* #undef HAVE_UPDWTMPX */

/* Define if you have the vsprintf function.  */
#define HAVE_VSPRINTF 1

/* Define if you have the </sys/sync/queue.h> header file.  */
/* #undef HAVE__SYS_SYNC_QUEUE_H */

/* Define if you have the </sys/sync/sema.h> header file.  */
/* #undef HAVE__SYS_SYNC_SEMA_H */

/* Define if you have the <arpa/nameser.h> header file.  */
#define HAVE_ARPA_NAMESER_H 1

/* Define if you have the <bstring.h> header file.  */
/* #undef HAVE_BSTRING_H */

/* Define if you have the <errno.h> header file.  */
#define HAVE_ERRNO_H 1

/* Define if you have the <fcntl.h> header file.  */
#define HAVE_FCNTL_H 1

/* Define if you have the <machine/inline.h> header file.  */
/* #undef HAVE_MACHINE_INLINE_H */

/* Define if you have the <memory.h> header file.  */
#define HAVE_MEMORY_H 1

/* Define if you have the <net/if.h> header file.  */
#define HAVE_NET_IF_H 1

/* Define if you have the <netdb.h> header file.  */
#define HAVE_NETDB_H 1

/* Define if you have the <netinet/in.h> header file.  */
#define HAVE_NETINET_IN_H 1

/* Define if you have the <netinet/in_systm.h> header file.  */
#define HAVE_NETINET_IN_SYSTM_H 1

/* Define if you have the <netinfo/ni.h> header file.  */
/* #undef HAVE_NETINFO_NI_H */

/* Define if you have the <poll.h> header file.  */
#define HAVE_POLL_H 1

/* Define if you have the <resolv.h> header file.  */
#define HAVE_RESOLV_H 1

/* Define if you have the <sched.h> header file.  */
#define HAVE_SCHED_H 1

/* Define if you have the <sgtty.h> header file.  */
#define HAVE_SGTTY_H 1

/* Define if you have the <stdlib.h> header file.  */
#define HAVE_STDLIB_H 1

/* Define if you have the <string.h> header file.  */
#define HAVE_STRING_H 1

/* Define if you have the <sun/audioio.h> header file.  */
/* #undef HAVE_SUN_AUDIOIO_H */

/* Define if you have the <sys/audioio.h> header file.  */
/* #undef HAVE_SYS_AUDIOIO_H */

/* Define if you have the <sys/clkdefs.h> header file.  */
/* #undef HAVE_SYS_CLKDEFS_H */

/* Define if you have the <sys/file.h> header file.  */
#define HAVE_SYS_FILE_H 1

/* Define if you have the <sys/i8253.h> header file.  */
/* #undef HAVE_SYS_I8253_H */

/* Define if you have the <sys/ioctl.h> header file.  */
#define HAVE_SYS_IOCTL_H 1

/* Define if you have the <sys/lock.h> header file.  */
#define HAVE_SYS_LOCK_H 1

/* Define if you have the <sys/mman.h> header file.  */
#define HAVE_SYS_MMAN_H 1

/* Define if you have the <sys/modem.h> header file.  */
/* #undef HAVE_SYS_MODEM_H */

/* Define if you have the <sys/param.h> header file.  */
#define HAVE_SYS_PARAM_H 1

/* Define if you have the <sys/pcl720.h> header file.  */
/* #undef HAVE_SYS_PCL720_H */

/* Define if you have the <sys/ppsclock.h> header file.  */
/* #undef HAVE_SYS_PPSCLOCK_H */

/* Define if you have the <sys/ppstime.h> header file.  */
/* #undef HAVE_SYS_PPSTIME_H */

/* Define if you have the <sys/proc.h> header file.  */
#define HAVE_SYS_PROC_H 1

/* Define if you have the <sys/resource.h> header file.  */
#define HAVE_SYS_RESOURCE_H 1

/* Define if you have the <sys/sched.h> header file.  */
/* #undef HAVE_SYS_SCHED_H */

/* Define if you have the <sys/select.h> header file.  */
#define HAVE_SYS_SELECT_H 1

/* Define if you have the <sys/sio.h> header file.  */
/* #undef HAVE_SYS_SIO_H */

/* Define if you have the <sys/sockio.h> header file.  */
#define HAVE_SYS_SOCKIO_H 1

/* Define if you have the <sys/stat.h> header file.  */
#define HAVE_SYS_STAT_H 1

/* Define if you have the <sys/stream.h> header file.  */
/* #undef HAVE_SYS_STREAM_H */

/* Define if you have the <sys/stropts.h> header file.  */
/* #undef HAVE_SYS_STROPTS_H */

/* Define if you have the <sys/sysctl.h> header file.  */
#define HAVE_SYS_SYSCTL_H 1

/* Define if you have the <sys/syssgi.h> header file.  */
/* #undef HAVE_SYS_SYSSGI_H */

/* Define if you have the <sys/termios.h> header file.  */
#define HAVE_SYS_TERMIOS_H 1

/* Define if you have the <sys/time.h> header file.  */
#define HAVE_SYS_TIME_H 1

/* Define if you have the <sys/timepps.h> header file.  */
#define HAVE_SYS_TIMEPPS_H 1

/* Define if you have the <sys/timers.h> header file.  */
#define HAVE_SYS_TIMERS_H 1

/* Define if you have the <sys/timex.h> header file.  */
#define HAVE_SYS_TIMEX_H 1

/* Define if you have the <sys/tpro.h> header file.  */
/* #undef HAVE_SYS_TPRO_H */

/* Define if you have the <sys/types.h> header file.  */
#define HAVE_SYS_TYPES_H 1

/* Define if you have the <sys/wait.h> header file.  */
#define HAVE_SYS_WAIT_H 1

/* Define if you have the <termio.h> header file.  */
/* #undef HAVE_TERMIO_H */

/* Define if you have the <termios.h> header file.  */
#define HAVE_TERMIOS_H 1

/* Define if you have the <timepps.h> header file.  */
/* #undef HAVE_TIMEPPS_H */

/* Define if you have the <timex.h> header file.  */
/* #undef HAVE_TIMEX_H */

/* Define if you have the <unistd.h> header file.  */
#define HAVE_UNISTD_H 1

/* Define if you have the <utmp.h> header file.  */
#define HAVE_UTMP_H 1

/* Define if you have the <utmpx.h> header file.  */
/* #undef HAVE_UTMPX_H */

/* Define if you have the advapi32 library (-ladvapi32).  */
/* #undef HAVE_LIBADVAPI32 */

/* Define if you have the elf library (-lelf).  */
/* #undef HAVE_LIBELF */

/* Define if you have the gen library (-lgen).  */
/* #undef HAVE_LIBGEN */

/* Define if you have the kvm library (-lkvm).  */
#define HAVE_LIBKVM 1

/* Define if you have the ld library (-lld).  */
/* #undef HAVE_LIBLD */

/* Define if you have the mld library (-lmld).  */
/* #undef HAVE_LIBMLD */

/* Define if you have the nsl library (-lnsl).  */
/* #undef HAVE_LIBNSL */

/* Define if you have the rt library (-lrt).  */
/* #undef HAVE_LIBRT */

/* Define if you have the socket library (-lsocket).  */
/* #undef HAVE_LIBSOCKET */

/* Name of package */
#define PACKAGE "ntp"

/* Version number of package */
#define VERSION "4.0.99b"

/* Define if compiler has function prototypes */
#define PROTOTYPES 1

/* Do we have struct ntptimeval? */
#define HAVE_STRUCT_NTPTIMEVAL 1

/* Does ntptimeval use struct timespec? */
#define TIMESPEC_IN_NTPTIMEVAL 1

