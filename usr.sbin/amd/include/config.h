/* 
 * $FreeBSD$
 *
 * portions derived from 
 *	$NetBSD: config.h,v 1.11 1998/08/08 22:33:37 christos Exp $
 *
 */

/* config.h.  Generated automatically by configure.  */
/* aux/config.h.in.  Generated automatically from ./aux/configure.in by autoheader.  */
/*
 * Start of am-utils-6.x config.h file.
 * Erez Zadok <ezk@cs.columbia.edu>
 *
 * DO NOT EDIT BY HAND.
 * Note: acconfig.h generates config.h.in, which generates config.h.
 */

#ifndef _CONFIG_H
#define _CONFIG_H


/*
 * Check for types of amd filesystems available.
 */

/* Define if have automount filesystem */
#define HAVE_AM_FS_AUTO 1

/* Define if have direct automount filesystem */
#define HAVE_AM_FS_DIRECT 1

/* Define if have "top-level" filesystem */
#define HAVE_AM_FS_TOPLVL 1

/* Define if have error filesystem */
#define HAVE_AM_FS_ERROR 1

/* Define if have inheritance filesystem */
#define HAVE_AM_FS_INHERIT 1

/* Define if have program filesystem */
#define HAVE_AM_FS_PROGRAM 1

/* Define if have symbolic-link filesystem */
#define HAVE_AM_FS_LINK 1

/* Define if have symlink with existence check filesystem */
#define HAVE_AM_FS_LINKX 1

/* Define if have NFS host-tree filesystem */
#define HAVE_AM_FS_HOST 1

/* Define if have nfsl (NFS with local link check) filesystem */
#define HAVE_AM_FS_NFSL 1

/* Define if have multi-NFS filesystem */
#define HAVE_AM_FS_NFSX 1

/* Define if have union filesystem */
#define HAVE_AM_FS_UNION 1


/*
 * Check for types of maps available.
 */

/* Define if have file maps (everyone should have it!) */
#define HAVE_MAP_FILE 1

/* Define if have NIS maps */
#define HAVE_MAP_NIS 1

/* Define if have NIS+ maps */
/* #undef HAVE_MAP_NISPLUS */

/* Define if have DBM maps */
/* #undef HAVE_MAP_DBM */

/* Define if have NDBM maps */
#define HAVE_MAP_NDBM 1

/* Define if have HESIOD maps */
/* #undef HAVE_MAP_HESIOD */

/* Define if have LDAP maps */
/* #undef HAVE_MAP_LDAP */

/* Define if have PASSWD maps */
#define HAVE_MAP_PASSWD 1

/* Define if have UNION maps */
#define HAVE_MAP_UNION 1

/*
 * Check for filesystem types available.
 */

/* Define if have UFS filesystem */
#define HAVE_FS_UFS 1

/* Define if have XFS filesystem (irix) */
/* #undef HAVE_FS_XFS */

/* Define if have EFS filesystem (irix) */
/* #undef HAVE_FS_EFS */

/* Define if have NFS filesystem */
#define HAVE_FS_NFS 1

/* Define if have NFS3 filesystem */
#define HAVE_FS_NFS3 1

/* Define if have PCFS filesystem */
#define HAVE_FS_PCFS 1

/* Define if have LOFS filesystem */
#define HAVE_FS_LOFS 1

/* Define if have HSFS filesystem */
/* #undef HAVE_FS_HSFS */

/* Define if have CDFS filesystem */
#define HAVE_FS_CDFS 1

/* Define if have TFS filesystem */
/* #undef HAVE_FS_TFS */

/* Define if have TMPFS filesystem */
/* #undef HAVE_FS_TMPFS */

/* Define if have MFS filesystem */
#define HAVE_FS_MFS 1

/* Define if have CFS (crypto) filesystem */
/* #undef HAVE_FS_CFS */

/* Define if have AUTOFS filesystem */
/* #undef HAVE_FS_AUTOFS */

/* Define if have CACHEFS filesystem */
/* #undef HAVE_FS_CACHEFS */

/* Define if have NULLFS (loopback on bsd44) filesystem */
#define HAVE_FS_NULLFS 1

/* Define if have UNIONFS filesystem */
#define HAVE_FS_UNIONFS 1

/* Define if have UMAPFS (uid/gid mapping) filesystem */
#define HAVE_FS_UMAPFS 1


/*
 * Check for the type of the mount(2) system name for a filesystem.
 * Normally this is "nfs" (e.g. Solaris) or an integer (older systems)
 */

/* Mount(2) type/name for UFS filesystem */
#define MOUNT_TYPE_UFS "ufs"

/* Mount(2) type/name for XFS filesystem (irix) */
/* #undef MOUNT_TYPE_XFS */

/* Mount(2) type/name for EFS filesystem (irix) */
/* #undef MOUNT_TYPE_EFS */

/* Mount(2) type/name for NFS filesystem */
#define MOUNT_TYPE_NFS "nfs"

/* Mount(2) type/name for NFS3 filesystem */
#define MOUNT_TYPE_NFS3 "nfs"

/* Mount(2) type/name for PCFS filesystem */
/* XXX: conf/trap/trap_hpux.h may override this definition for HPUX 9.0 */
#define MOUNT_TYPE_PCFS "msdos"

/* Mount(2) type/name for LOFS filesystem */
#define MOUNT_TYPE_LOFS "lofs"

/* Mount(2) type/name for CDFS filesystem */
#define MOUNT_TYPE_CDFS "cd9660"

/* Mount(2) type/name for TFS filesystem */
/* #undef MOUNT_TYPE_TFS */

/* Mount(2) type/name for TMPFS filesystem */
/* #undef MOUNT_TYPE_TMPFS */

/* Mount(2) type/name for MFS filesystem */
#define MOUNT_TYPE_MFS "mfs"

/* Mount(2) type/name for CFS (crypto) filesystem */
/* #undef MOUNT_TYPE_CFS */

/* Mount(2) type/name for AUTOFS filesystem */
/* #undef MOUNT_TYPE_AUTOFS */

/* Mount(2) type/name for CACHEFS filesystem */
/* #undef MOUNT_TYPE_CACHEFS */

/* Mount(2) type/name for IGNORE filesystem (not real just ignore for df) */
/* #undef MOUNT_TYPE_IGNORE */

/* Mount(2) type/name for NULLFS (loopback on bsd44) filesystem */
#define MOUNT_TYPE_NULLFS "null"

/* Mount(2) type/name for UNIONFS filesystem */
#define MOUNT_TYPE_UNIONFS "union"

/* Mount(2) type/name for UMAPFS (uid/gid mapping) filesystem */
#define MOUNT_TYPE_UMAPFS "umap"


/*
 * Check for the string name for the mount-table of a filesystem.
 */

/* Mount-table entry name for UFS filesystem */
#define MNTTAB_TYPE_UFS "ufs"

/* Mount-table entry name for XFS filesystem (irix) */
/* #undef MNTTAB_TYPE_XFS */

/* Mount-table entry name for EFS filesystem (irix) */
/* #undef MNTTAB_TYPE_EFS */

/* Mount-table entry name for NFS filesystem */
#define MNTTAB_TYPE_NFS "nfs"

/* Mount-table entry name for NFS3 filesystem */
#define MNTTAB_TYPE_NFS3 "nfs"

/* Mount-table entry name for PCFS filesystem */
#define MNTTAB_TYPE_PCFS "msdos"

/* Mount-table entry name for LOFS filesystem */
#define MNTTAB_TYPE_LOFS "lofs"

/* Mount-table entry name for CDFS filesystem */
#define MNTTAB_TYPE_CDFS "cd9660"

/* Mount-table entry name for TFS filesystem */
/* #undef MNTTAB_TYPE_TFS */

/* Mount-table entry name for TMPFS filesystem */
/* #undef MNTTAB_TYPE_TMPFS */

/* Mount-table entry name for MFS filesystem */
#define MNTTAB_TYPE_MFS "mfs"

/* Mount-table entry name for CFS (crypto) filesystem */
/* #undef MNTTAB_TYPE_CFS */

/* Mount-table entry name for AUTOFS filesystem */
/* #undef MNTTAB_TYPE_AUTOFS */

/* Mount-table entry name for CACHEFS filesystem */
/* #undef MNTTAB_TYPE_CACHEFS */

/* Mount-table entry name for NULLFS (loopback on bsd44) filesystem */
#define MNTTAB_TYPE_NULLFS "null"

/* Mount-table entry name for UNIONFS filesystem */
#define MNTTAB_TYPE_UNIONFS "union"

/* Mount-table entry name for UMAPFS (uid/gid mapping) filesystem */
#define MNTTAB_TYPE_UMAPFS "umap"

/*
 * Name of mount table file name.
 */
