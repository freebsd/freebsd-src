dnl ######################################################################
dnl Find the correct type for MFS mount(2) arguments structure
AC_DEFUN(AMU_TYPE_MFS_ARGS,
[
AC_CACHE_CHECK(for structure type of mfs mount(2) arguments,
ac_cv_type_mfs_args,
[
# set to a default value
ac_cv_type_mfs_args=notfound
# look for "struct mfs_args"
if test "$ac_cv_type_mfs_args" = notfound
then
AC_TRY_COMPILE_ANYFS(
[ struct mfs_args a;
], ac_cv_type_mfs_args="struct mfs_args", ac_cv_type_mfs_args=notfound)
fi
])
if test "$ac_cv_type_mfs_args" != notfound
then
  AC_DEFINE_UNQUOTED(mfs_args_t, $ac_cv_type_mfs_args)
fi
])
dnl ======================================================================
