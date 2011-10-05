
AC_DEFUN([rk_LIBDISPATCH],[

AC_CHECK_HEADERS([dispatch/dispatch.h])

AC_FIND_FUNC_NO_LIBS(dispatch_async_f, dispatch,
[#ifdef HAVE_DISPATCH_DISPATCH_H
#include <dispatch/dispatch.h>
#endif],[0,0,0])

if test "$ac_cv_func_dispatch_async_f" = yes ; then
    AC_DEFINE([HAVE_GCD], 1, [Define if os support gcd.])
    libdispatch=yes
else
    libdispatch=no
fi

AM_CONDITIONAL(have_gcd, test "$libdispatch" = yes)

])