/* #undef MNTTAB_FILE_NAME */

/* Name of mount type to hide amd mount from df(1) */
#define HIDE_MOUNT_TYPE "nfs"

/*
 * Names of various mount table option strings.
 */

/* Mount Table option string: Read only */
/* #undef MNTTAB_OPT_RO */

/* Mount Table option string: Read/write */
/* #undef MNTTAB_OPT_RW */

/* Mount Table option string: Read/write with quotas */
/* #undef MNTTAB_OPT_RQ */

/* Mount Table option string: Check quotas */
/* #undef MNTTAB_OPT_QUOTA */

/* Mount Table option string: Don't check quotas */
/* #undef MNTTAB_OPT_NOQUOTA */

/* Mount Table option string: action to taken on error */
/* #undef MNTTAB_OPT_ONERROR */

/* Mount Table option string: min. time between inconsistencies */
/* #undef MNTTAB_OPT_TOOSOON */

/* Mount Table option string: Soft mount */
/* #undef MNTTAB_OPT_SOFT */

/* Mount Table option string: spongy mount */
/* #undef MNTTAB_OPT_SPONGY */

/* Mount Table option string: Hard mount */
/* #undef MNTTAB_OPT_HARD */

/* Mount Table option string: Set uid allowed */
/* #undef MNTTAB_OPT_SUID */

/* Mount Table option string: Set uid not allowed */
/* #undef MNTTAB_OPT_NOSUID */

/* Mount Table option string: SysV-compatible gid on create */
/* #undef MNTTAB_OPT_GRPID */

/* Mount Table option string: Change mount options */
/* #undef MNTTAB_OPT_REMOUNT */

/* Mount Table option string: Disallow mounts on subdirs */
/* #undef MNTTAB_OPT_NOSUB */

/* Mount Table option string: Do multi-component lookup */
/* #undef MNTTAB_OPT_MULTI */

/* Mount Table option string: Allow NFS ops to be interrupted */
/* #undef MNTTAB_OPT_INTR */

/* Mount Table option string: Don't allow interrupted ops */
/* #undef MNTTAB_OPT_NOINTR */

/* Mount Table option string: NFS server IP port number */
/* #undef MNTTAB_OPT_PORT */

/* Mount Table option string: Secure (AUTH_DES) mounting */
/* #undef MNTTAB_OPT_SECURE */

/* Mount Table option string: Secure (AUTH_Kerb) mounting */
/* #undef MNTTAB_OPT_KERB */

/* Mount Table option string: Max NFS read size (bytes) */
/* #undef MNTTAB_OPT_RSIZE */

/* Mount Table option string: Max NFS write size (bytes) */
/* #undef MNTTAB_OPT_WSIZE */

/* Mount Table option string: NFS timeout (1/10 sec) */
/* #undef MNTTAB_OPT_TIMEO */

/* Mount Table option string: Max retransmissions (soft mnts) */
/* #undef MNTTAB_OPT_RETRANS */

/* Mount Table option string: Attr cache timeout (sec) */
/* #undef MNTTAB_OPT_ACTIMEO */

/* Mount Table option string: Min attr cache timeout (files) */
/* #undef MNTTAB_OPT_ACREGMIN */

/* Mount Table option string: Max attr cache timeout (files) */
/* #undef MNTTAB_OPT_ACREGMAX */

/* Mount Table option string: Min attr cache timeout (dirs) */
/* #undef MNTTAB_OPT_ACDIRMIN */

/* Mount Table option string: Max attr cache timeout (dirs) */
/* #undef MNTTAB_OPT_ACDIRMAX */

/* Mount Table option string: Don't cache attributes at all */
/* #undef MNTTAB_OPT_NOAC */

/* Mount Table option string: No close-to-open consistency */
/* #undef MNTTAB_OPT_NOCTO */

/* Mount Table option string: Do mount retries in background */
/* #undef MNTTAB_OPT_BG */

/* Mount Table option string: Do mount retries in foreground */
/* #undef MNTTAB_OPT_FG */

/* Mount Table option string: Number of mount retries */
/* #undef MNTTAB_OPT_RETRY */

/* Mount Table option string: Device id of mounted fs */
/* #undef MNTTAB_OPT_DEV */

/* Mount Table option string: Filesystem id of mounted fs */
/* #undef MNTTAB_OPT_FSID */

/* Mount Table option string: Get static pathconf for mount */
/* #undef MNTTAB_OPT_POSIX */

/* Mount Table option string: Automount map */
/* #undef MNTTAB_OPT_MAP */

/* Mount Table option string: Automount   direct map mount */
/* #undef MNTTAB_OPT_DIRECT */

/* Mount Table option string: Automount indirect map mount */
/* #undef MNTTAB_OPT_INDIRECT */

/* Mount Table option string: Local locking (no lock manager) */
/* #undef MNTTAB_OPT_LLOCK */

/* Mount Table option string: Ignore this entry */
/* #undef MNTTAB_OPT_IGNORE */

/* Mount Table option string: No auto (what?) */
/* #undef MNTTAB_OPT_NOAUTO */

/* Mount Table option string: No connection */
/* #undef MNTTAB_OPT_NOCONN */

/* Mount Table option string: protocol version number indicator */
/* #undef MNTTAB_OPT_VERS */

/* Mount Table option string: protocol network_id indicator */
/* #undef MNTTAB_OPT_PROTO */

/* Mount Table option string: Synchronous local directory ops */
/* #undef MNTTAB_OPT_SYNCDIR */

/* Mount Table option string: Do no allow setting sec attrs */
/* #undef MNTTAB_OPT_NOSETSEC */

/* Mount Table option string: set symlink cache time-to-live */
/* #undef MNTTAB_OPT_SYMTTL */

/* Mount Table option string: compress */
/* #undef MNTTAB_OPT_COMPRESS */

/* Mount Table option string: paging threshold */
/* #undef MNTTAB_OPT_PGTHRESH */

/* Mount Table option string: max groups */
/* #undef MNTTAB_OPT_MAXGROUPS */

/*
 * Generic mount(2) options (hex numbers)
 */

/* asynchronous filesystem access */
#define MNT2_GEN_OPT_ASYNC 0x40

/* automounter filesystem (ignore) flag, used in bsdi-4.1 */
/* #undef MNT2_GEN_OPT_AUTOMNTFS */

/* cache (what?) */
/* #undef MNT2_GEN_OPT_CACHE */

/* 6-argument mount */
/* #undef MNT2_GEN_OPT_DATA */

/* old (4-argument) mount (compatibility) */
/* #undef MNT2_GEN_OPT_FSS */

/* ignore mount entry in df output */
/* #undef MNT2_GEN_OPT_IGNORE */

/* journaling filesystem (AIX's UFS/FFS) */
/* #undef MNT2_GEN_OPT_JFS */

/* old BSD group-id on create */
/* #undef MNT2_GEN_OPT_GRPID */

/* do multi-component lookup on files */
/* #undef MNT2_GEN_OPT_MULTI */

/* use type string instead of int */
/* #undef MNT2_GEN_OPT_NEWTYPE */

/* NFS mount */
/* #undef MNT2_GEN_OPT_NFS */

/* nocache (what?) */
/* #undef MNT2_GEN_OPT_NOCACHE */

/* not a device */
#define MNT2_GEN_OPT_NODEV 0x10

/* no exec calls allowed */
#define MNT2_GEN_OPT_NOEXEC 0x4

/* not a device  */
/* #undef MNT2_GEN_OPT_NONDEV */

/* Disallow mounts beneath this mount */
/* #undef MNT2_GEN_OPT_NOSUB */

/* Setuid programs disallowed */
#define MNT2_GEN_OPT_NOSUID 0x8

/* Return ENAMETOOLONG for long filenames */
/* #undef MNT2_GEN_OPT_NOTRUNC */

/* allow overlay mounts */
/* #undef MNT2_GEN_OPT_OVERLAY */

/* check quotas */
#define MNT2_GEN_OPT_QUOTA 0x2000

/* Read-only */
#define MNT2_GEN_OPT_RDONLY 0x1

/* change options on an existing mount */
/* #undef MNT2_GEN_OPT_REMOUNT */

/* read only */
/* #undef MNT2_GEN_OPT_RONLY */

/* synchronize data immediately to filesystem */
/* #undef MNT2_GEN_OPT_SYNC */

/* synchronous filesystem access (same as SYNC) */
#define MNT2_GEN_OPT_SYNCHRONOUS 0x2

/* Mount with Sys 5-specific semantics */
/* #undef MNT2_GEN_OPT_SYS5 */

/* Union mount */
/* #undef MNT2_GEN_OPT_UNION */

/*
 * NFS-specific mount(2) options (hex numbers)
 */

/* hide mount type from df(1) */
/* #undef MNT2_NFS_OPT_AUTO */

