dnl $Id: test-package.m4,v 1.9 2000/12/15 04:54:24 assar Exp $
dnl
dnl AC_TEST_PACKAGE_NEW(package,headers,libraries,extra libs,default locations, conditional)

AC_DEFUN(AC_TEST_PACKAGE,[AC_TEST_PACKAGE_NEW($1,[#include <$2>],$4,,$5)])

AC_DEFUN(AC_TEST_PACKAGE_NEW,[
AC_ARG_WITH($1,
[  --with-$1=dir                use $1 in dir])
AC_ARG_WITH($1-lib,
[  --with-$1-lib=dir            use $1 libraries in dir],
[if test "$withval" = "yes" -o "$withval" = "no"; then
  AC_MSG_ERROR([No argument for --with-$1-lib])
elif test "X$with_$1" = "X"; then
  with_$1=yes
fi])
AC_ARG_WITH($1-include,
[  --with-$1-include=dir        use $1 headers in dir],
[if test "$withval" = "yes" -o "$withval" = "no"; then
  AC_MSG_ERROR([No argument for --with-$1-include])
elif test "X$with_$1" = "X"; then
  with_$1=yes
fi])

AC_MSG_CHECKING(for $1)

case "$with_$1" in
yes)	;;
no)	;;
"")	;;
*)	if test "$with_$1_include" = ""; then
		with_$1_include="$with_$1/include"
	fi
	if test "$with_$1_lib" = ""; then
		with_$1_lib="$with_$1/lib$abilibdirext"
	fi
	;;
esac
header_dirs=
lib_dirs=
d='$5'
for i in $d; do
	header_dirs="$header_dirs $i/include"
	lib_dirs="$lib_dirs $i/lib$abilibdirext"
done

case "$with_$1_include" in
yes) ;;
no)  ;;
*)   header_dirs="$with_$1_include $header_dirs";;
esac
case "$with_$1_lib" in
yes) ;;
no)  ;;
*)   lib_dirs="$with_$1_lib $lib_dirs";;
esac

save_CFLAGS="$CFLAGS"
save_LIBS="$LIBS"
ires= lres=
for i in $header_dirs; do
	CFLAGS="-I$i $save_CFLAGS"
	AC_TRY_COMPILE([$2],,ires=$i;break)
done
for i in $lib_dirs; do
	LIBS="-L$i $3 $4 $save_LIBS"
	AC_TRY_LINK([$2],,lres=$i;break)
done
CFLAGS="$save_CFLAGS"
LIBS="$save_LIBS"

if test "$ires" -a "$lres" -a "$with_$1" != "no"; then
	$1_includedir="$ires"
	$1_libdir="$lres"
	INCLUDE_$1="-I$$1_includedir"
	LIB_$1="-L$$1_libdir $3"
	m4_ifval([$6],
		AC_DEFINE_UNQUOTED($6,1,[Define if you have the $1 package.]),
		AC_DEFINE_UNQUOTED(upcase($1),1,[Define if you have the $1 package.]))
	with_$1=yes
	AC_MSG_RESULT([headers $ires, libraries $lres])
else
	INCLUDE_$1=
	LIB_$1=
	with_$1=no
	AC_MSG_RESULT($with_$1)
fi
dnl m4_ifval([$6],
dnl 	AM_CONDITIONAL($6, test "$with_$1" = yes)
dnl 	AM_CONDITIONAL(upcase($1), test "$with_$1" = yes))
AC_SUBST(INCLUDE_$1)
AC_SUBST(LIB_$1)
])
