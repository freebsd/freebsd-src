/*
 * am_compat.h:
 *
 * This file contains compatibility functions and macros, all of which
 * should be auto-discovered, but for one reason or another (mostly
 * brain-damage on the part of system designers and header files) they cannot.
 *
 * Each compatibility macro/function must include instructions on how/when
 * it can be removed the am-utils code.
 *
 */

#ifndef _AM_COMPAT_H
# define _AM_COMPAT_H

/*
 * incomplete mount options definitions (sunos4, irix6, linux, etc.)
 */


/*
 * Complete MNTTAB_OPT_* options based on MNT2_NFS_OPT_* mount options.
 */
#if defined(MNT2_NFS_OPT_ACDIRMAX) && !defined(MNTTAB_OPT_ACDIRMAX)
# define MNTTAB_OPT_ACDIRMAX "acdirmax"
#endif /* defined(MNT2_NFS_OPT_ACDIRMAX) && !defined(MNTTAB_OPT_ACDIRMAX) */

#if defined(MNT2_NFS_OPT_ACDIRMIN) && !defined(MNTTAB_OPT_ACDIRMIN)
# define MNTTAB_OPT_ACDIRMIN "acdirmin"
#endif /* defined(MNT2_NFS_OPT_ACDIRMIN) && !defined(MNTTAB_OPT_ACDIRMIN) */

#if defined(MNT2_NFS_OPT_ACREGMAX) && !defined(MNTTAB_OPT_ACREGMAX)
# define MNTTAB_OPT_ACREGMAX "acregmax"
#endif /* defined(MNT2_NFS_OPT_ACREGMAX) && !defined(MNTTAB_OPT_ACREGMAX) */

#if defined(MNT2_NFS_OPT_ACREGMIN) && !defined(MNTTAB_OPT_ACREGMIN)
# define MNTTAB_OPT_ACREGMIN "acregmin"
#endif /* defined(MNT2_NFS_OPT_ACREGMIN) && !defined(MNTTAB_OPT_ACREGMIN) */

#if !defined(MNTTAB_OPT_IGNORE)
/* SunOS 4.1.x and others define "noauto" option, but not "auto" */
# if defined(MNTTAB_OPT_NOAUTO) && !defined(MNTTAB_OPT_AUTO)
#  define MNTTAB_OPT_AUTO "auto"
# endif /* defined(MNTTAB_OPT_NOAUTO) && !defined(MNTTAB_OPT_AUTO) */
#endif /* !defined(MNTTAB_OPT_IGNORE) */

#if defined(MNT2_NFS_OPT_NOAC) && !defined(MNTTAB_OPT_NOAC)
# define MNTTAB_OPT_NOAC "noac"
#endif /* defined(MNT2_NFS_OPT_NOAC) && !defined(MNTTAB_OPT_NOAC) */

#if defined(MNT2_NFS_OPT_NOCONN) && !defined(MNTTAB_OPT_NOCONN)
# define MNTTAB_OPT_NOCONN "noconn"
# ifndef MNTTAB_OPT_CONN
#  define MNTTAB_OPT_CONN "conn"
# endif /* MNTTAB_OPT_CONN */
#endif /* defined(MNT2_NFS_OPT_NOCONN) && !defined(MNTTAB_OPT_NOCONN) */

#if defined(MNT2_NFS_OPT_PGTHRESH) && !defined(MNTTAB_OPT_PGTHRESH)
# define MNTTAB_OPT_PGTHRESH "pgthresh"
#endif /* defined(MNT2_NFS_OPT_PGTHRESH) && !defined(MNTTAB_OPT_PGTHRESH) */

#if defined(MNT2_NFS_OPT_RETRANS) && !defined(MNTTAB_OPT_RETRANS)
# define MNTTAB_OPT_RETRANS "retrans"
#endif /* defined(MNT2_NFS_OPT_RETRANS) && !defined(MNTTAB_OPT_RETRANS) */

