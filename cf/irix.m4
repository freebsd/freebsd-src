dnl
dnl $Id$
dnl

AC_DEFUN([rk_IRIX],
[
irix=no
case "$host" in
*-*-irix*) 
	irix=yes
	;;
esac
AM_CONDITIONAL(IRIX, test "$irix" != no)dnl

])
