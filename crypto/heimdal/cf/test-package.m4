dnl $Id: test-package.m4,v 1.12 2002/09/10 15:23:38 joda Exp $
dnl
dnl rk_TEST_PACKAGE(package,headers,libraries,extra libs,
dnl			default locations, conditional, config-program)

AC_DEFUN(rk_TEST_PACKAGE,[
AC_ARG_WITH($1,
	AC_HELP_STRING([--with-$1=dir],[use $1 in dir]))
AC_ARG_WITH($1-lib,
	AC_HELP_STRING([--with-$1-lib=dir],[use $1 libraries in dir]),
[if test "$withval" = "yes" -o "$withval" = "no"; then
  AC_MSG_ERROR([No argument for --with-$1-lib])
elif test "X$with_$1" = "X"; then
  with_$1=yes
fi])
AC_ARG_WITH($1-include,
	AC_HELP_STRING([--with-$1-include=dir],[use $1 headers in dir]),
[if test "$withval" = "yes" -o "$withval" = "no"; then
  AC_MSG_ERROR([No argument for --with-$1-include])
elif test "X$with_$1" = "X"; then
  with_$1=yes
fi])
AC_ARG_WITH($1-config,
	AC_HELP_STRING([--with-$1-config=path],[config program for $1]))

m4_ifval([$6],
	m4_define([rk_pkgname], $6),
	m4_define([rk_pkgname], AS_TR_CPP($1)))

AC_MSG_CHECKING(for $1)

case "$with_$1" in
yes|"") d='$5' ;;
no)	d= ;;
*)	d="$with_$1" ;;
esac

header_dirs=
lib_dirs=
for i in $d; do
	if test "$with_$1_include" = ""; then
		if test -d "$i/include/$1"; then
			header_dirs="$header_dirs $i/include/$1"
		fi
		if test -d "$i/include"; then
			header_dirs="$header_dirs $i/include"
		fi
	fi
	if test "$with_$1_lib" = ""; then
		if test -d "$i/lib$abilibdirext"; then
			lib_dirs="$lib_dirs $i/lib$abilibdirext"
		fi
	fi
done

if test "$with_$1_include"; then
	header_dirs="$with_$1_include $header_dirs"
fi
if test "$with_$1_lib"; then
	lib_dirs="$with_$1_lib $lib_dirs"
fi

if test "$with_$1_config" = ""; then
	with_$1_config='$7'
fi

$1_cflags=
$1_libs=

case "$with_$1_config" in
yes|no|"")
	;;
*)
	$1_cflags="`$with_$1_config --cflags 2>&1`"
	$1_libs="`$with_$1_config --libs 2>&1`"
	;;
esac

found=no
if test "$with_$1" != no; then
	save_CFLAGS="$CFLAGS"
	save_LIBS="$LIBS"
	if test "$[]$1_cflags" -a "$[]$1_libs"; then
		CFLAGS="$[]$1_cflags $save_CFLAGS"
		LIBS="$[]$1_libs $save_LIBS"
		AC_TRY_LINK([$2],,[
			INCLUDE_$1="$[]$1_cflags"
			LIB_$1="$[]$1_libs"
			AC_MSG_RESULT([from $with_$1_config])
			found=yes])
	fi
	if test "$found" = no; then
		ires= lres=
		for i in $header_dirs; do
			CFLAGS="-I$i $save_CFLAGS"
			AC_TRY_COMPILE([$2],,ires=$i;break)
		done
		for i in $lib_dirs; do
			LIBS="-L$i $3 $4 $save_LIBS"
			AC_TRY_LINK([$2],,lres=$i;break)
		done
		if test "$ires" -a "$lres" -a "$with_$1" != "no"; then
			INCLUDE_$1="-I$ires"
			LIB_$1="-L$lres $3 $4"
			found=yes
			AC_MSG_RESULT([headers $ires, libraries $lres])
		fi
	fi
	CFLAGS="$save_CFLAGS"
	LIBS="$save_LIBS"
fi

if test "$found" = yes; then
	AC_DEFINE_UNQUOTED(rk_pkgname, 1, [Define if you have the $1 package.])
	with_$1=yes
else
	with_$1=no
	INCLUDE_$1=
	LIB_$1=
	AC_MSG_RESULT(no)
fi

AC_SUBST(INCLUDE_$1)
AC_SUBST(LIB_$1)
])
