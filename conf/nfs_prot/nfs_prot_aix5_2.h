/*
 * Copyright (c) 1997-2014 Erez Zadok
 * Copyright (c) 1990 Jan-Simon Pendry
 * Copyright (c) 1990 Imperial College of Science, Technology & Medicine
 * Copyright (c) 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Jan-Simon Pendry at Imperial College, London.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *
 * File: am-utils/conf/nfs_prot/nfs_prot_aix5_2.h
 *
 */

#ifndef _AMU_NFS_PROT_H
#define _AMU_NFS_PROT_H


/*
 * AIX 5.2 has a different aix5_nfs_args structure, hence the separate header.
 */

#ifdef HAVE_RPCSVC_NFS_PROT_H
# include <rpcsvc/nfs_prot.h>
#endif /* HAVE_RPCSVC_NFS_PROT_H */
#ifdef HAVE_NFS_NFSV2_H
# include <nfs/nfsv2.h>
#endif /* HAVE_NFS_NFSV2_H */
#ifdef HAVE_NFS_RPCV2_H
# include <nfs/rpcv2.h>
#endif /* HAVE_NFS_RPCV2_H */
#ifdef HAVE_SYS_FS_NFS_H
# include <sys/fs/nfs.h>
#endif /* HAVE_SYS_FS_NFS_H */
#ifdef HAVE_RPCSVC_MOUNT_H
/*
 * AIX 5.2 wants 'struct pathcnf', but I couldn't find its definition
 * anywhere.  Luckily, amd doesn't need the size of this structure in
 * any other structure that it uses.  So we sidestep it for now.
 */
struct pathcnf;
# include <rpcsvc/mount.h>
#endif /* HAVE_RPCSVC_MOUNT_H */
#ifdef HAVE_SYS_VFS_H
/* AIX 5.3 (ppc) wants definition of kernel-specific structure */
struct thread_credentials;
#endif /* HAVE_SYS_VFS_H */

/*
 * MACROS
 */

#ifndef MNTPATHLEN
# define MNTPATHLEN 1024
#endif /* not MNTPATHLEN */
#ifndef MNTNAMLEN
# define MNTNAMLEN 255
#endif /* not MNTNAMLEN */

/* compatible macro name with other OSs */
#ifdef UVMNT_FORCE
# define MS_FORCE UVMNT_FORCE
#endif /* UVMNT_FORCE */

/********************************************************************************/
/*
 * NFS mount option flags (out of mount.h)
 * Maybe one day IBM will include them in system headers.
 */
#ifndef MNTOPT_ACDIRMAX
# define MNTOPT_ACDIRMAX "acdirmax" /* max ac timeout for dirs (sec) */
# define MNTOPT_ACDIRMIN "acdirmin" /* min ac timeout for dirs (sec) */
# define MNTOPT_ACREGMAX "acregmax" /* max ac timeout for reg files (sec) */
# define MNTOPT_ACREGMIN "acregmin" /* min ac timeout for reg files (sec) */
# define MNTOPT_ACTIMEO  "actimeo" /* attr cache timeout (sec) */
# define MNTOPT_BG       "bg" /* mount op in background if 1st attempt fails */
# define MNTOPT_FASTATTR "fastattr" /* no sync_vp in nfs3_getattr */
# define MNTOPT_FG       "fg" /* mount op in fg if 1st attempt fails, default */
# define MNTOPT_GRPID    "grpid" /* SysV-compatible group-id on create */
# define MNTOPT_HARD     "hard"  /* hard mount (default) */
# define MNTOPT_INTR     "intr"  /* allow interrupts on hard mount */
# define MNTOPT_NOAC     "noac"  /* don't cache file attributes */
# define MNTOPT_NOACL	"noacl"  /* don't read acl's from server - default */
# define MNTOPT_ACL	"acl"  /* read acl's from server means we load ksec */
# define MNTOPT_NOAUTO   "noauto"/* hide entry from mount -a */
# define MNTOPT_NOCTO    "nocto" /* no "close to open" attr consistency */
# define MNTOPT_NODEV    "nodev" /*don't allow opening of devices accross mount*/
# define MNTOPT_NOINTR   "nointr" /* don't allow interrupts on hard mounts */
# define MNTOPT_NOQUOTA  "noquota" /* don't check quotas */
# define MNTOPT_NOSUID   "nosuid" /* no set uid allowed */
# define MNTOPT_BSY	"bsy"
# define MNTOPT_PORT     "port"  /* server IP port number */
# define MNTOPT_POSIX    "posix" /* ask for static pathconf values from mountd */
# define MNTOPT_QUOTA    "quota" /* quotas */
# define MNTOPT_RETRANS  "retrans" /* set number of request retries */
# define MNTOPT_RETRYS   "retry" /* # of times mount is attempted, def=10000*/
# define MNTOPT_RMNT     "remount" /* remount to rw if mode ro */
# define MNTOPT_RO       "ro"    /* read only */
# define MNTOPT_RSIZE    "rsize" /* set read size (bytes) */
# define MNTOPT_RW       "rw"    /* read/write */
# define MNTOPT_SECURE   "secure"/* use secure RPC for NFS */
# define MNTOPT_SHORTDEV "shortdev" /* server dev */
# define MNTOPT_SOFT     "soft"  /* soft mount */
# define MNTOPT_TIMEO    "timeo" /* set initial timeout (1/10 sec) */
# define MNTOPT_WSIZE    "wsize" /* set write size (bytes) */
# define MNTOPT_VERS "vers"      /* protocol version number indicator */
# define MNTOPT_PROTO    "proto"     /* protocol network_id indicator */
# define MNTOPT_LLOCK    "llock"     /* Local locking (no lock manager) */
# define MNTOPT_BIODS    "biods"     /* max biods per mount */
# define MNTOPT_EXTATTR  "extraattr" /* extended attribute usage */
# define MNTOPT_COMBEHND "combehind" /* extended attribute usage */
# define MNTOPT_NUMCLUST "numclust"  /* max numclust per mount */
# define MNTOPT_NODIRCACHE   "nodircache"    /* No readdir cache */

