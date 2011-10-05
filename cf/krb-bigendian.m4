dnl
dnl $Id$
dnl

dnl check if this computer is little or big-endian
dnl if we can figure it out at compile-time then don't define the cpp symbol
dnl otherwise test for it and define it.  also allow options for overriding
dnl it when cross-compiling

AC_DEFUN([KRB_C_BIGENDIAN], [
AC_ARG_ENABLE(bigendian,
	AS_HELP_STRING([--enable-bigendian],[the target is big endian]),
krb_cv_c_bigendian=yes)
AC_ARG_ENABLE(littleendian,
	AS_HELP_STRING([--enable-littleendian],[the target is little endian]),
krb_cv_c_bigendian=no)
AC_CACHE_CHECK([whether byte order is known at compile time],
krb_cv_c_bigendian_compile,
[AC_COMPILE_IFELSE([AC_LANG_SOURCE([[
#include <sys/types.h>
#include <sys/param.h>
#if !BYTE_ORDER || !BIG_ENDIAN || !LITTLE_ENDIAN
 bogus endian macros
#endif]])],[krb_cv_c_bigendian_compile=yes],[krb_cv_c_bigendian_compile=no])])
AC_CACHE_CHECK(whether byte ordering is bigendian, krb_cv_c_bigendian,[
  if test "$krb_cv_c_bigendian_compile" = "yes"; then
    AC_COMPILE_IFELSE([AC_LANG_SOURCE([[
#include <sys/types.h>
#include <sys/param.h>
#if BYTE_ORDER != BIG_ENDIAN
  not big endian
#endif]])],[krb_cv_c_bigendian=yes],[krb_cv_c_bigendian=no])
  else
    AC_RUN_IFELSE([AC_LANG_SOURCE([[main (int argc, char **argv) {
      /* Are we little or big endian?  From Harbison&Steele.  */
      union
      {
	long l;
	char c[sizeof (long)];
    } u;
    u.l = 1;
    exit (u.c[sizeof (long) - 1] == 1);
  }]])],[krb_cv_c_bigendian=no],[krb_cv_c_bigendian=yes],
  [AC_MSG_ERROR([specify either --enable-bigendian or --enable-littleendian])])
  fi
])
if test "$krb_cv_c_bigendian" = "yes"; then
  AC_DEFINE(WORDS_BIGENDIAN, 1, [define if target is big endian])dnl
fi
if test "$krb_cv_c_bigendian_compile" = "yes"; then
  AC_DEFINE(ENDIANESS_IN_SYS_PARAM_H, 1, [define if sys/param.h defines the endiness])dnl
fi
AH_BOTTOM([
#ifdef ENDIANESS_IN_SYS_PARAM_H
#  include <sys/types.h>
#  include <sys/param.h>
#  if BYTE_ORDER == BIG_ENDIAN
#  define WORDS_BIGENDIAN 1
#  endif
#endif
])
])
