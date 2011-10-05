dnl $Id$
dnl
dnl rk_CHECK_VAR(variable, includes)
AC_DEFUN([rk_CHECK_VAR], [
AC_MSG_CHECKING(for $1)
AC_CACHE_VAL(ac_cv_var_$1, [
m4_ifval([$2],[
	AC_LINK_IFELSE([AC_LANG_PROGRAM([[$2
	void * foo(void) { return &$1; }]],[[foo()]])],
	    [ac_cv_var_$1=yes],[ac_cv_var_$1=no])])
if test "$ac_cv_var_$1" != yes ; then
AC_LINK_IFELSE([AC_LANG_PROGRAM([[$2
extern int $1;
int foo(void) { return $1; }]],[[foo()]])],
	    [ac_cv_var_$1=yes],[ac_cv_var_$1=no])
fi
])
ac_foo=`eval echo \\$ac_cv_var_$1`
AC_MSG_RESULT($ac_foo)
if test "$ac_foo" = yes; then
	AC_DEFINE_UNQUOTED(AS_TR_CPP(HAVE_[]$1), 1, 
		[Define if you have the `]$1[' variable.])
	m4_ifval([$2], AC_CHECK_DECLS([$1],[],[],[$2]))
fi
])

AC_WARNING_ENABLE([obsolete])
AU_DEFUN([AC_CHECK_VAR], [rk_CHECK_VAR([$2], [$1])], [foo])
