dnl $Id: check-var.m4,v 1.2 1999/03/01 09:52:23 joda Exp $
dnl
dnl AC_CHECK_VAR(includes, variable)
AC_DEFUN(AC_CHECK_VAR, [
AC_MSG_CHECKING(for $2)
AC_CACHE_VAL(ac_cv_var_$2, [
AC_TRY_LINK([extern int $2;
int foo() { return $2; }],
	    [foo()],
	    ac_cv_var_$2=yes, ac_cv_var_$2=no)
])
define([foo], [HAVE_]translit($2, [a-z], [A-Z]))

AC_MSG_RESULT(`eval echo \\$ac_cv_var_$2`)
if test `eval echo \\$ac_cv_var_$2` = yes; then
	AC_DEFINE_UNQUOTED(foo, 1, [define if you have $2])
	AC_CHECK_DECLARATION([$1],[$2])
fi
undefine([foo])
])
