dnl ######################################################################
dnl Find the name of the nfs filehandle field in nfs_args_t.
AC_DEFUN(AMU_STRUCT_FIELD_NFS_FH,
[
dnl make sure this is called before macros which depend on it
AC_BEFORE([$0], [AC_TYPE_NFS_FH])
AC_CACHE_CHECK(for the name of the nfs filehandle field in nfs_args_t,
ac_cv_struct_field_nfs_fh,
[
# set to a default value
ac_cv_struct_field_nfs_fh=notfound
# look for name "fh" (most systems)
if test "$ac_cv_struct_field_nfs_fh" = notfound
then
AC_TRY_COMPILE_NFS(
[ nfs_args_t nat;
  char *cp = (char *) &(nat.fh);
], ac_cv_struct_field_nfs_fh=fh, ac_cv_struct_field_nfs_fh=notfound)
fi

# look for name "root" (for example Linux)
if test "$ac_cv_struct_field_nfs_fh" = notfound
then
AC_TRY_COMPILE_NFS(
[ nfs_args_t nat;
  char *cp = (char *) &(nat.root);
], ac_cv_struct_field_nfs_fh=root, ac_cv_struct_field_nfs_fh=notfound)
fi
])
if test "$ac_cv_struct_field_nfs_fh" != notfound
then
  AC_DEFINE_UNQUOTED(NFS_FH_FIELD, $ac_cv_struct_field_nfs_fh)
fi
])
dnl ======================================================================
