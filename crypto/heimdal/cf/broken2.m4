dnl $Id: broken2.m4,v 1.1 2000/12/15 14:27:33 assar Exp $
dnl
dnl AC_BROKEN but with more arguments

dnl AC_BROKEN2(func, includes, arguments)
AC_DEFUN(AC_BROKEN2,
[for ac_func in $1
do
AC_MSG_CHECKING([for $ac_func])
AC_CACHE_VAL(ac_cv_func_$ac_func,
[AC_TRY_LINK([$2],
[
/* The GNU C library defines this for functions which it implements
    to always fail with ENOSYS.  Some functions are actually named
    something starting with __ and the normal name is an alias.  */
#if defined (__stub_$1) || defined (__stub___$1)
choke me
#else
$ac_func($3)
#endif
], [eval "ac_cv_func_$ac_func=yes"], [eval "ac_cv_func_$ac_func=no"])])
if eval "test \"\${ac_cv_func_$ac_func}\" = yes"; then
  ac_tr_func=HAVE_[]upcase($ac_func)
  AC_DEFINE_UNQUOTED($ac_tr_func)
  AC_MSG_RESULT(yes)
else
  AC_MSG_RESULT(no)
  LIBOBJS[]="$LIBOBJS ${ac_func}.o"
fi
done
if false; then
	AC_CHECK_FUNCS($1)
fi
AC_SUBST(LIBOBJS)dnl
])
