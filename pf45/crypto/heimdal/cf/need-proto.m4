dnl $Id: need-proto.m4 14166 2004-08-26 12:35:42Z joda $
dnl
dnl
dnl Check if we need the prototype for a function
dnl

dnl AC_NEED_PROTO(includes, function)

AC_DEFUN([AC_NEED_PROTO], [
if test "$ac_cv_func_$2+set" != set -o "$ac_cv_func_$2" = yes; then
AC_CACHE_CHECK([if $2 needs a prototype], ac_cv_func_$2_noproto,
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[$1
struct foo { int foo; } xx;
extern int $2 (struct foo*);]],[[$2(&xx)]])],
[eval "ac_cv_func_$2_noproto=yes"],
[eval "ac_cv_func_$2_noproto=no"]))
if test "$ac_cv_func_$2_noproto" = yes; then
	AC_DEFINE(AS_TR_CPP(NEED_[]$2[]_PROTO), 1,
		[define if the system is missing a prototype for $2()])
fi
fi
])