#if defined(MNT2_NFS_OPT_RSIZE) && !defined(MNTTAB_OPT_RSIZE)
# define MNTTAB_OPT_RSIZE "rsize"
#endif /* defined(MNT2_NFS_OPT_RSIZE) && !defined(MNTTAB_OPT_RSIZE) */

#if defined(MNT2_NFS_OPT_SOFT) && !defined(MNTTAB_OPT_SOFT)
# define MNTTAB_OPT_SOFT "soft"
# ifndef MNTTAB_OPT_HARD
#  define MNTTAB_OPT_HARD "hard"
# endif /* not MNTTAB_OPT_HARD */
#endif /* defined(MNT2_NFS_OPT_SOFT) && !defined(MNTTAB_OPT_SOFT) */

#if defined(MNT2_NFS_OPT_TIMEO) && !defined(MNTTAB_OPT_TIMEO)
# define MNTTAB_OPT_TIMEO "timeo"
#endif /* defined(MNT2_NFS_OPT_TIMEO) && !defined(MNTTAB_OPT_TIMEO) */

#if defined(MNT2_NFS_OPT_WSIZE) && !defined(MNTTAB_OPT_WSIZE)
# define MNTTAB_OPT_WSIZE "wsize"
#endif /* defined(MNT2_NFS_OPT_WSIZE) && !defined(MNTTAB_OPT_WSIZE) */

#if defined(MNT2_NFS_OPT_MAXGRPS) && !defined(MNTTAB_OPT_MAXGROUPS)
# define MNTTAB_OPT_MAXGROUPS "maxgroups"
#endif /* defined(MNT2_NFS_OPT_MAXGRPS) && !defined(MNTTAB_OPT_MAXGROUPS) */

/*
 * Complete MNTTAB_OPT_* options based on MNT2_CDFS_OPT_* mount options.
 */
#if defined(MNT2_CDFS_OPT_DEFPERM) && !defined(MNTTAB_OPT_DEFPERM)
# define MNTTAB_OPT_DEFPERM "defperm"
#endif /* defined(MNT2_CDFS_OPT_DEFPERM) && !defined(MNTTAB_OPT_DEFPERM) */

#if defined(MNT2_CDFS_OPT_NODEFPERM) && !defined(MNTTAB_OPT_NODEFPERM)
# define MNTTAB_OPT_NODEFPERM "nodefperm"
/*
 * DEC OSF/1 V3.x/Digital UNIX V4.0 have M_NODEFPERM only, but
 * both mnttab ops.
 */
# ifndef MNTTAB_OPT_DEFPERM
#  define MNTTAB_OPT_DEFPERM "defperm"
# endif /* not MNTTAB_OPT_DEFPERM */
#endif /* defined(MNT2_CDFS_OPT_NODEFPERM) && !defined(MNTTAB_OPT_NODEFPERM) */

#if defined(MNT2_CDFS_OPT_NOVERSION) && !defined(MNTTAB_OPT_NOVERSION)
# define MNTTAB_OPT_NOVERSION "noversion"
#endif /* defined(MNT2_CDFS_OPT_NOVERSION) && !defined(MNTTAB_OPT_NOVERSION) */

#if defined(MNT2_CDFS_OPT_RRIP) && !defined(MNTTAB_OPT_RRIP)
# define MNTTAB_OPT_RRIP "rrip"
#endif /* defined(MNT2_CDFS_OPT_RRIP) && !defined(MNTTAB_OPT_RRIP) */
#if defined(MNT2_CDFS_OPT_NORRIP) && !defined(MNTTAB_OPT_NORRIP)
# define MNTTAB_OPT_NORRIP "norrip"
#endif /* defined(MNT2_CDFS_OPT_NORRIP) && !defined(MNTTAB_OPT_NORRIP) */

