dnl
dnl $Id$
dnl

AC_DEFUN([rk_SUNOS],[
sunos=no
case "$host" in 
*-*-solaris2.7)
	sunos=57
	;;
*-*-solaris2.[[89]] | *-*-solaris2.1[[0-9]])
	sunos=58
	;;
*-*-solaris2*)
	sunos=50
	;;
esac
if test "$sunos" != no; then
	AC_DEFINE_UNQUOTED(SunOS, $sunos, 
		[Define to what version of SunOS you are running.])
fi
])