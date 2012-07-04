#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ(2.56)
AC_INIT(ldns, 1.6.13, libdns@nlnetlabs.nl,libdns)
AC_CONFIG_SRCDIR([ldns-read-zone.c])
sinclude(../acx_nlnetlabs.m4)

OURCPPFLAGS=''
CPPFLAGS=${CPPFLAGS:-${OURCPPFLAGS}}
OURCFLAGS='-g'
CFLAGS=${CFLAGS:-${OURCFLAGS}}
AC_DEFINE(WINVER, 0x0502, [the version of the windows API enabled])

AC_AIX
# Checks for programs.
AC_PROG_CC
AC_PROG_MAKE_SET
AC_CHECK_PROGS(libtool, [glibtool libtool15 libtool], [../libtool])

ACX_CHECK_COMPILER_FLAG(std=c99, [C99FLAG="-std=c99"])
ACX_CHECK_COMPILER_FLAG(xc99, [C99FLAG="-xc99"])

AC_ARG_ENABLE(rpath,
        [AC_HELP_STRING([--disable-rpath], [Disable hardcoded rpath (default=enabled)])],
	enable_rpath=$enableval, enable_rpath=yes)

if test "x$enable_rpath" = xyes; then
    RPATH_VAL="-Wl,-rpath=\${libdir}"
fi


AC_TYPE_SIZE_T
ACX_CHECK_COMPILER_FLAG(O2, [CFLAGS="$CFLAGS -O2"])

ACX_CHECK_COMPILER_FLAG_NEEDED($C99FLAG -D__EXTENSIONS__ -D_BSD_SOURCE -D_POSIX_C_SOURCE=200112 -D_XOPEN_SOURCE=600,
[
#include "confdefs.h"
#include <stdlib.h>
#include <ctype.h>
#include <sys/time.h>
#ifdef HAVE_TIME_H
#include <time.h>
#endif
#include <unistd.h>
#ifdef HAVE_GETOPT_H
#include <getopt.h>
#endif

int test() {
	int a;
	char **opts = NULL;
	struct timeval tv;
	char *t;
	time_t time = 0;
	char *buf = NULL;
	t = ctime_r(&time, buf);
	tv.tv_usec = 10;
	srandom(32);
	a = getopt(2, opts, "a");
	a = isascii(32);
	return a;
}
], [CFLAGS="$CFLAGS $C99FLAG -D__EXTENSIONS__ -D_BSD_SOURCE -D_POSIX_C_SOURCE=200112 -D_XOPEN_SOURCE=600"])


