/* $FreeBSD: src/lib/libstand/cd9660.c,v 1.4.2.1 2000/05/04 13:47:49 ps Exp $ */
/*	$NetBSD: cd9660.c,v 1.5 1997/06/26 19:11:33 drochner Exp $	*/

/*
 * Copyright (C) 1996 Wolfgang Solfrank.
 * Copyright (C) 1996 TooLs GmbH.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by TooLs GmbH.
 * 4. The name of TooLs GmbH may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY TOOLS GMBH ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL TOOLS GMBH BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Stand-alone ISO9660 file reading package.
 *
 * Note: This doesn't support Rock Ridge extensions, extended attributes,
 * blocksizes other than 2048 bytes, multi-extent files, etc.
 */
#include <sys/param.h>
#include <string.h>
#include <sys/dirent.h>
#include <isofs/cd9660/iso.h>

#include "stand.h"

static int	cd9660_open(const char *path, struct open_file *f);
static int	cd9660_close(struct open_file *f);
static int	cd9660_read(struct open_file *f, void *buf, size_t size, size_t *resid);
static int	cd9660_write(struct open_file *f, void *buf, size_t size, size_t *resid);
static off_t	cd9660_seek(struct open_file *f, off_t offset, int where);
static int	cd9660_stat(struct open_file *f, struct stat *sb);
static int	cd9660_readdir(struct open_file *f, struct dirent *d);

struct fs_ops cd9660_fsops = {
	"cd9660",
	cd9660_open,
	cd9660_close,
	cd9660_read,
	cd9660_write,
	cd9660_seek,
	cd9660_stat,
	cd9660_readdir
};

struct file {
	int 		f_isdir;	/* nonzero if file is directory */
	off_t 		f_off;		/* Current offset within file */
	daddr_t 	f_bno;		/* Starting block number */
	off_t 		f_size;		/* Size of file */
	daddr_t		f_buf_blkno;	/* block number of data block */	
	char		*f_buf;		/* buffer for data block */
};

struct ptable_ent {
	char namlen	[ISODCL( 1, 1)];	/* 711 */
	char extlen	[ISODCL( 2, 2)];	/* 711 */
	char block	[ISODCL( 3, 6)];	/* 732 */
	char parent	[ISODCL( 7, 8)];	/* 722 */
	char name	[1];
};
#define	PTFIXSZ		8
#define	PTSIZE(pp)	roundup(PTFIXSZ + isonum_711((pp)->namlen), 2)

#define	cdb2devb(bno)	((bno) * ISO_DEFAULT_BLOCK_SIZE / DEV_BSIZE)

/* XXX these should be in the system headers */
static __inline int
isonum_722(p)
	u_char *p;
{
	return (*p << 8)|p[1];
}

static __inline int
isonum_732(p)
	u_char *p;
{
	return (*p << 24)|(p[1] << 16)|(p[2] << 8)|p[3];
}

static int
dirmatch(path, dp)
	const char *path;
	struct iso_directory_record *dp;
{
	char *cp;
	int i;

	cp = dp->name;
	for (i = isonum_711(dp->name_len); --i >= 0; path++, cp++) {
		if (!*path || *path == '/')
			break;
		if (toupper(*path) == *cp)
			continue;
		return 0;
	}
	if (*path && *path != '/')
		return 0;
	/*
	 * Allow stripping of trailing dots and the version number.
	 * Note that this will find the first instead of the last version
	 * of a file.
	 */
	if (i >= 0 && (*cp == ';' || *cp == '.')) {
		/* This is to prevent matching of numeric extensions */
		if (*cp == '.' && cp[1] != ';')
			return 0;
		while (--i >= 0)
			if (*++cp != ';' && (*cp < '0' || *cp > '9'))
				return 0;
	}
	return 1;
}

static int
cd9660_open(path, f)
	const char *path;
	struct open_file *f;
{
	struct file *fp = 0;
	void *buf;
	struct iso_primary_descriptor *vd;
	size_t buf_size, read, dsize, off;
	daddr_t bno, boff;
	struct iso_directory_record rec;
	struct iso_directory_record *dp = 0;
	int rc;

