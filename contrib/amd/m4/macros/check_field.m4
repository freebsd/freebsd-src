dnl ######################################################################
dnl FIXED VERSION OF AUTOCONF 2.50 AC_CHECK_MEMBER.  g/cc will fail to check
dnl a member if the .member is itself a data structure, because you cannot
dnl compare, in C, a data structure against NULL; you can compare a native
dnl data type (int, char) or a pointer.  Solution: do what I did in my
dnl original member checking macro: try to take the address of the member.
dnl You can always take the address of anything.
dnl -Erez Zadok, Feb 6, 2002.
dnl
# AC_CHECK_MEMBER2(AGGREGATE.MEMBER,
#                 [ACTION-IF-FOUND], [ACTION-IF-NOT-FOUND],
#                 [INCLUDES])
# ---------------------------------------------------------
# AGGREGATE.MEMBER is for instance `struct passwd.pw_gecos', shell
# variables are not a valid argument.
AC_DEFUN([AC_CHECK_MEMBER2],
[AS_LITERAL_IF([$1], [],
               [AC_FATAL([$0: requires literal arguments])])dnl
m4_if(m4_regexp([$1], [\.]), -1,
      [AC_FATAL([$0: Did not see any dot in `$1'])])dnl
AS_VAR_PUSHDEF([ac_Member], [ac_cv_member_$1])dnl
dnl Extract the aggregate name, and the member name
AC_CACHE_CHECK([for $1], ac_Member,
[AC_COMPILE_IFELSE([AC_LANG_PROGRAM([AC_INCLUDES_DEFAULT([$4])],
[dnl AGGREGATE ac_aggr;
static m4_patsubst([$1], [\..*]) ac_aggr;
dnl ac_aggr.MEMBER;
if (&(ac_aggr.m4_patsubst([$1], [^[^.]*\.])))
return 0;])],
                [AS_VAR_SET(ac_Member, yes)],
                [AS_VAR_SET(ac_Member, no)])])
AS_IF([test AS_VAR_GET(ac_Member) = yes], [$2], [$3])dnl
AS_VAR_POPDEF([ac_Member])dnl
])# AC_CHECK_MEMBER

# AC_CHECK_MEMBERS2([AGGREGATE.MEMBER, ...],
#                  [ACTION-IF-FOUND], [ACTION-IF-NOT-FOUND]
#                  [INCLUDES])
# ---------------------------------------------------------
# The first argument is an m4 list.
AC_DEFUN([AC_CHECK_MEMBERS2],
[m4_foreach([AC_Member], [$1],
  [AC_CHECK_MEMBER2(AC_Member,
         [AC_DEFINE_UNQUOTED(AS_TR_CPP(HAVE_[]AC_Member), 1,
                            [Define if `]m4_patsubst(AC_Member,
                                                     [^[^.]*\.])[' is
                             member of `]m4_patsubst(AC_Member, [\..*])['.])
$2],
                 [$3],
                 [$4])])])


dnl ######################################################################
dnl find if structure $1 has field field $2
AC_DEFUN(AMU_CHECK_FIELD,
[
AC_CHECK_MEMBERS2($1, , ,[
AMU_MOUNT_HEADERS(
[
/* now set the typedef */
#ifdef HAVE_STRUCT_MNTENT
typedef struct mntent mntent_t;
#else /* not HAVE_STRUCT_MNTENT */
# ifdef HAVE_STRUCT_MNTTAB
typedef struct mnttab mntent_t;
# endif /*  HAVE_STRUCT_MNTTAB */
#endif /* not HAVE_STRUCT_MNTENT */

/*
 * for various filesystem specific mount arguments
 */

#ifdef HAVE_SYS_FS_PC_FS_H
# include <sys/fs/pc_fs.h>
#endif /* HAVE_SYS_FS_PC_FS_H */
#ifdef HAVE_MSDOSFS_MSDOSFSMOUNT_H
# include <msdosfs/msdosfsmount.h>
#endif /* HAVE_MSDOSFS_MSDOSFSMOUNT_H */

#ifdef HAVE_SYS_FS_EFS_CLNT_H
# include <sys/fs/efs_clnt.h>
#endif /* HAVE_SYS_FS_EFS_CLNT_H */
#ifdef HAVE_SYS_FS_XFS_CLNT_H
# include <sys/fs/xfs_clnt.h>
#endif /* HAVE_SYS_FS_XFS_CLNT_H */
#ifdef HAVE_SYS_FS_UFS_MOUNT_H
# include <sys/fs/ufs_mount.h>
#endif /* HAVE_SYS_FS_UFS_MOUNT_H */
#ifdef HAVE_SYS_FS_AUTOFS_H
# include <sys/fs/autofs.h>
#endif /* HAVE_SYS_FS_AUTOFS_H */
#ifdef HAVE_RPCSVC_AUTOFS_PROT_H
# include <rpcsvc/autofs_prot.h>
#else  /* not HAVE_RPCSVC_AUTOFS_PROT_H */
# ifdef HAVE_SYS_FS_AUTOFS_PROT_H
#  include <sys/fs/autofs_prot.h>
# endif /* HAVE_SYS_FS_AUTOFS_PROT_H */
#endif /* not HAVE_RPCSVC_AUTOFS_PROT_H */
#ifdef HAVE_HSFS_HSFS_H
# include <hsfs/hsfs.h>
#endif /* HAVE_HSFS_HSFS_H */

#ifdef HAVE_IFADDRS_H
# include <ifaddrs.h>
#endif /* HAVE_IFADDRS_H */

])
])
])
dnl ======================================================================
