dnl ######################################################################
dnl Bugreport name
AC_DEFUN(AMU_PACKAGE_BUGREPORT,
[AC_MSG_CHECKING(bug-reporting address)
AC_DEFINE_UNQUOTED(PACKAGE_BUGREPORT, "$1")
AC_MSG_RESULT(\"$1\")
])
dnl ======================================================================
