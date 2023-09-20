dnl from autoconf 2.13 acspecific.m4, with changes to check for daylight

AC_DEFUN([AC_STRUCT_TIMEZONE_DAYLIGHT],
[AC_REQUIRE([AC_STRUCT_TM])dnl
AC_CACHE_CHECK([for tm_zone in struct tm], ac_cv_struct_tm_zone,
[AC_COMPILE_IFELSE(
[AC_LANG_PROGRAM([#include <sys/types.h>
#include <$ac_cv_struct_tm>], [struct tm tm; tm.tm_zone;])],
  ac_cv_struct_tm_zone=yes, ac_cv_struct_tm_zone=no)])
if test "$ac_cv_struct_tm_zone" = yes; then
  AC_DEFINE(HAVE_TM_ZONE,1,[HAVE_TM_ZONE])
fi

# On SGI, apparently tzname is a #define, but that's ok, AC_CHECK_DECL will
# consider it declared and we won't give our own extern.
AC_CHECK_DECLS([tzname], , , [#include <time.h>])
AC_CACHE_CHECK(for tzname, ac_cv_var_tzname,
[AC_LINK_IFELSE(
[AC_LANG_PROGRAM([#include <time.h>],
[[#if !HAVE_DECL_TZNAME
extern char *tzname[];
#endif
return tzname[0][0];]])], [ac_cv_var_tzname=yes], [ac_cv_var_tzname=no])])
  if test $ac_cv_var_tzname = yes; then
    AC_DEFINE(HAVE_TZNAME,1,[HAVE_TZNAME])
  fi

AC_CACHE_CHECK([for tm_isdst in struct tm], ac_cv_struct_tm_isdst,
[AC_COMPILE_IFELSE(
[AC_LANG_PROGRAM([#include <sys/types.h>
#include <$ac_cv_struct_tm>], [struct tm tm; tm.tm_isdst;])],
  ac_cv_struct_tm_isdst=yes, ac_cv_struct_tm_isdst=no)])
if test "$ac_cv_struct_tm_isdst" = yes; then
  AC_DEFINE(HAVE_TM_ISDST,1,[HAVE_TM_ISDST])
fi


AC_CHECK_DECLS([daylight], , , [#include <time.h>
#include <stdlib.h>])
AC_CACHE_CHECK(for daylight, ac_cv_var_daylight,
[AC_LINK_IFELSE(
[AC_LANG_PROGRAM([#include <time.h>
#include <stdlib.h>],
[#if !HAVE_DECL_DAYLIGHT
extern int daylight;
#endif
daylight = atoi("1");])], ac_cv_var_daylight=yes, ac_cv_var_daylight=no)])
  if test $ac_cv_var_daylight = yes; then
    AC_DEFINE(HAVE_DAYLIGHT,1,[HAVE_DAYLIGHT])
  fi
])

AC_DEFUN([AC_STRUCT_OPTION_GETOPT_H],
[AC_CACHE_CHECK([for struct option in getopt], ac_cv_struct_option_getopt_h,
[AC_COMPILE_IFELSE(
[AC_LANG_PROGRAM([#include <getopt.h>], [struct option op; op.name;])],
  ac_cv_struct_option_getopt_h=yes, ac_cv_struct_option_getopt_h=no)])
if test "$ac_cv_struct_option_getopt_h" = yes; then
  AC_DEFINE(HAVE_STRUCT_OPTION,1,[HAVE_STRUCT_OPTION])
fi
])
