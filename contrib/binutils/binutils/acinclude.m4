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

## Replacement for AC_PROG_LEX and AC_DECL_YYTEXT
## by Alexandre Oliva <oliva@dcc.unicamp.br>

## We need to override the installed aclocal/lex.m4 because of a bug in
## this definition in the recommended automake snapshot of 000227:
## There were double-quotes around ``$missing_dir/missing flex'' which was
## bad since aclocal wraps it in double-quotes.

dnl AM_PROG_LEX
dnl Look for flex, lex or missing, then run AC_PROG_LEX and AC_DECL_YYTEXT
AC_DEFUN(AM_PROG_LEX,
[missing_dir=ifelse([$1],,`cd $ac_aux_dir && pwd`,$1)
AC_CHECK_PROGS(LEX, flex lex, [$missing_dir/missing flex])
AC_PROG_LEX
AC_DECL_YYTEXT])