ACX_CHECK_COMPILER_FLAG_NEEDED($C99FLAG, [#include <stdbool.h>], [CFLAGS="$CFLAGS $C99FLAG"])

AC_C_INLINE
AC_CHECK_TYPE(int8_t, char)
AC_CHECK_TYPE(int16_t, short)
AC_CHECK_TYPE(int32_t, int)
AC_CHECK_TYPE(int64_t, long long)
AC_CHECK_TYPE(uint8_t, unsigned char)
AC_CHECK_TYPE(uint16_t, unsigned short)
AC_CHECK_TYPE(uint32_t, unsigned int)
AC_CHECK_TYPE(uint64_t, unsigned long long)
AC_CHECK_TYPE(ssize_t, int)

AC_CHECK_HEADERS([sys/types.h getopt.h stdlib.h stdio.h assert.h netinet/in.h ctype.h time.h pcap.h arpa/inet.h sys/time.h sys/socket.h sys/select.h netdb.h],,, [AC_INCLUDES_DEFAULT])
AC_CHECK_HEADERS([winsock2.h ws2tcpip.h],,, [AC_INCLUDES_DEFAULT])
AC_CHECK_HEADERS([netinet/in_systm.h net/if.h netinet/ip.h netinet/udp.h netinet/igmp.h netinet/if_ether.h netinet/ip6.h],,, [
AC_INCLUDES_DEFAULT 
#ifdef HAVE_NETINET_IN_SYSTM_H
#include <netinet/in_systm.h>
#endif
#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#ifdef HAVE_NET_IF_H
#include <net/if.h>
#endif])

ACX_TYPE_SOCKLEN_T
AC_CHECK_HEADERS([sys/param.h sys/mount.h],,,
[AC_INCLUDES_DEFAULT]
[
  [
   #if HAVE_SYS_PARAM_H
   # include <sys/param.h>
   #endif
  ]
])
AC_CHECK_TYPE(in_addr_t, [], [AC_DEFINE([in_addr_t], [uint32_t], [in_addr_t])], [
#if HAVE_SYS_TYPES_H
# include <sys/types.h>
#endif
#if HAVE_NETINET_IN_H
# include <netinet/in.h>
#endif])
AC_CHECK_TYPE(in_port_t, [], [AC_DEFINE([in_port_t], [uint16_t], [in_port_t])], [
#if HAVE_SYS_TYPES_H
# include <sys/types.h>
#endif
#if HAVE_NETINET_IN_H
# include <netinet/in.h>
#endif])

# check to see if libraries are needed for these functions.
AC_SEARCH_LIBS(socket, socket)
tmp_LIBS="$LIBS"
LIBS=
AC_SEARCH_LIBS(inet_pton, nsl)
AC_SUBST(LIBNSL_LIBS,$LIBS)
LIBS="$tmp_LIBS"

tmp_CPPFLAGS="$CPPFLAGS"
tmp_LDFLAGS="$LDFLAGS"
tmp_LIBS="$LIBS"

ACX_WITH_SSL_OPTIONAL

AC_SUBST(LIBSSL_CPPFLAGS)
AC_SUBST(LIBSSL_LDFLAGS)
AC_SUBST(LIBSSL_LIBS)

CPPFLAGS="$tmp_CPPFLAGS"
LDFLAGS="$tmp_LDFLAGS"
LIBS="$tmp_LIBS"

# use libcrypto to check the following:
tmp_LDFLAGS="$LDFLAGS"
tmp_LIBS="$LIBS"
LDFLAGS="$LDFLAGS $LIBSSL_LDFLAGS"
LIBS="$LIBS $LIBSSL_LIBS"
AC_ARG_ENABLE(sha2, AC_HELP_STRING([--disable-sha2], [Disable SHA256 and SHA512 RRSIG support]))
case "$enable_sha2" in
    no)
        ;;
    *)
        AC_MSG_CHECKING(for SHA256 and SHA512)
        AC_CHECK_FUNC(SHA256_Init,, [
            AC_MSG_ERROR([SHA2 enabled, but no SHA2 functions found in OpenSSL])
        ])
        AC_DEFINE_UNQUOTED([USE_SHA2], [1], [Define this to enable SHA256 and SHA512 support.])
        ;;
esac

AC_ARG_ENABLE(gost, AC_HELP_STRING([--disable-gost], [Disable GOST support]))
case "$enable_gost" in
    no)
        ;;
    *)
        if test "x$HAVE_SSL" != "xyes"; then
            AC_MSG_ERROR([GOST enabled, but no SSL support])
        fi
        AC_MSG_CHECKING(for GOST)
        AC_CHECK_FUNC(EVP_PKEY_set_type_str, [],[AC_MSG_ERROR([OpenSSL >= 1.0.0 is needed for GOST support or rerun with --disable-gost])])
        AC_CHECK_FUNC(EC_KEY_new, [], [AC_MSG_ERROR([No ECC functions found in OpenSSL: please upgrade OpenSSL or rerun with --disable-gost])])
        AC_DEFINE_UNQUOTED([USE_GOST], [1], [Define this to enable GOST support.])
        ;;
esac

AC_ARG_ENABLE(ecdsa, AC_HELP_STRING([--enable-ecdsa], [Enable ECDSA support, experimental]))
case "$enable_ecdsa" in
    yes)
	AC_CHECK_FUNC(ECDSA_sign,,[AC_MSG_ERROR([OpenSSL does not support ECDSA])])
        AC_DEFINE_UNQUOTED([USE_ECDSA], [1], [Define this to enable ECDSA support.])
        AC_WARN([
        *****************************************************************
        *** YOU HAVE ENABLED ECDSA WHICH IS EXPERIMENTAL AT THIS TIME ***
        *** PLEASE DO NOT USE THIS ON THE PUBLIC INTERNET             ***
        *****************************************************************])
      ;;
    no|*)
      ;;
esac

LDFLAGS="$tmp_LDFLAGS"
LIBS="$tmp_LIBS"

#AC_CHECK_HEADER(ldns/ldns.h,,  [
#	AC_MSG_ERROR([Can't find ldns headers (make copy-headers in devel source.)])
#	], [AC_INCLUDES_DEFAULT]
#)

AC_CHECK_LIB(pcap, pcap_open_offline,, [
	AC_MSG_WARN([Can't find pcap library (needed for ldns-dpa, will not build dpa now.)])
	]
)


AC_DEFUN([AC_CHECK_GETADDRINFO_WITH_INCLUDES],
[AC_REQUIRE([AC_PROG_CC])
AC_MSG_CHECKING(for getaddrinfo)
AC_CACHE_VAL(ac_cv_func_getaddrinfo,
[ac_cv_func_getaddrinfo=no
AC_LINK_IFELSE( 
[
#ifdef __cplusplus
extern "C"
{
#endif
char* getaddrinfo();
char* (*f) () = getaddrinfo;
#ifdef __cplusplus
}
#endif
int main() {
	;
	return 0;
}
],
dnl this case on linux, solaris, bsd
[ac_cv_func_getaddrinfo="yes"],
dnl no quick getaddrinfo, try mingw32 and winsock2 library.
ORIGLIBS="$LIBS"
LIBS="$LIBS -lws2_32"
AC_LINK_IFELSE(
AC_LANG_PROGRAM(
[
#ifdef HAVE_WS2TCPIP_H
#include <ws2tcpip.h>
#endif
],
[
	(void)getaddrinfo(NULL, NULL, NULL, NULL);
]
), 
[ac_cv_func_getaddrinfo="yes"
LDFLAGS="$LDFLAGS -lws2_32"
],
[ac_cv_func_getaddrinfo="no" 
LIBS="$ORIGLIBS"
])
)
])

AC_MSG_RESULT($ac_cv_func_getaddrinfo)
if test $ac_cv_func_getaddrinfo = yes; then
  AC_DEFINE(HAVE_GETADDRINFO, 1, [Whether getaddrinfo is available])
fi
])dnl

AC_CHECK_GETADDRINFO_WITH_INCLUDES

AC_FUNC_FORK
AC_CHECK_FUNCS(isblank srandom random sleep)
AC_CHECK_DECLS([in6addr_any],,,[AC_INCLUDES_DEFAULT
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#ifdef HAVE_TIME_H
#include <time.h>
#endif
#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif
#ifdef HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif
#ifdef HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif
#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#ifdef HAVE_NETINET_UDP_H
#include <netinet/udp.h>
#endif
#ifdef HAVE_NETINET_IGMP_H
#include <netinet/igmp.h>
#endif
])

# check for ldns
AC_ARG_WITH(ldns, 
	AC_HELP_STRING([--with-ldns=PATH        specify prefix of path of ldns library to use])
	,
	[
		specialldnsdir="$withval"
		CPPFLAGS="$CPPFLAGS -I$withval/include"
		LDFLAGS="-L$withval -L$withval/lib $LDFLAGS"
		LDNSDIR="$withval"
	]
)

# check for ldns development source tree
AC_MSG_CHECKING([for ldns devel source])
ldns_dev_dir=..
if test -f $ldns_dev_dir/ldns/util.h && \
	grep LDNS_VERSION $ldns_dev_dir/ldns/util.h >/dev/null; then
	ldns_version=`grep LDNS_VERSION $ldns_dev_dir/ldns/util.h | sed -e 's/^.*"\(.*\)".*$/\1/'`
	AC_MSG_RESULT([using $ldns_dev_dir with $ldns_version])
	CPPFLAGS="$CPPFLAGS -I$ldns_dev_dir/include"
	LDFLAGS="-L$ldns_dev_dir -L$ldns_dev_dir/lib $LDFLAGS"
	LIBS="$LIBS -lldns"
	AC_DEFINE(HAVE_LIBLDNS, 1, [If the ldns library is available.])
	LDNSDIR="$ldns_dev_dir"
else
	AC_MSG_RESULT([no])
	AC_CHECK_LIB(ldns, ldns_rr_new,, [
			AC_MSG_ERROR([Can't find ldns library])
		]
	)
fi

AC_SUBST(LDNSDIR)


AH_BOTTOM([

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>

#if STDC_HEADERS
#include <stdlib.h>
#include <stddef.h>
#endif

#ifdef HAVE_STDINT_H
#include <stdint.h>
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

#ifdef HAVE_NETINET_UDP_H
#include <netinet/udp.h>
#endif

#ifdef HAVE_TIME_H
#include <time.h>
#endif

#ifdef HAVE_PCAP_H
#include <pcap.h>
#endif

#ifdef HAVE_NETINET_IN_SYSTM_H
#include <netinet/in_systm.h>
#endif

#ifdef HAVE_NETINET_IP_H
#include <netinet/ip.h>
#endif

#ifdef HAVE_NET_IF_H
#include <net/if.h>
#endif

#ifdef HAVE_NETINET_IF_ETHER_H
#include <netinet/if_ether.h>
#endif

#ifdef HAVE_WINSOCK2_H
#define USE_WINSOCK 1
#include <winsock2.h>
#endif

#ifdef HAVE_WS2TCPIP_H
#include <ws2tcpip.h>
#endif

#ifndef HAVE_GETADDRINFO
#include <fake-rfc2553.h>
#endif

#ifndef HAVE_RANDOM
/* random can be replaced by rand for ldnsexamples */
#define random rand
#endif

#ifndef HAVE_SRANDOM
/* srandom can be replaced by srand for ldnsexamples */
#define srandom srand
#endif

extern char *optarg;
extern int optind, opterr;

#ifndef EXIT_FAILURE
#define EXIT_FAILURE  1
#endif
#ifndef EXIT_SUCCESS
#define EXIT_SUCCESS  0
#endif

#ifdef S_SPLINT_S
#define FD_ZERO(a) /* a */
#define FD_SET(a,b) /* a, b */
#endif
])

AC_CONFIG_FILES([Makefile])
AC_CONFIG_HEADER([config.h])
AC_OUTPUT
