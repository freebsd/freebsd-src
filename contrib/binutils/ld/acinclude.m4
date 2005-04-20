sinclude(../bfd/acinclude.m4)

dnl sinclude(../libtool.m4) already included in bfd/acinclude.m4
dnl The lines below arrange for aclocal not to bring libtool.m4
dnl AM_PROG_LIBTOOL into aclocal.m4, while still arranging for automake
dnl to add a definition of LIBTOOL to Makefile.in.
ifelse(yes,no,[
AC_DEFUN([AM_PROG_LIBTOOL],)
AC_SUBST(LIBTOOL)
])

dnl sinclude(../gettext.m4) already included in bfd/acinclude.m4
ifelse(yes,no,[
AC_DEFUN([CY_WITH_NLS],)
AC_SUBST(INTLLIBS)
])
