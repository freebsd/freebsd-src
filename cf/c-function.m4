dnl
dnl $Id$
dnl

dnl
dnl Test for __FUNCTION__
dnl

AC_DEFUN([AC_C___FUNCTION__], [
AC_MSG_CHECKING(for __FUNCTION__)
AC_CACHE_VAL(ac_cv___function__, [
AC_RUN_IFELSE([AC_LANG_SOURCE([[
#include <string.h>

static char *foo(void)
{
  return __FUNCTION__;
}

int main(int argc, char **argc)
{
  return strcmp(foo(), "foo") != 0;
}
]])],
[ac_cv___function__=yes],
[ac_cv___function__=no],
[ac_cv___function__=no])])
if test "$ac_cv___function__" = "yes"; then
  AC_DEFINE(HAVE___FUNCTION__, 1, [define if your compiler has __FUNCTION__])
fi
AC_MSG_RESULT($ac_cv___function__)
])

