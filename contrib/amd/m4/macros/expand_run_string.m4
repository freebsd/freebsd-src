dnl ######################################################################
dnl Run a program and print its output as a string
dnl Takes: (header, code-to-run, [action-if-found, [action-if-not-found]])
AC_DEFUN(AMU_EXPAND_RUN_STRING,
[
value="notfound"
AC_TRY_RUN(
[
$1
main(argc)
int argc;
{
$2
exit(0);
}], value=`./conftest dummy 2>>config.log`, value="notfound", value="notfound")
if test "$value" = notfound
then
  :
  $4
else
  :
  $3
fi
])
dnl ======================================================================
