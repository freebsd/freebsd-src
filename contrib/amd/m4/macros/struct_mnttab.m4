dnl ######################################################################
dnl Find if struct mnttab exists anywhere in mount.h or mnttab.h headers
AC_DEFUN(AMU_STRUCT_MNTTAB,
[
AC_CACHE_CHECK(for struct mnttab,
ac_cv_have_struct_mnttab,
[
# try to compile a program which may have a definition for the structure
AC_TRY_COMPILE(
AMU_MOUNT_HEADERS
,
[
struct mnttab mt;
], ac_cv_have_struct_mnttab=yes, ac_cv_have_struct_mnttab=no)
])
if test "$ac_cv_have_struct_mnttab" = yes
then
  AC_DEFINE(HAVE_STRUCT_MNTTAB)
fi
])
dnl ======================================================================
