/*	$NetBSD: nfs.c,v 1.2 1998/01/24 12:43:09 drochner Exp $	*/

/*-
 *  Copyright (c) 1993 John Brezak
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *  1. Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *  3. The name of the author may not be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR `AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <string.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>

#include "rpcv2.h"
#include "nfsv2.h"

#include "stand.h"
#include "net.h"
#include "netif.h"
#include "rpc.h"

#define NFS_DEBUGxx

#define NFSREAD_SIZE 1024

/* Define our own NFS attributes without NQNFS stuff. */
#ifdef OLD_NFSV2
struct nfsv2_fattrs {
	n_long	fa_type;
	n_long	fa_mode;
	n_long	fa_nlink;
	n_long	fa_uid;
	n_long	fa_gid;
	n_long	fa_size;
	n_long	fa_blocksize;
	n_long	fa_rdev;
	n_long	fa_blocks;
	n_long	fa_fsid;
	n_long	fa_fileid;
	struct nfsv2_time fa_atime;
	struct nfsv2_time fa_mtime;
	struct nfsv2_time fa_ctime;
};

struct nfs_read_args {
	u_char	fh[NFS_FHSIZE];
	n_long	off;
	n_long	len;
	n_long	xxx;			/* XXX what's this for? */
};

/* Data part of nfs rpc reply (also the largest thing we receive) */
struct nfs_read_repl {
	n_long	errno;
	struct	nfsv2_fattrs fa;
	n_long	count;
	u_char	data[NFSREAD_SIZE];
};

#ifndef NFS_NOSYMLINK
struct nfs_readlnk_repl {
	n_long	errno;
	n_long	len;
	char	path[NFS_MAXPATHLEN];
};
#endif

struct nfs_readdir_args {
	u_char	fh[NFS_FHSIZE];
	n_long	cookie;
	n_long	count;
};

struct nfs_readdir_data {
	n_long	fileid;
	n_long	len;
	char	name[0];
};

struct nfs_readdir_off {
	n_long	cookie;
	n_long	follows;
};

struct nfs_iodesc {
	struct	iodesc	*iodesc;
	off_t	off;
	u_char	fh[NFS_FHSIZE];
	struct nfsv2_fattrs fa;	/* all in network order */
};
#else	/* !OLD_NFSV2 */

/* NFSv3 definitions */
#define	NFS_V3MAXFHSIZE		64
#define	NFS_VER3		3
#define	RPCMNT_VER3		3
#define	NFSPROCV3_LOOKUP	3
#define	NFSPROCV3_READLINK	5
#define	NFSPROCV3_READ		6
#define	NFSPROCV3_READDIR	16

typedef struct {
	uint32_t val[2];
} n_quad;

struct nfsv3_time {
	uint32_t nfs_sec;
	uint32_t nfs_nsec;
};

struct nfsv3_fattrs {
	uint32_t fa_type;
	uint32_t fa_mode;
	uint32_t fa_nlink;
	uint32_t fa_uid;
	uint32_t fa_gid;
	n_quad fa_size;
	n_quad fa_used;
	n_quad fa_rdev;
	n_quad fa_fsid;
	n_quad fa_fileid;
	struct nfsv3_time fa_atime;
	struct nfsv3_time fa_mtime;
	struct nfsv3_time fa_ctime;
};

/*
 * For NFSv3, the file handle is variable in size, so most fixed sized
 * structures for arguments won't work. For most cases, a structure
 * that starts with any fixed size section is followed by an array
 * that covers the maximum size required.
 */
struct nfsv3_readdir_repl {
	uint32_t errno;
	uint32_t ok;
	struct nfsv3_fattrs fa;
	uint32_t cookiev0;
	uint32_t cookiev1;
};

struct nfsv3_readdir_entry {
	uint32_t follows;
	uint32_t fid0;
	uint32_t fid1;
	uint32_t len;
	uint32_t nameplus[0];
};

struct nfs_iodesc {
	struct iodesc *iodesc;
	off_t off;
	uint32_t fhsize;
	u_char fh[NFS_V3MAXFHSIZE];
	struct nfsv3_fattrs fa;	/* all in network order */
};
#endif	/* OLD_NFSV2 */

/*
 * XXX interactions with tftp? See nfswrapper.c for a confusing
 *     issue.
 */
int		nfs_open(const char *path, struct open_file *f);
static int	nfs_close(struct open_file *f);
static int	nfs_read(struct open_file *f, void *buf, size_t size, size_t *resid);
static int	nfs_write(struct open_file *f, void *buf, size_t size, size_t *resid);
static off_t	nfs_seek(struct open_file *f, off_t offset, int where);
static int	nfs_stat(struct open_file *f, struct stat *sb);
static int	nfs_readdir(struct open_file *f, struct dirent *d);

struct	nfs_iodesc nfs_root_node;

struct fs_ops nfs_fsops = {
	"nfs",
	nfs_open,
	nfs_close,
	nfs_read,
	nfs_write,
	nfs_seek,
	nfs_stat,
	nfs_readdir
};

#ifdef	OLD_NFSV2
/*
 * Fetch the root file handle (call mount daemon)
 * Return zero or error number.
 */
