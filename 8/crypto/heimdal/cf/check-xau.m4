dnl $Id: check-xau.m4 15454 2005-06-16 21:02:16Z lha $
dnl
dnl check for Xau{Read,Write}Auth and XauFileName
dnl
AC_DEFUN([AC_CHECK_XAU],[
save_CFLAGS="$CFLAGS"
CFLAGS="$X_CFLAGS $CFLAGS"
save_LIBS="$LIBS"
dnl LIBS="$X_LIBS $X_PRE_LIBS $X_EXTRA_LIBS $LIBS"
LIBS="$X_PRE_LIBS $X_EXTRA_LIBS $LIBS"
save_LDFLAGS="$LDFLAGS"
LDFLAGS="$LDFLAGS $X_LIBS"

## check for XauWriteAuth first, so we detect the case where
## XauReadAuth is in -lX11, but XauWriteAuth is only in -lXau this
## could be done by checking for XauReadAuth in -lXau first, but this
## breaks in IRIX 6.5

AC_FIND_FUNC_NO_LIBS(XauWriteAuth, X11 Xau,[#include <X11/Xauth.h>],[0,0])
ac_xxx="$LIBS"
LIBS="$LIB_XauWriteAuth $LIBS"
AC_FIND_FUNC_NO_LIBS(XauReadAuth, X11 Xau,[#include <X11/Xauth.h>],[0])
LIBS="$LIB_XauReadAauth $LIBS"
AC_FIND_FUNC_NO_LIBS(XauFileName, X11 Xau,[#include <X11/Xauth.h>])
LIBS="$ac_xxx"

## set LIB_XauReadAuth to union of these tests, since this is what the
## Makefiles are using
case "$ac_cv_funclib_XauWriteAuth" in
yes)	;;
no)	;;
*)	if test "$ac_cv_funclib_XauReadAuth" = yes; then
		if test "$ac_cv_funclib_XauFileName" = yes; then
			LIB_XauReadAuth="$LIB_XauWriteAuth"
		else
			LIB_XauReadAuth="$LIB_XauWriteAuth $LIB_XauFileName"
		fi
	else
		if test "$ac_cv_funclib_XauFileName" = yes; then
			LIB_XauReadAuth="$LIB_XauReadAuth $LIB_XauWriteAuth"
		else
			LIB_XauReadAuth="$LIB_XauReadAuth $LIB_XauWriteAuth $LIB_XauFileName"
		fi
	fi
	;;
esac

if test "$AUTOMAKE" != ""; then
	AM_CONDITIONAL(NEED_WRITEAUTH, test "$ac_cv_func_XauWriteAuth" != "yes")
else
	AC_SUBST(NEED_WRITEAUTH_TRUE)
	AC_SUBST(NEED_WRITEAUTH_FALSE)
	if test "$ac_cv_func_XauWriteAuth" != "yes"; then
		NEED_WRITEAUTH_TRUE=
		NEED_WRITEAUTH_FALSE='#'
	else
		NEED_WRITEAUTH_TRUE='#'
		NEED_WRITEAUTH_FALSE=
	fi
fi
CFLAGS=$save_CFLAGS
LIBS=$save_LIBS
LDFLAGS=$save_LDFLAGS
])
