dnl $Id: roken-frag.m4,v 1.34 2001/11/30 03:29:47 assar Exp $
dnl
dnl some code to get roken working
dnl
dnl rk_ROKEN(subdir)
dnl
AC_DEFUN(rk_ROKEN, [

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

dnl C characteristics

AC_REQUIRE([AC_C___ATTRIBUTE__])
AC_REQUIRE([AC_C_INLINE])
AC_REQUIRE([AC_C_CONST])
AC_WFLAGS(-Wall -Wmissing-prototypes -Wpointer-arith -Wbad-function-cast -Wmissing-declarations -Wnested-externs)

AC_REQUIRE([rk_DB])

dnl C types

AC_REQUIRE([AC_TYPE_SIZE_T])
AC_CHECK_TYPE_EXTRA(ssize_t, int, [#include <unistd.h>])
AC_REQUIRE([AC_TYPE_PID_T])
AC_REQUIRE([AC_TYPE_UID_T])
AC_HAVE_TYPE([long long])

AC_REQUIRE([rk_RETSIGTYPE])

dnl Checks for header files.
AC_REQUIRE([AC_HEADER_STDC])
AC_REQUIRE([AC_HEADER_TIME])

AC_CHECK_HEADERS([\
	arpa/inet.h				\
	arpa/nameser.h				\
	config.h				\
	crypt.h					\
	dirent.h				\
	errno.h					\
	err.h					\
	fcntl.h					\
	grp.h					\
	ifaddrs.h				\
	net/if.h				\
	netdb.h					\
	netinet/in.h				\
	netinet/in6.h				\
	netinet/in_systm.h			\
	netinet6/in6.h				\
	netinet6/in6_var.h			\
	paths.h					\
	pwd.h					\
	resolv.h				\
	rpcsvc/ypclnt.h				\
	shadow.h				\
	sys/bswap.h				\
	sys/ioctl.h				\
	sys/param.h				\
	sys/proc.h				\
	sys/resource.h				\
	sys/socket.h				\
	sys/sockio.h				\
	sys/stat.h				\
	sys/sysctl.h				\
	sys/time.h				\
	sys/tty.h				\
	sys/types.h				\
	sys/uio.h				\
	sys/utsname.h				\
	sys/wait.h				\
	syslog.h				\
	termios.h				\
	unistd.h				\
	userconf.h				\
	usersec.h				\
	util.h					\
	vis.h					\
])
	
AC_REQUIRE([CHECK_NETINET_IP_AND_TCP])

AM_CONDITIONAL(have_err_h, test "$ac_cv_header_err_h" = yes)
AM_CONDITIONAL(have_fnmatch_h, test "$ac_cv_header_fnmatch_h" = yes)
AM_CONDITIONAL(have_ifaddrs_h, test "$ac_cv_header_ifaddrs_h" = yes)
AM_CONDITIONAL(have_vis_h, test "$ac_cv_header_vis_h" = yes)

dnl Check for functions and libraries

AC_FIND_FUNC(socket, socket)
AC_FIND_FUNC(gethostbyname, nsl)
AC_FIND_FUNC(syslog, syslog)

AC_KRB_IPV6

AC_FIND_FUNC(gethostbyname2, inet6 ip6)

AC_FIND_FUNC(res_search, resolv,
[
#include <stdio.h>
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#ifdef HAVE_ARPA_NAMESER_H
#include <arpa/nameser.h>
#endif
#ifdef HAVE_RESOLV_H
#include <resolv.h>
#endif
],
[0,0,0,0,0])

AC_FIND_FUNC(dn_expand, resolv,
[
#include <stdio.h>
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#ifdef HAVE_ARPA_NAMESER_H
#include <arpa/nameser.h>
#endif
#ifdef HAVE_RESOLV_H
#include <resolv.h>
#endif
],
[0,0,0,0,0])

AC_BROKEN_SNPRINTF
AC_BROKEN_VSNPRINTF

AC_BROKEN_GLOB
if test "$ac_cv_func_glob_working" != yes; then
	LIBOBJS="$LIBOBJS glob.o"
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
	random					\
	setprogname				\
	setstate				\
	strsvis					\
	strunvis				\
	strvis					\
	strvisx					\
	svis					\
	sysconf					\
	sysctl					\
	uname					\
	unvis					\
	vasnprintf				\
	vasprintf				\
	vis					\
])

if test "$ac_cv_func_cgetent" = no; then
	LIBOBJS="$LIBOBJS getcap.o"
fi

AC_REQUIRE([AC_FUNC_GETLOGIN])

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
if test "$ac_cv_func_hstrerror" = yes; then
AC_NEED_PROTO([
#ifdef HAVE_NETDB_H
#include <netdb.h>
#endif],
hstrerror)
fi

dnl sigh, wish this could be done in a loop
if test "$ac_cv_func_asprintf" = yes; then
AC_NEED_PROTO([
#include <stdio.h>
#include <string.h>],
asprintf)dnl
fi
if test "$ac_cv_func_vasprintf" = yes; then
AC_NEED_PROTO([
#include <stdio.h>
#include <string.h>],
vasprintf)dnl
fi
if test "$ac_cv_func_asnprintf" = yes; then
AC_NEED_PROTO([
#include <stdio.h>
#include <string.h>],
asnprintf)dnl
fi
if test "$ac_cv_func_vasnprintf" = yes; then
AC_NEED_PROTO([
#include <stdio.h>
#include <string.h>],
vasnprintf)dnl
fi

AC_FIND_FUNC_NO_LIBS(bswap16,,
[#ifdef HAVE_SYS_BSWAP_H
#include <sys/bswap.h>
#endif],0)

AC_FIND_FUNC_NO_LIBS(bswap32,,
[#ifdef HAVE_SYS_BSWAP_H
#include <sys/bswap.h>
#endif],0)

AC_FIND_FUNC_NO_LIBS(pidfile,util,
[#ifdef HAVE_UTIL_H
#include <util.h>
#endif],0)

AC_FIND_IF_NOT_BROKEN(getaddrinfo,,
[#ifdef HAVE_NETDB_H
#include <netdb.h>
#endif],[0,0,0,0])

AC_FIND_IF_NOT_BROKEN(getnameinfo,,
[#ifdef HAVE_NETDB_H
#include <netdb.h>
#endif],[0,0,0,0,0,0,0])

AC_FIND_IF_NOT_BROKEN(freeaddrinfo,,
[#ifdef HAVE_NETDB_H
#include <netdb.h>
#endif],[0])

AC_FIND_IF_NOT_BROKEN(gai_strerror,,
[#ifdef HAVE_NETDB_H
#include <netdb.h>
#endif],[0])

AC_BROKEN([					\
	chown					\
	copyhostent				\
	daemon					\
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

if test "$ac_cv_func_getnameinfo" = "yes"; then
  rk_BROKEN_GETNAMEINFO
  if test "$ac_cv_func_getnameinfo_broken" = yes; then
    LIBOBJS="$LIBOBJS getnameinfo.o"
  fi
fi

if test "$ac_cv_func_getaddrinfo" = "yes"; then
  rk_BROKEN_GETADDRINFO
  if test "$ac_cv_func_getaddrinfo_numserv" = no; then
    LIBOBJS="$LIBOBJS getaddrinfo.o freeaddrinfo.o"
  fi
fi

AC_NEED_PROTO([#include <stdlib.h>], setenv)
AC_NEED_PROTO([#include <stdlib.h>], unsetenv)
AC_NEED_PROTO([#include <unistd.h>], gethostname)
AC_NEED_PROTO([#include <unistd.h>], mkstemp)
AC_NEED_PROTO([#include <unistd.h>], getusershell)

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

AC_NEED_PROTO([
#include <string.h>
],
strtok_r)

AC_NEED_PROTO([
#include <string.h>
],
strsep)

dnl variables

rk_CHECK_VAR(h_errno, 
[#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_NETDB_H
#include <netdb.h>
#endif])

rk_CHECK_VAR(h_errlist, 
[#ifdef HAVE_NETDB_H
#include <netdb.h>
#endif])

rk_CHECK_VAR(h_nerr, 
[#ifdef HAVE_NETDB_H
#include <netdb.h>
#endif])

rk_CHECK_VAR([__progname], 
[#ifdef HAVE_ERR_H
#include <err.h>
#endif])

AC_CHECK_DECLARATION([#include <stdlib.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif], optarg)
AC_CHECK_DECLARATION([#include <stdlib.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif], optind)
AC_CHECK_DECLARATION([#include <stdlib.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif], opterr)
AC_CHECK_DECLARATION([#include <stdlib.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif], optopt)

AC_CHECK_DECLARATION([#include <stdlib.h>], environ)

dnl
dnl Check for fields in struct tm
dnl

AC_HAVE_STRUCT_FIELD(struct tm, tm_gmtoff, [#include <time.h>])
AC_HAVE_STRUCT_FIELD(struct tm, tm_zone, [#include <time.h>])

dnl
dnl or do we have a variable `timezone' ?
dnl

rk_CHECK_VAR(timezone,[#include <time.h>])

AC_HAVE_TYPE([sa_family_t],[#include <sys/socket.h>])
AC_HAVE_TYPE([socklen_t],[#include <sys/socket.h>])
AC_HAVE_TYPE([struct sockaddr], [#include <sys/socket.h>])
AC_HAVE_TYPE([struct sockaddr_storage], [#include <sys/socket.h>])
AC_HAVE_TYPE([struct addrinfo], [#include <netdb.h>])
AC_HAVE_TYPE([struct ifaddrs], [#include <ifaddrs.h>])

dnl
dnl Check for struct winsize
dnl

AC_KRB_STRUCT_WINSIZE

dnl
dnl Check for struct spwd
dnl

AC_KRB_STRUCT_SPWD

dnl won't work with automake
dnl moved to AC_OUTPUT in configure.in
dnl AC_CONFIG_FILES($1/Makefile)

LIB_roken="${LIB_roken} \$(LIB_crypt) \$(LIB_dbopen)"

AC_SUBST(DIR_roken)dnl
AC_SUBST(LIB_roken)dnl
AC_SUBST(INCLUDES_roken)dnl
])
