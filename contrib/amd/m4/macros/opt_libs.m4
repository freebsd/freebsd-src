dnl ######################################################################
dnl Initial settings for LIBS (-l options)
dnl NOTE: this is for configuration as well as compilations!
AC_DEFUN(AMU_OPT_LIBS,
[AC_MSG_CHECKING(for configuration/compilation (-l) library flags)
AC_ARG_ENABLE(libs,
AC_HELP_STRING([--enable-libs=ARG],
		[configure/compile with ARG (-l) library flags]),
[
if test "$enableval" = "" || test "$enableval" = "yes" || test "$enableval" = "no"; then
  AC_MSG_ERROR(libs must be supplied if option is used)
fi
# use supplied options
LIBS="$LIBS $enableval"
export LIBS
AC_MSG_RESULT($enableval)
], [
  # default is to have no additional flags
  AC_MSG_RESULT(none)
])
])
dnl ======================================================================