int
nfs_getrootfh(struct iodesc *d, char *path, u_char *fhp)
{
	int len;
	struct args {
		n_long	len;
		char	path[FNAME_SIZE];
	} *args;
	struct repl {
		n_long	errno;
		u_char	fh[NFS_FHSIZE];
	} *repl;
	struct {
		n_long	h[RPC_HEADER_WORDS];
		struct args d;
	} sdata;
	struct {
		n_long	h[RPC_HEADER_WORDS];
		struct repl d;
	} rdata;
	size_t cc;

#ifdef NFS_DEBUG
	if (debug)
		printf("nfs_getrootfh: %s\n", path);
#endif

	args = &sdata.d;
	repl = &rdata.d;

	bzero(args, sizeof(*args));
	len = strlen(path);
	if (len > sizeof(args->path))
		len = sizeof(args->path);
	args->len = htonl(len);
	bcopy(path, args->path, len);
	len = 4 + roundup(len, 4);

	cc = rpc_call(d, RPCPROG_MNT, RPCMNT_VER1, RPCMNT_MOUNT,
	    args, len, repl, sizeof(*repl));
	if (cc == -1) {
		/* errno was set by rpc_call */
		return (errno);
	}
	if (cc < 4)
		return (EBADRPC);
	if (repl->errno)
		return (ntohl(repl->errno));
	bcopy(repl->fh, fhp, sizeof(repl->fh));
	return (0);
}

/*
 * Lookup a file.  Store handle and attributes.
 * Return zero or error number.
 */
int
nfs_lookupfh(struct nfs_iodesc *d, const char *name, struct nfs_iodesc *newfd)
{
	int len, rlen;
	struct args {
		u_char	fh[NFS_FHSIZE];
		n_long	len;
		char	name[FNAME_SIZE];
	} *args;
	struct repl {
		n_long	errno;
		u_char	fh[NFS_FHSIZE];
		struct	nfsv2_fattrs fa;
	} *repl;
	struct {
		n_long	h[RPC_HEADER_WORDS];
		struct args d;
	} sdata;
	struct {
		n_long	h[RPC_HEADER_WORDS];
		struct repl d;
	} rdata;
	ssize_t cc;

#ifdef NFS_DEBUG
	if (debug)
		printf("lookupfh: called\n");
#endif

	args = &sdata.d;
	repl = &rdata.d;

	bzero(args, sizeof(*args));
	bcopy(d->fh, args->fh, sizeof(args->fh));
	len = strlen(name);
	if (len > sizeof(args->name))
		len = sizeof(args->name);
	bcopy(name, args->name, len);
	args->len = htonl(len);
	len = 4 + roundup(len, 4);
	len += NFS_FHSIZE;

	rlen = sizeof(*repl);

	cc = rpc_call(d->iodesc, NFS_PROG, NFS_VER2, NFSPROC_LOOKUP,
	    args, len, repl, rlen);
	if (cc == -1)
		return (errno);		/* XXX - from rpc_call */
	if (cc < 4)
		return (EIO);
	if (repl->errno) {
		/* saerrno.h now matches NFS error numbers. */
		return (ntohl(repl->errno));
	}
	bcopy( repl->fh, &newfd->fh, sizeof(newfd->fh));
	bcopy(&repl->fa, &newfd->fa, sizeof(newfd->fa));
	return (0);
}

#ifndef NFS_NOSYMLINK
/*
 * Get the destination of a symbolic link.
 */
int
nfs_readlink(struct nfs_iodesc *d, char *buf)
{
	struct {
		n_long	h[RPC_HEADER_WORDS];
		u_char fh[NFS_FHSIZE];
	} sdata;
	struct {
		n_long	h[RPC_HEADER_WORDS];
		struct nfs_readlnk_repl d;
	} rdata;
	ssize_t cc;

#ifdef NFS_DEBUG
	if (debug)
		printf("readlink: called\n");
#endif

	bcopy(d->fh, sdata.fh, NFS_FHSIZE);
	cc = rpc_call(d->iodesc, NFS_PROG, NFS_VER2, NFSPROC_READLINK,
		      sdata.fh, NFS_FHSIZE,
		      &rdata.d, sizeof(rdata.d));
	if (cc == -1)
		return (errno);

	if (cc < 4)
		return (EIO);

	if (rdata.d.errno)
		return (ntohl(rdata.d.errno));

	rdata.d.len = ntohl(rdata.d.len);
	if (rdata.d.len > NFS_MAXPATHLEN)
		return (ENAMETOOLONG);

	bcopy(rdata.d.path, buf, rdata.d.len);
	buf[rdata.d.len] = 0;
	return (0);
}
#endif

/*
 * Read data from a file.
 * Return transfer count or -1 (and set errno)
 */
