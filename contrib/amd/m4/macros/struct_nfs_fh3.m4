dnl ######################################################################
dnl Find the structure of an NFS V3 filehandle.
dnl if found, defined am_nfs_fh3 to it, else leave it undefined.
AC_DEFUN(AMU_STRUCT_NFS_FH3,
[
AC_CACHE_CHECK(for type/structure of NFS V3 filehandle,
ac_cv_struct_nfs_fh3,
[
# try to compile a program which may have a definition for the type
dnl need a series of compilations, which will test out every possible type
dnl such as struct nfs_fh3, fhandle3_t, nfsv3fh_t, etc.
# set to a default value
ac_cv_struct_nfs_fh3=notfound

# look for "nfs_fh3_freebsd3"
if test "$ac_cv_struct_nfs_fh3" = notfound
then
AC_TRY_COMPILE_NFS(
[ nfs_fh3_freebsd3 nh;
], ac_cv_struct_nfs_fh3="nfs_fh3_freebsd3", ac_cv_struct_nfs_fh3=notfound)
fi

# look for "nfs_fh3"
if test "$ac_cv_struct_nfs_fh3" = notfound
then
AC_TRY_COMPILE_NFS(
[ nfs_fh3 nh;
], ac_cv_struct_nfs_fh3="nfs_fh3", ac_cv_struct_nfs_fh3=notfound)
fi

# look for "struct nfs_fh3"
if test "$ac_cv_struct_nfs_fh3" = notfound
then
AC_TRY_COMPILE_NFS(
[ struct nfs_fh3 nh;
], ac_cv_struct_nfs_fh3="struct nfs_fh3", ac_cv_struct_nfs_fh3=notfound)
fi

# look for "nfsv3fh_t"
if test "$ac_cv_struct_nfs_fh3" = notfound
then
AC_TRY_COMPILE_NFS(
[ nfsv3fh_t nh;
], ac_cv_struct_nfs_fh3="nfsv3fh_t", ac_cv_struct_nfs_fh3=notfound)
fi

# look for "fhandle3_t"
if test "$ac_cv_struct_nfs_fh3" = notfound
then
AC_TRY_COMPILE_NFS(
[ fhandle3_t nh;
], ac_cv_struct_nfs_fh3="fhandle3_t", ac_cv_struct_nfs_fh3=notfound)
fi

])

if test "$ac_cv_struct_nfs_fh3" != notfound
then
  AC_DEFINE_UNQUOTED(am_nfs_fh3, $ac_cv_struct_nfs_fh3)
fi
])
dnl ======================================================================
