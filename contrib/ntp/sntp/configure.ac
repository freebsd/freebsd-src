# Process this file with autoconf to produce a configure script.
AC_INIT(main.c)
AM_INIT_AUTOMAKE([msntp],[1.6])
AC_CONFIG_SRCDIR([header.h])
AC_CANONICAL_HOST
AM_CONFIG_HEADER(config.h)

# Checks for programs.
AC_PROG_CC

# So far, the only shared library we might use is libopts.
# It's a small library - we might as well use a static version of it.
AC_DISABLE_SHARED

m4_defun([_LT_AC_LANG_CXX_CONFIG], [:])
m4_defun([_LT_AC_LANG_F77_CONFIG], [:])

AC_PROG_LIBTOOL

# NTP has (so far) been relying on leading-edge autogen.
# Therefore, by default:
# - use the version we ship with
# - do not install it
# - build a static copy (AC_DISABLE_SHARED - done earlier)
case "${enable_local_libopts+set}" in
 set) ;;
 *) enable_local_libopts=yes ;;
esac
case "${enable_libopts_install+set}" in
 set) ;;
 *) enable_libopts_install=no ;;
esac
LIBOPTS_CHECK

AC_MSG_CHECKING([[if $CC can handle #warning]])
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[]], [[#warning foo]])],[ac_cv_cpp_warning=yes],[ac_cv_cpp_warning=no])
AC_MSG_RESULT([$ac_cv_cpp_warning])

case "$ac_cv_cpp_warning" in
 no)
    AC_DEFINE([NO_OPTION_NAME_WARNINGS], [1], [Should we avoid #warning on option name collisions?])
    AC_MSG_RESULT([[Enabling NO_OPTION_NAME_WARNINGS as #warning does not work]])
    ;;
esac

AC_C_INLINE

case "$ac_cv_c_inline" in
 '')
    ;;
 *)
    AC_DEFINE(HAVE_INLINE,1,[inline keyword or macro available])
    AC_SUBST(HAVE_INLINE)
esac


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
case "$ac_cv_have_ss_family_in_struct_ss" in
 yes)
    AC_DEFINE(HAVE_SS_FAMILY_IN_SS, 1, [Does struct sockaddr_storage have ss_family?])
    ;;
esac

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
case "$ac_cv_have___ss_family_in_struct_ss" in
 yes)
    AC_DEFINE(HAVE___SS_FAMILY_IN_SS, 1, [Does struct sockaddr_storage have __ss_family?])
    ;;
esac

case "$ac_cv_have_ss_family_in_struct_ss$ac_cv_have___ss_family_in_struct_ss" in
 noyes)
    AC_DEFINE_UNQUOTED([ss_family], [__ss_family], [normalize ss_family access])
    AC_DEFINE_UNQUOTED([ss_len], [__ss_len], [normalize ss_len access])
    ;;
esac

# Checks for library functions.
#AC_FUNC_MEMCMP	 dnl HMS: we don't have a memcmp.c to use here, so why bother?
AC_FUNC_SETVBUF_REVERSED
AC_TYPE_SIGNAL
AC_CHECK_FUNCS([alarm])
AC_CHECK_FUNC([gethostent], , AC_SEARCH_LIBS([gethostent], [nsl], , , [-lsocket]))
AC_CHECK_FUNCS([gettimeofday inet_ntoa memset])
AC_CHECK_FUNCS([socket], , AC_SEARCH_LIBS([socket], [socket]))
AC_CHECK_FUNCS([sqrt], , AC_SEARCH_LIBS([sqrt], [m]))
AC_CHECK_FUNCS([strrchr])

case "$host" in
 *-*-hpux10.*)	# at least for hppa2.0-hp-hpux10.20
    case "$GCC" in
     yes)
	;;
     *) CFLAGS="$CFLAGS -Wp,-H18816"
	;;
    esac
    ;;
 *-*-linux*)
    CFLAGS="$CFLAGS -DADJTIME_MISSING"
    ;;
 *-*-sunos*)
    CFLAGS="$CFLAGS -DNONBLOCK_BROKEN"
    ;;
esac

AC_MSG_CHECKING(for bin subdirectory)
AC_ARG_WITH(binsubdir,
	AC_HELP_STRING([--with-binsubdir], [bin ={bin,sbin}]),
	use_binsubdir="$withval", use_binsubdir="bin")

case "$use_binsubdir" in
 bin)
    ;;
 sbin)
    ;;
 *)
    AC_MSG_ERROR([<$use_binsubdir> is illegal - must be "bin" or "sbin"])
    ;;
esac
AC_MSG_RESULT($use_binsubdir)
BINSUBDIR=$use_binsubdir
AC_SUBST(BINSUBDIR)

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
