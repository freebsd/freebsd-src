dnl Process this file with autoconf to produce a configure script.
AC_INIT
AC_CONFIG_SRCDIR([src/roff/groff/groff.cc])
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
AC_PROG_INSTALL
AC_PROG_LN_S
dnl use a dummy substitution if no csh hack is necessary to avoid errors
dnl with non-GNU sed programs
GROFF_CSH_HACK(SH_SCRIPT_SED_CMD='1s/.*/:/', SH_SCRIPT_SED_CMD='1s/a/a/')
AC_SUBST(SH_SCRIPT_SED_CMD)
dnl checks for headers
AC_CHECK_HEADERS(stdlib.h unistd.h dirent.h limits.h sys/dir.h \
                 string.h strings.h math.h)
GROFF_ISC_SYSV3
GROFF_POSIX
GROFF_SRAND
GROFF_NEED_DECLARATION(hypot)
GROFF_NEED_DECLARATION(popen)
GROFF_NEED_DECLARATION(pclose)
GROFF_NEED_DECLARATION(putenv)
GROFF_NEED_DECLARATION(strncasecmp)
GROFF_SYS_NERR
GROFF_SYS_ERRLIST
GROFF_OSFCN_H
GROFF_LIMITS_H
dnl checks for typedefs
GROFF_TIME_T
AC_TYPE_SIGNAL
GROFF_STRUCT_EXCEPTION
dnl checks for libraries
GROFF_LIBM
dnl checks for functions
AC_FUNC_MMAP
saved_libs="$LIBS"
LIBS="$LIBS -lc $LIBM"
AC_REPLACE_FUNCS(fmod strtol getcwd strerror putenv)
LIBS="$saved_libs"
AC_CHECK_FUNCS(rename strcasecmp strncasecmp strsep strdup)
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
$srcdir/mkinstalldirs src/xditview
AC_CONFIG_FILES([Makefile src/xditview/Imakefile])
AC_OUTPUT
