dnl ######################################################################
dnl Initial settings for LDFLAGS (-L options)
dnl NOTE: this is for configuration as well as compilations!
AC_DEFUN(AMU_OPT_LDFLAGS,
[AC_MSG_CHECKING(for configuration/compilation (-L) library flags)
AC_ARG_ENABLE(ldflags,
AC_HELP_STRING([--enable-ldflags=ARG],
		[configure/compile with ARG (-L) library flags]),
[
if test "$enableval" = "" || test "$enableval" = "yes" || test "$enableval" = "no"; then
  AC_MSG_ERROR(ldflags must be supplied if option is used)
fi
# use supplied options
LDFLAGS="$LDFLAGS $enableval"
export LDFLAGS
AC_MSG_RESULT($enableval)
], [
  # default is to have no additional flags
  AC_MSG_RESULT(none)
])
])
dnl ======================================================================
