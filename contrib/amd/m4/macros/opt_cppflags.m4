dnl ######################################################################
dnl Initial settings for CPPFLAGS (-I options)
dnl NOTE: this is for configuration as well as compilations!
AC_DEFUN(AMU_OPT_CPPFLAGS,
[AC_MSG_CHECKING(for configuration/compilation (-I) preprocessor flags)
AC_ARG_ENABLE(cppflags,
AC_HELP_STRING([--enable-cppflags=ARG],
	[configure/compile with ARG (-I) preprocessor flags]),
[
if test "$enableval" = "" || test "$enableval" = "yes" || test "$enableval" = "no"; then
  AC_MSG_ERROR(cppflags must be supplied if option is used)
fi
# use supplied options
CPPFLAGS="$CPPFLAGS $enableval"
export CPPFLAGS
AC_MSG_RESULT($enableval)
], [
  # default is to have no additional flags
  AC_MSG_RESULT(none)
])
])
dnl ======================================================================
