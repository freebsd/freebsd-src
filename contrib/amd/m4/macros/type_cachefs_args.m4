dnl ######################################################################
dnl Find the correct type for CACHEFS mount(2) arguments structure
AC_DEFUN(AMU_TYPE_CACHEFS_ARGS,
[
AC_CACHE_CHECK(for structure type of cachefs mount(2) arguments,
ac_cv_type_cachefs_args,
[
# set to a default value
ac_cv_type_cachefs_args=notfound
# look for "struct cachefs_mountargs"
if test "$ac_cv_type_cachefs_args" = notfound
then
AC_TRY_COMPILE_ANYFS(
[ struct cachefs_mountargs a;
], ac_cv_type_cachefs_args="struct cachefs_mountargs", ac_cv_type_cachefs_args=notfound)
fi
])
if test "$ac_cv_type_cachefs_args" != notfound
then
  AC_DEFINE_UNQUOTED(cachefs_args_t, $ac_cv_type_cachefs_args)
fi
])
dnl ======================================================================
