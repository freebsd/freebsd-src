dnl $Id$
dnl
dnl check for getpwnam_r, and if it's posix or not

AC_DEFUN([AC_CHECK_GETPWNAM_R_POSIX],[
AC_FIND_FUNC_NO_LIBS(getpwnam_r,c_r)
if test "$ac_cv_func_getpwnam_r" = yes; then
	AC_CACHE_CHECK(if getpwnam_r is posix,ac_cv_func_getpwnam_r_posix,
	ac_libs="$LIBS"
	LIBS="$LIBS $LIB_getpwnam_r"
	AC_RUN_IFELSE([AC_LANG_SOURCE([[
#define _POSIX_PTHREAD_SEMANTICS
#include <pwd.h>
int main(int argc, char **argv)
{
	struct passwd pw, *pwd;
	return getpwnam_r("", &pw, 0, 0, &pwd) < 0;
}
]])],[ac_cv_func_getpwnam_r_posix=yes],[ac_cv_func_getpwnam_r_posix=no],[:])
LIBS="$ac_libs")
	AC_CACHE_CHECK(if _POSIX_PTHREAD_SEMANTICS is needed,ac_cv_func_getpwnam_r_posix_def,
	ac_libs="$LIBS"
	LIBS="$LIBS $LIB_getpwnam_r"
	AC_RUN_IFELSE([AC_LANG_SOURCE([[
#include <pwd.h>
int main(int argc, char **argv)
{
	struct passwd pw, *pwd;
	return getpwnam_r("", &pw, 0, 0, &pwd) < 0;
}
]])],[ac_cv_func_getpwnam_r_posix_def=no],[ac_cv_func_getpwnam_r_posix_def=yes],[:])
LIBS="$ac_libs")
if test "$ac_cv_func_getpwnam_r_posix" = yes; then
	AC_DEFINE(POSIX_GETPWNAM_R, 1, [Define if getpwnam_r has POSIX flavour.])
fi
if test "$ac_cv_func_getpwnam_r_posix" = yes -a "$ac_cv_func_getpwnam_r_posix_def" = yes; then
	AC_DEFINE(_POSIX_PTHREAD_SEMANTICS, 1, [Define to get POSIX getpwnam_r in some systems.])
fi
fi
])
