dnl ######################################################################
dnl Find the correct type for UDF mount(2) arguments structure
AC_DEFUN([AMU_TYPE_UDF_ARGS],
[
AC_CACHE_CHECK(for structure type of udf mount(2) arguments,
ac_cv_type_udf_args,
[
# set to a default value
ac_cv_type_udf_args=notfound

# look for "struct udf_args"
if test "$ac_cv_type_udf_args" = notfound
then
AC_TRY_COMPILE_ANYFS(
[ struct udf_args a;
], ac_cv_type_udf_args="struct udf_args", ac_cv_type_udf_args=notfound)
fi

])
if test "$ac_cv_type_udf_args" != notfound
then
  AC_DEFINE_UNQUOTED(udf_args_t, $ac_cv_type_udf_args)
fi
])
dnl ======================================================================