ssize_t
nfs_readdata(struct nfs_iodesc *d, off_t off, void *addr, size_t len)
{
	struct nfs_read_args *args;
	struct nfs_read_repl *repl;
	struct {
		n_long	h[RPC_HEADER_WORDS];
		struct nfs_read_args d;
	} sdata;
	struct {
		n_long	h[RPC_HEADER_WORDS];
		struct nfs_read_repl d;
	} rdata;
	size_t cc;
	long x;
	int hlen, rlen;

	args = &sdata.d;
	repl = &rdata.d;

	bcopy(d->fh, args->fh, NFS_FHSIZE);
	args->off = htonl((n_long)off);
	if (len > NFSREAD_SIZE)
		len = NFSREAD_SIZE;
	args->len = htonl((n_long)len);
	args->xxx = htonl((n_long)0);
	hlen = sizeof(*repl) - NFSREAD_SIZE;

	cc = rpc_call(d->iodesc, NFS_PROG, NFS_VER2, NFSPROC_READ,
	    args, sizeof(*args),
	    repl, sizeof(*repl));
	if (cc == -1) {
		/* errno was already set by rpc_call */
		return (-1);
	}
	if (cc < hlen) {
		errno = EBADRPC;
		return (-1);
	}
	if (repl->errno) {
		errno = ntohl(repl->errno);
		return (-1);
	}
	rlen = cc - hlen;
	x = ntohl(repl->count);
	if (rlen < x) {
		printf("nfsread: short packet, %d < %ld\n", rlen, x);
		errno = EBADRPC;
		return(-1);
	}
	bcopy(repl->data, addr, x);
	return (x);
}

/*
 * Open a file.
 * return zero or error number
 */
int
nfs_open(const char *upath, struct open_file *f)
{
	struct iodesc *desc;
	struct nfs_iodesc *currfd;
	char buf[2 * NFS_FHSIZE + 3];
	u_char *fh;
	char *cp;
	int i;
#ifndef NFS_NOSYMLINK
	struct nfs_iodesc *newfd;
	struct nfsv2_fattrs *fa;
	char *ncp;
	int c;
	char namebuf[NFS_MAXPATHLEN + 1];
	char linkbuf[NFS_MAXPATHLEN + 1];
	int nlinks = 0;
#endif
	int error;
	char *path;

#ifdef NFS_DEBUG
 	if (debug)
 	    printf("nfs_open: %s (rootpath=%s)\n", upath, rootpath);
#endif
	if (!rootpath[0]) {
		printf("no rootpath, no nfs\n");
		return (ENXIO);
	}

	/*
	 * This is silly - we should look at dv_type but that value is
	 * arch dependant and we can't use it here.
	 */
#ifndef __i386__
	if (strcmp(f->f_dev->dv_name, "net") != 0)
		return(EINVAL);
#else
	if (strcmp(f->f_dev->dv_name, "pxe") != 0)
		return(EINVAL);
#endif

	if (!(desc = socktodesc(*(int *)(f->f_devdata))))
		return(EINVAL);

	/* Bind to a reserved port. */
	desc->myport = htons(--rpc_port);
	desc->destip = rootip;
	if ((error = nfs_getrootfh(desc, rootpath, nfs_root_node.fh)))
		return (error);
	nfs_root_node.iodesc = desc;

	fh = &nfs_root_node.fh[0];
	buf[0] = 'X';
	cp = &buf[1];
	for (i = 0; i < NFS_FHSIZE; i++, cp += 2)
		sprintf(cp, "%02x", fh[i]);
	sprintf(cp, "X");
	setenv("boot.nfsroot.server", inet_ntoa(rootip), 1);
	setenv("boot.nfsroot.path", rootpath, 1);
	setenv("boot.nfsroot.nfshandle", buf, 1);

#ifndef NFS_NOSYMLINK
	/* Fake up attributes for the root dir. */
	fa = &nfs_root_node.fa;
	fa->fa_type  = htonl(NFDIR);
	fa->fa_mode  = htonl(0755);
	fa->fa_nlink = htonl(2);

	currfd = &nfs_root_node;
	newfd = 0;

	cp = path = strdup(upath);
	if (path == NULL) {
	    error = ENOMEM;
	    goto out;
	}
	while (*cp) {
		/*
		 * Remove extra separators
		 */
		while (*cp == '/')
			cp++;

		if (*cp == '\0')
			break;
		/*
		 * Check that current node is a directory.
		 */
		if (currfd->fa.fa_type != htonl(NFDIR)) {
			error = ENOTDIR;
			goto out;
		}

		/* allocate file system specific data structure */
		newfd = malloc(sizeof(*newfd));
		newfd->iodesc = currfd->iodesc;
		newfd->off = 0;

		/*
		 * Get next component of path name.
		 */
		{
			int len = 0;

			ncp = cp;
			while ((c = *cp) != '\0' && c != '/') {
				if (++len > NFS_MAXNAMLEN) {
					error = ENOENT;
					goto out;
				}
				cp++;
			}
			*cp = '\0';
		}

		/* lookup a file handle */
		error = nfs_lookupfh(currfd, ncp, newfd);
		*cp = c;
		if (error)
			goto out;

		/*
		 * Check for symbolic link
		 */
		if (newfd->fa.fa_type == htonl(NFLNK)) {
			int link_len, len;

			error = nfs_readlink(newfd, linkbuf);
			if (error)
				goto out;

			link_len = strlen(linkbuf);
			len = strlen(cp);

			if (link_len + len > MAXPATHLEN
			    || ++nlinks > MAXSYMLINKS) {
				error = ENOENT;
				goto out;
			}

			bcopy(cp, &namebuf[link_len], len + 1);
			bcopy(linkbuf, namebuf, link_len);

			/*
			 * If absolute pathname, restart at root.
			 * If relative pathname, restart at parent directory.
			 */
			cp = namebuf;
			if (*cp == '/') {
				if (currfd != &nfs_root_node)
					free(currfd);
				currfd = &nfs_root_node;
			}

			free(newfd);
			newfd = 0;

			continue;
		}

		if (currfd != &nfs_root_node)
			free(currfd);
		currfd = newfd;
		newfd = 0;
	}

	error = 0;

out:
	if (newfd)
		free(newfd);
	if (path)
		free(path);
#else
        /* allocate file system specific data structure */
        currfd = malloc(sizeof(*currfd));
        currfd->iodesc = desc;
        currfd->off = 0;

        error = nfs_lookupfh(&nfs_root_node, upath, currfd);
#endif
	if (!error) {
		f->f_fsdata = (void *)currfd;
		return (0);
	}

#ifdef NFS_DEBUG
	if (debug)
		printf("nfs_open: %s lookupfh failed: %s\n",
		    path, strerror(error));
#endif
#ifndef NFS_NOSYMLINK
	if (currfd != &nfs_root_node)
#endif
		free(currfd);

	return (error);
}

