dnl ######################################################################
dnl check if an automounter filesystem exists (it almost always does).
dnl Usage: AC_CHECK_AMU_FS(<fs>, <msg>, [<depfs>])
dnl Print the message in <msg>, and declare HAVE_AMU_FS_<fs> true.
dnl If <depfs> is defined, then define this filesystem as tru only of the
dnl filesystem for <depfs> is true.
AC_DEFUN(AMU_CHECK_AMU_FS,
[
# store variable name of fs
ac_upcase_am_fs_name=`echo $1 | tr 'abcdefghijklmnopqrstuvwxyz' 'ABCDEFGHIJKLMNOPQRSTUVWXYZ'`
ac_safe=HAVE_AMU_FS_$ac_upcase_am_fs_name
# check for cache and set it if needed
AMU_CACHE_CHECK_DYNAMIC(for $2 filesystem ($1),
ac_cv_am_fs_$1,
[
# true by default
eval "ac_cv_am_fs_$1=yes"
# if <depfs> exists but is defined to "no", set this filesystem to no.
if test -n "$3"
then
  # flse by default if arg 3 was supplied
  eval "ac_cv_am_fs_$1=no"
  if test "`eval echo '$''{ac_cv_fs_'$3'}'`" = yes
  then
    eval "ac_cv_am_fs_$1=yes"
  fi
  # some filesystems do not have a mnttab entry, but exist based on headers
  if test "`eval echo '$''{ac_cv_fs_header_'$3'}'`" = yes
  then
    eval "ac_cv_am_fs_$1=yes"
  fi
fi
])
# check if need to define variable
if test "`eval echo '$''{ac_cv_am_fs_'$1'}'`" = yes
then
  AC_DEFINE_UNQUOTED($ac_safe)
fi
])
dnl ======================================================================
