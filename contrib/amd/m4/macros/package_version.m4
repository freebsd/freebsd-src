dnl ######################################################################
dnl Version of package
AC_DEFUN(AMU_PACKAGE_VERSION,
[AC_MSG_CHECKING(version of package)
AC_DEFINE_UNQUOTED(PACKAGE_VERSION, "$1")
AC_MSG_RESULT(\"$1\")
])
dnl ======================================================================
