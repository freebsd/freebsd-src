dnl ######################################################################
dnl Find NFS-specific mount(2) options (hex numbers)
dnl Usage: AMU_CHECK_MNT2_NFS_OPT(<fs>)
dnl Check if there is an entry for NFSMNT_<fs> in sys/mntent.h or
dnl mntent.h, then define MNT2_NFS_OPT_<fs> to the hex number.
AC_DEFUN(AMU_CHECK_MNT2_NFS_OPT,
[
# what name to give to the fs
ac_fs_name=$1
# store variable name of fs
ac_upcase_fs_name=`echo $ac_fs_name | tr 'abcdefghijklmnopqrstuvwxyz' 'ABCDEFGHIJKLMNOPQRSTUVWXYZ'`
ac_safe=MNT2_NFS_OPT_$ac_upcase_fs_name
# check for cache and set it if needed
AMU_CACHE_CHECK_DYNAMIC(for NFS-specific mount(2) option $ac_fs_name,
ac_cv_mnt2_nfs_opt_$ac_fs_name,
[
# undefine by default
eval "ac_cv_mnt2_nfs_opt_$ac_fs_name=notfound"
value=notfound

# first try NFSMNT_* (most systems)
if test "$value" = notfound
then
AMU_EXPAND_CPP_HEX(
AMU_MOUNT_HEADERS
, NFSMNT_$ac_upcase_fs_name)
fi

# next try NFS_MOUNT_* (linux)
if test "$value" = notfound
then
AMU_EXPAND_CPP_HEX(
AMU_MOUNT_HEADERS
, NFS_MOUNT_$ac_upcase_fs_name)
fi

# set cache variable to value
eval "ac_cv_mnt2_nfs_opt_$ac_fs_name=$value"
])
# outside cache check, if ok, define macro
ac_tmp=`eval echo '$''{ac_cv_mnt2_nfs_opt_'$ac_fs_name'}'`
if test "${ac_tmp}" != notfound
then
  AC_DEFINE_UNQUOTED($ac_safe, $ac_tmp)
fi
])
dnl ======================================================================

dnl ######################################################################
dnl run AMU_CHECK_MNT2_NFS_OPT on each argument given
dnl Usage: AMU_CHECK_MNT2_NFS_OPTS(arg arg arg ...)
AC_DEFUN(AMU_CHECK_MNT2_NFS_OPTS,
[
for ac_tmp_arg in $1
do
AMU_CHECK_MNT2_NFS_OPT($ac_tmp_arg)
done
])
dnl ======================================================================