#if defined(MNT2_CDFS_OPT_GENS) && !defined(MNTTAB_OPT_GENS)
# define MNTTAB_OPT_GENS "gens"
#endif /* defined(MNT2_CDFS_OPT_GENS) && !defined(MNTTAB_OPT_GENS) */
#if defined(MNT2_CDFS_OPT_EXTATT) && !defined(MNTTAB_OPT_EXTATT)
# define MNTTAB_OPT_EXTATT "extatt"
#endif /* defined(MNT2_CDFS_OPT_EXTATT) && !defined(MNTTAB_OPT_EXTATT) */

/*
 * Complete MNTTAB_OPT_* options based on MNT2_GEN_OPT_* mount options.
 */
#if defined(MNT2_GEN_OPT_GRPID) && !defined(MNTTAB_OPT_GRPID)
# define MNTTAB_OPT_GRPID "grpid"
#endif /* defined(MNT2_GEN_OPT_GRPID) && !defined(MNTTAB_OPT_GRPID) */

#if defined(MNT2_GEN_OPT_NOCACHE) && !defined(MNTTAB_OPT_NOCACHE)
# define MNTTAB_OPT_NOCACHE "nocache"
#endif /* defined(MNT2_GEN_OPT_NOCACHE) && !defined(MNTTAB_OPT_NOCACHE) */

#if defined(MNT2_GEN_OPT_NOSUID) && !defined(MNTTAB_OPT_NOSUID)
# define MNTTAB_OPT_NOSUID "nosuid"
#endif /* defined(MNT2_GEN_OPT_NOSUID) && !defined(MNTTAB_OPT_NOSUID) */

#if defined(MNT2_GEN_OPT_OVERLAY) && !defined(MNTTAB_OPT_OVERLAY)
# define MNTTAB_OPT_OVERLAY "overlay"
#endif /* defined(MNT2_GEN_OPT_OVERLAY) && !defined(MNTTAB_OPT_OVERLAY) */

/*
 * Complete MNTTAB_OPT_* options and their inverse based on MNT2_GEN_OPT_*
 * options.
 */
#if defined(MNT2_GEN_OPT_NODEV) && !defined(MNTTAB_OPT_NODEV)
# define MNTTAB_OPT_NODEV "nodev"
/* this is missing under some versions of Linux */
# ifndef MNTTAB_OPT_DEV
#  define MNTTAB_OPT_DEV "dev"
# endif /* not MNTTAB_OPT_DEV */
#endif /* defined(MNT2_GEN_OPT_NODEV) && !defined(MNTTAB_OPT_NODEV) */

#if defined(MNT2_GEN_OPT_NOEXEC) && !defined(MNTTAB_OPT_NOEXEC)
# define MNTTAB_OPT_NOEXEC "noexec"
/* this is missing under some versions of Linux */
# ifndef MNTTAB_OPT_EXEC
#  define MNTTAB_OPT_EXEC "exec"
# endif /* not MNTTAB_OPT_EXEC */
#endif /* defined(MNT2_GEN_OPT_NOEXEC) && !defined(MNTTAB_OPT_NOEXEC) */

#if defined(MNT2_GEN_OPT_QUOTA) && !defined(MNTTAB_OPT_QUOTA)
# define MNTTAB_OPT_QUOTA "quota"
#endif /* defined(MNT2_GEN_OPT_QUOTA) && !defined(MNTTAB_OPT_QUOTA) */

#if defined(MNT2_GEN_OPT_SYNC) && !defined(MNTTAB_OPT_SYNC)
# define MNTTAB_OPT_SYNC "sync"
#endif /* defined(MNT2_GEN_OPT_SYNC) && !defined(MNTTAB_OPT_SYNC) */


/*
 * Add missing MNTTAB_OPT_* options.
 */
#ifndef MNTTAB_OPT_ACTIMEO
# define MNTTAB_OPT_ACTIMEO "actimeo"
#endif /* not MNTTAB_OPT_ACTIMEO */

#ifndef MNTTAB_OPT_INTR
# define MNTTAB_OPT_INTR "intr"
#endif /* not MNTTAB_OPT_INTR */

