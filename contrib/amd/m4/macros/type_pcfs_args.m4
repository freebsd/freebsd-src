dnl ######################################################################
dnl Find the correct type for PC/FS mount(2) arguments structure
AC_DEFUN(AMU_TYPE_PCFS_ARGS,
[
AC_CACHE_CHECK(for structure type of pcfs mount(2) arguments,
ac_cv_type_pcfs_args,
[
# set to a default value
ac_cv_type_pcfs_args=notfound

# look for "struct msdos_args"
if test "$ac_cv_type_pcfs_args" = notfound
then
AC_TRY_COMPILE_ANYFS(
[ struct msdos_args a;
], ac_cv_type_pcfs_args="struct msdos_args", ac_cv_type_pcfs_args=notfound)
fi

# look for "struct pc_args"
if test "$ac_cv_type_pcfs_args" = notfound
then
AC_TRY_COMPILE_ANYFS(
[ struct pc_args a;
], ac_cv_type_pcfs_args="struct pc_args", ac_cv_type_pcfs_args=notfound)
fi

# look for "struct pcfs_args"
if test "$ac_cv_type_pcfs_args" = notfound
then
AC_TRY_COMPILE_ANYFS(
[ struct pcfs_args a;
], ac_cv_type_pcfs_args="struct pcfs_args", ac_cv_type_pcfs_args=notfound)
fi

# look for "struct msdosfs_args"
if test "$ac_cv_type_pcfs_args" = notfound
then
AC_TRY_COMPILE_ANYFS(
[ struct msdosfs_args a;
], ac_cv_type_pcfs_args="struct msdosfs_args", ac_cv_type_pcfs_args=notfound)
fi

])

if test "$ac_cv_type_pcfs_args" != notfound
then
  AC_DEFINE_UNQUOTED(pcfs_args_t, $ac_cv_type_pcfs_args)
fi
])
dnl ======================================================================
