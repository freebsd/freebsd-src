dnl ######################################################################
dnl check if a local configuration file exists
AC_DEFUN(AMU_LOCALCONFIG,
[AC_MSG_CHECKING(a local configuration file)
if test -f localconfig.h
then
  AC_DEFINE(HAVE_LOCALCONFIG_H)
  AC_MSG_RESULT(yes)
else
  AC_MSG_RESULT(no)
fi
])
dnl ======================================================================
