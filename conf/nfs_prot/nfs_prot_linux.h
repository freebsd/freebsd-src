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
 * File: am-utils/conf/nfs_prot/nfs_prot_linux.h
 *
 */

#ifndef _AMU_NFS_PROT_H
#define _AMU_NFS_PROT_H

#ifdef HAVE_RPCSVC_NFS_PROT_H
# include <rpcsvc/nfs_prot.h>
#endif /* HAVE_RPCSVC_NFS_PROT_H */

/*
 * Hard-code support for some file systems so the built amd
 * binary can always run them.  Also, this helps detection of iso9660
 * file system for which the module isn't named as the file system mount
 * name.
 */
#include <linux/version.h>
#ifndef KERNEL_VERSION
# define KERNEL_VERSION(a,b,c) (((a) << 16) + ((b) << 8) + (c))
#endif /* not KERNEL_VERSION */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,0)
# ifndef MS_BIND
#  define MS_BIND 4096
# endif /* not MS_BIND */

# ifndef MNTTYPE_LOFS
#  define MNTTYPE_LOFS "bind"
# endif /* not MNTTYPE_LOFS */
#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,0) */

#ifndef MNTTYPE_ISO9660
# define MNTTYPE_ISO9660 "iso9660"
#endif /* not MNTTYPE_ISO9660 */

#ifndef FHSIZE
# define FHSIZE 32
#endif /* not FHSIZE */
#ifndef FHSIZE3
# define FHSIZE3 64
#endif /* not FHSIZE3 */

#ifdef HAVE_FS_NFS3
#ifndef MNTTYPE_NFS3
# define MNTTYPE_NFS3	"nfs"
#endif /* not MNTTYPE_NFS3 */
#endif /* HAVE_FS_NFS3 */

/*
 * These two force/lazy unmount flags are sometimes missing from some linux
 * systems' headers.
 */
#ifdef HAVE_UMOUNT2
# ifndef MNT_FORCE
#  define MNT_FORCE	0x1	/* from <sys/mount.h> */
# endif /* not MNT_FORCE */
# ifndef MNT_DETACH
#  define MNT_DETACH	0x2	/* from kernel <linux/fs.h> */
# endif /* not MNT_DETACH */
#endif /* HAVE_UMOUNT2 */

/* XXX: hack until we have a better way to detect /dev/loop devices */
#ifdef HAVE_LINUX_LOOP_H
# define HAVE_LOOP_DEVICE
extern char *setup_loop_device(const char *file);
extern int delete_loop_device(const char *device);
#endif /* HAVE_LINUX_LOOP_H */


/*
 * MACROS:
 */
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
#define fhsize		root.size
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

/* Autofs trick */
typedef int autofs_args_t;

/*
 * We use our own definitions here, because the definitions in the
 * kernel change the API (though not the ABI) *way* too often.
 */
#undef nfs_args_t
struct nfs2_fh {
  char		data[FHSIZE];
};

struct nfs3_fh {
  u_short	size;
  u_char	data[FHSIZE3];
};

struct nfs_args {
  int			version;	/* 1 */
  int			fd;		/* 1 */
  struct nfs2_fh	old_root;	/* 1 */
  int			flags;		/* 1 */
  int			rsize;		/* 1 */
  int			wsize;		/* 1 */
  int			timeo;		/* 1 */
  int			retrans;	/* 1 */
  int			acregmin;	/* 1 */
  int			acregmax;	/* 1 */
  int			acdirmin;	/* 1 */
  int			acdirmax;	/* 1 */
  struct sockaddr_in	addr;		/* 1 */
  char			hostname[256];	/* 1 */
  int			namlen;		/* 2 */
  unsigned int		bsize;		/* 3 */
  struct nfs3_fh	root;		/* 4 */
  int			pseudoflavor;	/* 5 */
  char			context[256 + 1];/* 6 */
};
typedef struct nfs_args nfs_args_t;

#define	NFS4_MOUNT_VERSION	1

struct nfs_string {
  unsigned int len;
  char *data;
};

struct nfs4_args {
  int 			version;	/* 1 */
  int			flags;		/* 1 */
  int			rsize;		/* 1 */
  int			wsize;		/* 1 */
  int			timeo;		/* 1 */
  int			retrans;	/* 1 */
  int			acregmin;	/* 1 */
  int			acregmax;	/* 1 */
  int			acdirmin;	/* 1 */
  int			acdirmax;	/* 1 */
  struct nfs_string	client_addr;	/* 1 */
  struct nfs_string	mnt_path;	/* 1 */
  struct nfs_string	hostname;	/* 1 */
  unsigned int		host_addrlen;	/* 1 */
  struct sockaddr      *host_addr;	/* 1 */
  int 			proto;		/* 1 */
  int 			auth_flavourlen;/* 1 */
  int 		       *auth_flavours;	/* 1 */
};

typedef struct nfs4_args nfs4_args_t;

/* HACK: these are not defined on Linux but we still need them
 * internally.  We have to be able to differentiate between having a
 * timeout value set to zero and not having the option at all. These
 * flags are masked off before passing the nfs_args structure to
 * mount2() */

#define MNT2_NFS_OPT_ACREGMIN 0x10000
#define MNT2_NFS_OPT_ACREGMAX 0x20000
#define MNT2_NFS_OPT_ACDIRMIN 0x40000
#define MNT2_NFS_OPT_ACDIRMAX 0x80000
#define MNT2_NFS_OPT_FLAGMASK 0xFFFF

/*
 * Missing definitions on redhat alpha linux
 */
#ifdef _SELECTBITS_H
# ifndef __FD_ZERO
/* This line MUST be split!  Otherwise m4 will not change it.  */
#  define __FD_ZERO(set)  \
  ((void) memset ((__ptr_t) (set), 0, sizeof(fd_set)))
# endif /* not __FD_ZERO */
# ifndef __FD_SET
#  define __FD_SET(d, set)        ((set)->fds_bits[__FDELT(d)] |= __FDMASK(d))
# endif /* not __FD_SET */
# ifndef __FD_CLR
#  define __FD_CLR(d, set)        ((set)->fds_bits[__FDELT(d)] &= ~__FDMASK(d))
# endif /* not __FD_CLR */
# ifndef __FD_ISSET
#  define __FD_ISSET(d, set)      ((set)->fds_bits[__FDELT(d)] & __FDMASK(d))
# endif /* not __FD_ISSET */

#endif /* _SELECTBITS_H */

/* turn off this (b/c of hlfsd) */
#undef HAVE_RPC_AUTH_DES_H

extern int linux_version_code();
/* use a private mapper from errno's to NFS errors */
extern int linux_nfs_error(int e);
#define nfs_error(e)	linux_nfs_error(e)

#endif /* not _AMU_NFS_PROT_H */
