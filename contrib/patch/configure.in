# Configure `patch'.
# Copyright 1993, 1997 Free Software Foundation, Inc.
dnl Process this file with autoconf to produce a configure script.

AC_PREREQ(2.12)
AC_INIT(patch.c)
AC_CONFIG_HEADER(config.h:config.hin)
AC_ARG_PROGRAM

PACKAGE=patch
VERSION=2.5
AC_SUBST(PACKAGE)
AC_SUBST(VERSION)

AC_PROG_CC
AC_PROG_CPP
AC_PROG_INSTALL
AC_PROG_MAKE_SET
# Use ed_PROGRAM, not ED_PROGRAM,
# because <errno.h> reserves symbols starting with `E'.
AC_PATH_PROG(ed_PROGRAM, ed, ed)

# If available, prefer support for large files unless the user specified
# one of the CPPFLAGS, LDFLAGS, or LIBS variables.
AC_MSG_CHECKING(whether large file support needs explicit enabling)
ac_getconfs=''
ac_result=yes
ac_set=''
ac_shellvars='CPPFLAGS LDFLAGS LIBS'
for ac_shellvar in $ac_shellvars; do
  case $ac_shellvar in
  CPPFLAGS) ac_lfsvar=LFS_CFLAGS ;;
  *) ac_lfsvar=LFS_$ac_shellvar ;;
  esac
  eval test '"${'$ac_shellvar'+set}"' = set && ac_set=$ac_shellvar
  (getconf $ac_lfsvar) >/dev/null 2>&1 || { ac_result=no; break; }
  ac_getconf=`getconf $ac_lfsvar`
  ac_getconfs=$ac_getconfs$ac_getconf
  eval ac_test_$ac_shellvar=\$ac_getconf
done
case "$ac_result$ac_getconfs" in
yes) ac_result=no ;;
esac
case "$ac_result$ac_set" in
yes?*) ac_result="yes, but $ac_set is already set, so use its settings"
esac
AC_MSG_RESULT($ac_result)
case $ac_result in
yes)
  for ac_shellvar in $ac_shellvars; do
    eval $ac_shellvar=\$ac_test_$ac_shellvar
  done ;;
esac

AC_AIX
AC_MINIX
AC_ISC_POSIX

AC_C_CONST

AC_HEADER_DIRENT
AC_HEADER_STDC
AC_CHECK_HEADERS(fcntl.h limits.h string.h unistd.h utime.h varargs.h)

AC_TYPE_MODE_T
AC_TYPE_OFF_T
AC_TYPE_SIGNAL
AC_TYPE_SIZE_T

dnl Some systems have utime.h but don't declare the struct anywhere.
AC_MSG_CHECKING(for struct utimbuf)
AC_CACHE_VAL(patch_cv_sys_struct_utimbuf,
[AC_TRY_COMPILE([#include <sys/types.h>
#if HAVE_UTIME_H
#include <utime.h>
#endif], [static struct utimbuf x; x.actime = x.modtime;],
  patch_cv_sys_struct_utimbuf=yes,
  patch_cv_sys_struct_utimbuf=no)])
AC_MSG_RESULT($patch_cv_sys_struct_utimbuf)
if test $patch_cv_sys_struct_utimbuf = yes; then
   AC_DEFINE(HAVE_STRUCT_UTIMBUF)
fi

# Check for NetBSD 1.0 bug, where memchr(..., 0) returns nonzero.
AC_MSG_CHECKING(for working memchr)
AC_CACHE_VAL(ac_cv_func_memchr,
[AC_TRY_RUN([#include <string.h>
main () { exit (memchr ("", 0, 0) != 0 || memchr ("", 1, 0) != 0); }],
  ac_cv_func_memchr=yes,
  ac_cv_func_memchr=no,
  AC_MSG_WARN([We are cross-compiling so we assume memchr does not work.])
  ac_cv_func_memchr=no)])dnl
AC_MSG_RESULT($ac_cv_func_memchr)
if test $ac_cv_func_memchr = yes; then
  AC_DEFINE(HAVE_MEMCHR)
fi

AC_CHECK_FUNC(getopt_long, , [LIBOBJS="$LIBOBJS getopt1.o getopt.o"])
AC_SUBST(LIBOBJS)
AC_CHECK_FUNCS(_doprintf isascii memcmp mkdir mktemp pathconf raise sigaction sigprocmask sigsetmask)
AC_REPLACE_FUNCS(memchr rename)
AC_FUNC_CLOSEDIR_VOID
AC_FUNC_VPRINTF

AC_SYS_LONG_FILE_NAMES

AC_MSG_CHECKING([for d_ino member in directory struct])
AC_CACHE_VAL(patch_cv_sys_d_ino_in_dirent,
[AC_TRY_LINK([
#include <sys/types.h>
#if HAVE_DIRENT_H
# include <dirent.h>
#else
# define dirent direct
# if HAVE_SYS_NDIR_H
#  include <sys/ndir.h>
# endif
# ifdef HAVE_SYS_DIR_H
#  include <sys/dir.h>
# endif
# ifdef HAVE_NDIR_H
#  include <ndir.h>
# endif
#endif
],
  [struct dirent dp; dp.d_ino = 0;],
    patch_cv_sys_d_ino_in_dirent=yes,
    patch_cv_sys_d_ino_in_dirent=no)])
AC_MSG_RESULT($patch_cv_sys_d_ino_in_dirent)
if test $patch_cv_sys_d_ino_in_dirent = yes; then
  AC_DEFINE(D_INO_IN_DIRENT)
fi

AC_OUTPUT(Makefile)
