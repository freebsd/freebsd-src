dnl Process this file with autoconf to produce a configure script.
AC_INIT
AC_CONFIG_HEADERS(src/include/config.h:src/include/config.hin)
AC_CONFIG_SRCDIR([src/roff/groff/groff.cpp])
dnl checks for programs
GROFF_SRCDIR
GROFF_BUILDDIR
AC_PROG_CC
AC_PROG_CXX
GROFF_CXX_CHECK
GROFF_EBCDIC
GROFF_OS390
GROFF_PRINT
AC_PATH_PROG(PERLPATH, perl, /usr/bin/perl)
GROFF_PROG_YACC
AC_PROG_RANLIB
GROFF_INSTALL_SH
GROFF_INSTALL_INFO
AC_PROG_INSTALL
AC_PROG_LN_S
dnl use a dummy substitution if no csh hack is necessary to avoid errors
dnl with non-GNU sed programs
GROFF_CSH_HACK(SH_SCRIPT_SED_CMD='1s/.*/:/', SH_SCRIPT_SED_CMD='1s/a/a/')
AC_SUBST(SH_SCRIPT_SED_CMD)
dnl checks for headers
AC_CHECK_HEADERS(stdlib.h unistd.h dirent.h limits.h sys/dir.h \
                 string.h strings.h math.h sys/time.h)
GROFF_ISC_SYSV3
GROFF_POSIX
GROFF_SRAND
GROFF_NEED_DECLARATION(gettimeofday)
GROFF_NEED_DECLARATION(hypot)
GROFF_NEED_DECLARATION(popen)
GROFF_NEED_DECLARATION(pclose)
GROFF_NEED_DECLARATION(putenv)
GROFF_NEED_DECLARATION(rand)
GROFF_NEED_DECLARATION(snprintf)
GROFF_NEED_DECLARATION(srand)
GROFF_NEED_DECLARATION(strcasecmp)
GROFF_NEED_DECLARATION(strncasecmp)
GROFF_SYS_NERR
GROFF_SYS_ERRLIST
GROFF_OSFCN_H
GROFF_LIMITS_H
GROFF_STDINT_H
GROFF_INTTYPES_H
dnl checks for typedefs
GROFF_UNSIGNED_LONG_LONG
GROFF_UINTMAX_T
GROFF_TIME_T
AC_TYPE_SIGNAL
GROFF_STRUCT_EXCEPTION
dnl checks for libraries
GROFF_LIBM
dnl checks for functions
AC_FUNC_MMAP
saved_libs="$LIBS"
LIBS="$LIBS -lc $LIBM"
AC_REPLACE_FUNCS(fmod getcwd putenv snprintf strcasecmp \
                 strerror strncasecmp strtol)
LIBS="$saved_libs"
AC_CHECK_FUNCS(gettimeofday isatty rename setlocale strsep)
GROFF_MKSTEMP
AC_DECL_SYS_SIGLIST
dnl checks for compiler characteristics
GROFF_ARRAY_DELETE
GROFF_TRADITIONAL_CPP
dnl checks for operating system services
GROFF_WCOREFLAG
dnl other random stuff
GROFF_BROKEN_SPOOLER_FLAGS
GROFF_PAGE
GROFF_G
GROFF_TMAC
GROFF_HTML_PROGRAMS
GROFF_PNMTOPS_NOSETPAGE
AC_CONFIG_FILES(stamp-h, [echo timestamp > stamp-h])
AC_CONFIG_FILES([Makefile doc/Makefile src/xditview/Imakefile])
AC_OUTPUT
dnl
AC_MSG_NOTICE([

  Configuration completed.

  You can now say `make' to compile the groff package and `make install'
  to install it afterwards.

  If you want to compile xditview, change the directory to `src/xditview'
  and follow the instructions given in the file `INSTALL'.
])
