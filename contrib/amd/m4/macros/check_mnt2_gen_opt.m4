dnl ######################################################################
dnl Find generic mount(2) options (hex numbers)
dnl Usage: AMU_CHECK_MNT2_GEN_OPT(<fs>)
dnl Check if there is an entry for MS_<fs>, MNT_<fs>, or M_<fs>
dnl (in that order) in mntent.h, sys/mntent.h, or mount.h...
dnl then define MNT2_GEN_OPT_<fs> to the hex number.
AC_DEFUN(AMU_CHECK_MNT2_GEN_OPT,
[
# what name to give to the fs
ac_fs_name=$1
# store variable name of fs
ac_upcase_fs_name=`echo $ac_fs_name | tr 'abcdefghijklmnopqrstuvwxyz' 'ABCDEFGHIJKLMNOPQRSTUVWXYZ'`
ac_safe=MNT2_GEN_OPT_$ac_upcase_fs_name
# check for cache and set it if needed
AMU_CACHE_CHECK_DYNAMIC(for generic mount(2) option $ac_fs_name,
ac_cv_mnt2_gen_opt_$ac_fs_name,
[
# undefine by default
eval "ac_cv_mnt2_gen_opt_$ac_fs_name=notfound"
value=notfound

# first, try MS_* (most systems).  Must be the first test!
if test "$value" = notfound
then
AMU_EXPAND_CPP_HEX(
AMU_MOUNT_HEADERS
, MS_$ac_upcase_fs_name)
fi

# if failed, try MNT_* (bsd44 systems)
if test "$value" = notfound
then
AMU_EXPAND_CPP_HEX(
AMU_MOUNT_HEADERS
, MNT_$ac_upcase_fs_name)
fi

# if failed, try MS_*  as an integer (linux systems)
if test "$value" = notfound
then
AMU_EXPAND_CPP_INT(
AMU_MOUNT_HEADERS
, MS_$ac_upcase_fs_name)
fi

# If failed try M_* (must be last test since svr4 systems define M_DATA etc.
# in <sys/stream.h>
# This test was off for now, because of the conflicts with other systems.
# but I turned it back on by faking the inclusion of <sys/stream.h> already.
if test "$value" = notfound
then
AMU_EXPAND_CPP_HEX(
#ifndef _sys_stream_h
# define _sys_stream_h
#endif /* not _sys_stream_h */
#ifndef _SYS_STREAM_H
# define _SYS_STREAM_H
#endif	/* not _SYS_STREAM_H */
AMU_MOUNT_HEADERS
, M_$ac_upcase_fs_name)
fi

# set cache variable to value
eval "ac_cv_mnt2_gen_opt_$ac_fs_name=$value"
])
# outside cache check, if ok, define macro
ac_tmp=`eval echo '$''{ac_cv_mnt2_gen_opt_'$ac_fs_name'}'`
if test "${ac_tmp}" != notfound
then
  AC_DEFINE_UNQUOTED($ac_safe, $ac_tmp)
fi
])
dnl ======================================================================

dnl ######################################################################
dnl run AMU_CHECK_MNT2_GEN_OPT on each argument given
dnl Usage: AMU_CHECK_MNT2_GEN_OPTS(arg arg arg ...)
AC_DEFUN(AMU_CHECK_MNT2_GEN_OPTS,
[
for ac_tmp_arg in $1
do
AMU_CHECK_MNT2_GEN_OPT($ac_tmp_arg)
done
])
dnl ======================================================================