int
nfs_close(struct open_file *f)
{
	struct nfs_iodesc *fp = (struct nfs_iodesc *)f->f_fsdata;

#ifdef NFS_DEBUG
	if (debug)
		printf("nfs_close: fp=0x%lx\n", (u_long)fp);
#endif

	if (fp != &nfs_root_node && fp)
		free(fp);
	f->f_fsdata = (void *)0;

	return (0);
}

/*
 * read a portion of a file
 */
int
nfs_read(struct open_file *f, void *buf, size_t size, size_t *resid)
{
	struct nfs_iodesc *fp = (struct nfs_iodesc *)f->f_fsdata;
	ssize_t cc;
	char *addr = buf;

#ifdef NFS_DEBUG
	if (debug)
		printf("nfs_read: size=%lu off=%d\n", (u_long)size,
		       (int)fp->off);
#endif
	while ((int)size > 0) {
		twiddle();
		cc = nfs_readdata(fp, fp->off, (void *)addr, size);
		/* XXX maybe should retry on certain errors */
		if (cc == -1) {
#ifdef NFS_DEBUG
			if (debug)
				printf("nfs_read: read: %s", strerror(errno));
#endif
			return (errno);	/* XXX - from nfs_readdata */
		}
		if (cc == 0) {
#ifdef NFS_DEBUG
			if (debug)
				printf("nfs_read: hit EOF unexpectantly");
#endif
			goto ret;
		}
		fp->off += cc;
		addr += cc;
		size -= cc;
	}
ret:
	if (resid)
		*resid = size;

	return (0);
}

/*
 * Not implemented.
 */
int
nfs_write(struct open_file *f, void *buf, size_t size, size_t *resid)
{
	return (EROFS);
}

off_t
nfs_seek(struct open_file *f, off_t offset, int where)
{
	struct nfs_iodesc *d = (struct nfs_iodesc *)f->f_fsdata;
	n_long size = ntohl(d->fa.fa_size);

	switch (where) {
	case SEEK_SET:
		d->off = offset;
		break;
	case SEEK_CUR:
		d->off += offset;
		break;
	case SEEK_END:
		d->off = size - offset;
		break;
	default:
		errno = EINVAL;
		return (-1);
	}

	return (d->off);
}

/* NFNON=0, NFREG=1, NFDIR=2, NFBLK=3, NFCHR=4, NFLNK=5 */
int nfs_stat_types[8] = {
	0, S_IFREG, S_IFDIR, S_IFBLK, S_IFCHR, S_IFLNK, 0 };

int
nfs_stat(struct open_file *f, struct stat *sb)
{
	struct nfs_iodesc *fp = (struct nfs_iodesc *)f->f_fsdata;
	n_long ftype, mode;

	ftype = ntohl(fp->fa.fa_type);
	mode  = ntohl(fp->fa.fa_mode);
	mode |= nfs_stat_types[ftype & 7];

	sb->st_mode  = mode;
	sb->st_nlink = ntohl(fp->fa.fa_nlink);
	sb->st_uid   = ntohl(fp->fa.fa_uid);
	sb->st_gid   = ntohl(fp->fa.fa_gid);
	sb->st_size  = ntohl(fp->fa.fa_size);

	return (0);
}

static int
nfs_readdir(struct open_file *f, struct dirent *d)
{
	struct nfs_iodesc *fp = (struct nfs_iodesc *)f->f_fsdata;
	struct nfs_readdir_args *args;
	struct nfs_readdir_data *rd;
	struct nfs_readdir_off  *roff = NULL;
	static char *buf;
	static n_long cookie = 0;
	size_t cc;
	n_long eof;

	struct {
		n_long h[RPC_HEADER_WORDS];
		struct nfs_readdir_args d;
	} sdata;
	static struct {
		n_long h[RPC_HEADER_WORDS];
		u_char d[NFS_READDIRSIZE];
	} rdata;

	if (cookie == 0) {
	refill:
		args = &sdata.d;
		bzero(args, sizeof(*args));

		bcopy(fp->fh, args->fh, NFS_FHSIZE);
		args->cookie = htonl(cookie);
		args->count  = htonl(NFS_READDIRSIZE);

		cc = rpc_call(fp->iodesc, NFS_PROG, NFS_VER2, NFSPROC_READDIR,
			      args, sizeof(*args),
			      rdata.d, sizeof(rdata.d));
		buf  = rdata.d;
		roff = (struct nfs_readdir_off *)buf;
		if (ntohl(roff->cookie) != 0)
			return EIO;
	}
	roff = (struct nfs_readdir_off *)buf;

	if (ntohl(roff->follows) == 0) {
		eof = ntohl((roff+1)->cookie);
		if (eof) {
			cookie = 0;
			return ENOENT;
		}
		goto refill;
	}

	buf += sizeof(struct nfs_readdir_off);
	rd = (struct nfs_readdir_data *)buf;
	d->d_namlen = ntohl(rd->len);
	bcopy(rd->name, d->d_name, d->d_namlen);
	d->d_name[d->d_namlen] = '\0';

	buf += (sizeof(struct nfs_readdir_data) + roundup(htonl(rd->len),4));
	roff = (struct nfs_readdir_off *)buf;
	cookie = ntohl(roff->cookie);
	return 0;
}
#else	/* !OLD_NFSV2 */
/*
 * Fetch the root file handle (call mount daemon)
 * Return zero or error number.
 */
