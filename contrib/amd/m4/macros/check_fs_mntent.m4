dnl ######################################################################
dnl check if a filesystem type exists (if its header files exist)
dnl Usage: AC_CHECK_FS_MNTENT(<filesystem>, [<fssymbol>])
dnl
dnl Check in some headers for MNTTYPE_<filesystem> macro.  If that exist,
dnl then define HAVE_FS_<filesystem>.  If <fssymbol> exits, then define
dnl HAVE_FS_<fssymbol> instead...
AC_DEFUN(AMU_CHECK_FS_MNTENT,
[
# find what name to give to the fs
if test -n "$2"
then
  ac_fs_name=$2
  ac_fs_as_name=" (from: $1)"
else
  ac_fs_name=$1
  ac_fs_as_name=""
fi
# store variable name of filesystem
ac_upcase_fs_name=`echo $ac_fs_name | tr 'abcdefghijklmnopqrstuvwxyz' 'ABCDEFGHIJKLMNOPQRSTUVWXYZ'`
ac_safe=HAVE_FS_$ac_upcase_fs_name
# check for cache and set it if needed
AMU_CACHE_CHECK_DYNAMIC(for $ac_fs_name$ac_fs_as_name mntent definition,
ac_cv_fs_$ac_fs_name,
[
# assume not found
eval "ac_cv_fs_$ac_fs_name=no"
for ac_fs_tmp in $1
do
  ac_upcase_fs_symbol=`echo $ac_fs_tmp | tr 'abcdefghijklmnopqrstuvwxyz' 'ABCDEFGHIJKLMNOPQRSTUVWXYZ'`

  # first look for MNTTYPE_*
  AC_EGREP_CPP(yes,
  AMU_MOUNT_HEADERS(
  [
#ifdef MNTTYPE_$ac_upcase_fs_symbol
    yes
#endif /* MNTTYPE_$ac_upcase_fs_symbol */
  ]), [eval "ac_cv_fs_$ac_fs_name=yes"], [eval "ac_cv_fs_$ac_fs_name=no"] )
  # check if need to terminate "for" loop
  if test "`eval echo '$''{ac_cv_fs_'$ac_fs_name'}'`" != no
  then
    break
  fi

  # now try to look for MOUNT_ macro
  AC_EGREP_CPP(yes,
  AMU_MOUNT_HEADERS(
  [
#ifdef MOUNT_$ac_upcase_fs_symbol
    yes
#endif /* MOUNT_$ac_upcase_fs_symbol */
  ]), [eval "ac_cv_fs_$ac_fs_name=yes"], [eval "ac_cv_fs_$ac_fs_name=no"] )
  # check if need to terminate "for" loop
  if test "`eval echo '$''{ac_cv_fs_'$ac_fs_name'}'`" != no
  then
    break
  fi

  # now try to look for MNT_ macro
  AC_EGREP_CPP(yes,
  AMU_MOUNT_HEADERS(
  [
#ifdef MNT_$ac_upcase_fs_symbol
    yes
#endif /* MNT_$ac_upcase_fs_symbol */
  ]), [eval "ac_cv_fs_$ac_fs_name=yes"], [eval "ac_cv_fs_$ac_fs_name=no"] )
  # check if need to terminate "for" loop
  if test "`eval echo '$''{ac_cv_fs_'$ac_fs_name'}'`" != no
  then
    break
  fi

  # now try to look for GT_ macro (ultrix)
  AC_EGREP_CPP(yes,
  AMU_MOUNT_HEADERS(
  [
#ifdef GT_$ac_upcase_fs_symbol
    yes
#endif /* GT_$ac_upcase_fs_symbol */
  ]), [eval "ac_cv_fs_$ac_fs_name=yes"], [eval "ac_cv_fs_$ac_fs_name=no"] )
  # check if need to terminate "for" loop
  if test "`eval echo '$''{ac_cv_fs_'$ac_fs_name'}'`" != no
  then
    break
  fi

  # look for a loadable filesystem module (linux)
  if test -f /lib/modules/$host_os_version/fs/$ac_fs_tmp.o
  then
    eval "ac_cv_fs_$ac_fs_name=yes"
    break
  fi

  # look for a loadable filesystem module (linux 2.4+)
  if test -f /lib/modules/$host_os_version/kernel/fs/$ac_fs_tmp/$ac_fs_tmp.o
  then
    eval "ac_cv_fs_$ac_fs_name=yes"
    break
  fi

  # look for a loadable filesystem module (linux redhat-5.1)
  if test -f /lib/modules/preferred/fs/$ac_fs_tmp.o
  then
    eval "ac_cv_fs_$ac_fs_name=yes"
    break
  fi

  # in addition look for statically compiled filesystem (linux)
  if egrep "[[^a-zA-Z0-9_]]$ac_fs_tmp$" /proc/filesystems >/dev/null 2>&1
  then
    eval "ac_cv_fs_$ac_fs_name=yes"
    break
  fi

  if test "$ac_fs_tmp" = "nfs3" -a "$ac_cv_header_linux_nfs_mount_h" = "yes"
  then
    # hack hack hack
    # in 6.1, which has fallback to v2/udp, we might want
    # to always use version 4.
    # in 6.0 we do not have much choice
    #
    let nfs_mount_version="`grep NFS_MOUNT_VERSION /usr/include/linux/nfs_mount.h | awk '{print $''3;}'`"
    if test $nfs_mount_version -ge 4
    then
      eval "ac_cv_fs_$ac_fs_name=yes"
      break
    fi
  fi

  # run a test program for bsdi3
  AC_TRY_RUN(
  [
#include <sys/param.h>
#include <sys/mount.h>
main()
{
  int i;
  struct vfsconf vf;
  i = getvfsbyname("$ac_fs_tmp", &vf);
  if (i < 0)
    exit(1);
  else
    exit(0);
}
  ], [eval "ac_cv_fs_$ac_fs_name=yes"
      break
     ]
  )

done
])
# check if need to define variable
if test "`eval echo '$''{ac_cv_fs_'$ac_fs_name'}'`" = yes
then
  AC_DEFINE_UNQUOTED($ac_safe)
# append ops_<fs>.o object to AMD_FS_OBJS for automatic compilation
# if first time we add something to this list, then also tell autoconf
# to replace instances of it in Makefiles.
  if test -z "$AMD_FS_OBJS"
  then
    AMD_FS_OBJS="ops_${ac_fs_name}.o"
    AC_SUBST(AMD_FS_OBJS)
  else
    # since this object file could have already been added before
    # we need to ensure we do not add it twice.
    case "${AMD_FS_OBJS}" in
      *ops_${ac_fs_name}.o* ) ;;
      * )
        AMD_FS_OBJS="$AMD_FS_OBJS ops_${ac_fs_name}.o"
      ;;
    esac
  fi
fi
])
dnl ======================================================================
