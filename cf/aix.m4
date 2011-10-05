dnl
dnl $Id$
dnl

AC_DEFUN([rk_AIX],[

aix=no
case "$host" in 
*-*-aix3*)
	aix=3
	;;
*-*-aix[[4-9]]*)
	aix=4
	;;
esac

AM_CONDITIONAL(AIX, test "$aix" != no)dnl
AM_CONDITIONAL(AIX4, test "$aix" = 4)dnl

AC_ARG_ENABLE(dynamic-afs,
	AS_HELP_STRING([--disable-dynamic-afs],
		[do not use loaded AFS library with AIX]))

if test "$aix" != no; then

	AC_DEFINE(NEED_QSORT, 1, [if your qsort is not a stable sort])

	if test "$enable_dynamic_afs" != no; then
		AC_REQUIRE([rk_DLOPEN])
		if test "$ac_cv_func_dlopen" = no; then
			AC_FIND_FUNC_NO_LIBS(loadquery, ld)
		fi
		if test "$ac_cv_func_dlopen" != no; then
			AIX_EXTRA_KAFS='$(LIB_dlopen)'
		elif test "$ac_cv_func_loadquery" != no; then
			AIX_EXTRA_KAFS='$(LIB_loadquery)'
		else
			AC_MSG_NOTICE([not using dynloaded AFS library])
			AIX_EXTRA_KAFS=
			enable_dynamic_afs=no
		fi
	else
		AIX_EXTRA_KAFS=
	fi
fi

AM_CONDITIONAL(AIX_DYNAMIC_AFS, test "$enable_dynamic_afs" != no)dnl
AC_SUBST(AIX_EXTRA_KAFS)dnl

if test "$aix" != no; then
	AC_DEFINE([_ALL_SOURCE],1,[Required for functional/sane headers on AIX])
fi

AH_BOTTOM([#if _AIX
/* XXX this is gross, but kills about a gazillion warnings */
struct ether_addr;
struct sockaddr;
struct sockaddr_dl;
struct sockaddr_in;
#endif])

])
