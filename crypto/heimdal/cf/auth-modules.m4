dnl $Id: auth-modules.m4,v 1.5.6.1 2004/04/01 07:27:32 joda Exp $
dnl
dnl Figure what authentication modules should be built
dnl
dnl rk_AUTH_MODULES(module-list)

AC_DEFUN([rk_AUTH_MODULES],[
AC_MSG_CHECKING([which authentication modules should be built])

z='m4_ifval([$1], $1, [sia pam afskauthlib])'
LIB_AUTH_SUBDIRS=
for i in $z; do
case $i in
sia)
if test "$ac_cv_header_siad_h" = yes; then
	LIB_AUTH_SUBDIRS="$LIB_AUTH_SUBDIRS sia"
fi
;;
pam)
case "${host}" in
*-*-freebsd*)	ac_cv_want_pam_krb4=no ;;
*)		ac_cv_want_pam_krb4=yes ;;
esac

if test "$ac_cv_want_pam_krb4" = yes -a \
    "$ac_cv_header_security_pam_modules_h" = yes -a \
    "$enable_shared" = yes; then
	LIB_AUTH_SUBDIRS="$LIB_AUTH_SUBDIRS pam"
fi
;;
afskauthlib)
case "${host}" in
*-*-irix[[56]]*) LIB_AUTH_SUBDIRS="$LIB_AUTH_SUBDIRS afskauthlib" ;;
esac
;;
esac
done
if test "$LIB_AUTH_SUBDIRS"; then
	AC_MSG_RESULT($LIB_AUTH_SUBDIRS)
else
	AC_MSG_RESULT(none)
fi

AC_SUBST(LIB_AUTH_SUBDIRS)dnl
])
