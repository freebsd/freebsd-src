/*-
 * Copyright (c) 2007 Doug Rabson
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
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	$FreeBSD$
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

/*
 *	Stand-alone file reading package.
 */

#include <sys/param.h>
#include <sys/disklabel.h>
#include <sys/time.h>
#include <sys/queue.h>
#include <stddef.h>
#include <stdarg.h>
#include <string.h>
#include <stand.h>
#include <bootstrap.h>

#include "zfsimpl.c"

static int	zfs_open(const char *path, struct open_file *f);
static int	zfs_write(struct open_file *f, void *buf, size_t size, size_t *resid);
static int	zfs_close(struct open_file *f);
static int	zfs_read(struct open_file *f, void *buf, size_t size, size_t *resid);
static off_t	zfs_seek(struct open_file *f, off_t offset, int where);
static int	zfs_stat(struct open_file *f, struct stat *sb);
static int	zfs_readdir(struct open_file *f, struct dirent *d);

struct devsw zfs_dev;

struct fs_ops zfs_fsops = {
	"zfs",
	zfs_open,
	zfs_close,
	zfs_read,
	zfs_write,
	zfs_seek,
	zfs_stat,
	zfs_readdir
};

/*
 * In-core open file.
 */
struct file {
	off_t		f_seekp;	/* seek pointer */
	dnode_phys_t	f_dnode;
	uint64_t	f_zap_type;	/* zap type for readdir */
	uint64_t	f_num_leafs;	/* number of fzap leaf blocks */
	zap_leaf_phys_t	*f_zap_leaf;	/* zap leaf buffer */
};

/*
 * Open a file.
 */
static int
zfs_open(const char *upath, struct open_file *f)
{
	spa_t *spa = (spa_t *) f->f_devdata;
	struct file *fp;
	int rc;

	if (f->f_dev != &zfs_dev)
		return (EINVAL);

	rc = zfs_mount_pool(spa);
	if (rc)
		return (rc);

	/* allocate file system specific data structure */
	fp = malloc(sizeof(struct file));
	bzero(fp, sizeof(struct file));
	f->f_fsdata = (void *)fp;

	if (spa->spa_root_objset.os_type != DMU_OST_ZFS) {
		printf("Unexpected object set type %lld\n",
		    spa->spa_root_objset.os_type);
		rc = EIO;
		goto out;
	}

	rc = zfs_lookup(spa, upath, &fp->f_dnode);
	if (rc)
		goto out;

	fp->f_seekp = 0;
out:
	if (rc) {
		f->f_fsdata = NULL;
		free(fp);
	}
	return (rc);
}

static int
zfs_close(struct open_file *f)
{
	struct file *fp = (struct file *)f->f_fsdata;

	dnode_cache_obj = 0;
	f->f_fsdata = (void *)0;
	if (fp == (struct file *)0)
		return (0);

	free(fp);
	return (0);
}

/*
 * Copy a portion of a file into kernel memory.
 * Cross block boundaries when necessary.
 */
static int
zfs_read(struct open_file *f, void *start, size_t size, size_t *resid	/* out */)
{
	spa_t *spa = (spa_t *) f->f_devdata;
	struct file *fp = (struct file *)f->f_fsdata;
	const znode_phys_t *zp = (const znode_phys_t *) fp->f_dnode.dn_bonus;
	size_t n;
	int rc;

	n = size;
	if (fp->f_seekp + n > zp->zp_size)
		n = zp->zp_size - fp->f_seekp;
	
	rc = dnode_read(spa, &fp->f_dnode, fp->f_seekp, start, n);
	if (rc)
		return (rc);

	if (0) {
	    int i;
	    for (i = 0; i < n; i++)
		putchar(((char*) start)[i]);
	}
	fp->f_seekp += n;
	if (resid)
		*resid = size - n;

	return (0);
}

/*
 * Don't be silly - the bootstrap has no business writing anything.
 */
static int
zfs_write(struct open_file *f, void *start, size_t size, size_t *resid	/* out */)
{

	return (EROFS);
}

static off_t
zfs_seek(struct open_file *f, off_t offset, int where)
{
	struct file *fp = (struct file *)f->f_fsdata;
	znode_phys_t *zp = (znode_phys_t *) fp->f_dnode.dn_bonus;

	switch (where) {
	case SEEK_SET:
		fp->f_seekp = offset;
		break;
	case SEEK_CUR:
		fp->f_seekp += offset;
		break;
	case SEEK_END:
		fp->f_seekp = zp->zp_size - offset;
		break;
	default:
		errno = EINVAL;
		return (-1);
	}
	return (fp->f_seekp);
}

