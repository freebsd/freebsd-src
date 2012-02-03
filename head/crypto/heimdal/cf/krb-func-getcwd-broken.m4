dnl $Id: krb-func-getcwd-broken.m4 15455 2005-06-16 21:03:43Z lha $
dnl
dnl
dnl test for broken getcwd in (SunOS braindamage)
dnl

AC_DEFUN([AC_KRB_FUNC_GETCWD_BROKEN], [
if test "$ac_cv_func_getcwd" = yes; then
AC_MSG_CHECKING(if getcwd is broken)
AC_CACHE_VAL(ac_cv_func_getcwd_broken, [
ac_cv_func_getcwd_broken=no

AC_RUN_IFELSE([AC_LANG_SOURCE([[
#include <errno.h>
char *getcwd(char*, int);

void *popen(char *cmd, char *mode)
{
	errno = ENOTTY;
	return 0;
}

int main(int argc, char **argv)
{
	char *ret;
	ret = getcwd(0, 1024);
	if(ret == 0 && errno == ENOTTY)
		return 0;
	return 1;
}
]])], [ac_cv_func_getcwd_broken=yes],[:],[:])
])
if test "$ac_cv_func_getcwd_broken" = yes; then
	AC_DEFINE(BROKEN_GETCWD, 1, [Define if getcwd is broken (like in SunOS 4).])dnl
	AC_LIBOBJ(getcwd)
	AC_MSG_RESULT($ac_cv_func_getcwd_broken)
else
	AC_MSG_RESULT([seems ok])
fi
fi
])
