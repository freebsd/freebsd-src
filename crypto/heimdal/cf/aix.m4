dnl
dnl $Id: aix.m4,v 1.5 2000/11/05 17:15:46 joda Exp $
dnl

AC_DEFUN(KRB_AIX,[
aix=no
case "$host" in 
*-*-aix3*)
	aix=3
	;;
*-*-aix4*)
	aix=4
	;;
esac
AM_CONDITIONAL(AIX, test "$aix" != no)dnl
AM_CONDITIONAL(AIX4, test "$aix" = 4)
aix_dynamic_afs=yes
AM_CONDITIONAL(AIX_DYNAMIC_AFS, test "$aix_dynamic_afs" = yes)dnl

AC_FIND_FUNC_NO_LIBS(dlopen, dl)

if test "$aix" != no; then
	if test "$aix_dynamic_afs" = yes; then
		if test "$ac_cv_funclib_dlopen" = yes; then
			AIX_EXTRA_KAFS=
		elif test "$ac_cv_funclib_dlopen" != no; then
			AIX_EXTRA_KAFS="$ac_cv_funclib_dlopen"
		else
			AIX_EXTRA_KAFS=-lld
		fi
	else
		AIX_EXTRA_KAFS=
	fi
fi

AM_CONDITIONAL(HAVE_DLOPEN, test "$ac_cv_funclib_dlopen" != no)dnl
AC_SUBST(AIX_EXTRA_KAFS)dnl

])