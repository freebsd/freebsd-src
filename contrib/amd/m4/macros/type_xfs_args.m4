dnl ######################################################################
dnl Find the correct type for XFS mount(2) arguments structure
AC_DEFUN(AMU_TYPE_XFS_ARGS,
[
AC_CACHE_CHECK(for structure type of xfs mount(2) arguments,
ac_cv_type_xfs_args,
[
# set to a default value
ac_cv_type_xfs_args=notfound

# look for "struct xfs_args"
if test "$ac_cv_type_xfs_args" = notfound
then
AC_TRY_COMPILE_ANYFS(
[ struct xfs_args a;
], ac_cv_type_xfs_args="struct xfs_args", ac_cv_type_xfs_args=notfound)
fi

])
if test "$ac_cv_type_xfs_args" != notfound
then
  AC_DEFINE_UNQUOTED(xfs_args_t, $ac_cv_type_xfs_args)
fi
])
dnl ======================================================================
