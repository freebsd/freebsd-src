dnl ######################################################################
dnl Find the type of the 3rd argument (in) to svc_sendreply() call
AC_DEFUN(AMU_TYPE_SVC_IN_ARG,
[
AC_CACHE_CHECK(for type of 3rd arg ('in') arg to svc_sendreply(),
ac_cv_type_svc_in_arg,
[
# try to compile a program which may have a definition for the type
dnl need a series of compilations, which will test out every possible type
dnl such as caddr_t, char *, etc.
# set to a default value
ac_cv_type_svc_in_arg=notfound
# look for "caddr_t"
if test "$ac_cv_type_svc_in_arg" = notfound
then
AC_TRY_COMPILE_RPC(
[ SVCXPRT *SX;
  xdrproc_t xp;
  caddr_t p;
  svc_sendreply(SX, xp, p);
], ac_cv_type_svc_in_arg="caddr_t", ac_cv_type_svc_in_arg=notfound)
fi
# look for "char *"
if test "$ac_cv_type_svc_in_arg" = notfound
then
AC_TRY_COMPILE_RPC(
[ SVCXPRT *SX;
  xdrproc_t xp;
  char *p;
  svc_sendreply(SX, xp, p);
], ac_cv_type_svc_in_arg="char *", ac_cv_type_svc_in_arg=notfound)
fi
])
if test "$ac_cv_type_svc_in_arg" != notfound
then
  AC_DEFINE_UNQUOTED(SVC_IN_ARG_TYPE, $ac_cv_type_svc_in_arg)
fi
])
dnl ======================================================================
