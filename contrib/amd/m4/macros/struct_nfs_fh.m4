dnl ######################################################################
dnl Find the structure of an nfs filehandle.
dnl if found, defined am_nfs_fh to it, else leave it undefined.
dnl THE ORDER OF LOOKUPS IN THIS FILE IS VERY IMPORTANT!!!
AC_DEFUN(AMU_STRUCT_NFS_FH,
[
AC_CACHE_CHECK(for type/structure of NFS V2 filehandle,
ac_cv_struct_nfs_fh,
[
# try to compile a program which may have a definition for the type
dnl need a series of compilations, which will test out every possible type
dnl such as struct nfs_fh, fhandle_t, nfsv2fh_t, etc.
# set to a default value
ac_cv_struct_nfs_fh=notfound

# look for "nfs_fh"
if test "$ac_cv_struct_nfs_fh" = notfound
then
AC_TRY_COMPILE_NFS(
[ nfs_fh nh;
], ac_cv_struct_nfs_fh="nfs_fh", ac_cv_struct_nfs_fh=notfound)
fi

# look for "struct nfs_fh"
if test "$ac_cv_struct_nfs_fh" = notfound
then
AC_TRY_COMPILE_NFS(
[ struct nfs_fh nh;
], ac_cv_struct_nfs_fh="struct nfs_fh", ac_cv_struct_nfs_fh=notfound)
fi

# look for "struct nfssvcfh"
if test "$ac_cv_struct_nfs_fh" = notfound
then
AC_TRY_COMPILE_NFS(
[ struct nfssvcfh nh;
], ac_cv_struct_nfs_fh="struct nfssvcfh", ac_cv_struct_nfs_fh=notfound)
fi

# look for "nfsv2fh_t"
if test "$ac_cv_struct_nfs_fh" = notfound
then
AC_TRY_COMPILE_NFS(
[ nfsv2fh_t nh;
], ac_cv_struct_nfs_fh="nfsv2fh_t", ac_cv_struct_nfs_fh=notfound)
fi

# look for "fhandle_t"
if test "$ac_cv_struct_nfs_fh" = notfound
then
AC_TRY_COMPILE_NFS(
[ fhandle_t nh;
], ac_cv_struct_nfs_fh="fhandle_t", ac_cv_struct_nfs_fh=notfound)
fi

])

if test "$ac_cv_struct_nfs_fh" != notfound
then
  AC_DEFINE_UNQUOTED(am_nfs_fh, $ac_cv_struct_nfs_fh)
fi
])
dnl ======================================================================
