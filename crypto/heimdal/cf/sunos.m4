dnl
dnl $Id: sunos.m4,v 1.1 2002/08/28 19:53:51 joda Exp $
dnl

AC_DEFUN([rk_SUNOS],[
sunos=no
case "$host" in 
*-*-sunos4*)
	sunos=40
	;;
*-*-solaris2.7)
	sunos=57
	;;
*-*-solaris2.[89])
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