/* set max secs for dir attr cache */
#define MNT2_NFS_OPT_ACDIRMAX 0x200000

/* set min secs for dir attr cache */
#define MNT2_NFS_OPT_ACDIRMIN 0x100000

/* set max secs for file attr cache */
#define MNT2_NFS_OPT_ACREGMAX 0x80000

/* set min secs for file attr cache */
#define MNT2_NFS_OPT_ACREGMIN 0x40000

/* Authentication error */
/* #undef MNT2_NFS_OPT_AUTHERR */

/* set dead server retry thresh */
#define MNT2_NFS_OPT_DEADTHRESH 0x4000

/* Dismount in progress */
/* #undef MNT2_NFS_OPT_DISMINPROG */

/* Dismounted */
/* #undef MNT2_NFS_OPT_DISMNT */

/* Don't estimate rtt dynamically */
#define MNT2_NFS_OPT_DUMBTIMR 0x800

/* System V-style gid inheritance */
/* #undef MNT2_NFS_OPT_GRPID */

/* Has authenticator */
/* #undef MNT2_NFS_OPT_HASAUTH */

/* provide name of server's fs to system */
/* #undef MNT2_NFS_OPT_FSNAME */

/* set hostname for error printf */
/* #undef MNT2_NFS_OPT_HOSTNAME */

/* ignore mount point */
/* #undef MNT2_NFS_OPT_IGNORE */

/* allow interrupts on hard mount */
#define MNT2_NFS_OPT_INT 0x40

/* Bits set internally */
/* #undef MNT2_NFS_OPT_INTERNAL */

/* Use Kerberos authentication */
/* #undef MNT2_NFS_OPT_KERB */

/* use kerberos credentials */
/* #undef MNT2_NFS_OPT_KERBEROS */

/* transport's knetconfig structure */
/* #undef MNT2_NFS_OPT_KNCONF */

/* set lease term (nqnfs) */
#define MNT2_NFS_OPT_LEASETERM 0x1000

/* Local locking (no lock manager) */
/* #undef MNT2_NFS_OPT_LLOCK */

/* set maximum grouplist size */
#define MNT2_NFS_OPT_MAXGRPS 0x20

/* Mnt server for mnt point */
/* #undef MNT2_NFS_OPT_MNTD */

/* Assume writes were mine */
/* #undef MNT2_NFS_OPT_MYWRITE */

/* mount NFS Version 3 */
#define MNT2_NFS_OPT_NFSV3 0x200

/* don't cache attributes */
/* #undef MNT2_NFS_OPT_NOAC */

/* Don't Connect the socket */
#define MNT2_NFS_OPT_NOCONN 0x80

/* no close-to-open consistency */
/* #undef MNT2_NFS_OPT_NOCTO */

/* disallow interrupts on hard mounts */
/* #undef MNT2_NFS_OPT_NOINT */

/* Get lease for lookup */
/* #undef MNT2_NFS_OPT_NQLOOKLEASE */

/* Use Nqnfs protocol */
#define MNT2_NFS_OPT_NQNFS 0x100

/* static pathconf kludge info */
/* #undef MNT2_NFS_OPT_POSIX */

/* Rcv socket lock */
/* #undef MNT2_NFS_OPT_RCVLOCK */

/* Do lookup with readdir (nqnfs) */
/* #undef MNT2_NFS_OPT_RDIRALOOK */

/* set read ahead */
#define MNT2_NFS_OPT_READAHEAD 0x2000

/* Allocate a reserved port */
#define MNT2_NFS_OPT_RESVPORT 0x8000

/* set number of request retries */
#define MNT2_NFS_OPT_RETRANS 0x10

/* read only */
/* #undef MNT2_NFS_OPT_RONLY */

/* use RPC to do secure NFS time sync */
/* #undef MNT2_NFS_OPT_RPCTIMESYNC */

/* set read size */
#define MNT2_NFS_OPT_RSIZE 0x4

/* secure mount */
/* #undef MNT2_NFS_OPT_SECURE */

/* Send socket lock */
/* #undef MNT2_NFS_OPT_SNDLOCK */

/* soft mount (hard is default) */
#define MNT2_NFS_OPT_SOFT 0x1

/* spongy mount */
/* #undef MNT2_NFS_OPT_SPONGY */

/* set initial timeout */
#define MNT2_NFS_OPT_TIMEO 0x8

/* use TCP for mounts */
/* #undef MNT2_NFS_OPT_TCP */

/* Wait for authentication */
/* #undef MNT2_NFS_OPT_WAITAUTH */

/* Wants an authenticator */
/* #undef MNT2_NFS_OPT_WANTAUTH */

/* Want receive socket lock */
/* #undef MNT2_NFS_OPT_WANTRCV */

/* Want send socket lock */
/* #undef MNT2_NFS_OPT_WANTSND */

/* set write size */
#define MNT2_NFS_OPT_WSIZE 0x2

/* set symlink cache time-to-live */
/* #undef MNT2_NFS_OPT_SYMTTL */

/* paging threshold */
/* #undef MNT2_NFS_OPT_PGTHRESH */

/*
 * CDFS-specific mount(2) options (hex numbers)
 */

/* Ignore permission bits */
/* #undef MNT2_CDFS_OPT_DEFPERM */

/* Use on-disk permission bits */
/* #undef MNT2_CDFS_OPT_NODEFPERM */

/* Strip off extension from version string */
/* #undef MNT2_CDFS_OPT_NOVERSION */

/* Use Rock Ridge Interchange Protocol (RRIP) extensions */
/* #undef MNT2_CDFS_OPT_RRIP */

/*
 * Existence of fields in structures.
 */

/* does mntent_t have mnt_cnode field? */
/* #undef HAVE_FIELD_MNTENT_T_MNT_CNODE */

/* does mntent_t have mnt_time field? */
/* #undef HAVE_FIELD_MNTENT_T_MNT_TIME */

/* does mntent_t have mnt_time field and is of type "char *" ? */
/* #undef HAVE_FIELD_MNTENT_T_MNT_TIME_STRING */

/* does mntent_t have mnt_ro field? */
/* #undef HAVE_FIELD_MNTENT_T_MNT_RO */

/* does cdfs_args_t have flags field? */
#define HAVE_FIELD_CDFS_ARGS_T_FLAGS 1

/* does cdfs_args_t have fspec field? */
#define HAVE_FIELD_CDFS_ARGS_T_FSPEC 1

/* does cdfs_args_t have iso_flags field? */
/* #undef HAVE_FIELD_CDFS_ARGS_T_ISO_FLAGS */

/* does cdfs_args_t have iso_pgthresh field? */
/* #undef HAVE_FIELD_CDFS_ARGS_T_ISO_PGTHRESH */

/* does cdfs_args_t have norrip field? */
/* #undef HAVE_FIELD_CDFS_ARGS_T_NORRIP */

/* does cdfs_args_t have ssector field? */
#define HAVE_FIELD_CDFS_ARGS_T_SSECTOR 1

/* does pcfs_args_t have dsttime field? */
/* #undef HAVE_FIELD_PCFS_ARGS_T_DSTTIME */

/* does pcfs_args_t have fspec field? */
#define HAVE_FIELD_PCFS_ARGS_T_FSPEC 1

/* does pcfs_args_t have gid field? */
#define HAVE_FIELD_PCFS_ARGS_T_GID 1

/* does pcfs_args_t have mask field? */
#define HAVE_FIELD_PCFS_ARGS_T_MASK 1

/* does pcfs_args_t have secondswest field? */
/* #undef HAVE_FIELD_PCFS_ARGS_T_SECONDSWEST */

/* does pcfs_args_t have uid field? */
#define HAVE_FIELD_PCFS_ARGS_T_UID 1

/* does ufs_args_t have flags field? */
/* #undef HAVE_FIELD_UFS_ARGS_T_FLAGS */

/* does ufs_args_t have fspec field? */
#define HAVE_FIELD_UFS_ARGS_T_FSPEC 1

/* does efs_args_t have flags field? */
/* #undef HAVE_FIELD_EFS_ARGS_T_FLAGS */

/* does efs_args_t have fspec field? */
/* #undef HAVE_FIELD_EFS_ARGS_T_FSPEC */

/* does xfs_args_t have flags field? */
/* #undef HAVE_FIELD_XFS_ARGS_T_FLAGS */

/* does xfs_args_t have fspec field? */
/* #undef HAVE_FIELD_XFS_ARGS_T_FSPEC */

/* does ufs_ars_t have ufs_flags field? */
/* #undef HAVE_FIELD_UFS_ARGS_T_UFS_FLAGS */

/* does ufs_ars_t have ufs_pgthresh field? */
/* #undef HAVE_FIELD_UFS_ARGS_T_UFS_PGTHRESH */

