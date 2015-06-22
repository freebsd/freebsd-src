dnl ######################################################################
dnl NTP compiler basics
dnl
AC_DEFUN([NTP_PROG_CC], [

dnl must come before AC_PROG_CC or similar
AC_USE_SYSTEM_EXTENSIONS

dnl  we need to check for cross compile tools for vxWorks here
AC_PROG_CC
# Ralf Wildenhues: With per-target flags we need CC_C_O
# AM_PROG_CC_C_O supersets AC_PROG_CC_C_O
AM_PROG_CC_C_O
AC_PROG_GCC_TRADITIONAL
NTP_COMPILER
AC_C_BIGENDIAN
AC_C_VOLATILE
AC_PROG_CPP

])dnl
dnl ======================================================================
