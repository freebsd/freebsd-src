dnl ######################################################################
dnl Find name of mount table file, and define it as MNTTAB_FILE_NAME
dnl
dnl Solaris defines MNTTAB as /etc/mnttab, the file where /sbin/mount
dnl stores its cache of mounted filesystems.  But under SunOS, the same
dnl macro MNTTAB, is defined as the _source_ of filesystems to mount, and
dnl is set to /etc/fstab.  That is why I have to first check out
dnl if MOUNTED exists, and if not, check for the MNTTAB macro.
dnl
AC_DEFUN(AMU_CHECK_MNTTAB_FILE_NAME,
[
AC_CACHE_CHECK(for name of mount table file name,
ac_cv_mnttab_file_name,
[
# expand cpp value for MNTTAB
AMU_EXPAND_CPP_STRING(
AMU_MOUNT_HEADERS(
[
/* see M4 comment at the top of the definition of this macro */
#ifdef MOUNTED
# define _MNTTAB_FILE_NAME MOUNTED
# else /* not MOUNTED */
# ifdef MNTTAB
#  define _MNTTAB_FILE_NAME MNTTAB
# endif /* MNTTAB */
#endif /* not MOUNTED */
]),
_MNTTAB_FILE_NAME,
[ ac_cv_mnttab_file_name=$value
],
[
ac_cv_mnttab_file_name=notfound
# check explicitly for /etc/mnttab
if test "$ac_cv_mnttab_file_name" = notfound
then
  if test -f /etc/mnttab
  then
    ac_cv_mnttab_file_name="/etc/mnttab"
  fi
fi
# check explicitly for /etc/mtab
if test "$ac_cv_mnttab_file_name" = notfound
then
  if test -f /etc/mtab
  then
    ac_cv_mnttab_file_name="/etc/mtab"
  fi
fi
])
])
# test value and create macro as needed
if test "$ac_cv_mnttab_file_name" != notfound
then
  AC_DEFINE_UNQUOTED(MNTTAB_FILE_NAME, "$ac_cv_mnttab_file_name")
fi
])
dnl ======================================================================
