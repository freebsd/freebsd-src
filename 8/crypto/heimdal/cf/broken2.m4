dnl $Id: broken2.m4 14181 2004-08-31 12:53:36Z joda $
dnl
dnl AC_BROKEN but with more arguments

dnl AC_BROKEN2(func, includes, arguments)
AC_DEFUN([AC_BROKEN2],
[AC_MSG_CHECKING([for $1])
AC_CACHE_VAL(ac_cv_func_[]$1,
[AC_LINK_IFELSE([AC_LANG_PROGRAM([[$2]],[[
/* The GNU C library defines this for functions which it implements
    to always fail with ENOSYS.  Some functions are actually named
    something starting with __ and the normal name is an alias.  */
#if defined (__stub_$1) || defined (__stub___$1)
choke me
#else
$1($3);
#endif
]])], [eval "ac_cv_func_[]$1=yes"], [eval "ac_cv_func_[]$1=no"])])
if eval "test \"\${ac_cv_func_[]$1}\" = yes"; then
  AC_DEFINE_UNQUOTED(AS_TR_CPP(HAVE_[]$1), 1, define)
  AC_MSG_RESULT(yes)
else
  AC_MSG_RESULT(no)
  rk_LIBOBJ($1)
fi])