# define NFSMNT_NOACL    0x0     /* turn off acl support (not supported) */
# define NFSMNT_SHORTDEV 0x0     /* server does not support 32-bit device no.
                                   (not supported) */
# define NFSMNT_SOFT	0x001	/* soft mount (hard is default) */
# define NFSMNT_WSIZE	0x002	/* set write size */
# define NFSMNT_RSIZE	0x004	/* set read size */
# define NFSMNT_TIMEO	0x008	/* set initial timeout */
# define NFSMNT_RETRANS	0x010	/* set number of request retrys */
# define NFSMNT_HOSTNAME	0x020	/* set hostname for error printf */
# define NFSMNT_INT	0x040	/* allow interrupts on hard mount */
# define NFSMNT_NOAC	0x080	/* don't cache attributes */
# define NFSMNT_ACREGMIN	0x0100	/* set min secs for file attr cache */
# define NFSMNT_ACREGMAX	0x0200	/* set max secs for file attr cache */
# define NFSMNT_ACDIRMIN	0x0400	/* set min secs for dir attr cache */
# define NFSMNT_ACDIRMAX	0x0800	/* set max secs for dir attr cache */
# define NFSMNT_SECURE	0x1000	/* secure mount */
# define NFSMNT_NOCTO	0x2000	/* no close-to-open consistency */
# define NFSMNT_KNCONF       0x4000  /* transport's knetconfig structure */
# define NFSMNT_GRPID        0x8000  /* System V-style gid inheritance */
# define NFSMNT_RPCTIMESYNC  0x10000 /* use RPC to do secure NFS time sync */
# define NFSMNT_KERBEROS     0x20000 /* use kerberos credentials */
# define NFSMNT_POSIX        0x40000 /* static pathconf kludge info */
# define NFSMNT_LLOCK    0x80000 /* Local locking (no lock manager) */
# define NFSMNT_ACL	0x100000	/* turn on acl support */
# define NFSMNT_BIODS        0x200000 /* number of biods per mount */
# define NFSMNT_EXTATTR      0x400000 /* extended attribute usage */
# define NFSMNT_FASTATTR     0x800000 /* no sync_vp in nfs3_getattr */
# define NFSMNT_COMBEHND     0x1000000 /* allow commit behind */
# define NFSMNT_NUMCLUST     0x2000000 /* number of cluster per mount */
# define NFSMNT_NODIRCACHE   0x4000000 /* No readdir cache */
#endif /* not MNTOPT_ACDIRMAX */
/********************************************************************************/

/* map field names */
#define ex_dir ex_name
#define gr_name g_name
#define gr_next g_next
#define ml_directory ml_path
#define ml_hostname ml_name
#define ml_next ml_nxt

