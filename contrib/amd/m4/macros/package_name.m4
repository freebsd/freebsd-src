dnl ######################################################################
dnl Package name
AC_DEFUN(AMU_PACKAGE_NAME,
[AC_MSG_CHECKING(package name)
AC_DEFINE_UNQUOTED(PACKAGE_NAME, "$1")
AC_MSG_RESULT(\"$1\")
])
dnl ======================================================================
