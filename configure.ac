#       $NetBSD: configure.ac,v 1.22 2010/01/12 06:57:31 lukem Exp $
#
# Process this file with autoconf to produce a configure script.

AC_INIT([tnftp], [20100108], [lukem@NetBSD.org])
AC_PREREQ([2.61])

AC_COPYRIGHT([
Copyright (c) 1999-2010 The NetBSD Foundation, Inc.
All rights reserved.
])
AC_REVISION([$Revision: 1.22 $])

AS_SHELL_SANITIZE

AC_CONFIG_SRCDIR([tnftp.h])
AC_CONFIG_AUX_DIR([buildaux])
AC_CONFIG_HEADERS([tnftp_config.h])
AC_CONFIG_LIBOBJ_DIR([libnetbsd])

AM_INIT_AUTOMAKE([-Wall -Werror foreign nostdinc silent-rules])
AM_MAINTAINER_MODE

#
# Arguments for which features are included.
#
AC_ARG_ENABLE([editcomplete],
              [AS_HELP_STRING([--enable-editcomplete],
                              [turn on command line editing and completion
                               [default=enabled]])],
              [opt_editcomplete=$enableval],
              [opt_editcomplete=yes])
AC_ARG_ENABLE([ipv6],
              [AS_HELP_STRING([--enable-ipv6],
                              [enable IPv6 support (if your OS supports it)
                               [default=enabled]])],
              [opt_ipv6=$enableval],
              [opt_ipv6=yes])
AC_ARG_WITH([socks],
            [AS_HELP_STRING([--with-socks],
                            [enable support for (Dante) SOCKS5 proxy
                             [default=auto]])],
            [],
            [with_socks=auto])

#
# Autoheader templates symbols.
#
AH_TEMPLATE([HAVE_POLL],
            [Define if we have poll() and it is not emulated.])
AH_TEMPLATE([HAVE_PRINTF_QD],
            [Define if *printf() uses %qd to print `long long'
             (otherwise uses %lld).])
AH_TEMPLATE([HAVE_PRINTF_LONG_LONG],
            [Define if `long long' is supported and
             *printf() supports %lld or %qd to print them.])
AH_TEMPLATE([USE_INET6],
            [Define if using IPv6 support.])
AH_TEMPLATE([USE_SOCKS],
            [Define if using (Dante) SOCKS5 proxy.])

#
# Checks for programs.
#
AC_PROG_CC
AC_PROG_AWK
AC_PROG_LIBTOOL

#
# Checks for tool features.
#
AS_CASE([$target_os],
        [darwin*],
        [# Mac OS X linker needs -search_paths_first so our private libraries
         # are used before system libraries.
         #
         old_LDFLAGS=$LDFLAGS
         LDFLAGS="$LDFLAGS -Wl,-search_paths_first"
         AC_MSG_CHECKING([if ld accepts -search_paths_first])
         AC_LINK_IFELSE([AC_LANG_PROGRAM([], [[int i;]])],
                        [AC_MSG_RESULT([yes])],
                        [AC_MSG_RESULT([no])
                         LDFLAGS=$old_LDFLAGS])],
        )

#
# Checks for libraries.
#
AS_IF([test "$opt_editcomplete" = yes],
      [AC_SEARCH_LIBS([tgetent],
                      [termcap termlib curses ncurses tinfo],
                      [],
                      [AC_MSG_ERROR(
                           [no relevant library found containing tgetent])])
       ])

AC_SEARCH_LIBS([gethostbyname], [nsl])
AC_SEARCH_LIBS([socket],
               [socket],
               [],
               [AC_CHECK_LIB([socket],
                             [socket],
                             [LIBS="-lsocket -lnsl $LIBS"],
                             [],
                             [-lnsl])])

