dnl ######################################################################
dnl check if compiler can handle "void *"
AC_DEFUN(AMU_C_VOID_P,
[
AC_CACHE_CHECK(if compiler can handle void *,
ac_cv_c_void_p,
[
# try to compile a program which uses void *
AC_TRY_COMPILE(
[ ],
[
void *vp;
], ac_cv_c_void_p=yes, ac_cv_c_void_p=no)
])
if test "$ac_cv_c_void_p" = yes
then
  AC_DEFINE(voidp, void *)
else
  AC_DEFINE(voidp, char *)
fi
])
dnl ======================================================================
