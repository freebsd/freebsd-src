/* debugging code */
#undef DEBUG

/* Minutes per DST adjustment */
#undef DSTMINUTES

/* MD5 authentication */
#undef MD5

/* DES authentication (COCOM only) */
#undef DES

/* time_t */
#undef time_t

/* reference clock interface */
#undef REFCLOCK

/* Audio CHU? */
#undef AUDIO_CHU

/* PARSE kernel PLL PPS support */
#undef PPS_SYNC

/* ACTS modem service */
#undef CLOCK_ACTS

/* Arbiter 1088A/B GPS receiver */
#undef CLOCK_ARBITER

/* DHD19970505: ARCRON support. */
#undef CLOCK_ARCRON_MSF

/* Austron 2200A/2201A GPS receiver */
#undef CLOCK_AS2201

/* PPS interface */
#undef CLOCK_ATOM

/* PPS auxiliary interface for ATOM */
#undef PPS_SAMPLE

/* Datum/Bancomm bc635/VME interface */
#undef CLOCK_BANC

/* Diems Computime Radio Clock */
#undef CLOCK_COMPUTIME

/* Chronolog K-series WWVB receiver */
#undef CLOCK_CHRONOLOG

/* Datum Programmable Time System */
#undef CLOCK_DATUM

/* ELV/DCF7000 clock */
#undef CLOCK_DCF7000

/* Dumb generic hh:mm:ss local clock */
#undef CLOCK_DUMBCLOCK

/* Forum Graphic GPS datating station driver */
#undef CLOCK_FG

/* TrueTime GPS receiver/VME interface */
#undef CLOCK_GPSVME

/* Heath GC-1000 WWV/WWVH receiver */
#undef CLOCK_HEATH

/* HOPF 6021 clock */
#undef CLOCK_HOPF6021

/* HOPF PCI clock device */
#undef CLOCK_HOPF_PCI

/* HOPF serial clock device*/
#undef CLOCK_HOPF_SERIAL

/* HP 58503A GPS receiver */
#undef CLOCK_HPGPS

/* Sun IRIG audio decoder */
#undef CLOCK_IRIG

/* JJY receiver */
#undef CLOCK_JJY

/* Rockwell Jupiter GPS clock */
#undef CLOCK_JUPITER

/* Leitch CSD 5300 Master Clock System Driver */
#undef CLOCK_LEITCH

/* local clock reference */
#undef CLOCK_LOCAL

/* Meinberg clocks */
#undef CLOCK_MEINBERG

/* EES M201 MSF receiver */
#undef CLOCK_MSFEES

/* Magnavox MX4200 GPS receiver */
#undef CLOCK_MX4200

/* NMEA GPS receiver */
#undef CLOCK_NMEA

/* Motorola UT Oncore GPS */
#undef CLOCK_ONCORE

/* Palisade clock */
#undef CLOCK_PALISADE

/* PARSE driver interface */
#undef CLOCK_PARSE

/* Conrad parallel port radio clock */
#undef CLOCK_PCF

/* PCL 720 clock support */
#undef CLOCK_PPS720

/* PST/Traconex 1020 WWV/WWVH receiver */
#undef CLOCK_PST

/* PTB modem service */
#undef CLOCK_PTBACTS

/* DCF77 raw time code */
#undef CLOCK_RAWDCF

/* RCC 8000 clock */
#undef CLOCK_RCC8000

/* Schmid DCF77 clock */
#undef CLOCK_SCHMID

/* clock thru shared memory */
#undef CLOCK_SHM

/* Spectracom 8170/Netclock/2 WWVB receiver */
#undef CLOCK_SPECTRACOM

/* KSI/Odetics TPRO/S GPS receiver/IRIG interface */
#undef CLOCK_TPRO

/* TRAK 8810 GPS receiver */
#undef CLOCK_TRAK

/* Trimble GPS receiver/TAIP protocol */
#undef CLOCK_TRIMTAIP

/* Trimble GPS receiver/TSIP protocol */
#undef CLOCK_TRIMTSIP

/* Kinemetrics/TrueTime receivers */
#undef CLOCK_TRUETIME

/* Ultralink M320 WWVB receiver */
#undef CLOCK_ULINK

/* USNO modem service */
#undef CLOCK_USNO

/* WHARTON 400A Series protocol */
#undef CLOCK_WHARTON_400A

/* WWV audio driver */
#undef CLOCK_WWV

/* VARITEXT protocol */
#undef CLOCK_VARITEXT

/* define if we need to declare int errno; */
#undef DECL_ERRNO

