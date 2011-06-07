/* $FreeBSD$ */


/* config.h.  Generated automatically by configure.  */
/* config.h.in.  Generated automatically from configure.in by autoheader.  */
/* $Id: config.h.in,v 1.24 2000/09/18 00:40:12 lukem Exp $ */


/* Define if on AIX 3.
   System headers sometimes define this.
   We just want to avoid a redefinition error message.  */
#ifndef _ALL_SOURCE
/* #undef _ALL_SOURCE */
#endif

/* Define if the closedir function returns void instead of int.  */
/* #undef CLOSEDIR_VOID */

/* Define if the `getpgrp' function takes no argument.  */
#define GETPGRP_VOID 1

/* Define if your C compiler doesn't accept -c and -o together.  */
/* #undef NO_MINUS_C_MINUS_O */

/* Define if your Fortran 77 compiler doesn't accept -c and -o together. */
/* #undef F77_NO_MINUS_C_MINUS_O */

/* Define to `long' if <sys/types.h> doesn't define.  */
/* #undef off_t */

/* Define to the type of arg1 for select(). */
/* #undef SELECT_TYPE_ARG1 */

/* Define to the type of args 2, 3 and 4 for select(). */
/* #undef SELECT_TYPE_ARG234 */

/* Define to the type of arg5 for select(). */
/* #undef SELECT_TYPE_ARG5 */

/* Define if you have the ANSI C header files.  */
#define STDC_HEADERS 1

/* Define if you can safely include both <sys/time.h> and <time.h>.  */
#define TIME_WITH_SYS_TIME 1

/* Define if the closedir function returns void instead of int.  */
/* #undef VOID_CLOSEDIR */

/* The number of bytes in a off_t.  */
#define SIZEOF_OFF_T 0

/* Define if you have the err function.  */
#define HAVE_ERR 1

/* Define if you have the fgetln function.  */
#define HAVE_FGETLN 1

/* Define if you have the fparseln function.  */
#define HAVE_FPARSELN 1

/* Define if you have the fseeko function.  */
#define HAVE_FSEEKO 1

/* Define if you have the getaddrinfo function.  */
#define HAVE_GETADDRINFO 1

/* Define if you have the gethostbyname2 function.  */
#define HAVE_GETHOSTBYNAME2 1

/* Define if you have the getnameinfo function.  */
#define HAVE_GETNAMEINFO 1

/* Define if you have the getpassphrase function.  */
/* #undef HAVE_GETPASSPHRASE */

/* Define if you have the getpgrp function.  */
#define HAVE_GETPGRP 1

/* Define if you have the glob function.  */
#define USE_GLOB_H 1

/* Define if you have the inet_ntop function.  */
#define HAVE_INET_NTOP 1

/* Define if you have the inet_pton function.  */
#define HAVE_INET_PTON 1

/* Define if you have the issetugid function.  */
#define HAVE_ISSETUGID 1

/* Define if you have the memmove function.  */
#define HAVE_MEMMOVE 1

/* Define if you have the mkstemp function.  */
#define HAVE_MKSTEMP 1

/* Define if you have the poll function.  */
#define HAVE_POLL 1

/* Define if you have the select function.  */
#define HAVE_SELECT 1

/* Define if you have the setprogname function.  */
#define HAVE_SETPROGNAME 1

/* Define if you have the sl_init function.  */
#define HAVE_SL_INIT 1

/* Define if you have the snprintf function.  */
#define HAVE_SNPRINTF 1

/* Define if you have the strdup function.  */
#define HAVE_STRDUP 1

/* Define if you have the strerror function.  */
#define HAVE_STRERROR 1

/* Define if you have the strlcat function.  */
#define HAVE_STRLCAT 1

/* Define if you have the strlcpy function.  */
#define HAVE_STRLCPY 1

/* Define if you have the strptime function.  */
#define HAVE_STRPTIME 1

/* Define if you have the strsep function.  */
#define HAVE_STRSEP 1

/* Define if you have the strtoll function.  */
#define HAVE_STRTOLL 1

/* Define if you have the strunvis function.  */
#define HAVE_STRUNVIS 1

/* Define if you have the strvis function.  */
#define HAVE_STRVIS 1

/* Define if you have the timegm function.  */
#define HAVE_TIMEGM 1

/* Define if you have the usleep function.  */
#define HAVE_USLEEP 1

/* Define if you have the <arpa/nameser.h> header file.  */
#define HAVE_ARPA_NAMESER_H 1

/* Define if you have the <dirent.h> header file.  */
#define HAVE_DIRENT_H 1

