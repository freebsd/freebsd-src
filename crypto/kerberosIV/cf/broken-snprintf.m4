dnl $Id: broken-snprintf.m4,v 1.3 1999/03/01 09:52:22 joda Exp $
dnl
AC_DEFUN(AC_BROKEN_SNPRINTF, [
AC_CACHE_CHECK(for working snprintf,ac_cv_func_snprintf_working,
ac_cv_func_snprintf_working=yes
AC_TRY_RUN([
#include <stdio.h>
#include <string.h>
int main()
{
changequote(`,')dnl
	char foo[3];
changequote([,])dnl
	snprintf(foo, 2, "12");
	return strcmp(foo, "1");
}],:,ac_cv_func_snprintf_working=no,:))

if test "$ac_cv_func_snprintf_working" = yes; then
	AC_DEFINE_UNQUOTED(HAVE_SNPRINTF, 1, [define if you have a working snprintf])
fi
if test "$ac_cv_func_snprintf_working" = yes; then
AC_NEED_PROTO([#include <stdio.h>],snprintf)
fi
])

AC_DEFUN(AC_BROKEN_VSNPRINTF,[
AC_CACHE_CHECK(for working vsnprintf,ac_cv_func_vsnprintf_working,
ac_cv_func_vsnprintf_working=yes
AC_TRY_RUN([
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

int foo(int num, ...)
{
changequote(`,')dnl
	char bar[3];
changequote([,])dnl
	va_list arg;
	va_start(arg, num);
	vsnprintf(bar, 2, "%s", arg);
	va_end(arg);
	return strcmp(bar, "1");
}


int main()
{
	return foo(0, "12");
}],:,ac_cv_func_vsnprintf_working=no,:))

if test "$ac_cv_func_vsnprintf_working" = yes; then
	AC_DEFINE_UNQUOTED(HAVE_VSNPRINTF, 1, [define if you have a working vsnprintf])
fi
if test "$ac_cv_func_vsnprintf_working" = yes; then
AC_NEED_PROTO([#include <stdio.h>],vsnprintf)
fi
])