/* does struct fhstatus have a fhs_fh field? */
/* #undef HAVE_FIELD_STRUCT_FHSTATUS_FHS_FH */

/* does struct statfs have an f_fstypename field? */
#define HAVE_FIELD_STRUCT_STATFS_F_FSTYPENAME 1

/* does struct nfs_args have an acdirmin field? */
#define HAVE_FIELD_NFS_ARGS_T_ACDIRMIN 1

/* does struct nfs_args have an acregmin field? */
#define HAVE_FIELD_NFS_ARGS_T_ACREGMIN 1

/* does struct nfs_args have a bsize field? */
/* #undef HAVE_FIELD_NFS_ARGS_T_BSIZE */

/* does struct nfs_args have an fh_len field? */
/* #undef HAVE_FIELD_NFS_ARGS_T_FH_LEN */

/* does struct nfs_args have an fhsize field? */
#define HAVE_FIELD_NFS_ARGS_T_FHSIZE 1

/* does struct nfs_args have a gfs_flags field? */
/* #undef HAVE_FIELD_NFS_ARGS_T_GFS_FLAGS */

/* does struct nfs_args have a namlen field? */
/* #undef HAVE_FIELD_NFS_ARGS_T_NAMLEN */

/* does struct nfs_args have an optstr field? */
/* #undef HAVE_FIELD_NFS_ARGS_T_OPTSTR */

/* does struct nfs_args have a proto field? */
#define HAVE_FIELD_NFS_ARGS_T_PROTO 1

/* does struct nfs_args have a socket type field? */
#define HAVE_FIELD_NFS_ARGS_T_SOTYPE 1

/* does struct nfs_args have a version field? */
#define HAVE_FIELD_NFS_ARGS_T_VERSION 1

/* does struct ifreq have field ifr_addr? */
#define HAVE_FIELD_STRUCT_IFREQ_IFR_ADDR 1

/* does struct ifaddrs have field ifa_next? */
/* #undef HAVE_FIELD_STRUCT_IFADDRS_IFA_NEXT */

/* does struct sockaddr have field sa_len? */
#define HAVE_FIELD_STRUCT_SOCKADDR_SA_LEN 1

/* does struct autofs_args have an addr field? */
/* #undef HAVE_FIELD_AUTOFS_ARGS_T_ADDR */

/* does umntrequest have an rdevid field? */
/* #undef HAVE_FIELD_UMNTREQUEST_RDEVID */


/* should signal handlers be reinstalled? */
/* #undef REINSTALL_SIGNAL_HANDLER */


/**************************************************************************/
/*** Everything above this line is part of the "TOP" of acconfig.h.	***/
/**************************************************************************/


/* Define if on AIX 3.
   System headers sometimes define this.
   We just want to avoid a redefinition error message.  */
#ifndef _ALL_SOURCE
/* #undef _ALL_SOURCE */
#endif

/* Define if using alloca.c.  */
/* #undef C_ALLOCA */

/* Define to empty if the keyword does not work.  */
/* #undef const */

/* Define to one of _getb67, GETB67, getb67 for Cray-2 and Cray-YMP systems.
   This function is required for alloca.c support on those systems.  */
/* #undef CRAY_STACKSEG_END */

/* Define to the type of elements in the array set by `getgroups'.
   Usually this is either `int' or `gid_t'.  */
#define GETGROUPS_T gid_t

/* Define to `int' if <sys/types.h> doesn't define.  */
/* #undef gid_t */

/* Define if you have alloca, as a function or macro.  */
#define HAVE_ALLOCA 1

/* Define if you have <alloca.h> and it should be used (not on Ultrix).  */
/* #undef HAVE_ALLOCA_H */

/* Define if you support file names longer than 14 characters.  */
#define HAVE_LONG_FILE_NAMES 1

/* Define if system calls automatically restart after interruption
   by a signal.  */
#define HAVE_RESTARTABLE_SYSCALLS 1

/* Define if your struct stat has st_rdev.  */
#define HAVE_ST_RDEV 1

/* Define if you have <sys/wait.h> that is POSIX.1 compatible.  */
#define HAVE_SYS_WAIT_H 1

/* Define if you have <vfork.h>.  */
/* #undef HAVE_VFORK_H */

/* Define if you have the wait3 system call.  */
#define HAVE_WAIT3 1

/* Define as __inline if that's what the C compiler calls it.  */
/* #undef inline */

/* Define if your C compiler doesn't accept -c and -o together.  */
/* #undef NO_MINUS_C_MINUS_O */

/* Define if you need to in order for stat and other things to work.  */
/* #undef _POSIX_SOURCE */

/* Define as the return type of signal handlers (int or void).  */
#define RETSIGTYPE void

/* If using the C implementation of alloca, define if you know the
   direction of stack growth for your system; otherwise it will be
   automatically deduced at run-time.
 STACK_DIRECTION > 0 => grows toward higher addresses
 STACK_DIRECTION < 0 => grows toward lower addresses
 STACK_DIRECTION = 0 => direction of growth unknown
 */
/* #undef STACK_DIRECTION */

/* Define if the `S_IS*' macros in <sys/stat.h> do not work properly.  */
/* #undef STAT_MACROS_BROKEN */

/* Define if you have the ANSI C header files.  */
#define STDC_HEADERS 1

/* Define if you can safely include both <sys/time.h> and <time.h>.  */
#define TIME_WITH_SYS_TIME 1

/* Define if your <sys/time.h> declares struct tm.  */
/* #undef TM_IN_SYS_TIME */

/* Define to `int' if <sys/types.h> doesn't define.  */
/* #undef uid_t */

/* Define vfork as fork if vfork does not work.  */
/* #undef vfork */

/* Define if your processor stores words with the most significant
   byte first (like Motorola and SPARC, unlike Intel and VAX).  */
/* #undef WORDS_BIGENDIAN */
#include <sys/types.h>
#include <machine/endian.h>
#if BYTE_ORDER == BIG_ENDIAN
#define WORDS_BIGENDIAN
#endif

/* Define if lex declares yytext as a char * by default, not a char[].  */
#define YYTEXT_POINTER 1

/* Turn off general debugging by default */
/* #undef DEBUG */

/* Turn off memory debugging by default */
/* #undef DEBUG_MEM */

/* Enable "amq -M" remote mount code (insecure due to IP spoofing) */
/* #undef ENABLE_AMQ_MOUNT */

/* Define package name (must be defined by configure.in) */
#define PACKAGE "am-utils"

/* Define version of package (must be defined by configure.in) */
#define VERSION "6.0.3s1"

/* We [FREEBSD-NATIVE] pick some parameters from our local config file */
#include "config_local.h"

/* Define name of host machine's cpu (eg. sparc) */
/* #define HOST_CPU "i386" */

/* Define name of host machine's architecture (eg. sun4) */
/* #define HOST_ARCH "i386" */

/* Define name of host machine's vendor (eg. sun) */
#define HOST_VENDOR "unknown"

/* Define name and version of host machine (eg. solaris2.5.1) */
/* #define HOST_OS "freebsd3.0" */

/* Define only name of host machine OS (eg. solaris2) */
/* #define HOST_OS_NAME "freebsd3" */

/* Define only version of host machine (eg. 2.5.1) */
/* #define HOST_OS_VERSION "3.0" */

/* Define the header version of (linux) hosts (eg. 2.2.10) */
/* #define HOST_HEADER_VERSION "4.0" */

/* Define name of host */
/* #define HOST_NAME "dragon.nuxi.com" */

/* Define user name */
/* #define USER_NAME "obrien" */

/* Define configuration date */
/* #define CONFIG_DATE "Fri Aug 21 19:35:55 PDT 1998" */

/* what type of network transport type is in use?  TLI or sockets? */
/* #undef HAVE_TRANSPORT_TYPE_TLI */

/* Define to "void *" if compiler can handle, otherwise "char *" */
#define voidp void *

/* Define a type/structure for an NFS V2 filehandle */
#define am_nfs_fh nfs_fh

/* Define a type/structure for an NFS V3 filehandle */
#define am_nfs_fh3 nfs_fh3

/* define name of am-utils' NFS protocol header */
#define AMU_NFS_PROTOCOL_HEADER "./conf/nfs_prot/nfs_prot_freebsd3.h"

/* Define a type for the nfs_args structure */
#define nfs_args_t struct nfs_args

/* Define the field name for the filehandle within nfs_args_t */
#define NFS_FH_FIELD fh

/* Define if plain fhandle type exists */
#define HAVE_FHANDLE 1

/* Define the type of the 3rd argument ('in') to svc_getargs() */
#define SVC_IN_ARG_TYPE caddr_t

/* Define to the type of xdr procedure type */
#define XDRPROC_T_TYPE xdrproc_t

