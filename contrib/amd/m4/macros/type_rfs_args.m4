dnl ######################################################################
dnl Find the correct type for RFS mount(2) arguments structure
AC_DEFUN(AMU_TYPE_RFS_ARGS,
[
AC_CACHE_CHECK(for structure type of rfs mount(2) arguments,
ac_cv_type_rfs_args,
[
# set to a default value
ac_cv_type_rfs_args=notfound
# look for "struct rfs_args"
if test "$ac_cv_type_rfs_args" = notfound
then
AC_TRY_COMPILE_ANYFS(
[ struct rfs_args a;
], ac_cv_type_rfs_args="struct rfs_args", ac_cv_type_rfs_args=notfound)
fi
])
if test "$ac_cv_type_rfs_args" != notfound
then
  AC_DEFINE_UNQUOTED(rfs_args_t, $ac_cv_type_rfs_args)
fi
])
dnl ======================================================================
