dnl ######################################################################
dnl check for type of xdrproc_t (usually in <rpc/xdr.h>)
AC_DEFUN(AMU_TYPE_XDRPROC_T,
[
AC_CACHE_CHECK(for xdrproc_t,
ac_cv_type_xdrproc_t,
[
# try to compile a program which may have a definition for the type
dnl need a series of compilations, which will test out every possible type
# look for "xdrproc_t"
AC_TRY_COMPILE_RPC(
[ xdrproc_t xdr_int;
], ac_cv_type_xdrproc_t=yes, ac_cv_type_xdrproc_t=no)
])
if test "$ac_cv_type_xdrproc_t" = yes
then
  AC_DEFINE_UNQUOTED(XDRPROC_T_TYPE, xdrproc_t)
fi
])
dnl ======================================================================
