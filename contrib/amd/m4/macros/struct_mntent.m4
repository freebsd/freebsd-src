dnl ######################################################################
dnl Find if struct mntent exists anywhere in mount.h or mntent.h headers
AC_DEFUN(AMU_STRUCT_MNTENT,
[
AC_CACHE_CHECK(for struct mntent,
ac_cv_have_struct_mntent,
[
# try to compile a program which may have a definition for the structure
AC_TRY_COMPILE(
AMU_MOUNT_HEADERS
,
[
struct mntent mt;
], ac_cv_have_struct_mntent=yes, ac_cv_have_struct_mntent=no)
])
if test "$ac_cv_have_struct_mntent" = yes
then
  AC_DEFINE(HAVE_STRUCT_MNTENT)
fi
])
dnl ======================================================================
