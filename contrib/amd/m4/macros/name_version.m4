dnl ######################################################################
dnl Version of package
AC_DEFUN(AC_NAME_VERSION,
[AC_MSG_CHECKING(version of package)
AC_DEFINE_UNQUOTED(VERSION, "$1")
AC_MSG_RESULT(\"$1\")
])
dnl ======================================================================
