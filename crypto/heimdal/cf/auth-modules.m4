dnl $Id: auth-modules.m4,v 1.1 1999/03/21 13:48:00 joda Exp $
dnl
dnl Figure what authentication modules should be built

AC_DEFUN(AC_AUTH_MODULES,[
AC_MSG_CHECKING(which authentication modules should be built)

LIB_AUTH_SUBDIRS=

if test "$ac_cv_header_siad_h" = yes; then
	LIB_AUTH_SUBDIRS="$LIB_AUTH_SUBDIRS sia"
fi

if test "$ac_cv_header_security_pam_modules_h" = yes -a "$enable_shared" = yes; then
	LIB_AUTH_SUBDIRS="$LIB_AUTH_SUBDIRS pam"
fi

case "${host}" in
changequote(,)dnl
*-*-irix[56]*) LIB_AUTH_SUBDIRS="$LIB_AUTH_SUBDIRS afskauthlib" ;;
changequote([,])dnl
esac

AC_MSG_RESULT($LIB_AUTH_SUBDIRS)

AC_SUBST(LIB_AUTH_SUBDIRS)dnl
])
