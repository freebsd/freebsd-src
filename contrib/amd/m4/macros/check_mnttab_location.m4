dnl ######################################################################
dnl check if the mount table is kept in a file or in the kernel.
AC_DEFUN(AMU_CHECK_MNTTAB_LOCATION,
[
AMU_CACHE_CHECK_DYNAMIC(where mount table is kept,
ac_cv_mnttab_location,
[
# assume location is on file
ac_cv_mnttab_location=file
AC_CHECK_FUNCS(mntctl getmntinfo getmountent,
ac_cv_mnttab_location=kernel)
# Solaris 8 Beta Refresh and up use the mntfs pseudo filesystem to store the
# mount table in kernel (cf. mnttab(4): the MS_NOMNTTAB option in
# <sys/mount.h> inhibits that a mount shows up there and thus can be used to
# check for the in-kernel mount table
if test "$ac_cv_mnt2_gen_opt_nomnttab" != notfound
then
  ac_cv_mnttab_location=kernel
fi
])
if test "$ac_cv_mnttab_location" = file
then
 AC_DEFINE(MOUNT_TABLE_ON_FILE)
fi
])
dnl ======================================================================