#define	dr_drok_u	diropres
#define ca_attributes	attributes
#define ca_where	where
#define da_fhandle	dir
#define da_name		name
#define dl_entries	entries
#define dl_eof		eof
#define dr_status	status
#define dr_u		diropres_u
#define drok_attributes	attributes
#define drok_fhandle	file
#define fh_data		data
#define la_fhandle	from
#define la_to		to
#define na_atime	atime
#define na_blocks	blocks
#define na_blocksize	blocksize
#define na_ctime	ctime
#define na_fileid	fileid
#define na_fsid		fsid
#define na_gid		gid
#define na_mode		mode
#define na_mtime	mtime
#define na_nlink	nlink
#define na_rdev		rdev
#define na_size		size
#define na_type		type
#define na_uid		uid
#define ne_cookie	cookie
#define ne_fileid	fileid
#define ne_name		name
#define ne_nextentry	nextentry
#define ns_attr_u	attributes
#define ns_status	status
#define ns_u		attrstat_u
#define nt_seconds	seconds
#define nt_useconds	useconds
#define ra_count	count
#define ra_fhandle	file
#define ra_offset	offset
#define ra_totalcount	totalcount
#define raok_attributes	attributes
#define raok_len_u	data_len
#define raok_u		data
#define raok_val_u	data_val
#define rda_cookie	cookie
#define rda_count	count
#define rda_fhandle	dir
#define rdr_reply_u	reply
#define rdr_status	status
#define rdr_u		readdirres_u
#define rlr_data_u	data
#define rlr_status	status
#define rlr_u		readlinkres_u
#define rna_from	from
#define rna_to		to
#define rr_reply_u	reply
#define rr_status	status
#define rr_u		readres_u
#define sa_atime	atime
#define sa_gid		gid
#define sa_mode		mode
#define sa_mtime	mtime
#define sa_size		size
#define sa_uid		uid
#define sag_attributes	attributes
#define sag_fhandle	file
#define sfr_reply_u	reply
#define sfr_status	status
#define sfr_u		statfsres_u
#define sfrok_bavail	bavail
#define sfrok_bfree	bfree
#define sfrok_blocks	blocks
#define sfrok_bsize	bsize
#define sfrok_tsize	tsize
#define sla_attributes	attributes
#define sla_from	from
#define sla_to		to
#define wra_beginoffset	beginoffset
#define wra_fhandle	file
#define wra_len_u	data_len
#define wra_offset	offset
#define wra_totalcount	totalcount
#define wra_u		data
#define wra_val_u	data_val


/*
 * TYPEDEFS:
 */
typedef char *dirpath;
typedef char *name;
typedef struct exports *exports;
typedef struct exports exportnode;
typedef struct groups *groups;
typedef struct groups groupnode;
typedef struct mountlist *mountlist;

/*
 * If these definitions fail to compile on your AIX 5.2 system, be
 * sure you install all of the necessary header files.
 */
typedef attrstat	nfsattrstat;
typedef createargs	nfscreateargs;
typedef dirlist		nfsdirlist;
typedef diropargs	nfsdiropargs;
typedef diropokres	nfsdiropokres;
typedef diropres	nfsdiropres;
typedef entry		nfsentry;
typedef fattr		nfsfattr;
typedef ftype		nfsftype;
typedef linkargs	nfslinkargs;
typedef readargs	nfsreadargs;
typedef readdirargs	nfsreaddirargs;
typedef readdirres	nfsreaddirres;
typedef readlinkres	nfsreadlinkres;
typedef readokres	nfsreadokres;
typedef readres		nfsreadres;
typedef renameargs	nfsrenameargs;
typedef sattr		nfssattr;
typedef sattrargs	nfssattrargs;
typedef statfsokres	nfsstatfsokres;
typedef statfsres	nfsstatfsres;
typedef symlinkargs	nfssymlinkargs;
typedef writeargs	nfswriteargs;


/*
 * EXTERNALS:
 */


/*
 * STRUCTURES:
 */

/*
 * AIX 5.2 has NFS V3, but it is undefined in the header files.
 * so I define everything that's needed for NFS V3 here.
 */
#ifdef MNT_NFS3

struct aix5_nfs_args {
  struct sockaddr_in addr;	/* file server address */
  struct sockaddr_in *syncaddr;	/* secure NFS time sync addr */
  int proto;
  char *hostname;		/* server's hostname */
  char *netname;		/* server's netname */
  caddr_t fh;			/* File handle to be mounted */
  int flags;			/* flags */
  int wsize;			/* write size in bytes */
  int rsize;			/* read size in bytes */
  int timeo;			/* initial timeout in .1 secs */
  int retrans;			/* times to retry send */
  int acregmin;			/* attr cache file min secs */
  int acregmax;			/* attr cache file max secs */
  int acdirmin;			/* attr cache dir min secs */
  int acdirmax;			/* attr cache dir max secs */
  struct ppathcnf *pathconf;	/* static pathconf kludge */
  int biods;			/* biods per mount */
  int numclust;			/* numclust per mount */
};

#endif /* MNT_NFS3 */

/*
 **************************************************************************
 * AIX 5.2's autofs is not ported or tested yet...
 * For now, undefine it or define dummy entries.
 **************************************************************************
 */
#ifdef MNT_AUTOFS
# undef MNT_AUTOFS
#endif /* MNT_AUTOFS */
#ifdef HAVE_FS_AUTOFS
# undef HAVE_FS_AUTOFS
#endif /* HAVE_FS_AUTOFS */

/*
 * EXTERNALS:
 */
extern bool_t xdr_groups(XDR *xdrs, groups *objp);
extern char *yperr_string (int incode);

#endif /* not _AMU_NFS_PROT_H */
