dnl $Id: have-type.m4 14166 2004-08-26 12:35:42Z joda $
dnl
dnl check for existance of a type

dnl AC_HAVE_TYPE(TYPE,INCLUDES)
AC_DEFUN([AC_HAVE_TYPE], [
AC_REQUIRE([AC_HEADER_STDC])
cv=`echo "$1" | sed 'y%./+- %__p__%'`
AC_MSG_CHECKING(for $1)
AC_CACHE_VAL([ac_cv_type_$cv],
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[
#include <sys/types.h>
#if STDC_HEADERS
#include <stdlib.h>
#include <stddef.h>
#endif
$2]],
[[$1 foo;]])],
[eval "ac_cv_type_$cv=yes"],
[eval "ac_cv_type_$cv=no"]))dnl
ac_foo=`eval echo \\$ac_cv_type_$cv`
AC_MSG_RESULT($ac_foo)
if test "$ac_foo" = yes; then
  ac_tr_hdr=HAVE_`echo $1 | sed 'y%abcdefghijklmnopqrstuvwxyz./- %ABCDEFGHIJKLMNOPQRSTUVWXYZ____%'`
if false; then
	AC_CHECK_TYPES($1)
fi
  AC_DEFINE_UNQUOTED($ac_tr_hdr, 1, [Define if you have type `$1'])
fi
])