/* Define if mount table is on file, undefine if in kernel */
/* #undef MOUNT_TABLE_ON_FILE */

/* Define if have struct mntent in one of the standard headers */
/* #undef HAVE_STRUCT_MNTENT */

/* Define if have struct mnttab in one of the standard headers */
/* #undef HAVE_STRUCT_MNTTAB */

/* Define if have struct nfs_args in one of the standard nfs headers */
#define HAVE_STRUCT_NFS_ARGS 1

/* Define if have struct nfs_mount_data in one of the standard nfs headers */
/* #undef HAVE_STRUCT_NFS_MOUNT_DATA */

/* Define if have struct nfs_gfs_mount in one of the standard nfs headers */
/* #undef HAVE_STRUCT_NFS_GFS_MOUNT */

/* Type of the 3rd argument to yp_order() */
#define YP_ORDER_OUTORDER_TYPE int

/* Type of the 6th argument to recvfrom() */
#define RECVFROM_FROMLEN_TYPE int

/* Type of the 5rd argument to authunix_create() */
#define AUTH_CREATE_GIDLIST_TYPE gid_t

/* The string used in printf to print the mount-type field of mount(2) */
#define MTYPE_PRINTF_TYPE "%s"

/* Type of the mount-type field in the mount() system call */
#define MTYPE_TYPE const char *

/* Define a type for the pcfs_args structure */
#define pcfs_args_t struct msdosfs_args

/* Define a type for the autofs_args structure */
/* #undef autofs_args_t */

/* Define a type for the cachefs_args structure */
/* #undef cachefs_args_t */

/* Define a type for the tmpfs_args structure */
/* #undef tmpfs_args_t */

/* Define a type for the ufs_args structure */
#define ufs_args_t struct ufs_args

/* Define a type for the efs_args structure */
/* #undef efs_args_t */

/* Define a type for the xfs_args structure */
/* #undef xfs_args_t */

/* Define a type for the lofs_args structure */
/* #undef lofs_args_t */

/* Define a type for the cdfs_args structure */
#define cdfs_args_t struct iso_args

/* Define a type for the mfs_args structure */
#define mfs_args_t struct mfs_args

/* Define a type for the rfs_args structure */
/* #undef rfs_args_t */

/* define if have a bad version of memcmp() */
/* #undef HAVE_BAD_MEMCMP */

/* define if have a bad version of yp_all() */
/* #undef HAVE_BAD_YP_ALL */

/* define if must use NFS "noconn" option */
/* #undef USE_UNCONNECTED_NFS_SOCKETS */
/* define if must NOT use NFS "noconn" option */
#define USE_CONNECTED_NFS_SOCKETS 1

/* Define if you have the __seterr_reply function.  */
/* #undef HAVE___SETERR_REPLY */

/* Define if you have the _seterr_reply function.  */
#define HAVE__SETERR_REPLY 1

/* Define if you have the bcmp function.  */
#define HAVE_BCMP 1

/* Define if you have the bcopy function.  */
#define HAVE_BCOPY 1

/* Define if you have the bzero function.  */
#define HAVE_BZERO 1

/* Define if you have the clnt_create function.  */
#define HAVE_CLNT_CREATE 1

/* Define if you have the clnt_create_timed function.  */
/* #undef HAVE_CLNT_CREATE_TIMED */

/* Define if you have the clnt_spcreateerror function.  */
#define HAVE_CLNT_SPCREATEERROR 1

/* Define if you have the clnt_sperrno function.  */
#define HAVE_CLNT_SPERRNO 1

/* Define if you have the cnodeid function.  */
/* #undef HAVE_CNODEID */

/* Define if you have the dbm_open function.  */
#define HAVE_DBM_OPEN 1

/* Define if you have the dg_mount function.  */
/* #undef HAVE_DG_MOUNT */

/* Define if you have the fgets function.  */
#define HAVE_FGETS 1

/* Define if you have the flock function.  */
#define HAVE_FLOCK 1

/* Define if you have the fsmount function.  */
/* #undef HAVE_FSMOUNT */

/* Define if you have the get_myaddress function.  */
#define HAVE_GET_MYADDRESS 1

/* Define if you have the getccent function.  */
/* #undef HAVE_GETCCENT */

/* Define if you have the getcwd function.  */
#define HAVE_GETCWD 1

/* Define if you have the getdomainname function.  */
#define HAVE_GETDOMAINNAME 1

/* Define if you have the getdtablesize function.  */
#define HAVE_GETDTABLESIZE 1

/* Define if you have the gethostname function.  */
#define HAVE_GETHOSTNAME 1

/* Define if you have the getifaddrs function.  */
/* #undef HAVE_GETIFADDRS */

/* Define if you have the getmntinfo function.  */
#define HAVE_GETMNTINFO 1

/* Define if you have the getmountent function.  */
/* #undef HAVE_GETMOUNTENT */

/* Define if you have the getpagesize function.  */
#define HAVE_GETPAGESIZE 1

/* Define if you have the getpwnam function.  */
#define HAVE_GETPWNAM 1

/* Define if you have the gettimeofday function.  */
#define HAVE_GETTIMEOFDAY 1

/* Define if you have the hasmntopt function.  */
/* #undef HAVE_HASMNTOPT */

/* Define if you have the hes_init function.  */
/* #undef HAVE_HES_INIT */

/* Define if you have the hesiod_init function.  */
/* #undef HAVE_HESIOD_INIT */

/* Define if you have the hesiod_reload function.  */
/* #undef HAVE_HESIOD_RELOAD */

/* Define if you have the hesiod_to_bind function.  */
/* #undef HAVE_HESIOD_TO_BIND */

/* Define if you have the ldap_open function.  */
/* #undef HAVE_LDAP_OPEN */

/* Define if you have the memcmp function.  */
#define HAVE_MEMCMP 1

/* Define if you have the memcpy function.  */
#define HAVE_MEMCPY 1

/* Define if you have the memmove function.  */
#define HAVE_MEMMOVE 1

/* Define if you have the memset function.  */
#define HAVE_MEMSET 1

/* Define if you have the mkdir function.  */
#define HAVE_MKDIR 1

/* Define if you have the mkstemp function.  */
#define HAVE_MKSTEMP 1

/* Define if you have the mntctl function.  */
/* #undef HAVE_MNTCTL */

/* Define if you have the mount function.  */
#define HAVE_MOUNT 1

/* Define if you have the mountsyscall function.  */
/* #undef HAVE_MOUNTSYSCALL */

/* Define if you have the nis_domain_of function.  */
/* #undef HAVE_NIS_DOMAIN_OF */

/* Define if you have the opendir function.  */
#define HAVE_OPENDIR 1

/* Define if you have the plock function.  */
/* #undef HAVE_PLOCK */

/* Define if you have the regcomp function.  */
#define HAVE_REGCOMP 1

/* Define if you have the regexec function.  */
#define HAVE_REGEXEC 1

/* Define if you have the rmdir function.  */
#define HAVE_RMDIR 1

/* Define if you have the select function.  */
#define HAVE_SELECT 1

/* Define if you have the seteuid function.  */
#define HAVE_SETEUID 1

/* Define if you have the setitimer function.  */
#define HAVE_SETITIMER 1

/* Define if you have the setresuid function.  */
/* #undef HAVE_SETRESUID */

/* Define if you have the setsid function.  */
#define HAVE_SETSID 1

/* Define if you have the sigaction function.  */
#define HAVE_SIGACTION 1

/* Define if you have the signal function.  */
#define HAVE_SIGNAL 1

/* Define if you have the socket function.  */
#define HAVE_SOCKET 1

/* Define if you have the strcasecmp function.  */
#define HAVE_STRCASECMP 1

/* Define if you have the strchr function.  */
#define HAVE_STRCHR 1

/* Define if you have the strcspn function.  */
#define HAVE_STRCSPN 1

/* Define if you have the strdup function.  */
#define HAVE_STRDUP 1

/* Define if you have the strerror function.  */
#define HAVE_STRERROR 1

/* Define if you have the strspn function.  */
#define HAVE_STRSPN 1

/* Define if you have the strstr function.  */
#define HAVE_STRSTR 1

/* Define if you have the svc_getreq function.  */
#define HAVE_SVC_GETREQ 1

/* Define if you have the svc_getreqset function.  */
#define HAVE_SVC_GETREQSET 1

/* Define if you have the sysfs function.  */
/* #undef HAVE_SYSFS */

/* Define if you have the syslog function.  */
#define HAVE_SYSLOG 1

/* Define if you have the ualarm function.  */
#define HAVE_UALARM 1

/* Define if you have the umount function.  */
/* #undef HAVE_UMOUNT */

/* Define if you have the uname function.  */
#define HAVE_UNAME 1

