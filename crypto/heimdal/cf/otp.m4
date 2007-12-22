dnl $Id: otp.m4,v 1.2 2002/05/19 20:51:08 joda Exp $
dnl
dnl check requirements for OTP library
dnl
AC_DEFUN([rk_OTP],[
AC_REQUIRE([rk_DB])dnl
AC_ARG_ENABLE(otp,
	AC_HELP_STRING([--disable-otp],[if you don't want OTP support]))
if test "$enable_otp" = yes -a "$db_type" = unknown; then
	AC_MSG_ERROR([OTP requires a NDBM/DB compatible library])
fi
if test "$enable_otp" != no; then
	if test "$db_type" != unknown; then
		enable_otp=yes
	else
		enable_otp=no
	fi
fi
if test "$enable_otp" = yes; then
	AC_DEFINE(OTP, 1, [Define if you want OTP support in applications.])
	LIB_otp='$(top_builddir)/lib/otp/libotp.la'
	AC_SUBST(LIB_otp)
fi
AC_MSG_CHECKING([whether to enable OTP library])
AC_MSG_RESULT($enable_otp)
AM_CONDITIONAL(OTP, test "$enable_otp" = yes)dnl
])
