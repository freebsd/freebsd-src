dnl ######################################################################
dnl check if a filesystem exists (if any of its header files exist).
dnl Usage: AC_CHECK_FS_HEADERS(<headers>..., <fs>, [<fssymbol>])
dnl Check if any of the headers <headers> exist.  If any exist, then
dnl define HAVE_FS_<fs>.  If <fssymbol> exits, then define
dnl HAVE_FS_<fssymbol> instead...
AC_DEFUN(AMU_CHECK_FS_HEADERS,
[
# find what name to give to the fs
if test -n "$3"
then
  ac_fs_name=$3
else
  ac_fs_name=$2
fi
# store variable name of fs
ac_upcase_fs_name=`echo $2 | tr 'abcdefghijklmnopqrstuvwxyz' 'ABCDEFGHIJKLMNOPQRSTUVWXYZ'`
ac_fs_headers_safe=HAVE_FS_$ac_upcase_fs_name
# check for cache and set it if needed
AMU_CACHE_CHECK_DYNAMIC(for $ac_fs_name filesystem in <$1>,
ac_cv_fs_header_$ac_fs_name,
[
# define to "no" by default
eval "ac_cv_fs_header_$ac_fs_name=no"
# and look to see if it was found
AC_CHECK_HEADERS($1,
[ eval "ac_cv_fs_header_$ac_fs_name=yes"
  break
])])
# check if need to define variable
if test "`eval echo '$''{ac_cv_fs_header_'$ac_fs_name'}'`" = yes
then
  AC_DEFINE_UNQUOTED($ac_fs_headers_safe)
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