int
nfs_getrootfh(struct iodesc *d, char *path, uint32_t *fhlenp, u_char *fhp)
{
	int len;
	struct args {
		uint32_t len;
		char path[FNAME_SIZE];
	} *args;
	struct repl {
		uint32_t errno;
		uint32_t fhsize;
		u_char fh[NFS_V3MAXFHSIZE];
		uint32_t authcnt;
		uint32_t auth[7];
	} *repl;
	struct {
		uint32_t h[RPC_HEADER_WORDS];
		struct args d;
	} sdata;
	struct {
		uint32_t h[RPC_HEADER_WORDS];
		struct repl d;
	} rdata;
	size_t cc;

#ifdef NFS_DEBUG
	if (debug)
		printf("nfs_getrootfh: %s\n", path);
#endif

	args = &sdata.d;
	repl = &rdata.d;

	bzero(args, sizeof(*args));
	len = strlen(path);
	if (len > sizeof(args->path))
		len = sizeof(args->path);
	args->len = htonl(len);
	bcopy(path, args->path, len);
	len = sizeof(uint32_t) + roundup(len, sizeof(uint32_t));

	cc = rpc_call(d, RPCPROG_MNT, RPCMNT_VER3, RPCMNT_MOUNT,
	    args, len, repl, sizeof(*repl));
	if (cc == -1)
		/* errno was set by rpc_call */
		return (errno);
	if (cc < 2 * sizeof (uint32_t))
		return (EBADRPC);
	if (repl->errno != 0)
		return (ntohl(repl->errno));
	*fhlenp = ntohl(repl->fhsize);
	bcopy(repl->fh, fhp, *fhlenp);
	return (0);
}

/*
 * Lookup a file.  Store handle and attributes.
 * Return zero or error number.
 */
int
nfs_lookupfh(struct nfs_iodesc *d, const char *name, struct nfs_iodesc *newfd)
{
	int len, rlen, pos;
	struct args {
		uint32_t fhsize;
		uint32_t fhplusname[1 +
		    (NFS_V3MAXFHSIZE + FNAME_SIZE) / sizeof(uint32_t)];
	} *args;
	struct repl {
		uint32_t errno;
		uint32_t fhsize;
		uint32_t fhplusattr[(NFS_V3MAXFHSIZE +
		    2 * (sizeof(uint32_t) +
		    sizeof(struct nfsv3_fattrs))) / sizeof(uint32_t)];
	} *repl;
	struct {
		uint32_t h[RPC_HEADER_WORDS];
		struct args d;
	} sdata;
	struct {
		uint32_t h[RPC_HEADER_WORDS];
		struct repl d;
	} rdata;
	ssize_t cc;

#ifdef NFS_DEBUG
	if (debug)
		printf("lookupfh: called\n");
#endif

	args = &sdata.d;
	repl = &rdata.d;

	bzero(args, sizeof(*args));
	args->fhsize = htonl(d->fhsize);
	bcopy(d->fh, args->fhplusname, d->fhsize);
	len = strlen(name);
	if (len > FNAME_SIZE)
		len = FNAME_SIZE;
	pos = roundup(d->fhsize, sizeof(uint32_t)) / sizeof(uint32_t);
	args->fhplusname[pos++] = htonl(len);
	bcopy(name, &args->fhplusname[pos], len);
	len = sizeof(uint32_t) + pos * sizeof(uint32_t) +
	    roundup(len, sizeof(uint32_t));

	rlen = sizeof(*repl);

	cc = rpc_call(d->iodesc, NFS_PROG, NFS_VER3, NFSPROCV3_LOOKUP,
	    args, len, repl, rlen);
	if (cc == -1)
		return (errno);		/* XXX - from rpc_call */
	if (cc < 2 * sizeof(uint32_t))
		return (EIO);
	if (repl->errno != 0)
		/* saerrno.h now matches NFS error numbers. */
		return (ntohl(repl->errno));
	newfd->fhsize = ntohl(repl->fhsize);
	bcopy(repl->fhplusattr, &newfd->fh, newfd->fhsize);
	pos = roundup(newfd->fhsize, sizeof(uint32_t)) / sizeof(uint32_t);
	if (repl->fhplusattr[pos++] == 0)
		return (EIO);
	bcopy(&repl->fhplusattr[pos], &newfd->fa, sizeof(newfd->fa));
	return (0);
}

#ifndef NFS_NOSYMLINK
/*
 * Get the destination of a symbolic link.
 */
