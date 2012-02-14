dnl
dnl $Id: have-types.m4 13338 2004-02-12 14:21:14Z lha $
dnl

AC_DEFUN([AC_HAVE_TYPES], [
for i in $1; do
        AC_HAVE_TYPE($i)
done
if false;then
	AC_CHECK_FUNCS($1)
fi
])
