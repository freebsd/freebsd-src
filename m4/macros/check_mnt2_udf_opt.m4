dnl ######################################################################
dnl Find UDF-specific mount(2) options (hex numbers)
dnl Usage: AMU_CHECK_MNT2_UDF_OPT(<fs>)
dnl Check if there is an entry for MS_<fs> or M_<fs> in sys/mntent.h or
dnl mntent.h, then define MNT2_UDF_OPT_<fs> to the hex number.
AC_DEFUN([AMU_CHECK_MNT2_UDF_OPT],
[
# what name to give to the fs
ac_fs_name=$1
# store variable name of fs
ac_upcase_fs_name=`echo $ac_fs_name | tr 'abcdefghijklmnopqrstuvwxyz' 'ABCDEFGHIJKLMNOPQRSTUVWXYZ'`
ac_safe=MNT2_UDF_OPT_$ac_upcase_fs_name
# check for cache and set it if needed
AMU_CACHE_CHECK_DYNAMIC(for UDF-specific mount(2) option $ac_fs_name,
ac_cv_mnt2_udf_opt_$ac_fs_name,
[
# undefine by default
eval "ac_cv_mnt2_udf_opt_$ac_fs_name=notfound"
value=notfound

# XXX - tests for other systems need to be added here!

# if failed, try UDFMNT_* as a hex (netbsd systems)
if test "$value" = notfound
then
AMU_EXPAND_CPP_HEX(
AMU_MOUNT_HEADERS
, UDFMNT_$ac_upcase_fs_name)
fi

# set cache variable to value
eval "ac_cv_mnt2_udf_opt_$ac_fs_name=$value"
])
# outside cache check, if ok, define macro
ac_tmp=`eval echo '$''{ac_cv_mnt2_udf_opt_'$ac_fs_name'}'`
if test "${ac_tmp}" != notfound
then
  AC_DEFINE_UNQUOTED($ac_safe, $ac_tmp)
fi
])
dnl ======================================================================

dnl ######################################################################
dnl run AMU_CHECK_MNT2_UDF_OPT on each argument given
dnl Usage: AMU_CHECK_MNT2_UDF_OPTS(arg arg arg ...)
AC_DEFUN([AMU_CHECK_MNT2_UDF_OPTS],
[
for ac_tmp_arg in $1
do
AMU_CHECK_MNT2_UDF_OPT($ac_tmp_arg)
done
])
dnl ======================================================================
