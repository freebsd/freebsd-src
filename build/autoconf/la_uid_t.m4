# la_TYPE_UID_T
# -------------
AC_DEFUN([la_TYPE_UID_T],
[AC_REQUIRE([AC_CANONICAL_HOST])dnl
AC_CACHE_CHECK(for uid_t in sys/types.h, la_cv_type_uid_t,
[AC_EGREP_HEADER(uid_t, sys/types.h,
  la_cv_type_uid_t=yes, la_cv_type_uid_t=no)])
if test $la_cv_type_uid_t = no; then
  case $host in
    *mingw*) def_uid_t=short ;;
    *) def_uid_t=int ;;
  esac
  AC_DEFINE_UNQUOTED(uid_t, [$def_uid_t],
	[Define to match typeof st_uid field of struct stat if <sys/types.h> doesn't define.])
  AC_DEFINE_UNQUOTED(gid_t, [$def_uid_t],
	[Define to match typeof st_gid field of struct stat if <sys/types.h> doesn't define.])
fi
])
AU_ALIAS([AC_TYPE_UID_T], [la_TYPE_UID_T])

