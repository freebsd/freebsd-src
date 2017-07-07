/*! \file mitwhich.h
 *	some defines so that we can figure out which MS OS and subsystem an
 *	application is running under. Also support for finding out which
 *	TCP/IP stack is being used. This is useful when you need to find out
 *	about the domain or the nameservers.
 */

#if !defined( __MIT_WHICH_H )
#define __MIT_WHICH_H

// these should become resources and loaded at run time
#define NT_32 "Winsock 2.0"
#define NT_16 "Windows NT 16-bit Windows Sockets"
#define W95_32 "Microsoft Windows Sockets Version 1.1."
#define W95_16 "Microsoft Windows Sockets Version 1.1."
#define LWP_16 "Novell Winsock version 1.1"
// Note that these are currently in wshelper.h and should be somewhere else
#define MS_NT_32 1
#define MS_NT_16 2
#define MS_95_32 3
#define MS_95_16 4
#define NOVELL_LWP_16 5

#define MS_OS_WIN 1
#define MS_OS_95 2
#define MS_OS_NT 4
#define MS_OS_2000 12
#define MS_OS_XP 28
#define MS_OS_2003 60
#define MS_OS_NT_UNKNOWN 124
#define MS_OS_UNKNOWN 0

#define STACK_UNKNOWN 0
#define UNKNOWN_16_UNDER_32 -2
#define UNKNOWN_16_UNDER_16 -3
#define UNKNOWN_32_UNDER_32 -4
#define UNKNOWN_32_UNDER_16 -5


/*
   @comm these are the current MIT DNS servers, the wshelper and
   wshelp32 DLLs will do their best to find the correct DNS servers
   for the local machine however, if all else fails these will be used
   as a last resort. Site administrators outside of the MIT domain
   should change these defaults to their own defaults either by
   editing this file and recompiling or by editing the string tables
   of the binaries. Don't use App Studio to edit the .RC files.
\n
	#define DNS1	"18.70.0.160" \n
	#define DNS2	"18.71.0.151" \n
	#define DNS3	"18.72.0.3"   \n
\n
	#define DEFAULT_DOMAIN "mit.edu" \n
*/

#define DNS1	"18.70.0.160"
#define DNS2	"18.71.0.151"
#define DNS3	"18.72.0.3"

#define DEFAULT_DOMAIN "mit.edu"


#ifndef _PATH_RESCONF
#if !defined(WINDOWS) && !defined(_WINDOWS) && !defined(_WIN32)
#define _PATH_RESCONF  "/etc/resolv.conf"
#else
#define _PATH_RESCONF  "c:/net/tcp/resolv.cfg"
#endif
#endif


/* Microsoft TCP/IP registry values that we care about */
#define NT_TCP_PATH    "SYSTEM\\CurrentControlSet\\Services\\Tcpip\\Parameters"
#define NT_TCP_PATH_TRANS "SYSTEM\\CurrentControlSet\\Services\\Tcpip\\Parameters\\Transient"
#define W95_TCP_PATH   "SYSTEM\\CurrentControlSet\\Services\\VxD\\MSTCP"

#define NT_DOMAIN_KEY  "SYSTEM\\CurrentControlSet\\Services\\Tcpip\\Parameters\\Domain"
#define NT_NS_KEY      "SYSTEM\\CurrentControlSet\\Services\\Tcpip\\Parameters\\NameServer"

#define W95_DOMAIN_KEY "SYSTEM\\CurrentControlSet\\Services\\VxD\\MSTCP\\Domain"
#define W95_NS_KEY     "SYSTEM\\CurrentControlSet\\Services\\VxD\\MSTCP\\NameServer"


#endif // __MIT_WHICH_H