/* define if we may declare int h_errno; */
#undef DECL_H_ERRNO

/* define if it's OK to declare char *sys_errlist[]; */
#undef CHAR_SYS_ERRLIST

/* define if it's OK to declare int syscall P((int, struct timeval *, struct timeval *)); */
#undef DECL_SYSCALL

/* define if we have syscall is buggy (Solaris 2.4) */
#undef SYSCALL_BUG

/* Do we need extra room for SO_RCVBUF? (HPUX <8) */
#undef NEED_RCVBUF_SLOP

/* Should we open the broadcast socket? */
#undef OPEN_BCAST_SOCKET

/* Do we want the HPUX FindConfig()? */
#undef NEED_HPUX_FINDCONFIG

/* canonical system (cpu-vendor-os) string */
#undef STR_SYSTEM

/* define if NetInfo support is available */
#undef HAVE_NETINFO

/* define if [gs]ettimeofday() only takes 1 argument */
#undef SYSV_TIMEOFDAY

/* define if struct sockaddr has sa_len */
#undef HAVE_SA_LEN_IN_STRUCT_SOCKADDR

/* define if struct clockinfo has hz */
#undef HAVE_HZ_IN_STRUCT_CLOCKINFO

/* define if struct sigaction has sa_sigaction */
#undef HAVE_SA_SIGACTION_IN_STRUCT_SIGACTION

/* define if struct clockinfo has tickadj */
#undef HAVE_TICKADJ_IN_STRUCT_CLOCKINFO

/* define if struct ntptimeval uses time.tv_nsec instead of time.tv_usec */ 
#undef HAVE_TV_NSEC_IN_NTPTIMEVAL 

/* Does a system header defind struct ppsclockev? */
#undef HAVE_STRUCT_PPSCLOCKEV

/* define if function prototypes are OK */
#undef HAVE_PROTOTYPES

/* define if setpgrp takes 0 arguments */
#undef HAVE_SETPGRP_0

/* hardwire a value for tick? */
#undef PRESET_TICK

/* hardwire a value for tickadj? */
#undef PRESET_TICKADJ

/* is adjtime() accurate? */
#undef ADJTIME_IS_ACCURATE

/* should we NOT read /dev/kmem? */
#undef NOKMEM

/* use UDP Wildcard Delivery? */
#undef UDP_WILDCARD_DELIVERY

/* always slew the clock? */
#undef SLEWALWAYS

/* step, then slew the clock? */
#undef STEP_SLEW

/* force ntpdate to step the clock if !defined(STEP_SLEW) ? */
#undef FORCE_NTPDATE_STEP

/* synch TODR hourly? */
#undef DOSYNCTODR

/* do we set process groups with -pid? */
#undef UDP_BACKWARDS_SETOWN

/* must we have a CTTY for fsetown? */
#undef USE_FSETOWNCTTY

/* can we use SIGIO for tcp and udp IO? */
#undef HAVE_SIGNALED_IO

/* can we use SIGPOLL for UDP? */
#undef USE_UDP_SIGPOLL

/* can we use SIGPOLL for tty IO? */
#undef USE_TTY_SIGPOLL

/* should we use clock_settime()? */
#undef USE_CLOCK_SETTIME

/* do we want the CHU driver? */
#undef CLOCK_CHU

/* do we have the ppsclock streams module? */
#undef PPS

/* do we have the tty_clk line discipline/streams module? */
#undef TTYCLK

/* does the kernel support precision time discipline? */
#undef KERNEL_PLL

/* does the kernel support multicasting IP? */
#undef MCAST

/* do we have ntp_{adj,get}time in libc? */
#undef NTP_SYSCALLS_LIBC

/* do we have ntp_{adj,get}time in the kernel? */
#undef NTP_SYSCALLS_STD

/* do we have STREAMS/TLI? (Can we replace this with HAVE_SYS_STROPTS_H? */
#undef STREAMS_TLI

/* do we need an s_char typedef? */
#undef NEED_S_CHAR_TYPEDEF

/* include the GDT Surveying code? */
#undef GDT_SURVEYING

/* does SIOCGIFCONF return size in the buffer? */
#undef SIZE_RETURNED_IN_BUFFER

/* what is the name of TICK in the kernel? */
#undef K_TICK_NAME

/* Is K_TICK_NAME (nsec_per_tick, for example) in nanoseconds? */
#undef TICK_NANO

/* what is the name of TICKADJ in the kernel? */
#undef K_TICKADJ_NAME

/* Is K_TICKADJ_NAME (hrestime_adj, for example) in nanoseconds? */
#undef TICKADJ_NANO

