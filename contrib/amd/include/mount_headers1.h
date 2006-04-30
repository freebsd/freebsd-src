#ifdef HAVE_SYS_TYPES_H
# include <sys/types.h>
#endif /* HAVE_SYS_TYPES_H */
#ifdef HAVE_SYS_ERRNO_H
# include <sys/errno.h>
#endif /* HAVE_SYS_ERRNO_H */
#ifdef HAVE_SYS_PARAM_H
# include <sys/param.h>
#endif /* HAVE_SYS_PARAM_H */
#ifdef HAVE_SYS_TIME_H
# include <sys/time.h>
#endif /* HAVE_SYS_TIME_H */
#ifdef HAVE_SYS_UCRED_H
# include <sys/ucred.h>
#endif /* HAVE_SYS_UCRED_H */
#ifdef HAVE_SYS_SOCKET_H
# include <sys/socket.h>
#endif /* HAVE_SYS_SOCKET_H */
#ifdef HAVE_NETINET_IN_H
# include <netinet/in.h>
#endif /* HAVE_NETINET_IN_H */
#ifdef HAVE_NET_IF_H
# include <net/if.h>
#endif /* HAVE_NET_IF_H */
#ifdef HAVE_STDIO_H
# include <stdio.h>
#endif /* HAVE_STDIO_H */

#ifndef KERNEL
# define KERNEL_off_for_now_breaks_FreeBSD
#endif /* not KERNEL */

#ifdef HAVE_SYS_MNTENT_H
# include <sys/mntent.h>
#endif /* HAVE_SYS_MNTENT_H */
#ifdef HAVE_MNTENT_H
# include <mntent.h>
#endif /* HAVE_MNTENT_H */
#ifdef HAVE_SYS_MNTTAB_H
# include <sys/mnttab.h>
#endif /* HAVE_SYS_MNTTAB_H */
#if defined(HAVE_MNTTAB_H) && !defined(MNTTAB)
/*
 * Do not include it if MNTTAB is already defined because it probably
 * came from <sys/mnttab.h> and we do not want conflicting definitions.
 */
# include <mnttab.h>
#endif /* defined(HAVE_MNTTAB_H) && !defined(MNTTAB) */

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

#ifdef HAVE_SYS_FS_TYPES_H
# include <sys/fs_types.h>
#endif /* HAVE_SYS_FS_TYPES_H */

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

#ifdef HAVE_CDFS_CDFS_MOUNT_H
# include <cdfs/cdfs_mount.h>
#endif /* HAVE_CDFS_CDFS_MOUNT_H */
#ifdef HAVE_CDFS_CDFSMOUNT_H
# include <cdfs/cdfsmount.h>
#endif /* HAVE_CDFS_CDFSMOUNT_H */
#ifdef HAVE_ISOFS_CD9660_CD9660_MOUNT_H
# include <isofs/cd9660/cd9660_mount.h>
#endif /* HAVE_ISOFS_CD9660_CD9660_MOUNT_H */

#ifdef HAVE_RPC_RPC_H
# include <rpc/rpc.h>
#endif /* HAVE_RPC_RPC_H */
#ifdef HAVE_RPC_TYPES_H
# include <rpc/types.h>
#endif /* HAVE_RPC_TYPES_H */
/* Prevent multiple inclusion on Ultrix 4 */
#if defined(HAVE_RPC_XDR_H) && !defined(__XDR_HEADER__)
# include <rpc/xdr.h>
#endif /* defined(HAVE_RPC_XDR_H) && !defined(__XDR_HEADER__) */

/* ALWAYS INCLUDE AM-UTILS' SPECIFIC NFS PROTOCOL HEADER NEXT! */
