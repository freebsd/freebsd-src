dnl ######################################################################
dnl Package name
AC_DEFUN(AC_NAME_PACKAGE,
[AC_MSG_CHECKING(package name)
AC_DEFINE_UNQUOTED(PACKAGE, "$1")
AC_MSG_RESULT(\"$1\")
])
dnl ======================================================================