/* what is (probably) the name of DOSYNCTODR in the kernel? */
#undef K_DOSYNCTODR_NAME

/* what is (probably) the name of NOPRINTF in the kernel? */
#undef K_NOPRINTF_NAME

/* do we need HPUX adjtime() library support? */
#undef NEED_HPUX_ADJTIME

/* Might nlist() values require an extra level of indirection (AIX)? */
#undef NLIST_EXTRA_INDIRECTION

/* Other needed NLIST stuff */
#undef NLIST_STRUCT
#undef NLIST_NAME_UNION

/* Should we recommend a minimum value for tickadj? */
#undef MIN_REC_TICKADJ

/* Is there a problem using PARENB and IGNPAR (IRIX)? */
#undef NO_PARENB_IGNPAR

/* Should we not IGNPAR (Linux)? */
#undef RAWDCF_NO_IGNPAR

/* Does the compiler like "volatile"? */
#undef volatile

/* Does qsort expect to work on "void *" stuff? */
#undef QSORT_USES_VOID_P

/* What is the fallback value for HZ? */
#undef DEFAULT_HZ

/* Do we need to override the system's idea of HZ? */
#undef OVERRIDE_HZ

/* Do we want the SCO clock hacks? */
#undef SCO5_CLOCK

/* Do we want the ReliantUNIX clock hacks? */
#undef RELIANTUNIX_CLOCK

/* Does the kernel have an FLL bug? */
#undef KERNEL_FLL_BUG

/* Define if you have the TIOCGPPSEV ioctl (Solaris) */
#undef HAVE_TIOCGPPSEV

/* Define if you have the TIOCSPPS ioctl (Solaris) */
#undef HAVE_TIOCSPPS

/* Define if you have the CIOGETEV ioctl (SunOS, Linux) */
#undef HAVE_CIOGETEV

/* Define if you have the TIOCGSERIAL, TIOCSSERIAL, ASYNC_PPS_CD_POS, and ASYNC_PPS_CD_NEG ioctls (linux) */
#undef HAVE_TIO_SERIAL_STUFF

/* Define if you have the interface in the Draft RFC */
#undef HAVE_PPSAPI

/* Do we need to #define _SVID3 when we #include <termios.h>? */
#undef TERMIOS_NEEDS__SVID3

/* Do we have support for SHMEM_STATUS? */
#undef ONCORE_SHMEM_STATUS

/***/

/* Which way should we declare... */

/* adjtime()? */
#undef DECL_ADJTIME_0

/* bcopy()? */
#undef DECL_BCOPY_0

/* bzero()? */
#undef DECL_BZERO_0

/* cfset[io]speed()? */
#undef DECL_CFSETISPEED_0

/* hstrerror()? */
#undef DECL_HSTRERROR_0

/* inet_ntoa()? */
#undef DECL_INET_NTOA_0

/* ioctl()? */
#undef DECL_IOCTL_0

/* IPC? (bind, connect, recvfrom, sendto, setsockopt, socket) */
#undef DECL_IPC_0

/* memmove()? */
#undef DECL_MEMMOVE_0

/* memset()? */
#undef DECL_MEMSET_0

/* mkstemp()? */
#undef DECL_MKSTEMP_0

/* mktemp()? */
#undef DECL_MKTEMP_0

/* mrand48()? */
#undef DECL_MRAND48_0

/* nlist()? */
#undef DECL_NLIST_0

/* plock()? */
#undef DECL_PLOCK_0

/* rename()? */
#undef DECL_RENAME_0

/* select()? */
#undef DECL_SELECT_0

/* setitimer()? */
#undef DECL_SETITIMER_0

/* setpriority()? */
#undef DECL_SETPRIORITY_0
#undef DECL_SETPRIORITY_1

/* sigvec()? */
#undef DECL_SIGVEC_0

/* srand48()? */
#undef DECL_SRAND48_0

/* stdio stuff? */
#undef DECL_STDIO_0

/* stime()? */
#undef DECL_STIME_0
#undef DECL_STIME_1

/* strtol()? */
#undef DECL_STRTOL_0

/* syslog() stuff? */
#undef DECL_SYSLOG_0

/* time()? */
#undef DECL_TIME_0

/* [gs]ettimeofday()? */
#undef DECL_TIMEOFDAY_0

/* tolower()? */
#undef DECL_TOLOWER_0

/* toupper()? */
#undef DECL_TOUPPER_0

/* strerror()? */
#undef DECL_STRERROR_0

#undef ULONG_CONST