# Check for (Dante) SOCKS5.
#
AS_IF([test "$with_socks" != no],
      [AC_MSG_NOTICE([--with-socks=$with_socks; checking for required SOCKS5 features])
       AC_SEARCH_LIBS([SOCKSinit],
                      [socksd socks],
                      [AC_DEFINE([USE_SOCKS], [1])
                       AC_MSG_NOTICE([enabling SOCKS5 support])
                       with_socks=yes
                       AS_IF([test "$opt_ipv6" = yes],
                             [AC_MSG_WARN(
                [IPv6 is incompatible with socks, disabling IPv6 support])
                              opt_ipv6=no])],
                      [AS_IF([test "$with_socks" != "auto"],
                             [AC_MSG_FAILURE(
                [--with-socks was given, but SOCKSinit() wasn't found])])
                       AC_MSG_NOTICE([disabling --with-socks])
                       with_socks=no])])

#
# Checks for header files.
#
accheck_includes='
#include <stdio.h>
#if defined(HAVE_SYS_TYPES_H)
# include <sys/types.h>
#endif
#if defined(HAVE_SYS_STAT_H)
# include <sys/stat.h>
#endif
#if defined(STDC_HEADERS)
# include <stdarg.h>
# include <stdlib.h>
# include <string.h>
#endif
#if defined(TIME_WITH_SYS_TIME)
# include <sys/time.h>
# include <time.h>
#else
# if defined(HAVE_SYS_TIME_H)
#  include <sys/time.h>
# else
#  include <time.h>
# endif
#endif
#if defined(HAVE_LIBGEN_H)
# include <libgen.h>
#endif
#if defined(HAVE_PWD_H)
# include <pwd.h>
#endif
#if defined(HAVE_UNISTD_H)
# include <unistd.h>
#endif
#if defined(HAVE_POLL_H)
# include <poll.h>
#elif defined(HAVE_SYS_POLL_H)
# include <sys/poll.h>
#endif
#if defined(HAVE_SYS_SOCKET_H)
# include <sys/socket.h>
#endif
#if defined(HAVE_NETINET_IN_H)
# include <netinet/in.h>
#endif
#if defined(HAVE_NETINET_IN_SYSTM_H)
# include <netinet/in_systm.h>
#endif
#if defined(HAVE_NETDB_H)
# include <netdb.h>
#endif
#if defined(HAVE_ARPA_INET_H)
# include <arpa/inet.h>
#endif
#if defined(HAVE_DIRENT_H)
# include <dirent.h>
#else
# define dirent direct
# if defined(HAVE_SYS_NDIR_H)
#  include <sys/ndir.h>
# endif
# if defined(HAVE_SYS_DIR_H)
#  include <sys/dir.h>
# endif
# if defined(HAVE_NDIR_H)
#  include <ndir.h>
# endif
#endif
' # accheck_includes

AC_CHECK_HEADERS([sys/types.h sys/ioctl.h sys/param.h sys/stat.h \
                  sys/socket.h sys/syslimits.h sys/time.h sys/wait.h],
                 [], [], [$accheck_includes])
AC_HEADER_DIRENT
AC_HEADER_RESOLV
AC_HEADER_STAT
AC_HEADER_STDC
AC_HEADER_TIME
AC_HEADER_TIOCGWINSZ
AC_CHECK_HEADERS([arpa/ftp.h arpa/inet.h arpa/nameser.h err.h \
                  fcntl.h libgen.h limits.h malloc.h \
                  netinet/in.h netinet/in_systm.h netinet/ip.h \
                  paths.h poll.h pwd.h sys/poll.h regex.h \
                  setjmp.h signal.h stddef.h termcap.h termios.h \
                  unistd.h utime.h vis.h],
                  [], [], [$accheck_includes])

#
# Checks for typedefs, structures, and compiler characteristics.
#
AC_CHECK_DECLS([AI_NUMERICHOST, dirname, fclose, getpass, h_errno, pclose,
                optarg, optind],
               [], [], [$accheck_includes])
AC_TYPE_LONG_DOUBLE
AC_TYPE_LONG_LONG_INT
AC_TYPE_UINT32_T
AC_TYPE_OFF_T
AC_TYPE_PID_T
AC_TYPE_SIGNAL
AC_TYPE_SIZE_T
AC_STRUCT_TM
AC_CHECK_MEMBERS([struct sockaddr.sa_len, struct sockaddr_in.sin_len,
                  struct dirent.d_namlen],
                 [], [], [$accheck_includes])
AC_CHECK_TYPES([in_port_t, sa_family_t, socklen_t, struct addrinfo],
               [], [], [$accheck_includes])
AC_SYS_LARGEFILE

# If IPv6 is enabled, check for necessary items.
#
AS_IF([test "$opt_ipv6" = yes],
      [AC_MSG_NOTICE([--enable-ipv6; checking for required IPv6 features])
       AC_CHECK_DECLS([AF_INET6, NS_IN6ADDRSZ], [], [],
[$accheck_includes
#if defined(HAVE_ARPA_NAMESER_H)
#include <arpa/nameser.h>
#endif
])
       AC_CHECK_TYPES([struct sockaddr_in6], [], [opt_ipv6=no],
                      [$accheck_includes])
       AS_IF([test "$opt_ipv6" = yes],
             [AC_MSG_NOTICE([enabling IPv6 support])
              AC_DEFINE([USE_INET6], [1])],
             [AC_MSG_NOTICE([disabling IPv6 support])])
       ])

#
# Checks for library functions.
#
#XXX    remove alloca use
AC_FUNC_ALLOCA
AC_FUNC_CLOSEDIR_VOID
AC_FUNC_FORK
AC_FUNC_FSEEKO
AC_FUNC_STRCOLL
AC_REPLACE_FUNCS([dirname err fgetln getaddrinfo getnameinfo \
                  inet_ntop inet_pton mkstemp setprogname sl_init snprintf \
                  strdup strerror strlcat strlcpy strptime strsep strunvis \
                  strvis timegm usleep utimes])
AC_CHECK_FUNCS([getcwd gethostbyaddr gethostbyname gethostbyname2 gethostname \
                getpass getpassphrase getpgrp gettimeofday isascii \
                memchr memmove memset realpath regcomp \
                select setlocale socket strcasecmp strchr strcspn strdup \
                strerror strncasecmp strpbrk strrchr strstr strtol strtoul \
                utime])
AS_IF([test "$ac_cv_func_getpgrp" = yes], [AC_FUNC_GETPGRP])
AS_IF([test "$ac_cv_func_strptime" = yes], [AC_CHECK_DECLS([strptime])])

# Always replace glob(); the vendor's may not be secure.
#
AC_LIBOBJ([glob])

# We assume that if sprintf() supports %lld or %qd,
# then all of *printf() does. If not, disable long long
# support because we don't know how to display it.
#
AS_IF([test "$ac_cv_type_long_long_int" = yes],
      [AC_MSG_CHECKING([*printf() support for %lld])
       can_printf_longlong=no
       AC_RUN_IFELSE([AC_LANG_PROGRAM([[
$accheck_includes
]], [[
char buf[100];
sprintf(buf, "%lld", 4294967300LL);
exit(0 != strcmp(buf, "4294967300"));
]])],
                     [AC_MSG_RESULT([yes])
                      can_printf_longlong=yes],
                     [AC_MSG_RESULT([no])],
                     [AC_MSG_RESULT([unknown - cross-compiling])])
        AS_IF([test "$can_printf_longlong" != yes],
              [AC_MSG_CHECKING([*printf() support for %qd])
               AC_RUN_IFELSE([AC_LANG_PROGRAM([[
$accheck_includes
]], [[
char buf[100];
sprintf(buf, "%qd", 4294967300LL);
exit(0 != strcmp(buf, "4294967300"));
]])],
                             [AC_MSG_RESULT([yes])
                              can_printf_longlong=yes
                              AC_DEFINE([HAVE_PRINTF_QD], [1])],
                             [AC_MSG_RESULT([no])],
                             [AC_MSG_RESULT([unknown - cross-compiling])])])
        AC_MSG_CHECKING([if *printf() can print long long ints])
        AS_IF([test "$can_printf_longlong" = yes],
              [AC_MSG_RESULT([yes])
               AC_DEFINE([HAVE_PRINTF_LONG_LONG], [1])
               AC_REPLACE_FUNCS([strtoll])],
              [AC_MSG_RESULT([no])])])

# Handle Darwin 7 having a poll() compatibility function,
# and don't use it if it's emulated.
# Be conservative, if we don't find one of poll.h or sys/poll.h,
# don't attempt to use poll().
#
AC_CHECK_TYPES([struct pollfd], [], [], [$accheck_includes])
AC_CHECK_FUNC([poll],
              [AC_CHECK_DECL([_POLL_EMUL_H_],
                             [],
                             [AC_DEFINE([HAVE_POLL], [1])],
                             [$accheck_includes])])

AS_IF([test "$ac_cv_func_strptime" = yes],
      [AC_MSG_CHECKING([if strptime() needs separators between conversions])
       AC_RUN_IFELSE([AC_LANG_PROGRAM([[
$accheck_includes
]], [[
struct tm timebuf;
char * res;
memset(&timebuf, 0, sizeof(timebuf));
res = strptime("20011122161900", "%Y%m%d%H%M%S", &timebuf);
exit(!res);
]])],
                     [AC_MSG_RESULT([no])],
                     [AC_MSG_RESULT([yes - using local version])
                      AC_LIBOBJ([strptime])],
                     [AC_MSG_RESULT([unknown - cross-compiling])])])

# Use local libedit if editcomplete is requested
#
AS_IF([test "$opt_editcomplete" = yes],
      [AC_MSG_NOTICE([--enable-editcomplete; using internal libedit])],
      [CFLAGS="-DNO_EDITCOMPLETE $CFLAGS"])

# Replace sl_init() (et al) if it provides the older API.
#
AS_IF([test "$ac_cv_func_sl_init" = yes],
      [AC_MSG_CHECKING([if sl_add() returns int])
       AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[#include <stringlist.h>]],
                                          [[int f = sl_add((StringList *)0,
                                                           "foo");]])],
                         [AC_MSG_RESULT([yes])],
                         [AC_MSG_RESULT([no - using local version])
                          AC_LIBOBJ([sl_init])],
                         [AC_MSG_RESULT([unknown - cross-compiling])])])

# Replace getaddrinfo() if missing or AI_NUMERICHOST isn't declared.
#
AC_MSG_CHECKING([for working getaddrinfo()])
AS_IF([test "$ac_cv_have_decl_AI_NUMERICHOST" = yes],
      [AC_MSG_RESULT([yes])],
      [AS_IF([test "$ac_cv_func_getaddrinfo" = yes],
             [AC_LIBOBJ([getaddrinfo])
              AC_MSG_RESULT([no - using local version])],
             [AC_MSG_RESULT([using local version])])])

# Check for VIS_WHITE in <vis.h>
#
AS_IF([test "$ac_cv_header_vis_h" = yes],
      [AC_CHECK_DECL([VIS_WHITE], [], [], [
#include <vis.h>
])])

AM_CONDITIONAL([USE_LIBEDIT], [test "$opt_editcomplete" = yes])

#
# Create the Makefiles.
#
AC_CONFIG_FILES([
        Makefile
        libedit/Makefile
        libedit/makelist
        libnetbsd/Makefile
        src/Makefile
])

AC_OUTPUT

# Display feature results.
#
AC_MSG_NOTICE([ === Configuration results ===])
AC_MSG_NOTICE([Package:               $PACKAGE_STRING])
AC_MSG_NOTICE([Prefix:                $prefix])
AC_MSG_NOTICE([Command-line editing:  $opt_editcomplete])
AC_MSG_NOTICE([IPv6 support:          $opt_ipv6])
AC_MSG_NOTICE([SOCKS5 proxy support:  $with_socks])
AC_MSG_NOTICE([ =============================])
