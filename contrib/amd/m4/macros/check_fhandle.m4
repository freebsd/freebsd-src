dnl ######################################################################
dnl Find if type 'fhandle' exists
AC_DEFUN(AMU_CHECK_FHANDLE,
[
AC_CACHE_CHECK(if plain fhandle type exists,
ac_cv_have_fhandle,
[
# try to compile a program which may have a definition for the type
# set to a default value
ac_cv_have_fhandle=no
# look for "struct nfs_fh"
if test "$ac_cv_have_fhandle" = no
then
AC_TRY_COMPILE_NFS(
[ fhandle a;
], ac_cv_have_fhandle=yes, ac_cv_have_fhandle=no)
fi

])
if test "$ac_cv_have_fhandle" != no
then
  AC_DEFINE(HAVE_FHANDLE)
fi
])
dnl ======================================================================