/* Define if you have the unmount function.  */
#define HAVE_UNMOUNT 1

/* Define if you have the uvmount function.  */
/* #undef HAVE_UVMOUNT */

/* Define if you have the vfork function.  */
#define HAVE_VFORK 1

/* Define if you have the vfsmount function.  */
/* #undef HAVE_VFSMOUNT */

/* Define if you have the vmount function.  */
/* #undef HAVE_VMOUNT */

/* Define if you have the vsnprintf function.  */
#define HAVE_VSNPRINTF 1

/* Define if you have the wait3 function.  */
#define HAVE_WAIT3 1

/* Define if you have the waitpid function.  */
#define HAVE_WAITPID 1

/* Define if you have the xdr_attrstat function.  */
#define HAVE_XDR_ATTRSTAT 1

/* Define if you have the xdr_createargs function.  */
#define HAVE_XDR_CREATEARGS 1

/* Define if you have the xdr_dirlist function.  */
#define HAVE_XDR_DIRLIST 1

/* Define if you have the xdr_diropargs function.  */
#define HAVE_XDR_DIROPARGS 1

/* Define if you have the xdr_diropokres function.  */
#define HAVE_XDR_DIROPOKRES 1

/* Define if you have the xdr_diropres function.  */
#define HAVE_XDR_DIROPRES 1

/* Define if you have the xdr_dirpath function.  */
#define HAVE_XDR_DIRPATH 1

/* Define if you have the xdr_entry function.  */
#define HAVE_XDR_ENTRY 1

/* Define if you have the xdr_exportnode function.  */
#define HAVE_XDR_EXPORTNODE 1

/* Define if you have the xdr_exports function.  */
#define HAVE_XDR_EXPORTS 1

/* Define if you have the xdr_fattr function.  */
#define HAVE_XDR_FATTR 1

/* Define if you have the xdr_fhandle function.  */
#define HAVE_XDR_FHANDLE 1

/* Define if you have the xdr_fhstatus function.  */
#define HAVE_XDR_FHSTATUS 1

/* Define if you have the xdr_filename function.  */
#define HAVE_XDR_FILENAME 1

/* Define if you have the xdr_ftype function.  */
#define HAVE_XDR_FTYPE 1

/* Define if you have the xdr_groupnode function.  */
#define HAVE_XDR_GROUPNODE 1

/* Define if you have the xdr_groups function.  */
#define HAVE_XDR_GROUPS 1

/* Define if you have the xdr_linkargs function.  */
#define HAVE_XDR_LINKARGS 1

/* Define if you have the xdr_mntrequest function.  */
/* #undef HAVE_XDR_MNTREQUEST */

/* Define if you have the xdr_mntres function.  */
/* #undef HAVE_XDR_MNTRES */

/* Define if you have the xdr_mountbody function.  */
#define HAVE_XDR_MOUNTBODY 1

/* Define if you have the xdr_mountlist function.  */
#define HAVE_XDR_MOUNTLIST 1

/* Define if you have the xdr_mountres3 function.  */
/* #undef HAVE_XDR_MOUNTRES3 */

/* Define if you have the xdr_name function.  */
#define HAVE_XDR_NAME 1

/* Define if you have the xdr_nfs_fh function.  */
#define HAVE_XDR_NFS_FH 1

/* Define if you have the xdr_nfscookie function.  */
#define HAVE_XDR_NFSCOOKIE 1

/* Define if you have the xdr_nfspath function.  */
#define HAVE_XDR_NFSPATH 1

/* Define if you have the xdr_nfsstat function.  */
#define HAVE_XDR_NFSSTAT 1

/* Define if you have the xdr_nfstime function.  */
#define HAVE_XDR_NFSTIME 1

/* Define if you have the xdr_pointer function.  */
#define HAVE_XDR_POINTER 1

/* Define if you have the xdr_readargs function.  */
#define HAVE_XDR_READARGS 1

/* Define if you have the xdr_readdirargs function.  */
#define HAVE_XDR_READDIRARGS 1

/* Define if you have the xdr_readdirres function.  */
#define HAVE_XDR_READDIRRES 1

/* Define if you have the xdr_readlinkres function.  */
#define HAVE_XDR_READLINKRES 1

/* Define if you have the xdr_readokres function.  */
#define HAVE_XDR_READOKRES 1

/* Define if you have the xdr_readres function.  */
#define HAVE_XDR_READRES 1

/* Define if you have the xdr_renameargs function.  */
#define HAVE_XDR_RENAMEARGS 1

/* Define if you have the xdr_sattr function.  */
#define HAVE_XDR_SATTR 1

/* Define if you have the xdr_sattrargs function.  */
#define HAVE_XDR_SATTRARGS 1

/* Define if you have the xdr_statfsokres function.  */
#define HAVE_XDR_STATFSOKRES 1

/* Define if you have the xdr_statfsres function.  */
#define HAVE_XDR_STATFSRES 1

/* Define if you have the xdr_symlinkargs function.  */
#define HAVE_XDR_SYMLINKARGS 1

/* Define if you have the xdr_umntrequest function.  */
/* #undef HAVE_XDR_UMNTREQUEST */

/* Define if you have the xdr_umntres function.  */
/* #undef HAVE_XDR_UMNTRES */

/* Define if you have the xdr_writeargs function.  */
#define HAVE_XDR_WRITEARGS 1

/* Define if you have the yp_all function.  */
/* #undef HAVE_YP_ALL */

/* Define if you have the yp_get_default_domain function.  */
#define HAVE_YP_GET_DEFAULT_DOMAIN 1

/* Define if you have the <arpa/inet.h> header file.  */
#define HAVE_ARPA_INET_H 1

/* Define if you have the <arpa/nameser.h> header file.  */
#define HAVE_ARPA_NAMESER_H 1

/* Define if you have the <assert.h> header file.  */
#define HAVE_ASSERT_H 1

/* Define if you have the <bsd/rpc/rpc.h> header file.  */
/* #undef HAVE_BSD_RPC_RPC_H */

/* Define if you have the <cdfs/cdfs_mount.h> header file.  */
/* #undef HAVE_CDFS_CDFS_MOUNT_H */

/* Define if you have the <cdfs/cdfsmount.h> header file.  */
/* #undef HAVE_CDFS_CDFSMOUNT_H */

/* Define if you have the <cluster.h> header file.  */
/* #undef HAVE_CLUSTER_H */

/* Define if you have the <ctype.h> header file.  */
#define HAVE_CTYPE_H 1

/* Define if you have the <db1/ndbm.h> header file.  */
/* #undef HAVE_DB1_NDBM_H */

/* Define if you have the <dirent.h> header file.  */
#define HAVE_DIRENT_H 1

/* Define if you have the <errno.h> header file.  */
#define HAVE_ERRNO_H 1

/* Define if you have the <fcntl.h> header file.  */
#define HAVE_FCNTL_H 1

/* Define if you have the <grp.h> header file.  */
#define HAVE_GRP_H 1

/* Define if you have the <hesiod.h> header file.  */
/* #undef HAVE_HESIOD_H */

/* Define if you have the <hsfs/hsfs.h> header file.  */
/* #undef HAVE_HSFS_HSFS_H */

/* Define if you have the <ifaddrs.h> header file.  */
/* #undef HAVE_IFADDRS_H */

/* Define if you have the <irs.h> header file.  */
/* #undef HAVE_IRS_H */

/* Define if you have the <isofs/cd9660/cd9660_mount.h> header file.  */
#define HAVE_ISOFS_CD9660_CD9660_MOUNT_H 1

/* Define if you have the <lber.h> header file.  */
/* #undef HAVE_LBER_H */

/* Define if you have the <ldap.h> header file.  */
/* #undef HAVE_LDAP_H */

/* Define if you have the <libgen.h> header file.  */
/* #undef HAVE_LIBGEN_H */

/* Define if you have the <linux/auto_fs.h> header file.  */
/* #undef HAVE_LINUX_AUTO_FS_H */

/* Define if you have the <linux/fs.h> header file.  */
/* #undef HAVE_LINUX_FS_H */

/* Define if you have the <linux/nfs.h> header file.  */
/* #undef HAVE_LINUX_NFS_H */

/* Define if you have the <linux/nfs_mount.h> header file.  */
/* #undef HAVE_LINUX_NFS_MOUNT_H */

/* Define if you have the <linux/posix_types.h> header file.  */
/* #undef HAVE_LINUX_POSIX_TYPES_H */

/* Define if you have the <machine/endian.h> header file.  */
#define HAVE_MACHINE_ENDIAN_H 1

/* Define if you have the <malloc.h> header file.  */
/* #undef HAVE_MALLOC_H */

/* Define if you have the <memory.h> header file.  */
#define HAVE_MEMORY_H 1

