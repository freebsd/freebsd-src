dnl ######################################################################
dnl find if "extern char *optarg" exists in headers
AC_DEFUN(AMU_EXTERN_OPTARG,
[
AC_CACHE_CHECK(if external definition for optarg[] exists,
ac_cv_extern_optarg,
[
# try to compile program that uses the variable
AC_TRY_COMPILE(
[
#ifdef HAVE_STDIO_H
# include <stdio.h>
#endif /* HAVE_STDIO_H */
#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif /* HAVE_UNISTD_H */
#ifdef HAVE_STDLIB_H
# include <stdlib.h>
#endif /* HAVE_STDLIB_H */
#ifdef HAVE_SYS_ERRNO_H
# include <sys/errno.h>
#endif /* HAVE_SYS_ERRNO_H */
#ifdef HAVE_ERRNO_H
# include <errno.h>
#endif /* HAVE_ERRNO_H */
],
[
char *cp = optarg;
], ac_cv_extern_optarg=yes, ac_cv_extern_optarg=no)
])
if test "$ac_cv_extern_optarg" = yes
then
  AC_DEFINE(HAVE_EXTERN_OPTARG)
fi
])
dnl ======================================================================