int
nfs_readlink(struct nfs_iodesc *d, char *buf)
{
	struct args {
		uint32_t fhsize;
		u_char fh[NFS_V3MAXFHSIZE];
	} *args;
	struct repl {
		uint32_t errno;
		uint32_t ok;
		struct nfsv3_fattrs fa;
		uint32_t len;
		u_char path[NFS_MAXPATHLEN];
	} *repl;
	struct {
		uint32_t h[RPC_HEADER_WORDS];
		struct args d;
	} sdata;
	struct {
		uint32_t h[RPC_HEADER_WORDS];
		struct repl d;
	} rdata;
	ssize_t cc;

#ifdef NFS_DEBUG
	if (debug)
		printf("readlink: called\n");
#endif

	args = &sdata.d;
	repl = &rdata.d;

	bzero(args, sizeof(*args));
	args->fhsize = htonl(d->fhsize);
	bcopy(d->fh, args->fh, d->fhsize);
	cc = rpc_call(d->iodesc, NFS_PROG, NFS_VER3, NFSPROCV3_READLINK,
	    args, sizeof(uint32_t) + roundup(d->fhsize, sizeof(uint32_t)),
	    repl, sizeof(*repl));
	if (cc == -1)
		return (errno);

	if (cc < 2 * sizeof(uint32_t))
		return (EIO);

	if (repl->errno != 0)
		return (ntohl(repl->errno));

	if (repl->ok == 0)
		return (EIO);

	repl->len = ntohl(repl->len);
	if (repl->len > NFS_MAXPATHLEN)
		return (ENAMETOOLONG);

	bcopy(repl->path, buf, repl->len);
	buf[repl->len] = 0;
	return (0);
}
#endif

/*
 * Read data from a file.
 * Return transfer count or -1 (and set errno)
 */
ssize_t
nfs_readdata(struct nfs_iodesc *d, off_t off, void *addr, size_t len)
{
	struct args {
		uint32_t fhsize;
		uint32_t fhoffcnt[NFS_V3MAXFHSIZE / sizeof(uint32_t) + 3];
	} *args;
	struct repl {
		uint32_t errno;
		uint32_t ok;
		struct nfsv3_fattrs fa;
		uint32_t count;
		uint32_t eof;
		uint32_t len;
		u_char data[NFSREAD_SIZE];
	} *repl;
	struct {
		uint32_t h[RPC_HEADER_WORDS];
		struct args d;
	} sdata;
	struct {
		uint32_t h[RPC_HEADER_WORDS];
		struct repl d;
	} rdata;
	size_t cc;
	long x;
	int hlen, rlen, pos;

	args = &sdata.d;
	repl = &rdata.d;

	bzero(args, sizeof(*args));
	args->fhsize = htonl(d->fhsize);
	bcopy(d->fh, args->fhoffcnt, d->fhsize);
	pos = roundup(d->fhsize, sizeof(uint32_t)) / sizeof(uint32_t);
	args->fhoffcnt[pos++] = 0;
	args->fhoffcnt[pos++] = htonl((uint32_t)off);
	if (len > NFSREAD_SIZE)
		len = NFSREAD_SIZE;
	args->fhoffcnt[pos] = htonl((uint32_t)len);
	hlen = sizeof(*repl) - NFSREAD_SIZE;

	cc = rpc_call(d->iodesc, NFS_PROG, NFS_VER3, NFSPROCV3_READ,
	    args, 4 * sizeof(uint32_t) + roundup(d->fhsize, sizeof(uint32_t)),
	    repl, sizeof(*repl));
	if (cc == -1)
		/* errno was already set by rpc_call */
		return (-1);
	if (cc < hlen) {
		errno = EBADRPC;
		return (-1);
	}
	if (repl->errno != 0) {
		errno = ntohl(repl->errno);
		return (-1);
	}
	rlen = cc - hlen;
	x = ntohl(repl->count);
	if (rlen < x) {
		printf("nfsread: short packet, %d < %ld\n", rlen, x);
		errno = EBADRPC;
		return (-1);
	}
	bcopy(repl->data, addr, x);
	return (x);
}

/*
 * Open a file.
 * return zero or error number
 */
