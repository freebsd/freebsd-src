dnl ######################################################################
dnl check if a map exists (if some library function exists).
dnl Usage: AC_CHECK_MAP_FUNCS(<functions>..., <map>, [<mapsymbol>])
dnl Check if any of the functions <functions> exist.  If any exist, then
dnl define HAVE_MAP_<map>.  If <mapsymbol> exits, then defined
dnl HAVE_MAP_<mapsymbol> instead...
AC_DEFUN(AMU_CHECK_MAP_FUNCS,
[
# find what name to give to the map
if test -n "$3"
then
  ac_map_name=$3
else
  ac_map_name=$2
fi
# store variable name of map
ac_upcase_map_name=`echo $2 | tr 'abcdefghijklmnopqrstuvwxyz' 'ABCDEFGHIJKLMNOPQRSTUVWXYZ'`
ac_safe=HAVE_MAP_$ac_upcase_map_name
# check for cache and set it if needed
AMU_CACHE_CHECK_DYNAMIC(for $ac_map_name maps,
ac_cv_map_$ac_map_name,
[
# define to "no" by default
eval "ac_cv_map_$ac_map_name=no"
# and look to see if it was found
AC_CHECK_FUNCS($1,
[
  eval "ac_cv_map_$ac_map_name=yes"
  break
])])
# check if need to define variable
if test "`eval echo '$''{ac_cv_map_'$ac_map_name'}'`" = yes
then
  AC_DEFINE_UNQUOTED($ac_safe)
# append info_<map>.o object to AMD_INFO_OBJS for automatic compilation
# if first time we add something to this list, then also tell autoconf
# to replace instances of it in Makefiles.
  if test -z "$AMD_INFO_OBJS"
  then
    AMD_INFO_OBJS="info_${ac_map_name}.o"
    AC_SUBST(AMD_INFO_OBJS)
  else
    AMD_INFO_OBJS="$AMD_INFO_OBJS info_${ac_map_name}.o"
  fi
fi
])
dnl ======================================================================