/* Define if you have the <err.h> header file.  */
#define HAVE_ERR_H 1

/* Define if you have the <libutil.h> header file.  */
#define HAVE_LIBUTIL_H 1

/* Define if you have the <ndir.h> header file.  */
/* #undef HAVE_NDIR_H */

/* Define if you have the <paths.h> header file.  */
#define HAVE_PATHS_H 1

/* Define if you have the <poll.h> header file.  */
#define HAVE_POLL_H 1

/* Define if you have the <regex.h> header file.  */
#define HAVE_REGEX_H 1

/* Define if you have the <sys/dir.h> header file.  */
#define HAVE_SYS_DIR_H 1

/* Define if you have the <sys/ndir.h> header file.  */
/* #undef HAVE_SYS_NDIR_H */

/* Define if you have the <sys/poll.h> header file.  */
#define HAVE_SYS_POLL_H 1

/* Define if you have the <termcap.h> header file.  */
#define HAVE_TERMCAP_H 1

/* Define if you have the <util.h> header file.  */
/* #undef HAVE_UTIL_H */

/* Define if you have the <vis.h> header file.  */
#define HAVE_VIS_H 1

/* Define if you have the nsl library (-lnsl).  */
/* #undef HAVE_LIBNSL */

/* Define if you have the socket library (-lsocket).  */
/* #undef HAVE_LIBSOCKET */

/* Define if you have the tinfo library (-ltinfo).  */
#define HAVE_LIBTINFO 1

/* Define if you have the util library (-lutil).  */
#define HAVE_LIBUTIL 1

/* Define if your compiler supports `long long' */
#define HAVE_LONG_LONG 1

/* Define if in_port_t exists */
#define HAVE_IN_PORT_T 1

/* Define if sa_family_t exists in <sys/socket.h> */
#define HAVE_SA_FAMILY_T 1

/* Define if struct sockaddr.sa_len exists (implies sockaddr_in.sin_len, etc) */
#define HAVE_SOCKADDR_SA_LEN 1

/* Define if socklen_t exists */
#define HAVE_SOCKLEN_T 1

/* Define if AF_INET6 exists in <sys/socket.h> */
#define HAVE_AF_INET6 1

/* Define if `struct sockaddr_in6' exists in <netinet/in.h> */
#define HAVE_SOCKADDR_IN6 1

/* Define if `struct addrinfo' exists in <netdb.h> */
#define HAVE_ADDRINFO 1

/*
 * Define if <netdb.h> contains AI_NUMERICHOST et al.
 * Systems which only implement RFC2133 will need this.
 */
#define HAVE_RFC2553_NETDB 1

/* Define if `struct direct' has a d_namlen element */
#define HAVE_D_NAMLEN 1

/* Define if GLOB_BRACE exists in <glob.h> */
#define HAVE_GLOB_BRACE 1

/* Define if h_errno exists in <netdb.h> */
#define HAVE_H_ERRNO_D 1

/* Define if fclose() is declared in <stdio.h> */
#define HAVE_FCLOSE_D 1

/* Define if getpass() is declared in <stdlib.h> or <unistd.h> */
#define HAVE_GETPASS_D 1

/* Define if optarg is declared in <stdlib.h> or <unistd.h> */
#define HAVE_OPTARG_D 1

/* Define if optind is declared in <stdlib.h> or <unistd.h> */
#define HAVE_OPTIND_D 1

/* Define if pclose() is declared in <stdio.h> */
#define HAVE_PCLOSE_D 1

/* Define if `long long' is supported and sizeof(off_t) >= 8 */
#define HAVE_QUAD_SUPPORT 1

/* Define if strptime() is declared in <time.h> */
#define HAVE_STRPTIME_D 1

/*
 * Define this if compiling with SOCKS (the firewall traversal library).
 * Also, you must define connect, getsockname, bind, accept, listen, and
 * select to their R-versions.
 */
/* #undef	SOCKS */
/* #undef	SOCKS4 */
/* #undef	SOCKS5 */
/* #undef	connect */
/* #undef	getsockname */
/* #undef	bind */
/* #undef	accept */
/* #undef	listen */
/* #undef	select */
/* #undef	dup */
/* #undef	dup2 */
/* #undef	fclose */
/* #undef	gethostbyname */
/* #undef	getpeername */
/* #undef	read */
/* #undef	recv */
/* #undef	recvfrom */
/* #undef	rresvport */
/* #undef	send */
/* #undef	sendto */
/* #undef	shutdown */
/* #undef	write */
