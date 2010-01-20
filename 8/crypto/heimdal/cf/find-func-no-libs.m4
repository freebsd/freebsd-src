dnl $Id: find-func-no-libs.m4 13338 2004-02-12 14:21:14Z lha $
dnl
dnl
dnl Look for function in any of the specified libraries
dnl

dnl AC_FIND_FUNC_NO_LIBS(func, libraries, includes, arguments, extra libs, extra args)
AC_DEFUN([AC_FIND_FUNC_NO_LIBS], [
AC_FIND_FUNC_NO_LIBS2([$1], ["" $2], [$3], [$4], [$5], [$6])])
