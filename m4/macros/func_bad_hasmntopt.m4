dnl ######################################################################
dnl Check if we have as buggy hasmntopt() libc function
AC_DEFUN([AMU_FUNC_BAD_HASMNTOPT],
[
AC_CACHE_CHECK([for working hasmntopt], ac_cv_func_hasmntopt_working,
[AC_TRY_RUN(
AMU_MOUNT_HEADERS(
[[
#ifdef HAVE_MNTENT_H
/* some systems need <stdio.h> before <mntent.h> is included */
# ifdef HAVE_STDIO_H
#  include <stdio.h>
# endif /* HAVE_STDIO_H */
# include <mntent.h>
#endif /* HAVE_MNTENT_H */
#ifdef HAVE_SYS_MNTENT_H
# include <sys/mntent.h>
#endif /* HAVE_SYS_MNTENT_H */
#ifdef HAVE_SYS_MNTTAB_H
# include <sys/mnttab.h>
#endif /* HAVE_SYS_MNTTAB_H */
#if defined(HAVE_MNTTAB_H) && !defined(MNTTAB)
# include <mnttab.h>
#endif /* defined(HAVE_MNTTAB_H) && !defined(MNTTAB) */
#ifdef HAVE_STRUCT_MNTENT
typedef struct mntent mntent_t;
#else /* not HAVE_STRUCT_MNTENT */
# ifdef HAVE_STRUCT_MNTTAB
typedef struct mnttab mntent_t;
/* map struct mnttab field names to struct mntent field names */
#  define mnt_opts	mnt_mntopts
# endif /* not HAVE_STRUCT_MNTTAB */
#endif /* not HAVE_STRUCT_MNTENT */

int main()
{
  mntent_t mnt;
  char *tmp = NULL;

 /*
  * Test if hasmntopt will incorrectly find the string "soft", which
  * is part of the large "softlookup" function.
  */
  mnt.mnt_opts = "hard,softlookup,ro";

  if ((tmp = hasmntopt(&mnt, "soft")))
    exit(1);
  exit(0);
}
]]),
	[ac_cv_func_hasmntopt_working=yes],
	[ac_cv_func_hasmntopt_working=no]
)])
if test $ac_cv_func_hasmntopt_working = no
then
	AC_LIBOBJ([hasmntopt])
 	AC_DEFINE(HAVE_BAD_HASMNTOPT)
fi
])