int
nfs_open(const char *upath, struct open_file *f)
{
	struct iodesc *desc;
	struct nfs_iodesc *currfd;
	char buf[2 * NFS_V3MAXFHSIZE + 3];
	u_char *fh;
	char *cp;
	int i;
#ifndef NFS_NOSYMLINK
	struct nfs_iodesc *newfd;
	struct nfsv3_fattrs *fa;
	char *ncp;
	int c;
	char namebuf[NFS_MAXPATHLEN + 1];
	char linkbuf[NFS_MAXPATHLEN + 1];
	int nlinks = 0;
#endif
	int error;
	char *path;

#ifdef NFS_DEBUG
 	if (debug)
 	    printf("nfs_open: %s (rootpath=%s)\n", upath, rootpath);
#endif
	if (!rootpath[0]) {
		printf("no rootpath, no nfs\n");
		return (ENXIO);
	}

	/*
	 * This is silly - we should look at dv_type but that value is
	 * arch dependant and we can't use it here.
	 */
#ifndef __i386__
	if (strcmp(f->f_dev->dv_name, "net") != 0)
		return (EINVAL);
#else
	if (strcmp(f->f_dev->dv_name, "pxe") != 0)
		return (EINVAL);
#endif

	if (!(desc = socktodesc(*(int *)(f->f_devdata))))
		return (EINVAL);

	/* Bind to a reserved port. */
	desc->myport = htons(--rpc_port);
	desc->destip = rootip;
	if ((error = nfs_getrootfh(desc, rootpath, &nfs_root_node.fhsize,
	    nfs_root_node.fh)))
		return (error);
	nfs_root_node.iodesc = desc;

	fh = &nfs_root_node.fh[0];
	buf[0] = 'X';
	cp = &buf[1];
	for (i = 0; i < nfs_root_node.fhsize; i++, cp += 2)
		sprintf(cp, "%02x", fh[i]);
	sprintf(cp, "X");
	setenv("boot.nfsroot.server", inet_ntoa(rootip), 1);
	setenv("boot.nfsroot.path", rootpath, 1);
	setenv("boot.nfsroot.nfshandle", buf, 1);
	sprintf(buf, "%d", nfs_root_node.fhsize);
	setenv("boot.nfsroot.nfshandlelen", buf, 1);

#ifndef NFS_NOSYMLINK
	/* Fake up attributes for the root dir. */
	fa = &nfs_root_node.fa;
	fa->fa_type  = htonl(NFDIR);
	fa->fa_mode  = htonl(0755);
	fa->fa_nlink = htonl(2);

	currfd = &nfs_root_node;
	newfd = 0;

	cp = path = strdup(upath);
	if (path == NULL) {
		error = ENOMEM;
		goto out;
	}
	while (*cp) {
		/*
		 * Remove extra separators
		 */
		while (*cp == '/')
			cp++;

		if (*cp == '\0')
			break;
		/*
		 * Check that current node is a directory.
		 */
		if (currfd->fa.fa_type != htonl(NFDIR)) {
			error = ENOTDIR;
			goto out;
		}

		/* allocate file system specific data structure */
		newfd = malloc(sizeof(*newfd));
		if (newfd == NULL) {
			error = ENOMEM;
			goto out;
		}
		newfd->iodesc = currfd->iodesc;
		newfd->off = 0;

		/*
		 * Get next component of path name.
		 */
		{
			int len = 0;

			ncp = cp;
			while ((c = *cp) != '\0' && c != '/') {
				if (++len > NFS_MAXNAMLEN) {
					error = ENOENT;
					goto out;
				}
				cp++;
			}
			*cp = '\0';
		}

		/* lookup a file handle */
		error = nfs_lookupfh(currfd, ncp, newfd);
		*cp = c;
		if (error)
			goto out;

		/*
		 * Check for symbolic link
		 */
		if (newfd->fa.fa_type == htonl(NFLNK)) {
			int link_len, len;

			error = nfs_readlink(newfd, linkbuf);
			if (error)
				goto out;

			link_len = strlen(linkbuf);
			len = strlen(cp);

			if (link_len + len > MAXPATHLEN
			    || ++nlinks > MAXSYMLINKS) {
				error = ENOENT;
				goto out;
			}

			bcopy(cp, &namebuf[link_len], len + 1);
			bcopy(linkbuf, namebuf, link_len);

			/*
			 * If absolute pathname, restart at root.
			 * If relative pathname, restart at parent directory.
			 */
			cp = namebuf;
			if (*cp == '/') {
				if (currfd != &nfs_root_node)
					free(currfd);
				currfd = &nfs_root_node;
			}

			free(newfd);
			newfd = 0;

			continue;
		}

		if (currfd != &nfs_root_node)
			free(currfd);
		currfd = newfd;
		newfd = 0;
	}

	error = 0;

out:
	free(newfd);
	free(path);
#else
	/* allocate file system specific data structure */
	currfd = malloc(sizeof(*currfd));
	if (currfd != NULL) {
		currfd->iodesc = desc;
		currfd->off = 0;

		error = nfs_lookupfh(&nfs_root_node, upath, currfd);
	} else
		error = ENOMEM;
#endif
	if (!error) {
		f->f_fsdata = (void *)currfd;
		return (0);
	}

#ifdef NFS_DEBUG
	if (debug)
		printf("nfs_open: %s lookupfh failed: %s\n",
		    path, strerror(error));
#endif
#ifndef NFS_NOSYMLINK
	if (currfd != &nfs_root_node)
#endif
		free(currfd);

	return (error);
}

int
nfs_close(struct open_file *f)
{
	struct nfs_iodesc *fp = (struct nfs_iodesc *)f->f_fsdata;

#ifdef NFS_DEBUG
	if (debug)
		printf("nfs_close: fp=0x%lx\n", (u_long)fp);
#endif

	if (fp != &nfs_root_node && fp)
		free(fp);
	f->f_fsdata = (void *)0;

	return (0);
}

/*
 * read a portion of a file
 */
int
nfs_read(struct open_file *f, void *buf, size_t size, size_t *resid)
{
	struct nfs_iodesc *fp = (struct nfs_iodesc *)f->f_fsdata;
	ssize_t cc;
	char *addr = buf;

#ifdef NFS_DEBUG
	if (debug)
		printf("nfs_read: size=%lu off=%d\n", (u_long)size,
		       (int)fp->off);
#endif
	while ((int)size > 0) {
		twiddle();
		cc = nfs_readdata(fp, fp->off, (void *)addr, size);
		/* XXX maybe should retry on certain errors */
		if (cc == -1) {
#ifdef NFS_DEBUG
			if (debug)
				printf("nfs_read: read: %s", strerror(errno));
#endif
			return (errno);	/* XXX - from nfs_readdata */
		}
		if (cc == 0) {
#ifdef NFS_DEBUG
			if (debug)
				printf("nfs_read: hit EOF unexpectantly");
#endif
			goto ret;
		}
		fp->off += cc;
		addr += cc;
		size -= cc;
	}
ret:
	if (resid)
		*resid = size;

	return (0);
}

