dnl ######################################################################
dnl Find if struct nfs_args exists anywhere in typical headers
AC_DEFUN(AMU_STRUCT_NFS_ARGS,
[
dnl make sure this is called before [AC_TYPE_NFS_FH]
AC_BEFORE([$0], [AC_TYPE_NFS_FH])
AC_BEFORE([$0], [AC_STRUCT_FIELD_NFS_FH])
AC_CACHE_CHECK(for struct nfs_args,
ac_cv_have_struct_nfs_args,
[
# try to compile a program which may have a definition for the structure
# assume not found
ac_cv_have_struct_nfs_args=notfound

# look for "struct irix5_nfs_args" (specially set in conf/nfs_prot/)
if test "$ac_cv_have_struct_nfs_args" = notfound
then
AC_TRY_COMPILE_NFS(
[ struct irix5_nfs_args na;
], ac_cv_have_struct_nfs_args="struct irix5_nfs_args", ac_cv_have_struct_nfs_args=notfound)
fi

# look for "struct aix51_nfs_args" (specially set in conf/nfs_prot/)
if test "$ac_cv_have_struct_nfs_args" = notfound
then
AC_TRY_COMPILE_NFS(
[ struct aix51_nfs_args na;
], ac_cv_have_struct_nfs_args="struct aix51_nfs_args", ac_cv_have_struct_nfs_args=notfound)
fi

# look for "struct aix42_nfs_args" (specially set in conf/nfs_prot/)
if test "$ac_cv_have_struct_nfs_args" = notfound
then
AC_TRY_COMPILE_NFS(
[ struct aix42_nfs_args na;
], ac_cv_have_struct_nfs_args="struct aix42_nfs_args", ac_cv_have_struct_nfs_args=notfound)
fi

# look for "struct nfs_args"
if test "$ac_cv_have_struct_nfs_args" = notfound
then
AC_TRY_COMPILE_NFS(
[ struct nfs_args na;
], ac_cv_have_struct_nfs_args="struct nfs_args", ac_cv_have_struct_nfs_args=notfound)
fi

])

if test "$ac_cv_have_struct_nfs_args" != notfound
then
  AC_DEFINE(HAVE_STRUCT_NFS_ARGS)
  AC_DEFINE_UNQUOTED(nfs_args_t, $ac_cv_have_struct_nfs_args)
fi
])
dnl ======================================================================