static int
zfs_stat(struct open_file *f, struct stat *sb)
{
	struct file *fp = (struct file *)f->f_fsdata;
	znode_phys_t *zp = (znode_phys_t *) fp->f_dnode.dn_bonus;

	/* only important stuff */
	sb->st_mode = zp->zp_mode;
	sb->st_uid = zp->zp_uid;
	sb->st_gid = zp->zp_gid;
	sb->st_size = zp->zp_size;

	return (0);
}

static int
zfs_readdir(struct open_file *f, struct dirent *d)
{
	spa_t *spa = (spa_t *) f->f_devdata;
	struct file *fp = (struct file *)f->f_fsdata;
	znode_phys_t *zp = (znode_phys_t *) fp->f_dnode.dn_bonus;
	mzap_ent_phys_t mze;
	size_t bsize = fp->f_dnode.dn_datablkszsec << SPA_MINBLOCKSHIFT;
	int rc;

	if ((zp->zp_mode >> 12) != 0x4) {
		return (ENOTDIR);
	}

	/*
	 * If this is the first read, get the zap type.
	 */
	if (fp->f_seekp == 0) {
		rc = dnode_read(spa, &fp->f_dnode,
				0, &fp->f_zap_type, sizeof(fp->f_zap_type));
		if (rc)
			return (rc);

		if (fp->f_zap_type == ZBT_MICRO) {
			fp->f_seekp = offsetof(mzap_phys_t, mz_chunk);
		} else {
			rc = dnode_read(spa, &fp->f_dnode,
					offsetof(zap_phys_t, zap_num_leafs),
					&fp->f_num_leafs,
					sizeof(fp->f_num_leafs));
			if (rc)
				return (rc);

			fp->f_seekp = bsize;
			fp->f_zap_leaf = (zap_leaf_phys_t *)malloc(bsize);
			rc = dnode_read(spa, &fp->f_dnode,
					fp->f_seekp,
					fp->f_zap_leaf,
					bsize);
			if (rc)
				return (rc);
		}
	}

	if (fp->f_zap_type == ZBT_MICRO) {
	mzap_next:
		if (fp->f_seekp >= bsize)
			return (ENOENT);

		rc = dnode_read(spa, &fp->f_dnode,
				fp->f_seekp, &mze, sizeof(mze));
		fp->f_seekp += sizeof(mze);

		if (!mze.mze_name[0])
			goto mzap_next;

		d->d_fileno = ZFS_DIRENT_OBJ(mze.mze_value);
		d->d_type = ZFS_DIRENT_TYPE(mze.mze_value);
		strcpy(d->d_name, mze.mze_name);
		d->d_namlen = strlen(d->d_name);
		return (0);
	} else {
		zap_leaf_t zl;
		zap_leaf_chunk_t *zc, *nc;
		int chunk;
		size_t namelen;
		char *p;
		uint64_t value;

		/*
		 * Initialise this so we can use the ZAP size
		 * calculating macros.
		 */
		zl.l_bs = ilog2(bsize);
		zl.l_phys = fp->f_zap_leaf;

		/*
		 * Figure out which chunk we are currently looking at
		 * and consider seeking to the next leaf. We use the
		 * low bits of f_seekp as a simple chunk index.
		 */
	fzap_next:
		chunk = fp->f_seekp & (bsize - 1);
		if (chunk == ZAP_LEAF_NUMCHUNKS(&zl)) {
			fp->f_seekp = (fp->f_seekp & ~(bsize - 1)) + bsize;
			chunk = 0;

			/*
			 * Check for EOF and read the new leaf.
			 */
			if (fp->f_seekp >= bsize * fp->f_num_leafs)
				return (ENOENT);

			rc = dnode_read(spa, &fp->f_dnode,
					fp->f_seekp,
					fp->f_zap_leaf,
					bsize);
			if (rc)
				return (rc);
		}

		zc = &ZAP_LEAF_CHUNK(&zl, chunk);
		fp->f_seekp++;
		if (zc->l_entry.le_type != ZAP_CHUNK_ENTRY)
			goto fzap_next;

		namelen = zc->l_entry.le_name_length;
		if (namelen > sizeof(d->d_name))
			namelen = sizeof(d->d_name);

		/*
		 * Paste the name back together.
		 */
		nc = &ZAP_LEAF_CHUNK(&zl, zc->l_entry.le_name_chunk);
		p = d->d_name;
		while (namelen > 0) {
			int len;
			len = namelen;
			if (len > ZAP_LEAF_ARRAY_BYTES)
				len = ZAP_LEAF_ARRAY_BYTES;
			memcpy(p, nc->l_array.la_array, len);
			p += len;
			namelen -= len;
			nc = &ZAP_LEAF_CHUNK(&zl, nc->l_array.la_next);
		}
		d->d_name[sizeof(d->d_name) - 1] = 0;

		/*
		 * Assume the first eight bytes of the value are
		 * a uint64_t.
		 */
		value = fzap_leaf_value(&zl, zc);

		d->d_fileno = ZFS_DIRENT_OBJ(value);
		d->d_type = ZFS_DIRENT_TYPE(value);
		d->d_namlen = strlen(d->d_name);

		return (0);
	}
}

