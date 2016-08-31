dnl ######################################################################
dnl Compile a program with <any>FS headers to try and find a feature.
dnl The headers part are fixed.  Only three arguments are allowed:
dnl [$1] is the program to compile (2nd arg to AC_TRY_COMPILE)
dnl [$2] action to take if the program compiled (3rd arg to AC_TRY_COMPILE)
dnl [$3] action to take if program did not compile (4rd arg to AC_TRY_COMPILE)
AC_DEFUN([AC_TRY_COMPILE_ANYFS],
[# try to compile a program which may have a definition for a structure
AC_TRY_COMPILE(
[
#ifdef HAVE_SYS_TYPES_H
# include <sys/types.h>
#endif /* HAVE_SYS_TYPES_H */
#ifdef HAVE_SYS_ERRNO_H
# include <sys/errno.h>
#endif /* HAVE_SYS_ERRNO_H */
#ifdef HAVE_SYS_PARAM_H
# include <sys/param.h>
#endif /* HAVE_SYS_PARAM_H */

#if TIME_WITH_SYS_TIME
# include <sys/time.h>
# include <time.h>
#else /* not TIME_WITH_SYS_TIME */
# if HAVE_SYS_TIME_H
#  include <sys/time.h>
# else /* not HAVE_SYS_TIME_H */
#  include <time.h>
# endif /* not HAVE_SYS_TIME_H */
#endif /* not TIME_WITH_SYS_TIME */

#ifdef HAVE_NETINET_IN_H
# include <netinet/in.h>
#endif /* HAVE_NETINET_IN_H */
#ifdef HAVE_SYS_TIUSER_H
# include <sys/tiuser.h>
#endif /* HAVE_SYS_TIUSER_H */

#ifdef HAVE_SYS_MOUNT_H
# ifndef NFSCLIENT
#  define NFSCLIENT
# endif /* not NFSCLIENT */
# ifndef PCFS
#  define PCFS
# endif /* not PCFS */
# ifndef LOFS
#  define LOFS
# endif /* not LOFS */
# ifndef RFS
#  define RFS
# endif /* not RFS */
# ifndef MSDOSFS
#  define MSDOSFS
# endif /* not MSDOSFS */
# ifndef MFS
#  define MFS 1
# endif /* not MFS */
# ifndef CD9660
#  define CD9660
# endif /* not CD9660 */
# ifndef NFS
#  define NFS
# endif /* not NFS */
# include <sys/mount.h>
#endif /* HAVE_SYS_MOUNT_H */

#ifdef HAVE_SYS_VMOUNT_H
# include <sys/vmount.h>
#endif /* HAVE_SYS_VMOUNT_H */

/*
 * There is no point in including this on a glibc2 system
 * we're only asking for trouble
 */
#if defined HAVE_LINUX_FS_H && (!defined __GLIBC__ || __GLIBC__ < 2)
/*
 * There's a conflict of definitions on redhat alpha linux between
 * <netinet/in.h> and <linux/fs.h>.
 * Also a conflict in definitions of ntohl/htonl in RH-5.1 sparc64
 * between <netinet/in.h> and <linux/byteorder/generic.h> (2.1 kernels).
 */
# ifdef HAVE_SOCKETBITS_H
#  define _LINUX_SOCKET_H
#  undef BLKFLSBUF
#  undef BLKGETSIZE
#  undef BLKRAGET
#  undef BLKRASET
#  undef BLKROGET
#  undef BLKROSET
#  undef BLKRRPART
#  undef MS_MGC_VAL
#  undef MS_RMT_MASK
# endif /* HAVE_SOCKETBITS_H */
# ifdef HAVE_LINUX_POSIX_TYPES_H
#  include <linux/posix_types.h>
# endif /* HAVE_LINUX_POSIX_TYPES_H */
# ifndef _LINUX_BYTEORDER_GENERIC_H
#  define _LINUX_BYTEORDER_GENERIC_H
# endif /* _LINUX_BYTEORDER_GENERIC_H */
# ifndef _LINUX_STRING_H_
#  define _LINUX_STRING_H_
# endif /* not _LINUX_STRING_H_ */
# ifdef HAVE_LINUX_KDEV_T_H
#  define __KERNEL__
#  include <linux/kdev_t.h>
#  undef __KERNEL__
# endif /* HAVE_LINUX_KDEV_T_H */
# ifdef HAVE_LINUX_LIST_H
#  define __KERNEL__
#  include <linux/list.h>
#  undef __KERNEL__
# endif /* HAVE_LINUX_LIST_H */
# include <linux/fs.h>
#endif /* HAVE_LINUX_FS_H && (!__GLIBC__ || __GLIBC__ < 2) */

#ifdef HAVE_SYS_FS_AUTOFS_H
# include <sys/fs/autofs.h>
#endif /* HAVE_SYS_FS_AUTOFS_H */
#ifdef HAVE_SYS_FS_CACHEFS_FS_H
# include <sys/fs/cachefs_fs.h>
#endif /* HAVE_SYS_FS_CACHEFS_FS_H */

#ifdef HAVE_SYS_FS_PC_FS_H
# include <sys/fs/pc_fs.h>
#endif /* HAVE_SYS_FS_PC_FS_H */
#ifdef HAVE_MSDOSFS_MSDOSFSMOUNT_H
# include <msdosfs/msdosfsmount.h>
#endif /* HAVE_MSDOSFS_MSDOSFSMOUNT_H */
#ifdef HAVE_FS_MSDOSFS_MSDOSFSMOUNT_H
# include <fs/msdosfs/msdosfsmount.h>
#endif /* HAVE_FS_MSDOSFS_MSDOSFSMOUNT_H */

#ifdef HAVE_SYS_FS_TMP_H
# include <sys/fs/tmp.h>
#endif /* HAVE_SYS_FS_TMP_H */
#ifdef HAVE_FS_TMPFS_TMPFS_ARGS_H
# include <fs/tmpfs/tmpfs_args.h>
#endif /* HAVE_FS_TMPFS_TMPFS_ARGS_H */

#ifdef HAVE_UFS_UFS_MOUNT_H
# include <ufs/ufs_mount.h>
#endif /* HAVE_UFS_UFS_MOUNT_H */
#ifdef HAVE_UFS_UFS_UFSMOUNT_H
# ifndef MAXQUOTAS
#  define MAXQUOTAS     2
# endif /* not MAXQUOTAS */
struct netexport { int this_is_SO_wrong; }; /* for bsdi-2.1 */
/* netbsd-1.4 does't protect <ufs/ufs/ufsmount.h> */
# ifndef _UFS_UFS_UFSMOUNT_H
#  include <ufs/ufs/ufsmount.h>
#  define _UFS_UFS_UFSMOUNT_H
# endif /* not _UFS_UFS_UFSMOUNT_H */
#endif /* HAVE_UFS_UFS_UFSMOUNT_H */
#ifdef HAVE_SYS_FS_UFS_MOUNT_H
# include <sys/fs/ufs_mount.h>
#endif /* HAVE_SYS_FS_UFS_MOUNT_H */
#ifdef HAVE_SYS_FS_EFS_CLNT_H
# include <sys/fs/efs_clnt.h>
#endif /* HAVE_SYS_FS_EFS_CLNT_H */
#ifdef HAVE_FS_EFS_EFS_MOUNT_H
# include <fs/efs/efs_mount.h>
#endif /* HAVE_FS_EFS_EFS_MOUNT_H */
#ifdef HAVE_SYS_FS_XFS_CLNT_H
# include <sys/fs/xfs_clnt.h>
#endif /* HAVE_SYS_FS_XFS_CLNT_H */

#ifdef HAVE_CDFS_CDFS_MOUNT_H
# include <cdfs/cdfs_mount.h>
#endif /* HAVE_CDFS_CDFS_MOUNT_H */
#ifdef HAVE_HSFS_HSFS_H
# include <hsfs/hsfs.h>
#endif /* HAVE_HSFS_HSFS_H */
#ifdef HAVE_CDFS_CDFSMOUNT_H
# include <cdfs/cdfsmount.h>
#endif /* HAVE_CDFS_CDFSMOUNT_H */
#ifdef HAVE_ISOFS_CD9660_CD9660_MOUNT_H
# include <isofs/cd9660/cd9660_mount.h>
#endif /* HAVE_ISOFS_CD9660_CD9660_MOUNT_H */

#ifdef HAVE_FS_UDF_UDF_MOUNT_H
# include <fs/udf/udf_mount.h>
#endif /* HAVE_FS_UDF_UDF_MOUNT_H */
], [$1], [$2], [$3])
])
dnl ======================================================================
