dnl My version is similar to the one from Autoconf 2.52, but I also
dnl define HAVE_BAD_MEMCMP so that I can do smarter things to avoid
dnl linkage conflicts with bad memcmp versions that are in libc.
AC_DEFUN(AMU_FUNC_BAD_MEMCMP,
[
AC_FUNC_MEMCMP
if test "$ac_cv_func_memcmp_working" = no
then
AC_DEFINE(HAVE_BAD_MEMCMP)
fi
])