/* Define if you have the <mntent.h> header file.  */
/* #undef HAVE_MNTENT_H */

/* Define if you have the <mnttab.h> header file.  */
/* #undef HAVE_MNTTAB_H */

/* Define if you have the <mount.h> header file.  */
/* #undef HAVE_MOUNT_H */

/* Define if you have the <msdosfs/msdosfsmount.h> header file.  */
/* #undef HAVE_MSDOSFS_MSDOSFSMOUNT_H */

/* Define if you have the <fs/msdosfs/msdosfsmount.h> header file.  */
#define HAVE_FS_MSDOSFS_MSDOSFSMOUNT_H 1

/* Define if you have the <ndbm.h> header file.  */
#define HAVE_NDBM_H 1

/* Define if you have the <ndir.h> header file.  */
/* #undef HAVE_NDIR_H */

/* Define if you have the <net/errno.h> header file.  */
/* #undef HAVE_NET_ERRNO_H */

/* Define if you have the <net/if.h> header file.  */
#define HAVE_NET_IF_H 1

/* Define if you have the <net/if_var.h> header file.  */
#define HAVE_NET_IF_VAR_H 1

/* Define if you have the <net/route.h> header file.  */
#define HAVE_NET_ROUTE_H 1

/* Define if you have the <netconfig.h> header file.  */
/* #undef HAVE_NETCONFIG_H */

/* Define if you have the <netdb.h> header file.  */
#define HAVE_NETDB_H 1

/* Define if you have the <netdir.h> header file.  */
/* #undef HAVE_NETDIR_H */

/* Define if you have the <netinet/if_ether.h> header file.  */
#define HAVE_NETINET_IF_ETHER_H 1

/* Define if you have the <netinet/in.h> header file.  */
#define HAVE_NETINET_IN_H 1

/* Define if you have the <nfs/export.h> header file.  */
/* #undef HAVE_NFS_EXPORT_H */

/* Define if you have the <nfs/mount.h> header file.  */
/* #undef HAVE_NFS_MOUNT_H */

/* Define if you have the <nfs/nfs.h> header file.  */
#define HAVE_NFS_NFS_H 1

/* Define if you have the <nfs/nfs_clnt.h> header file.  */
/* #undef HAVE_NFS_NFS_CLNT_H */

/* Define if you have the <nfs/nfs_gfs.h> header file.  */
/* #undef HAVE_NFS_NFS_GFS_H */

/* Define if you have the <nfs/nfs_mount.h> header file.  */
/* #undef HAVE_NFS_NFS_MOUNT_H */

/* Define if you have the <nfs/nfsmount.h> header file.  */
#define HAVE_NFS_NFSMOUNT_H 1

/* Define if you have the <nfs/nfsproto.h> header file.  */
#define HAVE_NFS_NFSPROTO_H 1

/* Define if you have the <nfs/nfsv2.h> header file.  */
#define HAVE_NFS_NFSV2_H 1

/* Define if you have the <nfs/pathconf.h> header file.  */
/* #undef HAVE_NFS_PATHCONF_H */

/* Define if you have the <nfs/rpcv2.h> header file.  */
#define HAVE_NFS_RPCV2_H 1

/* Define if you have the <nsswitch.h> header file.  */
/* #undef HAVE_NSSWITCH_H */

/* Define if you have the <pwd.h> header file.  */
#define HAVE_PWD_H 1

/* Define if you have the <regex.h> header file.  */
#define HAVE_REGEX_H 1

/* Define if you have the <resolv.h> header file.  */
#define HAVE_RESOLV_H 1

/* Define if you have the <rpc/auth_des.h> header file.  */
#define HAVE_RPC_AUTH_DES_H 1

/* Define if you have the <rpc/pmap_clnt.h> header file.  */
#define HAVE_RPC_PMAP_CLNT_H 1

/* Define if you have the <rpc/pmap_prot.h> header file.  */
#define HAVE_RPC_PMAP_PROT_H 1

/* Define if you have the <rpc/rpc.h> header file.  */
#define HAVE_RPC_RPC_H 1

/* Define if you have the <rpc/types.h> header file.  */
#define HAVE_RPC_TYPES_H 1

/* Define if you have the <rpc/xdr.h> header file.  */
#define HAVE_RPC_XDR_H 1

/* Define if you have the <rpcsvc/mount.h> header file.  */
#define HAVE_RPCSVC_MOUNT_H 1

/* Define if you have the <rpcsvc/mountv3.h> header file.  */
/* #undef HAVE_RPCSVC_MOUNTV3_H */

/* Define if you have the <rpcsvc/nfs_prot.h> header file.  */
#define HAVE_RPCSVC_NFS_PROT_H 1

/* Define if you have the <rpcsvc/nis.h> header file.  */
#define HAVE_RPCSVC_NIS_H 1

/* Define if you have the <rpcsvc/yp_prot.h> header file.  */
#define HAVE_RPCSVC_YP_PROT_H 1

/* Define if you have the <rpcsvc/ypclnt.h> header file.  */
#define HAVE_RPCSVC_YPCLNT_H 1

/* Define if you have the <setjmp.h> header file.  */
#define HAVE_SETJMP_H 1

/* Define if you have the <signal.h> header file.  */
#define HAVE_SIGNAL_H 1

/* Define if you have the <socketbits.h> header file.  */
/* #undef HAVE_SOCKETBITS_H */

/* Define if you have the <statbuf.h> header file.  */
/* #undef HAVE_STATBUF_H */

/* Define if you have the <stdarg.h> header file.  */
#define HAVE_STDARG_H 1

/* Define if you have the <stdio.h> header file.  */
#define HAVE_STDIO_H 1

/* Define if you have the <stdlib.h> header file.  */
#define HAVE_STDLIB_H 1

/* Define if you have the <string.h> header file.  */
#define HAVE_STRING_H 1

/* Define if you have the <strings.h> header file.  */
#define HAVE_STRINGS_H 1

/* Define if you have the <sys/config.h> header file.  */
/* #undef HAVE_SYS_CONFIG_H */

/* Define if you have the <sys/dg_mount.h> header file.  */
/* #undef HAVE_SYS_DG_MOUNT_H */

/* Define if you have the <sys/dir.h> header file.  */
/* #undef HAVE_SYS_DIR_H */

/* Define if you have the <sys/errno.h> header file.  */
#define HAVE_SYS_ERRNO_H 1

/* Define if you have the <sys/file.h> header file.  */
#define HAVE_SYS_FILE_H 1

/* Define if you have the <sys/fs/autofs.h> header file.  */
/* #undef HAVE_SYS_FS_AUTOFS_H */

/* Define if you have the <sys/fs/autofs_prot.h> header file.  */
/* #undef HAVE_SYS_FS_AUTOFS_PROT_H */

/* Define if you have the <sys/fs/cachefs_fs.h> header file.  */
/* #undef HAVE_SYS_FS_CACHEFS_FS_H */

/* Define if you have the <sys/fs/efs_clnt.h> header file.  */
/* #undef HAVE_SYS_FS_EFS_CLNT_H */

/* Define if you have the <sys/fs/nfs.h> header file.  */
/* #undef HAVE_SYS_FS_NFS_H */

/* Define if you have the <sys/fs/nfs/mount.h> header file.  */
/* #undef HAVE_SYS_FS_NFS_MOUNT_H */

/* Define if you have the <sys/fs/nfs/nfs_clnt.h> header file.  */
/* #undef HAVE_SYS_FS_NFS_NFS_CLNT_H */

/* Define if you have the <sys/fs/nfs_clnt.h> header file.  */
/* #undef HAVE_SYS_FS_NFS_CLNT_H */

/* Define if you have the <sys/fs/pc_fs.h> header file.  */
/* #undef HAVE_SYS_FS_PC_FS_H */

/* Define if you have the <sys/fs/tmp.h> header file.  */
/* #undef HAVE_SYS_FS_TMP_H */

/* Define if you have the <sys/fs/ufs_mount.h> header file.  */
/* #undef HAVE_SYS_FS_UFS_MOUNT_H */

/* Define if you have the <sys/fs/xfs_clnt.h> header file.  */
/* #undef HAVE_SYS_FS_XFS_CLNT_H */

/* Define if you have the <sys/fs_types.h> header file.  */
/* #undef HAVE_SYS_FS_TYPES_H */

/* Define if you have the <sys/fsid.h> header file.  */
/* #undef HAVE_SYS_FSID_H */

/* Define if you have the <sys/fstyp.h> header file.  */
/* #undef HAVE_SYS_FSTYP_H */

/* Define if you have the <sys/ioctl.h> header file.  */
#define HAVE_SYS_IOCTL_H 1

/* Define if you have the <sys/lock.h> header file.  */
#define HAVE_SYS_LOCK_H 1

