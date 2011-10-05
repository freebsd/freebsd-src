dnl
dnl $Id$
dnl
dnl Test for variable size arrays.
dnl

AC_DEFUN([rk_C_VARARRAY], [
	AC_CACHE_CHECK([if the compiler supports variable-length arrays],[rk_cv_c_vararray],[
	AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[]],[[int x = 0; { int y[x]; }]])],
		[rk_cv_c_vararray=yes],
		[rk_cv_c_vararray=no])])
	if test "$rk_cv_c_vararray" = yes; then
		AC_DEFINE([HAVE_VARIABLE_LENGTH_ARRAY], [1],
			[Define if your compiler supports variable-length arrays.])
	fi
])
