fdnl ######################################################################
dnl find if mntent_t field mnt_time exists and is of type "char *"
AC_DEFUN(AMU_FIELD_MNTENT_T_MNT_TIME_STRING,
[
AC_CACHE_CHECK(if mntent_t field mnt_time exist as type string,
ac_cv_field_mntent_t_mnt_time_string,
[
# try to compile a program
AC_TRY_COMPILE(
AMU_MOUNT_HEADERS(
[
/* now set the typedef */
#ifdef HAVE_STRUCT_MNTENT
typedef struct mntent mntent_t;
#else /* not HAVE_STRUCT_MNTENT */
# ifdef HAVE_STRUCT_MNTTAB
typedef struct mnttab mntent_t;
# else /* not HAVE_STRUCT_MNTTAB */
#  error XXX: could not find definition for struct mntent or struct mnttab!
# endif /* not HAVE_STRUCT_MNTTAB */
#endif /* not HAVE_STRUCT_MNTENT */
]),
[
mntent_t mtt;
char *cp = "test";
int i;
mtt.mnt_time = cp;
i = mtt.mnt_time[0];
], ac_cv_field_mntent_t_mnt_time_string=yes, ac_cv_field_mntent_t_mnt_time_string=no)
])
if test "$ac_cv_field_mntent_t_mnt_time_string" = yes
then
  AC_DEFINE(HAVE_MNTENT_T_MNT_TIME_STRING)
fi
])
dnl ======================================================================
