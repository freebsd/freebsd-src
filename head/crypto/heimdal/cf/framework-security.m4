AC_DEFUN([rk_FRAMEWORK_SECURITY], [

AC_MSG_CHECKING([for framework security])
AC_CACHE_VAL(rk_cv_framework_security,
[
if test "$rk_cv_framework_security" != yes; then
	ac_save_LIBS="$LIBS"
	LIBS="$ac_save_LIBS -framework Security -framework CoreFoundation"
	AC_LINK_IFELSE([AC_LANG_PROGRAM([[#include <Security/Security.h>
]],
[[SecKeychainSearchRef searchRef;
SecKeychainSearchCreateFromAttributes(NULL,kSecCertificateItemClass,NULL, &searchRef);
CFRelease(&searchRef);
]])],[rk_cv_framework_security=yes])
	LIBS="$ac_save_LIBS"
fi
])

if test "$rk_cv_framework_security" = yes; then
   AC_DEFINE(HAVE_FRAMEWORK_SECURITY, 1, [Have -framework Security])
   AC_MSG_RESULT(yes)
else
   AC_MSG_RESULT(no)
fi
AM_CONDITIONAL(FRAMEWORK_SECURITY, test "$rk_cv_framework_security" = yes)

if test "$rk_cv_framework_security" = yes; then
   AC_NEED_PROTO([#include <Security/Security.h>],SecKeyGetCSPHandle)
fi

])