/*
 * Not implemented.
 */
int
nfs_write(struct open_file *f, void *buf, size_t size, size_t *resid)
{
	return (EROFS);
}

off_t
nfs_seek(struct open_file *f, off_t offset, int where)
{
	struct nfs_iodesc *d = (struct nfs_iodesc *)f->f_fsdata;
	uint32_t size = ntohl(d->fa.fa_size.val[1]);

	switch (where) {
	case SEEK_SET:
		d->off = offset;
		break;
	case SEEK_CUR:
		d->off += offset;
		break;
	case SEEK_END:
		d->off = size - offset;
		break;
	default:
		errno = EINVAL;
		return (-1);
	}

	return (d->off);
}

/* NFNON=0, NFREG=1, NFDIR=2, NFBLK=3, NFCHR=4, NFLNK=5, NFSOCK=6, NFFIFO=7 */
int nfs_stat_types[9] = {
	0, S_IFREG, S_IFDIR, S_IFBLK, S_IFCHR, S_IFLNK, S_IFSOCK, S_IFIFO, 0 };

int
nfs_stat(struct open_file *f, struct stat *sb)
{
	struct nfs_iodesc *fp = (struct nfs_iodesc *)f->f_fsdata;
	uint32_t ftype, mode;

	ftype = ntohl(fp->fa.fa_type);
	mode  = ntohl(fp->fa.fa_mode);
	mode |= nfs_stat_types[ftype & 7];

	sb->st_mode  = mode;
	sb->st_nlink = ntohl(fp->fa.fa_nlink);
	sb->st_uid   = ntohl(fp->fa.fa_uid);
	sb->st_gid   = ntohl(fp->fa.fa_gid);
	sb->st_size  = ntohl(fp->fa.fa_size.val[1]);

	return (0);
}

static int
nfs_readdir(struct open_file *f, struct dirent *d)
{
	struct nfs_iodesc *fp = (struct nfs_iodesc *)f->f_fsdata;
	struct nfsv3_readdir_repl *repl;
	struct nfsv3_readdir_entry *rent;
	static char *buf;
	static uint32_t cookie0 = 0;
	static uint32_t cookie1 = 0;
	size_t cc;
	static uint32_t cookieverf0 = 0;
	static uint32_t cookieverf1 = 0;
	int pos;

	struct args {
		uint32_t fhsize;
		uint32_t fhpluscookie[5 + NFS_V3MAXFHSIZE];
	} *args;
	struct {
		uint32_t h[RPC_HEADER_WORDS];
		struct args d;
	} sdata;
	static struct {
		uint32_t h[RPC_HEADER_WORDS];
		u_char d[NFS_READDIRSIZE];
	} rdata;

	if (cookie0 == 0 && cookie1 == 0) {
	refill:
		args = &sdata.d;
		bzero(args, sizeof(*args));

		args->fhsize = htonl(fp->fhsize);
		bcopy(fp->fh, args->fhpluscookie, fp->fhsize);
		pos = roundup(fp->fhsize, sizeof(uint32_t)) / sizeof(uint32_t);
		args->fhpluscookie[pos++] = cookie0;
		args->fhpluscookie[pos++] = cookie1;
		args->fhpluscookie[pos++] = cookieverf0;
		args->fhpluscookie[pos++] = cookieverf1;
		args->fhpluscookie[pos] = htonl(NFS_READDIRSIZE);

		cc = rpc_call(fp->iodesc, NFS_PROG, NFS_VER3, NFSPROCV3_READDIR,
		    args, 6 * sizeof(uint32_t) +
		    roundup(fp->fhsize, sizeof(uint32_t)),
		    rdata.d, sizeof(rdata.d));
		buf  = rdata.d;
		repl = (struct nfsv3_readdir_repl *)buf;
		if (repl->errno != 0)
			return (ntohl(repl->errno));
		cookieverf0 = repl->cookiev0;
		cookieverf1 = repl->cookiev1;
		buf += sizeof (struct nfsv3_readdir_repl);
	}
	rent = (struct nfsv3_readdir_entry *)buf;

	if (rent->follows == 0) {
		/* fid0 is actually eof */
		if (rent->fid0 != 0) {
			cookie0 = 0;
			cookie1 = 0;
			cookieverf0 = 0;
			cookieverf1 = 0;
			return (ENOENT);
		}
		goto refill;
	}

	d->d_namlen = ntohl(rent->len);
	bcopy(rent->nameplus, d->d_name, d->d_namlen);
	d->d_name[d->d_namlen] = '\0';

	pos = roundup(d->d_namlen, sizeof(uint32_t)) / sizeof(uint32_t);
	cookie0 = rent->nameplus[pos++];
	cookie1 = rent->nameplus[pos++];
	buf = (u_char *)&rent->nameplus[pos];
	return (0);
}
#endif	/* OLD_NFSV2 */