	/* First find the volume descriptor */
	buf = malloc(buf_size = ISO_DEFAULT_BLOCK_SIZE);
	vd = buf;
	for (bno = 16;; bno++) {
		twiddle();
		rc = f->f_dev->dv_strategy(f->f_devdata, F_READ, cdb2devb(bno),
					   ISO_DEFAULT_BLOCK_SIZE, buf, &read);
		if (rc)
			goto out;
		if (read != ISO_DEFAULT_BLOCK_SIZE) {
			rc = EIO;
			goto out;
		}
		rc = EINVAL;
		if (bcmp(vd->id, ISO_STANDARD_ID, sizeof vd->id) != 0)
			goto out;
		if (isonum_711(vd->type) == ISO_VD_END)
			goto out;
		if (isonum_711(vd->type) == ISO_VD_PRIMARY)
			break;
	}
	if (isonum_723(vd->logical_block_size) != ISO_DEFAULT_BLOCK_SIZE)
		goto out;

	rec = *(struct iso_directory_record *) vd->root_directory_record;
	if (*path == '/') path++; /* eat leading '/' */

	while (*path) {
		bno = isonum_733(rec.extent) + isonum_711(rec.ext_attr_length);
		dsize = isonum_733(rec.size);
		off = 0;
		boff = 0;

		while (off < dsize) {
			if ((off % ISO_DEFAULT_BLOCK_SIZE) == 0) {
				twiddle();
				rc = f->f_dev->dv_strategy
					(f->f_devdata, F_READ,
					 cdb2devb(bno + boff),
					 ISO_DEFAULT_BLOCK_SIZE,
					 buf, &read);
				if (rc)
					goto out;
				if (read != ISO_DEFAULT_BLOCK_SIZE) {
					rc = EIO;
					goto out;
				}
				boff++;
				dp = (struct iso_directory_record *) buf;
			}
			if (isonum_711(dp->length) == 0) {
			    /* skip to next block, if any */
			    off = boff * ISO_DEFAULT_BLOCK_SIZE;
			    continue;
			}

			if (dirmatch(path, dp))
				break;

			dp = (struct iso_directory_record *)
				((char *) dp + isonum_711(dp->length));
			off += isonum_711(dp->length);
		}
		if (off == dsize) {
			rc = ENOENT;
			goto out;
		}

		rec = *dp;
		while (*path && *path != '/') /* look for next component */
			path++;
		if (*path) path++; /* skip '/' */
	}

	/* allocate file system specific data structure */
	fp = malloc(sizeof(struct file));
	bzero(fp, sizeof(struct file));
	f->f_fsdata = (void *)fp;

	fp->f_isdir = (isonum_711(rec.flags) & 2) != 0;
	fp->f_off = 0;
	fp->f_bno = isonum_733(rec.extent) + isonum_711(rec.ext_attr_length);
	fp->f_size = isonum_733(rec.size);
	free(buf);

	return 0;

out:
	if (fp)
		free(fp);
	free(buf);

	return rc;
}

static int
cd9660_close(f)
	struct open_file *f;
{
	struct file *fp = (struct file *)f->f_fsdata;

	f->f_fsdata = 0;
	free(fp);

	return 0;
}

static int
buf_read_file(f, buf_p, size_p)
	struct open_file *f;
	char **buf_p;
	size_t *size_p;
{
	struct file *fp = (struct file *)f->f_fsdata;
	daddr_t blkno;
	int rc = 0;
	size_t read;

	blkno = fp->f_off / ISO_DEFAULT_BLOCK_SIZE + fp->f_bno;

	if (blkno != fp->f_buf_blkno) {
		if (fp->f_buf == (char *)0)
			fp->f_buf = malloc(ISO_DEFAULT_BLOCK_SIZE);

		twiddle();
		rc = f->f_dev->dv_strategy(f->f_devdata, F_READ,
		    cdb2devb(blkno), ISO_DEFAULT_BLOCK_SIZE, fp->f_buf, &read);
		if (rc)
			return (rc);
		if (read != ISO_DEFAULT_BLOCK_SIZE)
			return (EIO);

		fp->f_buf_blkno = blkno;
	}

	*buf_p = fp->f_buf + fp->f_off;
	*size_p = ISO_DEFAULT_BLOCK_SIZE - fp->f_off;

	if (*size_p > fp->f_size - fp->f_off)
		*size_p = fp->f_size - fp->f_off;
	return (rc);
}

