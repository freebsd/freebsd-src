dnl ######################################################################
dnl Find the correct type for TMPFS mount(2) arguments structure
AC_DEFUN(AMU_TYPE_TMPFS_ARGS,
[
AC_CACHE_CHECK(for structure type of tmpfs mount(2) arguments,
ac_cv_type_tmpfs_args,
[
# set to a default value
ac_cv_type_tmpfs_args=notfound
# look for "struct tmpfs_args"
if test "$ac_cv_type_tmpfs_args" = notfound
then
AC_TRY_COMPILE_ANYFS(
[ struct tmpfs_args a;
], ac_cv_type_tmpfs_args="struct tmpfs_args", ac_cv_type_tmpfs_args=notfound)
fi
])
if test "$ac_cv_type_tmpfs_args" != notfound
then
  AC_DEFINE_UNQUOTED(tmpfs_args_t, $ac_cv_type_tmpfs_args)
fi
])
dnl ======================================================================
