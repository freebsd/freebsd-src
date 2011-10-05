dnl $Id$
dnl
AC_DEFUN([AC_BROKEN_SNPRINTF], [
AC_CACHE_CHECK(for working snprintf,ac_cv_func_snprintf_working,
ac_cv_func_snprintf_working=yes
AC_RUN_IFELSE([AC_LANG_SOURCE([[
#include <stdio.h>
#include <string.h>
int main(int argc, char **argv)
{
	char foo[[3]];
	snprintf(foo, 2, "12");
	return strcmp(foo, "1") || snprintf(NULL, 0, "%d", 12) != 2;
}]])],[:],[ac_cv_func_snprintf_working=no],[:]))

if test "$ac_cv_func_snprintf_working" = yes; then
	AC_DEFINE_UNQUOTED(HAVE_SNPRINTF, 1, [define if you have a working snprintf])
fi
if test "$ac_cv_func_snprintf_working" = yes; then
AC_NEED_PROTO([#include <stdio.h>],snprintf)
fi
])

AC_DEFUN([AC_BROKEN_VSNPRINTF],[
AC_CACHE_CHECK(for working vsnprintf,ac_cv_func_vsnprintf_working,
ac_cv_func_vsnprintf_working=yes
AC_RUN_IFELSE([AC_LANG_SOURCE([[
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

int foo(int num, ...)
{
	char bar[[3]];
	va_list arg;
	va_start(arg, num);
	vsnprintf(bar, 2, "%s", arg);
	va_end(arg);
	return strcmp(bar, "1");
}

int bar(int num, int len, ...)
{
	int r;
	va_list arg;
	va_start(arg, len);
	r = vsnprintf(NULL, 0, "%s", arg);
	va_end(arg);
	return r != len;
}

int main(int argc, char **argv)
{
	return foo(0, "12") || bar(0, 2, "12");
}]])],[:],[ac_cv_func_vsnprintf_working=no],[:]))

if test "$ac_cv_func_vsnprintf_working" = yes; then
	AC_DEFINE_UNQUOTED(HAVE_VSNPRINTF, 1, [define if you have a working vsnprintf])
fi
if test "$ac_cv_func_vsnprintf_working" = yes; then
AC_NEED_PROTO([#include <stdio.h>],vsnprintf)
fi
])