#ifndef MNTTAB_OPT_PORT
# define MNTTAB_OPT_PORT "port"
#endif /* not MNTTAB_OPT_PORT */

#ifndef MNTTAB_OPT_RETRANS
# define MNTTAB_OPT_RETRANS "retrans"
#endif /* not MNTTAB_OPT_RETRANS */

#ifndef MNTTAB_OPT_RETRY
# define MNTTAB_OPT_RETRY "retry"
#endif /* not MNTTAB_OPT_RETRY */

#ifndef MNTTAB_OPT_RO
# define MNTTAB_OPT_RO "ro"
#endif /* not MNTTAB_OPT_RO */

#ifndef MNTTAB_OPT_RSIZE
# define MNTTAB_OPT_RSIZE "rsize"
#endif /* not MNTTAB_OPT_RSIZE */

#ifndef MNTTAB_OPT_RW
# define MNTTAB_OPT_RW "rw"
#endif /* not MNTTAB_OPT_RW */

#ifndef MNTTAB_OPT_TIMEO
# define MNTTAB_OPT_TIMEO "timeo"
#endif /* not MNTTAB_OPT_TIMEO */

#ifndef MNTTAB_OPT_WSIZE
# define MNTTAB_OPT_WSIZE "wsize"
#endif /* not MNTTAB_OPT_WSIZE */


/*
 * Incomplete filesystem definitions (sunos4, irix6, solaris2)
 */
#if defined(HAVE_FS_CDFS) && defined(MOUNT_TYPE_CDFS) && !defined(MNTTYPE_CDFS)
# define MNTTYPE_CDFS "hsfs"
#endif /* defined(HAVE_FS_CDFS) && defined(MOUNT_TYPE_CDFS) && !defined(MNTTYPE_CDFS) */

#ifndef cdfs_args_t
/*
 * Solaris has an HSFS filesystem, but does not define hsfs_args.
 * XXX: the definition here for solaris is wrong, since under solaris,
 * hsfs_args should be a single integer used as a bit-field for options.
 * so this code has to be fixed later.  -Erez.
 */
struct hsfs_args {
        char *fspec;    /* name of filesystem to mount */
        int norrip;
};
# define cdfs_args_t struct hsfs_args
# define HAVE_FIELD_CDFS_ARGS_T_NORRIP
#endif /* not cdfs_args_t */

/*
 * if does not define struct pc_args, assume integer bit-field (irix6)
 */
#if defined(HAVE_FS_PCFS) && !defined(pcfs_args_t)
# define pcfs_args_t u_int
#endif /* defined(HAVE_FS_PCFS) && !defined(pcfs_args_t) */

/*
 * if does not define struct ufs_args, assume integer bit-field (linux)
 */
#if defined(HAVE_FS_UFS) && !defined(ufs_args_t)
# define ufs_args_t u_int
#endif /* defined(HAVE_FS_UFS) && !defined(ufs_args_t) */

#if defined(HAVE_FS_AUTOFS) && defined(MOUNT_TYPE_AUTOFS) && !defined(MNTTYPE_AUTOFS)
# define MNTTYPE_AUTOFS "autofs"
#endif /* defined(HAVE_FS_AUTOFS) && defined(MOUNT_TYPE_AUTOFS) && !defined(MNTTYPE_AUTOFS) */

/*
 * If NFS3, then make sure that "proto" and "vers" mnttab options
 * are available.
 */
#ifdef HAVE_FS_NFS3
# ifndef MNTTAB_OPT_VERS
#  define MNTTAB_OPT_VERS "vers"
# endif /* not MNTTAB_OPT_VERS */
# ifndef MNTTAB_OPT_PROTO
#  define MNTTAB_OPT_PROTO "proto"
# endif /* not MNTTAB_OPT_PROTO */
#endif /* not HAVE_FS_NFS3 */

#endif /* not _AM_COMPAT_H */
