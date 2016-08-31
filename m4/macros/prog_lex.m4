dnl find f/lex, but error if none found
AC_DEFUN_ONCE([AMU_PROG_LEX],
[AC_CHECK_PROGS(LEX, flex lex, :)
if test -z "$LEXLIB"
then
  AC_CHECK_LIB(fl, yywrap, LEXLIB="-lfl",
    [AC_CHECK_LIB(l, yywrap, LEXLIB="-ll")])
fi
AC_SUBST(LEXLIB)
if test "x$LEX" != "x:"; then
  _AC_PROG_LEX_YYTEXT_DECL
else
  AC_MSG_ERROR([cannot find flex/lex -- needed for am-utils])
fi])
