dnl
dnl $Id: have-types.m4,v 1.1 1999/07/24 18:38:58 assar Exp $
dnl

AC_DEFUN(AC_HAVE_TYPES, [
for i in $1; do
        AC_HAVE_TYPE($i)
done
: << END
changequote(`,')dnl
@@@funcs="$funcs $1"@@@
changequote([,])dnl
END
])
