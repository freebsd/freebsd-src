dnl ######################################################################
dnl check if compiler can handle variable-length argument macros
AC_DEFUN([AMU_VARARGS_MACROS],
[
AC_CACHE_CHECK(if compiler can handle variable-length macros,
ac_cv_varargs_macros,
[
# try C99 style
AC_TRY_COMPILE(
[
#define foo(str,size,fmt,...)  bar(__FILE__,__LINE__,(str),(size),(fmt),__VA_ARGS__)
],
[
char a[80];
foo(a, sizeof(a), "%d,%d", 1, 2);
], ac_cv_varargs_macros=c99,
# else try gcc style
AC_TRY_COMPILE(
[
#define foo(str,size,args...)  bar(__FILE__,__LINE__,(str),(size),(fmt),args)
],
[
char a[80];
foo(a, sizeof(a), "%d,%d", 1, 2);
], ac_cv_varargs_macros=gcc, ac_cv_varargs_macros=none))
])
if test "$ac_cv_varargs_macros" = c99
then
  AC_DEFINE(HAVE_C99_VARARGS_MACROS, 1,
	 [System supports C99-style variable-length argument macros])
else
  if test "$ac_cv_varargs_macros" = gcc
  then
    AC_DEFINE(HAVE_GCC_VARARGS_MACROS, 1,
	 [System supports GCC-style variable-length argument macros])
  fi
fi
])
dnl ======================================================================
