dnl ######################################################################
dnl Do we want to compile with "ADDON" support? (hesiod, ldap, etc.)
AC_DEFUN(AMU_WITH_ADDON,
[AC_MSG_CHECKING([if $1 is wanted])
ac_upcase=`echo $1|tr 'abcdefghijklmnopqrstuvwxyz' 'ABCDEFGHIJKLMNOPQRSTUVWXYZ'`
AC_ARG_WITH($1,
 AC_HELP_STRING([--with-$1],
		[enable $2 support (default=yes if found)]
),[
if test "$withval" = "yes"; then
  with_$1=yes
elif test "$withval" = "no"; then
  with_$1=no
else
  AC_MSG_ERROR(please use \"yes\" or \"no\" with --with-$1)
fi
],[
with_$1=yes
])
if test "$with_$1" = "yes"
then
  AC_MSG_RESULT([yes, will enable if all libraries are found])
else
  AC_MSG_RESULT([no])
fi
])
