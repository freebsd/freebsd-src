dnl $Id: have-type.m4,v 1.4 1999/07/24 19:23:01 assar Exp $
dnl
dnl check for existance of a type

dnl AC_HAVE_TYPE(TYPE,INCLUDES)
AC_DEFUN(AC_HAVE_TYPE, [
cv=`echo "$1" | sed 'y%./+- %__p__%'`
AC_MSG_CHECKING(for $1)
AC_CACHE_VAL([ac_cv_type_$cv],
AC_TRY_COMPILE(
[#include <sys/types.h>
#if STDC_HEADERS
#include <stdlib.h>
#include <stddef.h>
#endif
$2],
[$1 foo;],
eval "ac_cv_type_$cv=yes",
eval "ac_cv_type_$cv=no"))dnl
AC_MSG_RESULT(`eval echo \\$ac_cv_type_$cv`)
if test `eval echo \\$ac_cv_type_$cv` = yes; then
  ac_tr_hdr=HAVE_`echo $1 | sed 'y%abcdefghijklmnopqrstuvwxyz./- %ABCDEFGHIJKLMNOPQRSTUVWXYZ____%'`
dnl autoheader tricks *sigh*
define(foo,translit($1, [ ], [_]))
: << END
@@@funcs="$funcs foo"@@@
END
undefine([foo])
  AC_DEFINE_UNQUOTED($ac_tr_hdr, 1)
fi
])
