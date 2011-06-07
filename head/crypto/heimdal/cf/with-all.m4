dnl
dnl $Id: with-all.m4 14147 2004-08-25 14:14:01Z joda $
dnl

dnl AC_WITH_ALL(name)

AC_DEFUN([AC_WITH_ALL], [
AC_ARG_WITH($1,
	AS_HELP_STRING([--with-$1=dir],
		[use $1 in dir]))

AC_ARG_WITH($1-lib,
	AS_HELP_STRING([--with-$1-lib=dir],
		[use $1 libraries in dir]),
[if test "$withval" = "yes" -o "$withval" = "no"; then
  AC_MSG_ERROR([No argument for --with-$1-lib])
elif test "X$with_$1" = "X"; then
  with_$1=yes
fi])

AC_ARG_WITH($1-include,
	AS_HELP_STRING([--with-$1-include=dir],
		[use $1 headers in dir]),
[if test "$withval" = "yes" -o "$withval" = "no"; then
  AC_MSG_ERROR([No argument for --with-$1-include])
elif test "X$with_$1" = "X"; then
  with_$1=yes
fi])

case "$with_$1" in
yes)	;;
no)	;;
"")	;;
*)	if test "$with_$1_include" = ""; then
		with_$1_include="$with_$1/include"
	fi
	if test "$with_$1_lib" = ""; then
		with_$1_lib="$with_$1/lib$abilibdirext"
	fi
	;;
esac
])