dnl ######################################################################
dnl find if "extern char *sys_errlist[]" exist in headers
AC_DEFUN(AMU_EXTERN_SYS_ERRLIST,
[
AC_CACHE_CHECK(if external definition for sys_errlist[] exists,
ac_cv_extern_sys_errlist,
[
# try to locate pattern in header files
#pattern="(extern)?.*char.*sys_errlist.*\[\]"
pattern="(extern)?.*char.*sys_errlist.*"
AC_EGREP_CPP(${pattern},
[
#ifdef HAVE_STDIO_H
# include <stdio.h>
#endif /* HAVE_STDIO_H */
#ifdef HAVE_SYS_ERRNO_H
# include <sys/errno.h>
#endif /* HAVE_SYS_ERRNO_H */
#ifdef HAVE_ERRNO_H
# include <errno.h>
#endif /* HAVE_ERRNO_H */
], ac_cv_extern_sys_errlist=yes, ac_cv_extern_sys_errlist=no)
])
# check if need to define variable
if test "$ac_cv_extern_sys_errlist" = yes
then
  AC_DEFINE(HAVE_EXTERN_SYS_ERRLIST)
fi
])
dnl ======================================================================