static int
vdev_read(vdev_t *vdev, void *priv, off_t offset, void *buf, size_t size)
{
	int fd;

	fd = (uintptr_t) priv;
	lseek(fd, offset, SEEK_SET);
	if (read(fd, buf, size) == size) {
		return 0;
	} else {
		return (EIO);
	}
}

/*
 * Convert a pool guid to a 'unit number' suitable for use with zfs_dev_open.
 */
int
zfs_guid_to_unit(uint64_t guid)
{
	spa_t *spa;
	int unit;

	unit = 0;
	STAILQ_FOREACH(spa, &zfs_pools, spa_link) {
		if (spa->spa_guid == guid)
			return unit;
		unit++;
	}
	return (-1);
}

static int
zfs_dev_init(void) 
{
	char devname[512];
	int unit, slice;
	int fd;

	/*
	 * Open all the disks we can find and see if we can reconstruct
	 * ZFS pools from them. Bogusly assumes that the disks are named
	 * diskN or diskNsM.
	 */
	zfs_init();
	for (unit = 0; unit < 32 /* XXX */; unit++) {
		sprintf(devname, "disk%d:", unit);
		fd = open(devname, O_RDONLY);
		if (fd == -1)
			continue;

		/*
		 * If we find a vdev, the zfs code will eat the fd, otherwise
		 * we close it.
		 */
		if (vdev_probe(vdev_read, (void*) (uintptr_t) fd, 0))
			close(fd);

		for (slice = 1; slice <= 4; slice++) {
			sprintf(devname, "disk%ds%d:", unit, slice);
			fd = open(devname, O_RDONLY);
			if (fd == -1)
				continue;
			if (vdev_probe(vdev_read, (void*) (uintptr_t) fd, 0))
				close(fd);
		}
	}

	return (0);
}

/*
 * Print information about ZFS pools
 */
static void
zfs_dev_print(int verbose)
{
	spa_t *spa;
	char line[80];
	int unit;

	if (verbose) {
		spa_all_status();
		return;
	}
	unit = 0;
	STAILQ_FOREACH(spa, &zfs_pools, spa_link) {
		sprintf(line, "    zfs%d:   %s\n", unit, spa->spa_name);
		pager_output(line);
		unit++;
	}
}

/*
 * Attempt to open the pool described by (dev) for use by (f).
 */
static int 
zfs_dev_open(struct open_file *f, ...)
{
	va_list		args;
	struct devdesc	*dev;
	int		unit, i;
	spa_t		*spa;

	va_start(args, f);
	dev = va_arg(args, struct devdesc*);
	va_end(args);

	/*
	 * We mostly ignore the stuff that devopen sends us. For now,
	 * use the unit to find a pool - later we will override the
	 * devname parsing so that we can name a pool and a fs within
	 * the pool.
	 */
	unit = dev->d_unit;
	free(dev);
	
	i = 0;
	STAILQ_FOREACH(spa, &zfs_pools, spa_link) {
		if (i == unit)
			break;
		i++;
	}
	if (!spa) {
		return (ENXIO);
	}

	f->f_devdata = spa;
	return (0);
}

static int 
zfs_dev_close(struct open_file *f)
{

	f->f_devdata = NULL;
	return (0);
}

static int 
zfs_dev_strategy(void *devdata, int rw, daddr_t dblk, size_t size, char *buf, size_t *rsize)
{

	return (ENOSYS);
}

struct devsw zfs_dev = {
	.dv_name = "zfs", 
	.dv_type = DEVT_ZFS, 
	.dv_init = zfs_dev_init,
	.dv_strategy = zfs_dev_strategy, 
	.dv_open = zfs_dev_open, 
	.dv_close = zfs_dev_close, 
	.dv_ioctl = noioctl,
	.dv_print = zfs_dev_print,
	.dv_cleanup = NULL
};
