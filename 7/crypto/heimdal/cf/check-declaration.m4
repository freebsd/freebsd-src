dnl $Id: check-declaration.m4,v 1.3.34.1 2004/04/01 07:27:32 joda Exp $
dnl
dnl
dnl Check if we need the declaration of a variable
dnl

dnl AC_HAVE_DECLARATION(includes, variable)
AC_DEFUN([AC_CHECK_DECLARATION], [
AC_MSG_CHECKING([if $2 is properly declared])
AC_CACHE_VAL(ac_cv_var_$2_declaration, [
AC_TRY_COMPILE([$1
extern struct { int foo; } $2;],
[$2.foo = 1;],
eval "ac_cv_var_$2_declaration=no",
eval "ac_cv_var_$2_declaration=yes")
])

define(foo, [HAVE_]translit($2, [a-z], [A-Z])[_DECLARATION])

AC_MSG_RESULT($ac_cv_var_$2_declaration)
if eval "test \"\$ac_cv_var_$2_declaration\" = yes"; then
	AC_DEFINE(foo, 1, [define if your system declares $2])
fi
undefine([foo])
])