static int
cd9660_read(f, start, size, resid)
	struct open_file *f;
	void *start;
	size_t size;
	size_t *resid;
{
	struct file *fp = (struct file *)f->f_fsdata;
	char *buf, *addr;
	size_t buf_size, csize;
	int rc = 0;

	addr = start;
	while (size) {
		if (fp->f_off < 0 || fp->f_off >= fp->f_size)
			break;

		rc = buf_read_file(f, &buf, &buf_size);
		if (rc)
			break;

		csize = size > buf_size ? buf_size : size;
		bcopy(buf, addr, csize);

		fp->f_off += csize;
		addr += csize;
		size -= csize;
	}
	if (resid)
		*resid = size;
	return (rc);
}

static int
cd9660_readdir(struct open_file *f, struct dirent *d)
{
	struct file *fp = (struct file *)f->f_fsdata;
	struct iso_directory_record *ep;
	size_t buf_size, reclen, namelen;
	int error = 0;
	char *buf;

again:
	if (fp->f_off >= fp->f_size)
		return (ENOENT);
	error = buf_read_file(f, &buf, &buf_size);
	if (error)
		return (error);
	ep = (struct iso_directory_record *)buf;

	if (isonum_711(ep->length) == 0) {
		daddr_t blkno;
		
		/* skip to next block, if any */
		blkno = fp->f_off / ISO_DEFAULT_BLOCK_SIZE;
		fp->f_off = (blkno + 1) * ISO_DEFAULT_BLOCK_SIZE;
		goto again;
	}

	namelen = isonum_711(ep->name_len);
	if (namelen == 1 && ep->name[0] == 1)
		namelen = 2;
	reclen = sizeof(struct dirent) - (MAXNAMLEN+1) + namelen + 1;
	reclen = (reclen + 3) & ~3;

	d->d_fileno = isonum_733(ep->extent);
	d->d_reclen = reclen;
	if (isonum_711(ep->flags) & 2)
		d->d_type = DT_DIR;
	else
		d->d_type = DT_REG;
	d->d_namlen = namelen;

	if (isonum_711(ep->name_len) == 1 && ep->name[0] == 0)
		strcpy(d->d_name, ".");
	else if (isonum_711(ep->name_len) == 1 && ep->name[0] == 1)
		strcpy(d->d_name, "..");
	else
		bcopy(ep->name, d->d_name, d->d_namlen);
	d->d_name[d->d_namlen] = 0;

	fp->f_off += isonum_711(ep->length);
	return (0);
}

static int
cd9660_write(f, start, size, resid)
	struct open_file *f;
	void *start;
	size_t size;
	size_t *resid;
{
	return EROFS;
}

static off_t
cd9660_seek(f, offset, where)
	struct open_file *f;
	off_t offset;
	int where;
{
	struct file *fp = (struct file *)f->f_fsdata;

	switch (where) {
	case SEEK_SET:
		fp->f_off = offset;
		break;
	case SEEK_CUR:
		fp->f_off += offset;
		break;
	case SEEK_END:
		fp->f_off = fp->f_size - offset;
		break;
	default:
		return -1;
	}
	return fp->f_off;
}

static int
cd9660_stat(f, sb)
	struct open_file *f;
	struct stat *sb;
{
	struct file *fp = (struct file *)f->f_fsdata;

	/* only important stuff */
	sb->st_mode = S_IRUSR | S_IRGRP | S_IROTH;
	if (fp->f_isdir)
		sb->st_mode |= S_IFDIR;
	else
		sb->st_mode |= S_IFREG;
	sb->st_uid = sb->st_gid = 0;
	sb->st_size = fp->f_size;
	return 0;
}
