dnl $Id: krb-prog-ranlib.m4 13338 2004-02-12 14:21:14Z lha $
dnl
dnl
dnl Also look for EMXOMF for OS/2
dnl

AC_DEFUN([AC_KRB_PROG_RANLIB],
[AC_CHECK_PROGS(RANLIB, ranlib EMXOMF, :)])
