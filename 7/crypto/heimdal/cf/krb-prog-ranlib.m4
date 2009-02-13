dnl $Id: krb-prog-ranlib.m4,v 1.1.42.1 2004/04/01 07:27:34 joda Exp $
dnl
dnl
dnl Also look for EMXOMF for OS/2
dnl

AC_DEFUN([AC_KRB_PROG_RANLIB],
[AC_CHECK_PROGS(RANLIB, ranlib EMXOMF, :)])
