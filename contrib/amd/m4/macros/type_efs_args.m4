dnl ######################################################################
dnl Find the correct type for EFS mount(2) arguments structure
AC_DEFUN(AMU_TYPE_EFS_ARGS,
[
AC_CACHE_CHECK(for structure type of efs mount(2) arguments,
ac_cv_type_efs_args,
[
# set to a default value
ac_cv_type_efs_args=notfound

# look for "struct efs_args"
if test "$ac_cv_type_efs_args" = notfound
then
AC_TRY_COMPILE_ANYFS(
[ struct efs_args a;
], ac_cv_type_efs_args="struct efs_args", ac_cv_type_efs_args=notfound)
fi

])
if test "$ac_cv_type_efs_args" != notfound
then
  AC_DEFINE_UNQUOTED(efs_args_t, $ac_cv_type_efs_args)
fi
])
dnl ======================================================================
