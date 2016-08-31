dnl ######################################################################
dnl Find the correct type for AUTOFS mount(2) arguments structure
AC_DEFUN([AMU_TYPE_AUTOFS_ARGS],
[
AC_CACHE_CHECK(for structure type of autofs mount(2) arguments,
ac_cv_type_autofs_args,
[
# set to a default value
ac_cv_type_autofs_args=notfound

# look for "struct auto_args"
if test "$ac_cv_type_autofs_args" = notfound
then
AC_TRY_COMPILE_ANYFS(
[ struct auto_args a;
], ac_cv_type_autofs_args="struct auto_args", ac_cv_type_autofs_args=notfound)
fi

# look for "struct autofs_args"
if test "$ac_cv_type_autofs_args" = notfound
then
AC_TRY_COMPILE_ANYFS(
[ struct autofs_args a;
], ac_cv_type_autofs_args="struct autofs_args", ac_cv_type_autofs_args=notfound)
fi

])
if test "$ac_cv_type_autofs_args" != notfound
then
  AC_DEFINE_UNQUOTED(autofs_args_t, $ac_cv_type_autofs_args)
fi
])
dnl ======================================================================
