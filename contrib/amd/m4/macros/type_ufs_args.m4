dnl ######################################################################
dnl Find the correct type for UFS mount(2) arguments structure
AC_DEFUN(AMU_TYPE_UFS_ARGS,
[
AC_CACHE_CHECK(for structure type of ufs mount(2) arguments,
ac_cv_type_ufs_args,
[
# set to a default value
ac_cv_type_ufs_args=notfound

# look for "struct ufs_args"
if test "$ac_cv_type_ufs_args" = notfound
then
AC_TRY_COMPILE_ANYFS(
[ struct ufs_args a;
], ac_cv_type_ufs_args="struct ufs_args", ac_cv_type_ufs_args=notfound)
fi

# look for "struct efs_args" (irix)
if test "$ac_cv_type_ufs_args" = notfound
then
AC_TRY_COMPILE_ANYFS(
[ struct efs_args a;
], ac_cv_type_ufs_args="struct efs_args", ac_cv_type_ufs_args=notfound)
fi

# look for "struct ufs_specific" (ultrix)
if test "$ac_cv_type_ufs_args" = notfound
then
AC_TRY_COMPILE_ANYFS(
[ struct ufs_specific a;
], ac_cv_type_ufs_args="struct ufs_specific", ac_cv_type_ufs_args=notfound)
fi

])
if test "$ac_cv_type_ufs_args" != notfound
then
  AC_DEFINE_UNQUOTED(ufs_args_t, $ac_cv_type_ufs_args)
fi
])
dnl ======================================================================
