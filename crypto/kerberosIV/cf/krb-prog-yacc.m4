dnl $Id: krb-prog-yacc.m4,v 1.1 1997/12/14 15:59:02 joda Exp $
dnl
dnl
dnl We prefer byacc or yacc because they do not use `alloca'
dnl

AC_DEFUN(AC_KRB_PROG_YACC,
[AC_CHECK_PROGS(YACC, byacc yacc 'bison -y')])
