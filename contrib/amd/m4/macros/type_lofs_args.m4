dnl ######################################################################
dnl Find the correct type for LOFS mount(2) arguments structure
AC_DEFUN(AMU_TYPE_LOFS_ARGS,
[
AC_CACHE_CHECK(for structure type of lofs mount(2) arguments,
ac_cv_type_lofs_args,
[
# set to a default value
ac_cv_type_lofs_args=notfound
# look for "struct lofs_args"
if test "$ac_cv_type_lofs_args" = notfound
then
AC_TRY_COMPILE_ANYFS(
[ struct lofs_args a;
], ac_cv_type_lofs_args="struct lofs_args", ac_cv_type_lofs_args=notfound)
fi
# look for "struct lo_args"
if test "$ac_cv_type_lofs_args" = notfound
then
AC_TRY_COMPILE_ANYFS(
[ struct lo_args a;
], ac_cv_type_lofs_args="struct lo_args", ac_cv_type_lofs_args=notfound)
fi
])
if test "$ac_cv_type_lofs_args" != notfound
then
  AC_DEFINE_UNQUOTED(lofs_args_t, $ac_cv_type_lofs_args)
fi
])
dnl ======================================================================
