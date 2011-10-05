dnl $Id$
dnl
dnl some code to get roken working
dnl
dnl rk_ROKEN(subdir)
dnl
AC_DEFUN([rk_ROKEN], [

AC_REQUIRE([rk_CONFIG_HEADER])

DIR_roken=roken
LIB_roken='$(top_builddir)/$1/libroken.la'
INCLUDES_roken='-I$(top_builddir)/$1 -I$(top_srcdir)/$1'

dnl Checks for programs
AC_REQUIRE([AC_PROG_CC])
AC_REQUIRE([AC_PROG_AWK])
AC_REQUIRE([AC_OBJEXT])
AC_REQUIRE([AC_EXEEXT])
AC_REQUIRE([AC_PROG_LIBTOOL])

AC_REQUIRE([AC_MIPS_ABI])

AC_DEFINE(rk_PATH_DELIM, '/', [Path name delimiter])

dnl C characteristics

AC_REQUIRE([AC_C___ATTRIBUTE__])
AC_REQUIRE([AC_C_INLINE])
AC_REQUIRE([AC_C_CONST])
rk_WFLAGS(-Wall -Wmissing-prototypes -Wpointer-arith -Wbad-function-cast -Wmissing-declarations -Wnested-externs)

AC_REQUIRE([rk_DB])

dnl C types

AC_REQUIRE([AC_TYPE_SIZE_T])
AC_HAVE_TYPE([ssize_t],[#include <unistd.h>])
AC_REQUIRE([AC_TYPE_PID_T])
AC_REQUIRE([AC_TYPE_UID_T])
AC_HAVE_TYPE([long long])

AC_REQUIRE([rk_RETSIGTYPE])

dnl Checks for header files.
AC_REQUIRE([AC_HEADER_STDC])
AC_REQUIRE([AC_HEADER_TIME])

AC_CHECK_HEADERS([\
	arpa/inet.h				\
	config.h				\
	crypt.h					\
	dirent.h				\
	errno.h					\
	err.h					\
	fcntl.h					\
	fnmatch.h				\
	grp.h					\
	ifaddrs.h				\
	netinet/in.h				\
	netinet/in6.h				\
	netinet/in_systm.h			\
	netinet6/in6.h				\
	paths.h					\
	poll.h					\
	pwd.h					\
	rpcsvc/ypclnt.h				\
	search.h				\
	shadow.h				\
	stdint.h				\
	sys/bswap.h				\
	sys/ioctl.h				\
	sys/mman.h				\
	sys/param.h				\
	sys/resource.h				\
	sys/sockio.h				\
	sys/stat.h				\
	sys/time.h				\
	sys/tty.h				\
	sys/types.h				\
	sys/uio.h				\
	sys/utsname.h				\
	sys/wait.h				\
	syslog.h				\
	termios.h				\
	winsock2.h				\
	ws2tcpip.h				\
	unistd.h				\
	userconf.h				\
	usersec.h				\
	util.h					\
])

AC_HAVE_TYPE([uintptr_t],[#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif])

dnl Sunpro 5.2 has a vis.h which is something different.
AC_CHECK_HEADERS(vis.h, , , [
#include <vis.h>
#ifndef VIS_SP
#error invis
#endif])
	
AC_CHECK_HEADERS(netdb.h, , , [AC_INCLUDES_DEFAULT
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
])

AC_CHECK_HEADERS(sys/socket.h, , , [AC_INCLUDES_DEFAULT
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
])

AC_CHECK_HEADERS(net/if.h, , , [AC_INCLUDES_DEFAULT
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#if HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif])

AC_CHECK_HEADERS(netinet6/in6_var.h, , , [AC_INCLUDES_DEFAULT
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#if HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#ifdef HAVE_NETINET6_IN6_H
#include <netinet6/in6.h>
#endif
])

AC_CHECK_HEADERS(sys/sysctl.h, , , [AC_INCLUDES_DEFAULT
#ifdef HAVE_SYS_PARAM_H
#include <sys/param.h>
#endif
])

AC_CHECK_HEADERS(sys/proc.h, , , [AC_INCLUDES_DEFAULT
#ifdef HAVE_SYS_PARAM_H
#include <sys/param.h>
#endif
])

AC_REQUIRE([CHECK_NETINET_IP_AND_TCP])

AM_CONDITIONAL(have_err_h, test "$ac_cv_header_err_h" = yes)
AM_CONDITIONAL(have_ifaddrs_h, test "$ac_cv_header_ifaddrs_h" = yes)
AM_CONDITIONAL(have_search_h, test "$ac_cv_header_search_h" = yes)
AM_CONDITIONAL(have_vis_h, test "$ac_cv_header_vis_h" = yes)

dnl Check for functions and libraries

AC_FIND_FUNC(socket, socket)
AC_FIND_FUNC(gethostbyname, nsl)
AC_FIND_FUNC(syslog, syslog)

AC_KRB_IPV6

AC_FIND_FUNC(gethostbyname2, inet6 ip6)

rk_RESOLV

AC_BROKEN_SNPRINTF
AC_BROKEN_VSNPRINTF

AC_BROKEN_GLOB
if test "$ac_cv_func_glob_working" != yes; then
	AC_LIBOBJ(glob)
fi
AM_CONDITIONAL(have_glob_h, test "$ac_cv_func_glob_working" = yes)


AC_CHECK_FUNCS([				\
	asnprintf				\
	asprintf				\
	atexit					\
	cgetent					\
	getconfattr				\
	getprogname				\
	getrlimit				\
	getspnam				\
	initstate				\
	issetugid				\
	on_exit					\
	poll					\
	random					\
	setprogname				\
	setstate				\
	strsvis					\
	strsvisx				\
	strunvis				\
	strvis					\
	strvisx					\
	svis					\
	sysconf					\
	sysctl					\
	tdelete					\
	tfind					\
	twalk					\
	uname					\
	unvis					\
	vasnprintf				\
	vasprintf				\
	vis					\
])

if test "$ac_cv_func_cgetent" = no; then
	AC_LIBOBJ(getcap)
fi
AM_CONDITIONAL(have_cgetent, test "$ac_cv_func_cgetent" = yes)

AC_REQUIRE([AC_FUNC_GETLOGIN])

AC_REQUIRE([AC_FUNC_MMAP])

AC_FIND_FUNC_NO_LIBS(getsockopt,,
[#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif],
[0,0,0,0,0])
AC_FIND_FUNC_NO_LIBS(setsockopt,,
[#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif],
[0,0,0,0,0])

AC_FIND_IF_NOT_BROKEN(hstrerror, resolv,
[#ifdef HAVE_NETDB_H
#include <netdb.h>
#endif],
17)
AC_NEED_PROTO([
#ifdef HAVE_NETDB_H
#include <netdb.h>
#endif],
hstrerror)

AC_FOREACH([rk_func], [asprintf vasprintf asnprintf vasnprintf],
	[AC_NEED_PROTO([
	#include <stdio.h>
	#include <string.h>],
	rk_func)])

AC_FIND_FUNC_NO_LIBS(bswap16,,
[#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_SYS_BSWAP_H
#include <sys/bswap.h>
#endif],0)

AC_FIND_FUNC_NO_LIBS(bswap32,,
[#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_SYS_BSWAP_H
#include <sys/bswap.h>
#endif],0)

AC_FIND_FUNC_NO_LIBS(pidfile,util,
[#ifdef HAVE_UTIL_H
#include <util.h>
#endif],0)

AC_FIND_IF_NOT_BROKEN(getaddrinfo,,
[#ifdef HAVE_NETDB_H
#include <netdb.h>
#endif
#ifdef HAVE_WS2TCPIP_H
#include <ws2tcpip.h>
#endif],[0,0,0,0])

AC_FIND_IF_NOT_BROKEN(getnameinfo,,
[#ifdef HAVE_NETDB_H
#include <netdb.h>
#endif
#ifdef HAVE_WS2TCPIP_H
#include <ws2tcpip.h>
#endif],[0,0,0,0,0,0,0])

AC_FIND_IF_NOT_BROKEN(freeaddrinfo,,
[#ifdef HAVE_NETDB_H
#include <netdb.h>
#endif
#ifdef HAVE_WS2TCPIP_H
#include <ws2tcpip.h>
#endif],[0])

AC_FIND_IF_NOT_BROKEN(gai_strerror,,
[#ifdef HAVE_NETDB_H
#include <netdb.h>
#endif
#ifdef HAVE_WS2TCPIP_H
#include <ws2tcpip.h>
#endif],[0])

dnl Darwin is weird, and in some senses not unix, launchd doesn't want
dnl servers to use daemon(), so its deprecated.
case "$host_os" in
	darwin*)
		;;
	*)
		AC_DEFINE([SUPPORT_DETACH], 1,
		    [Define if os support want to detach is daemonens.])
		AC_BROKEN([daemon]) ;;
esac

AC_BROKEN([					\
	chown					\
	copyhostent				\
	closefrom				\
	ecalloc					\
	emalloc					\
	erealloc				\
	estrdup					\
	err					\
	errx					\
	fchown					\
	flock					\
	fnmatch					\
	freehostent				\
	getcwd					\
	getdtablesize				\
	getegid					\
	geteuid					\
	getgid					\
	gethostname				\
	getifaddrs				\
	getipnodebyaddr				\
	getipnodebyname				\
	getopt					\
	gettimeofday				\
	getuid					\
	getusershell				\
	initgroups				\
	innetgr					\
	iruserok				\
	localtime_r				\
	lstat					\
	memmove					\
	mkstemp					\
	putenv					\
	rcmd					\
	readv					\
	recvmsg					\
	sendmsg					\
	setegid					\
	setenv					\
	seteuid					\
	strcasecmp				\
	strdup					\
	strerror				\
	strftime				\
	strlcat					\
	strlcpy					\
	strlwr					\
	strncasecmp				\
	strndup					\
	strnlen					\
	strptime				\
	strsep					\
	strsep_copy				\
	strtok_r				\
	strupr					\
	swab					\
	tsearch					\
	timegm					\
	unsetenv				\
	verr					\
	verrx					\
	vsyslog					\
	vwarn					\
	vwarnx					\
	warn					\
	warnx					\
	writev					\
])

AM_CONDITIONAL(have_fnmatch_h,
	test "$ac_cv_header_fnmatch_h" = yes -a "$ac_cv_func_fnmatch" = yes)

AC_FOREACH([rk_func], [strndup strsep strtok_r],
	[AC_NEED_PROTO([#include <string.h>], rk_func)])

AC_FOREACH([rk_func], [strsvis strsvisx strunvis strvis strvisx svis unvis vis],
[AC_NEED_PROTO([#ifdef HAVE_VIS_H
#include <vis.h>
#endif], rk_func)])

AC_MSG_CHECKING([checking for dirfd])
AC_LINK_IFELSE([AC_LANG_PROGRAM([[#include <sys/types.h>
#ifdef HAVE_DIRENT_H
#include <dirent.h>
#endif
]],
	[[DIR *d = 0; dirfd(d);]])],
	[ac_rk_have_dirfd=yes], [ac_rk_have_dirfd=no])
if test "$ac_rk_have_dirfd" = "yes" ; then
	AC_DEFINE_UNQUOTED(HAVE_DIRFD, 1, [have a dirfd function/macro])
fi
AC_MSG_RESULT($ac_rk_have_dirfd)

AC_HAVE_STRUCT_FIELD(DIR, dd_fd, [#include <sys/types.h>
#ifdef HAVE_DIRENT_H
#include <dirent.h>
#endif])


AC_BROKEN2(inet_aton,
[#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#ifdef HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif],
[0,0])

AC_BROKEN2(inet_ntop,
[#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#ifdef HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif],
[0, 0, 0, 0])

AC_BROKEN2(inet_pton,
[#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#ifdef HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif],
[0,0,0])

dnl
dnl Check for sa_len in struct sockaddr, 
dnl needs to come before the getnameinfo test
dnl
AC_HAVE_STRUCT_FIELD(struct sockaddr, sa_len, [#include <sys/types.h>
#include <sys/socket.h>])

if test "$ac_cv_func_getaddrinfo" = "yes"; then
  rk_BROKEN_GETADDRINFO
  if test "$ac_cv_func_getaddrinfo_numserv" = no; then
	AC_LIBOBJ(getaddrinfo)
	AC_LIBOBJ(freeaddrinfo)
  fi
fi

AC_NEED_PROTO([#include <stdlib.h>], setenv)
AC_NEED_PROTO([#include <stdlib.h>], unsetenv)
AC_NEED_PROTO([#include <unistd.h>], gethostname)
AC_NEED_PROTO([#include <unistd.h>], mkstemp)
AC_NEED_PROTO([#include <unistd.h>], getusershell)
AC_NEED_PROTO([#include <unistd.h>], daemon)
AC_NEED_PROTO([
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#ifdef HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif
#ifdef HAVE_NETDB_H
#include <netdb.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif],
iruserok)

AC_NEED_PROTO([
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#ifdef HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif],
inet_aton)

AC_FIND_FUNC_NO_LIBS(crypt, crypt)dnl

AC_REQUIRE([rk_BROKEN_REALLOC])dnl

dnl AC_KRB_FUNC_GETCWD_BROKEN

dnl strerror_r is great fun, on linux it exists before sus catched up,
dnl so the return type is diffrent, lets check for both

AC_PROTO_COMPAT([
#include <stdio.h>
#include <string.h>
],
strerror_r, int strerror_r(int, char *, size_t))

AC_CHECK_FUNC([strerror_r],
    [AC_DEFINE_UNQUOTED(HAVE_STRERROR_R, 1,
        [Define if you have the function strerror_r.])])

dnl
dnl Checks for prototypes and declarations
dnl

AC_PROTO_COMPAT([
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#ifdef HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif
#ifdef HAVE_NETDB_H
#include <netdb.h>
#endif
],
gethostbyname, struct hostent *gethostbyname(const char *))

AC_PROTO_COMPAT([
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#ifdef HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif
#ifdef HAVE_NETDB_H
#include <netdb.h>
#endif
],
gethostbyaddr, struct hostent *gethostbyaddr(const void *, size_t, int))

AC_PROTO_COMPAT([
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#ifdef HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif
#ifdef HAVE_NETDB_H
#include <netdb.h>
#endif
],
getservbyname, struct servent *getservbyname(const char *, const char *))

AC_PROTO_COMPAT([
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
],
getsockname, int getsockname(int, struct sockaddr*, socklen_t*))

AC_PROTO_COMPAT([
#ifdef HAVE_SYSLOG_H
#include <syslog.h>
#endif
],
openlog, void openlog(const char *, int, int))

AC_NEED_PROTO([
#ifdef HAVE_CRYPT_H
#include <crypt.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
],
crypt)

dnl variables

rk_CHECK_VAR(h_errno, 
[#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_NETDB_H
#include <netdb.h>
#endif
#ifdef HAVE_WS2TCPIP_H
#include <ws2tcpip.h>
#endif
])

rk_CHECK_VAR(h_errlist, 
[#ifdef HAVE_NETDB_H
#include <netdb.h>
#endif
#ifdef HAVE_WS2TCPIP_H
#include <ws2tcpip.h>
#endif])

rk_CHECK_VAR(h_nerr, 
[#ifdef HAVE_NETDB_H
#include <netdb.h>
#endif
#ifdef HAVE_WS2TCPIP_H
#include <ws2tcpip.h>
#endif])

rk_CHECK_VAR([__progname], 
[#ifdef HAVE_ERR_H
#include <err.h>
#endif])

AC_CHECK_DECLS([optarg, optind, opterr, optopt, environ],[],[],[
#include <stdlib.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif])

dnl
dnl Check for fields in struct tm
dnl

AC_HAVE_STRUCT_FIELD(struct tm, tm_gmtoff, [#include <time.h>])
AC_HAVE_STRUCT_FIELD(struct tm, tm_zone, [#include <time.h>])

dnl
dnl or do we have a variable `timezone' ?
dnl

rk_CHECK_VAR(timezone,[#include <time.h>])
rk_CHECK_VAR(altzone,[#include <time.h>])

AC_HAVE_TYPE([sa_family_t],[
#include <sys/types.h>
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#ifdef HAVE_WS2TCPIP_H
#include <ws2tcpip.h>
#endif])
AC_HAVE_TYPE([socklen_t],[
#include <sys/types.h>
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#ifdef HAVE_WS2TCPIP_H
#include <ws2tcpip.h>
#endif])
AC_HAVE_TYPE([struct sockaddr], [
#include <sys/types.h>
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#ifdef HAVE_WS2TCPIP_H
#include <ws2tcpip.h>
#endif])
AC_HAVE_TYPE([struct sockaddr_storage], [
#include <sys/types.h>
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#ifdef HAVE_WS2TCPIP_H
#include <ws2tcpip.h>
#endif])
AC_HAVE_TYPE([struct addrinfo], [
#include <sys/types.h>
#ifdef HAVE_NETDB_H
#include <netdb.h>
#endif
#ifdef HAVE_WS2TCPIP_H
#include <ws2tcpip.h>
#endif])
AC_HAVE_TYPE([struct ifaddrs], [#include <ifaddrs.h>])
AC_HAVE_TYPE([struct iovec],[
#include <sys/types.h>
#include <sys/uio.h>
])
AC_HAVE_TYPE([struct msghdr],[
#include <sys/types.h>
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#ifdef HAVE_WS2TCPIP_H
#include <ws2tcpip.h>
#endif])

dnl
dnl Check for struct winsize
dnl

AC_KRB_STRUCT_WINSIZE

dnl
dnl Check for struct spwd
dnl

AC_KRB_STRUCT_SPWD

#
# Check if we want samba's socket wrapper
#

samba_SOCKET_WRAPPER

dnl won't work with automake
dnl moved to AC_OUTPUT in configure.in
dnl AC_CONFIG_FILES($1/Makefile)

LIB_roken="${LIB_roken} \$(LIB_crypt) \$(LIB_dbopen)"

AC_SUBST(DIR_roken)dnl
AC_SUBST(LIB_roken)dnl
AC_SUBST(INCLUDES_roken)dnl
])
