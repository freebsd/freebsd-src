# Process this file with autoconf to produce a configure script.
AC_INIT(main.c)
AM_INIT_AUTOMAKE([msntp],[1.6])
AC_CONFIG_SRCDIR([header.h])
AC_CANONICAL_SYSTEM

# Checks for programs.
AC_PROG_CC

# Checks for libraries.

# Checks for header files.
AC_HEADER_STDC
AC_CHECK_HEADERS([arpa/inet.h fcntl.h float.h limits.h netdb.h netinet/in.h stdlib.h string.h sys/socket.h sys/time.h syslog.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_TYPE_SIZE_T
AC_HEADER_TIME
AC_STRUCT_TM

AC_CACHE_CHECK([for ss_family field in struct sockaddr_storage],
		ac_cv_have_ss_family_in_struct_ss, [
	AC_TRY_COMPILE(
		[
#include <sys/types.h>
#include <sys/socket.h>
		],
		[ struct sockaddr_storage s; s.ss_family = 1; ],
		[ ac_cv_have_ss_family_in_struct_ss="yes" ],
		[ ac_cv_have_ss_family_in_struct_ss="no" ],
	)
])
if test "x$ac_cv_have_ss_family_in_struct_ss" = "xyes" ; then
	AC_DEFINE(HAVE_SS_FAMILY_IN_SS, 1, [Does struct sockaddr_storage have ss_family?])
fi

AC_CACHE_CHECK([for __ss_family field in struct sockaddr_storage],
		ac_cv_have___ss_family_in_struct_ss, [
	AC_TRY_COMPILE(
		[
#include <sys/types.h>
#include <sys/socket.h>
		],
		[ struct sockaddr_storage s; s.__ss_family = 1; ],
		[ ac_cv_have___ss_family_in_struct_ss="yes" ],
		[ ac_cv_have___ss_family_in_struct_ss="no" ]
	)
])
if test "x$ac_cv_have___ss_family_in_struct_ss" = "xyes" ; then
	AC_DEFINE(HAVE___SS_FAMILY_IN_SS, 1, [Does struct sockaddr_storage have __ss_family?])
fi

case "$ac_cv_have_ss_family_in_struct_ss$ac_cv_have___ss_family_in_struct_ss" in
 noyes)
    AC_DEFINE_UNQUOTED(ss_family, __ss_family, [])
    AC_DEFINE_UNQUOTED(ss_len, __ss_len, [])
    ;;
esac

# Checks for library functions.
AC_FUNC_MEMCMP
AC_FUNC_SETVBUF_REVERSED
AC_TYPE_SIGNAL
AC_CHECK_FUNCS([alarm])
AC_CHECK_FUNC(gethostent, , AC_CHECK_LIB(nsl, gethostent, , , -lsocket))
AC_CHECK_FUNCS([gettimeofday inet_ntoa memset])
AC_CHECK_FUNCS(socket, , AC_CHECK_LIB(socket, socket))
AC_CHECK_FUNCS(sqrt, , AC_CHECK_LIB(m, sqrt))
AC_CHECK_FUNCS([strrchr])

case "$host" in
 *-*-linux*)
    CFLAGS="$CFLAGS -DADJTIME_MISSING"
    ;;
 *-*-sunos*)
    CFLAGS="$CFLAGS -DNONBLOCK_BROKEN"
    ;;
esac

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
