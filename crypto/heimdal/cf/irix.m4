dnl
dnl $Id: irix.m4,v 1.1 2002/08/28 19:11:44 joda Exp $
dnl

AC_DEFUN([rk_IRIX],
[
irix=no
case "$host" in
*-*-irix4*) 
	AC_DEFINE([IRIX4], 1,
		[Define if you are running IRIX 4.])
	irix=yes
	;;
*-*-irix*) 
	irix=yes
	;;
esac
AM_CONDITIONAL(IRIX, test "$irix" != no)dnl

AH_BOTTOM([
/* IRIX 4 braindamage */
#if IRIX == 4 && !defined(__STDC__)
#define __STDC__ 0
#endif
])
])
