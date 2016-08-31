dnl find bison/yacc, but error if none found
AC_DEFUN([AMU_PROG_YACC],
[AC_CHECK_PROGS(YACC, 'bison -y' byacc yacc, :)
if test "x$YACC" = "x:"; then
  AC_MSG_ERROR([cannot find bison/yacc/byacc -- needed for am-utils])
fi])