/* Define if you have the <sys/machine.h> header file.  */
/* #undef HAVE_SYS_MACHINE_H */

/* Define if you have the <sys/mbuf.h> header file.  */
/* #define HAVE_SYS_MBUF_H 1 XXX NO NO NO!!! */

/* Define if you have the <sys/mntctl.h> header file.  */
/* #undef HAVE_SYS_MNTCTL_H */

/* Define if you have the <sys/mntent.h> header file.  */
/* #undef HAVE_SYS_MNTENT_H */

/* Define if you have the <sys/mnttab.h> header file.  */
/* #undef HAVE_SYS_MNTTAB_H */

/* Define if you have the <sys/mount.h> header file.  */
#define HAVE_SYS_MOUNT_H 1

/* Define if you have the <sys/ndir.h> header file.  */
/* #undef HAVE_SYS_NDIR_H */

/* Define if you have the <sys/netconfig.h> header file.  */
/* #undef HAVE_SYS_NETCONFIG_H */

/* Define if you have the <sys/param.h> header file.  */
#define HAVE_SYS_PARAM_H 1

/* Define if you have the <sys/pathconf.h> header file.  */
/* #undef HAVE_SYS_PATHCONF_H */

/* Define if you have the <sys/proc.h> header file.  */
#define HAVE_SYS_PROC_H 1

/* Define if you have the <sys/resource.h> header file.  */
#define HAVE_SYS_RESOURCE_H 1

/* Define if you have the <sys/sema.h> header file.  */
/* #undef HAVE_SYS_SEMA_H */

/* Define if you have the <sys/signal.h> header file.  */
#define HAVE_SYS_SIGNAL_H 1

/* Define if you have the <sys/socket.h> header file.  */
#define HAVE_SYS_SOCKET_H 1

/* Define if you have the <sys/sockio.h> header file.  */
#define HAVE_SYS_SOCKIO_H 1

/* Define if you have the <sys/stat.h> header file.  */
#define HAVE_SYS_STAT_H 1

/* Define if you have the <sys/statfs.h> header file.  */
/* #undef HAVE_SYS_STATFS_H */

/* Define if you have the <sys/syscall.h> header file.  */
#define HAVE_SYS_SYSCALL_H 1

/* Define if you have the <sys/syslimits.h> header file.  */
#define HAVE_SYS_SYSLIMITS_H 1

/* Define if you have the <sys/syslog.h> header file.  */
#define HAVE_SYS_SYSLOG_H 1

/* Define if you have the <sys/time.h> header file.  */
#define HAVE_SYS_TIME_H 1

/* Define if you have the <sys/tiuser.h> header file.  */
/* #undef HAVE_SYS_TIUSER_H */

/* Define if you have the <sys/types.h> header file.  */
#define HAVE_SYS_TYPES_H 1

/* Define if you have the <sys/ucred.h> header file.  */
#define HAVE_SYS_UCRED_H 1

/* Define if you have the <sys/uio.h> header file.  */
#define HAVE_SYS_UIO_H 1

/* Define if you have the <sys/utsname.h> header file.  */
#define HAVE_SYS_UTSNAME_H 1

/* Define if you have the <sys/vfs.h> header file.  */
/* #undef HAVE_SYS_VFS_H */

/* Define if you have the <sys/vmount.h> header file.  */
/* #undef HAVE_SYS_VMOUNT_H */

/* Define if you have the <sys/vnode.h> header file.  */
#define HAVE_SYS_VNODE_H 1

/* Define if you have the <syslog.h> header file.  */
#define HAVE_SYSLOG_H 1

/* Define if you have the <time.h> header file.  */
#define HAVE_TIME_H 1

/* Define if you have the <tiuser.h> header file.  */
/* #undef HAVE_TIUSER_H */

/* Define if you have the <tmpfs/tmp.h> header file.  */
/* #undef HAVE_TMPFS_TMP_H */

/* Define if you have the <ufs/ufs/ufsmount.h> header file.  */
#define HAVE_UFS_UFS_UFSMOUNT_H 1

/* Define if you have the <ufs/ufs_mount.h> header file.  */
#define HAVE_UFS_UFS_MOUNT_H 1

/* Define if you have the <unistd.h> header file.  */
#define HAVE_UNISTD_H 1

/* Define if you have the <varargs.h> header file.  */
#define HAVE_VARARGS_H 1

/* Define if you have the <vfork.h> header file.  */
/* #undef HAVE_VFORK_H */

/* Define if you have the gdbm library (-lgdbm).  */
/* #undef HAVE_LIBGDBM */

/* Define if you have the malloc library (-lmalloc).  */
/* #undef HAVE_LIBMALLOC */

/* Define if you have the mapmalloc library (-lmapmalloc).  */
/* #undef HAVE_LIBMAPMALLOC */

/* Define if you have the nsl library (-lnsl).  */
/* #undef HAVE_LIBNSL */

/* Define if you have the rpc library (-lrpc).  */
/* #undef HAVE_LIBRPC */

/* Define if you have the rpcsvc library (-lrpcsvc).  */
#define HAVE_LIBRPCSVC 1

/* Define to `int' if <sys/types.h> does not define. */
/* #undef pid_t */

/* Define to `int' if <sys/types.h> does not define. */
/* #undef mode_t */

/* Define to `int' if <sys/types.h> does not define. */
/* #undef pid_t */

/* Define to `unsigned' if <sys/types.h> does not define. */
/* #undef size_t */

/* Define to `long' if <sys/types.h> does not define. */
/* #undef time_t */


/**************************************************************************/
/*** Everything below this line is part of the "BOTTOM" of acconfig.h.	***/
/**************************************************************************/

/*
 * Existence of external definitions.
 */

/* does extern definition for sys_errlist[] exist? */
#define HAVE_EXTERN_SYS_ERRLIST 1

/* does extern definition for optarg exist? */
#define HAVE_EXTERN_OPTARG 1

/* does extern definition for clnt_spcreateerror() exist? */
#define HAVE_EXTERN_CLNT_SPCREATEERROR 1

/* does extern definition for clnt_sperrno() exist? */
#define HAVE_EXTERN_CLNT_SPERRNO 1

/* does extern definition for free() exist? */
#define HAVE_EXTERN_FREE 1

/* does extern definition for get_myaddress() exist? */
#define HAVE_EXTERN_GET_MYADDRESS 1

/* does extern definition for getccent() (hpux) exist? */
/* #undef HAVE_EXTERN_GETCCENT */

/* does extern definition for getdomainname() exist? */
#define HAVE_EXTERN_GETDOMAINNAME 1

/* does extern definition for gethostname() exist? */
#define HAVE_EXTERN_GETHOSTNAME 1

/* does extern definition for getlogin() exist? */
#define HAVE_EXTERN_GETLOGIN 1

/* does extern definition for gettablesize() exist? */
/* #undef HAVE_EXTERN_GETTABLESIZE */

/* does extern definition for getpagesize() exist? */
#define HAVE_EXTERN_GETPAGESIZE 1

/* does extern definition for innetgr() exist? */
#define HAVE_EXTERN_INNETGR

/* does extern definition for mkstemp() exist? */
#define HAVE_EXTERN_MKSTEMP 1

/* does extern definition for sbrk() exist? */
#define HAVE_EXTERN_SBRK 1

/* does extern definition for seteuid() exist? */
#define HAVE_EXTERN_SETEUID 1

/* does extern definition for setitimer() exist? */
#define HAVE_EXTERN_SETITIMER 1

/* does extern definition for strcasecmp() exist? */
#define HAVE_EXTERN_STRCASECMP 1

/* does extern definition for strdup() exist? */
#define HAVE_EXTERN_STRDUP 1

/* does extern definition for strstr() exist? */
#define HAVE_EXTERN_STRSTR 1

/* does extern definition for usleep() exist? */
#define HAVE_EXTERN_USLEEP 1

/* does extern definition for wait3() exist? */
#define HAVE_EXTERN_WAIT3 1

/* does extern definition for vsnprintf() exist? */
#define HAVE_EXTERN_VSNPRINTF 1

/* does extern definition for xdr_opaque_auth() exist? */
#define HAVE_EXTERN_XDR_OPAQUE_AUTH 1

/****************************************************************************/
/*** INCLUDE localconfig.h if it exists, to allow users to make some      ***/
/*** compile time configuration changes.                                  ***/
/****************************************************************************/
/* does a local configuration file exist? */
/* #undef HAVE_LOCALCONFIG_H */
#ifdef HAVE_LOCALCONFIG_H
# include <localconfig.h>
#endif /* HAVE_LOCALCONFIG_H */

#endif /* not _CONFIG_H */

/*
 * Local Variables:
 * mode: c
 * End:
 */

/* End of am-utils-6.x config.h file */
