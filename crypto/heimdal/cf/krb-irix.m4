dnl
dnl $Id: krb-irix.m4,v 1.2 2000/12/13 12:48:45 assar Exp $
dnl

dnl requires AC_CANONICAL_HOST
AC_DEFUN(KRB_IRIX,[
irix=no
case "$host_os" in
irix*) irix=yes ;;
esac
AM_CONDITIONAL(IRIX, test "$irix" != no)dnl
])
