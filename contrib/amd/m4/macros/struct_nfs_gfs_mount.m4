dnl ######################################################################
dnl Find if struct nfs_gfs_mount exists anywhere in typical headers
AC_DEFUN(AMU_STRUCT_NFS_GFS_MOUNT,
[
dnl make sure this is called before [AC_TYPE_NFS_FH]
AC_BEFORE([$0], [AC_TYPE_NFS_FH])
AC_BEFORE([$0], [AC_STRUCT_FIELD_NFS_FH])
AC_CACHE_CHECK(for struct nfs_gfs_mount,
ac_cv_have_struct_nfs_gfs_mount,
[
# try to compile a program which may have a definition for the structure
AC_TRY_COMPILE_NFS(
[ struct nfs_gfs_mount ngm;
], ac_cv_have_struct_nfs_gfs_mount=yes, ac_cv_have_struct_nfs_gfs_mount=no)
])
if test "$ac_cv_have_struct_nfs_gfs_mount" = yes
then
  AC_DEFINE(HAVE_STRUCT_NFS_GFS_MOUNT)
  AC_DEFINE(nfs_args_t, struct nfs_gfs_mount)
fi
])
dnl ======================================================================
