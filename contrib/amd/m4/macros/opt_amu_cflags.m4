dnl ######################################################################
dnl Which options to add to CFLAGS for compilation?
dnl NOTE: this is only for final compiltions, not for configure tests)
AC_DEFUN(AMU_OPT_AMU_CFLAGS,
[AC_MSG_CHECKING(for additional C option compilation flags)
AC_ARG_ENABLE(am-cflags,
AC_HELP_STRING([--enable-am-cflags=ARG],
		[compile package with ARG additional C flags]),
[
if test "$enableval" = "" || test "$enableval" = "yes" || test "$enableval" = "no"; then
  AC_MSG_ERROR(am-cflags must be supplied if option is used)
fi
# user supplied a cflags option to configure
AMU_CFLAGS="$enableval"
AC_SUBST(AMU_CFLAGS)
AC_MSG_RESULT($enableval)
], [
  # default is to have no additional C flags
  AMU_CFLAGS=""
  AC_SUBST(AMU_CFLAGS)
  AC_MSG_RESULT(none)
])
])
dnl ======================================================================
