dnl ######################################################################
dnl Find the correct type for CDFS mount(2) arguments structure
AC_DEFUN(AMU_TYPE_CDFS_ARGS,
[
AC_CACHE_CHECK(for structure type of cdfs mount(2) arguments,
ac_cv_type_cdfs_args,
[
# set to a default value
ac_cv_type_cdfs_args=notfound

# look for "struct iso_args"
if test "$ac_cv_type_cdfs_args" = notfound
then
AC_TRY_COMPILE_ANYFS(
[ struct iso_args a;
], ac_cv_type_cdfs_args="struct iso_args", ac_cv_type_cdfs_args=notfound)
fi

# look for "struct iso9660_args"
if test "$ac_cv_type_cdfs_args" = notfound
then
AC_TRY_COMPILE_ANYFS(
[ struct iso9660_args a;
], ac_cv_type_cdfs_args="struct iso9660_args", ac_cv_type_cdfs_args=notfound)
fi

# look for "struct cdfs_args"
if test "$ac_cv_type_cdfs_args" = notfound
then
AC_TRY_COMPILE_ANYFS(
[ struct cdfs_args a;
], ac_cv_type_cdfs_args="struct cdfs_args", ac_cv_type_cdfs_args=notfound)
fi

# look for "struct hsfs_args"
if test "$ac_cv_type_cdfs_args" = notfound
then
AC_TRY_COMPILE_ANYFS(
[ struct hsfs_args a;
], ac_cv_type_cdfs_args="struct hsfs_args", ac_cv_type_cdfs_args=notfound)
fi

# look for "struct iso_specific" (ultrix)
if test "$ac_cv_type_cdfs_args" = notfound
then
AC_TRY_COMPILE_ANYFS(
[ struct iso_specific a;
], ac_cv_type_cdfs_args="struct iso_specific", ac_cv_type_cdfs_args=notfound)
fi

])
if test "$ac_cv_type_cdfs_args" != notfound
then
  AC_DEFINE_UNQUOTED(cdfs_args_t, $ac_cv_type_cdfs_args)
fi
])
dnl ======================================================================
