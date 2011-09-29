dnl $Id: find-func.m4 13338 2004-02-12 14:21:14Z lha $
dnl
dnl AC_FIND_FUNC(func, libraries, includes, arguments)
AC_DEFUN([AC_FIND_FUNC], [
AC_FIND_FUNC_NO_LIBS([$1], [$2], [$3], [$4])
if test -n "$LIB_$1"; then
	LIBS="$LIB_$1 $LIBS"
fi
])
