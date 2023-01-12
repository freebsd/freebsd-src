/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2022 The FreeBSD Foundation
 *
 * This software was developed by Mark Johnston under sponsorship from
 * the FreeBSD Foundation.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the distribution.
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
 */

#include <sys/stat.h>

#include <assert.h>
#include <dirent.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <util.h>

#include "makefs.h"
#include "zfs.h"

typedef struct {
	const char	*name;
	unsigned int	id;
	uint16_t	size;
	sa_bswap_type_t	bs;
} zfs_sattr_t;

typedef struct zfs_fs {
	zfs_objset_t	*os;

	/* Offset table for system attributes, indexed by a zpl_attr_t. */
	uint16_t	*saoffs;
	size_t		sacnt;
	const zfs_sattr_t *satab;
} zfs_fs_t;

/*
 * The order of the attributes doesn't matter, this is simply the one hard-coded
 * by OpenZFS, based on a zdb dump of the SA_REGISTRY table.
 */
typedef enum zpl_attr {
	ZPL_ATIME,
	ZPL_MTIME,
	ZPL_CTIME,
	ZPL_CRTIME,
	ZPL_GEN,
	ZPL_MODE,
	ZPL_SIZE,
	ZPL_PARENT,
	ZPL_LINKS,
	ZPL_XATTR,
	ZPL_RDEV,
	ZPL_FLAGS,
	ZPL_UID,
	ZPL_GID,
	ZPL_PAD,
	ZPL_ZNODE_ACL,
	ZPL_DACL_COUNT,
	ZPL_SYMLINK,
	ZPL_SCANSTAMP,
	ZPL_DACL_ACES,
	ZPL_DXATTR,
	ZPL_PROJID,
} zpl_attr_t;

/*
 * This table must be kept in sync with zpl_attr_layout[] and zpl_attr_t.
 */
static const zfs_sattr_t zpl_attrs[] = {
#define	_ZPL_ATTR(n, s, b)	{ .name = #n, .id = n, .size = s, .bs = b }
	_ZPL_ATTR(ZPL_ATIME, sizeof(uint64_t) * 2, SA_UINT64_ARRAY),
	_ZPL_ATTR(ZPL_MTIME, sizeof(uint64_t) * 2, SA_UINT64_ARRAY),
	_ZPL_ATTR(ZPL_CTIME, sizeof(uint64_t) * 2, SA_UINT64_ARRAY),
	_ZPL_ATTR(ZPL_CRTIME, sizeof(uint64_t) * 2, SA_UINT64_ARRAY),
	_ZPL_ATTR(ZPL_GEN, sizeof(uint64_t), SA_UINT64_ARRAY),
	_ZPL_ATTR(ZPL_MODE, sizeof(uint64_t), SA_UINT64_ARRAY),
	_ZPL_ATTR(ZPL_SIZE, sizeof(uint64_t), SA_UINT64_ARRAY),
	_ZPL_ATTR(ZPL_PARENT, sizeof(uint64_t), SA_UINT64_ARRAY),
	_ZPL_ATTR(ZPL_LINKS, sizeof(uint64_t), SA_UINT64_ARRAY),
	_ZPL_ATTR(ZPL_XATTR, sizeof(uint64_t), SA_UINT64_ARRAY),
	_ZPL_ATTR(ZPL_RDEV, sizeof(uint64_t), SA_UINT64_ARRAY),
	_ZPL_ATTR(ZPL_FLAGS, sizeof(uint64_t), SA_UINT64_ARRAY),
	_ZPL_ATTR(ZPL_UID, sizeof(uint64_t), SA_UINT64_ARRAY),
	_ZPL_ATTR(ZPL_GID, sizeof(uint64_t), SA_UINT64_ARRAY),
	_ZPL_ATTR(ZPL_PAD, sizeof(uint64_t), SA_UINT64_ARRAY),
	_ZPL_ATTR(ZPL_ZNODE_ACL, 88, SA_UINT64_ARRAY),
	_ZPL_ATTR(ZPL_DACL_COUNT, sizeof(uint64_t), SA_UINT64_ARRAY),
	_ZPL_ATTR(ZPL_SYMLINK, 0, SA_UINT8_ARRAY),
	_ZPL_ATTR(ZPL_SCANSTAMP, sizeof(uint64_t) * 4, SA_UINT8_ARRAY),
	_ZPL_ATTR(ZPL_DACL_ACES, 0, SA_ACL),
	_ZPL_ATTR(ZPL_DXATTR, 0, SA_UINT8_ARRAY),
	_ZPL_ATTR(ZPL_PROJID, sizeof(uint64_t), SA_UINT64_ARRAY),
#undef ZPL_ATTR
};

/*
 * This layout matches that of a filesystem created using OpenZFS on FreeBSD.
 * It need not match in general, but FreeBSD's loader doesn't bother parsing the
 * layout and just hard-codes attribute offsets.
 */
static const sa_attr_type_t zpl_attr_layout[] = {
	ZPL_MODE,
	ZPL_SIZE,
	ZPL_GEN,
	ZPL_UID,
	ZPL_GID,
	ZPL_PARENT,
	ZPL_FLAGS,
	ZPL_ATIME,
	ZPL_MTIME,
	ZPL_CTIME,
	ZPL_CRTIME,
	ZPL_LINKS,
	ZPL_DACL_COUNT,
	ZPL_DACL_ACES,
	ZPL_SYMLINK,
};

/*
 * Keys for the ZPL attribute tables in the SA layout ZAP.  The first two
 * indices are reserved for legacy attribute encoding.
 */
#define	SA_LAYOUT_INDEX_DEFAULT	2
#define	SA_LAYOUT_INDEX_SYMLINK	3

struct fs_populate_dir {
	SLIST_ENTRY(fs_populate_dir) next;
	int			dirfd;
	uint64_t		objid;
	zfs_zap_t		*zap;
};

struct fs_populate_arg {
	zfs_opt_t	*zfs;
	zfs_fs_t	*fs;			/* owning filesystem */
	uint64_t	rootdirid;		/* root directory dnode ID */
	int		rootdirfd;		/* root directory fd */
	SLIST_HEAD(, fs_populate_dir) dirs;	/* stack of directories */
};

static void fs_build_one(zfs_opt_t *, zfs_dsl_dir_t *, fsnode *, int);

static void
eclose(int fd)
{
	if (close(fd) != 0)
		err(1, "close");
}

static bool
fsnode_isroot(const fsnode *cur)
{
	return (strcmp(cur->name, ".") == 0);
}

/*
 * Visit each node in a directory hierarchy, in pre-order depth-first order.
 */
static void
fsnode_foreach(fsnode *root, int (*cb)(fsnode *, void *), void *arg)
{
	assert(root->type == S_IFDIR);

	for (fsnode *cur = root; cur != NULL; cur = cur->next) {
		assert(cur->type == S_IFREG || cur->type == S_IFDIR ||
		    cur->type == S_IFLNK);

		if (cb(cur, arg) == 0)
			continue;
		if (cur->type == S_IFDIR && cur->child != NULL)
			fsnode_foreach(cur->child, cb, arg);
	}
}

static void
fs_populate_dirent(struct fs_populate_arg *arg, fsnode *cur, uint64_t dnid)
{
	struct fs_populate_dir *dir;
	uint64_t type;

	switch (cur->type) {
	case S_IFREG:
		type = DT_REG;
		break;
	case S_IFDIR:
		type = DT_DIR;
		break;
	case S_IFLNK:
		type = DT_LNK;
		break;
	default:
		assert(0);
	}

	dir = SLIST_FIRST(&arg->dirs);
	zap_add_uint64(dir->zap, cur->name, ZFS_DIRENT_MAKE(type, dnid));
}

static void
fs_populate_attr(zfs_fs_t *fs, char *attrbuf, const void *val, uint16_t ind,
    size_t *szp)
{
	assert(ind < fs->sacnt);
	assert(fs->saoffs[ind] != 0xffff);

	memcpy(attrbuf + fs->saoffs[ind], val, fs->satab[ind].size);
	*szp += fs->satab[ind].size;
}

static void
fs_populate_varszattr(zfs_fs_t *fs, char *attrbuf, const void *val,
    size_t valsz, size_t varoff, uint16_t ind, size_t *szp)
{
	assert(ind < fs->sacnt);
	assert(fs->saoffs[ind] != 0xffff);
	assert(fs->satab[ind].size == 0);

	memcpy(attrbuf + fs->saoffs[ind] + varoff, val, valsz);
	*szp += valsz;
}

/*
 * Derive the relative fd/path combo needed to access a file.  Ideally we'd
 * always be able to use relative lookups (i.e., use the *at() system calls),
 * since they require less path translation and are more amenable to sandboxing,
 * but the handling of multiple staging directories makes that difficult.  To
 * make matters worse, we have no choice but to use relative lookups when
 * dealing with an mtree manifest, so both mechanisms are implemented.
 */
static void
fs_populate_path(const fsnode *cur, struct fs_populate_arg *arg,
    char *path, size_t sz, int *dirfdp)
{
	if (cur->contents != NULL) {
		size_t n;

		*dirfdp = AT_FDCWD;
		n = strlcpy(path, cur->contents, sz);
		assert(n < sz);
	} else if (cur->root == NULL) {
		size_t n;

		*dirfdp = SLIST_FIRST(&arg->dirs)->dirfd;
		n = strlcpy(path, cur->name, sz);
		assert(n < sz);
	} else {
		int n;

		*dirfdp = AT_FDCWD;
		n = snprintf(path, sz, "%s/%s/%s",
		    cur->root, cur->path, cur->name);
		assert(n >= 0);
		assert((size_t)n < sz);
	}
}

static int
fs_open(const fsnode *cur, struct fs_populate_arg *arg, int flags)
{
	char path[PATH_MAX];
	int fd;

	fs_populate_path(cur, arg, path, sizeof(path), &fd);

	fd = openat(fd, path, flags);
	if (fd < 0)
		err(1, "openat(%s)", path);
	return (fd);
}

static void
fs_readlink(const fsnode *cur, struct fs_populate_arg *arg,
    char *buf, size_t bufsz)
{
	char path[PATH_MAX];
	ssize_t n;
	int fd;

	fs_populate_path(cur, arg, path, sizeof(path), &fd);

	n = readlinkat(fd, path, buf, bufsz - 1);
	if (n == -1)
		err(1, "readlinkat(%s)", cur->name);
	buf[n] = '\0';
}

static void
fs_populate_time(zfs_fs_t *fs, char *attrbuf, struct timespec *ts,
    uint16_t ind, size_t *szp)
{
	uint64_t timebuf[2];

	assert(ind < fs->sacnt);
	assert(fs->saoffs[ind] != 0xffff);
	assert(fs->satab[ind].size == sizeof(timebuf));

	timebuf[0] = ts->tv_sec;
	timebuf[1] = ts->tv_nsec;
	fs_populate_attr(fs, attrbuf, timebuf, ind, szp);
}

static void
fs_populate_sattrs(struct fs_populate_arg *arg, const fsnode *cur,
    dnode_phys_t *dnode)
{
	char target[PATH_MAX];
	zfs_fs_t *fs;
	zfs_ace_hdr_t aces[3];
	struct stat *sb;
	sa_hdr_phys_t *sahdr;
	uint64_t daclcount, flags, gen, gid, links, mode, parent, objsize, uid;
	char *attrbuf;
	size_t bonussz, hdrsz;
	int layout;

	assert(dnode->dn_bonustype == DMU_OT_SA);
	assert(dnode->dn_nblkptr == 1);

	fs = arg->fs;
	sb = &cur->inode->st;

	switch (cur->type) {
	case S_IFREG:
		layout = SA_LAYOUT_INDEX_DEFAULT;
		links = cur->inode->nlink;
		objsize = sb->st_size;
		parent = SLIST_FIRST(&arg->dirs)->objid;
		break;
	case S_IFDIR:
		layout = SA_LAYOUT_INDEX_DEFAULT;
		links = 1; /* .. */
		objsize = 1; /* .. */

		/*
		 * The size of a ZPL directory is the number of entries
		 * (including "." and ".."), and the link count is the number of
		 * entries which are directories (including "." and "..").
		 */
		for (fsnode *c = fsnode_isroot(cur) ? cur->next : cur->child;
		    c != NULL; c = c->next) {
			if (c->type == S_IFDIR)
				links++;
			objsize++;
		}

		/* The root directory is its own parent. */
		parent = SLIST_EMPTY(&arg->dirs) ?
		    arg->rootdirid : SLIST_FIRST(&arg->dirs)->objid;
		break;
	case S_IFLNK:
		fs_readlink(cur, arg, target, sizeof(target));

		layout = SA_LAYOUT_INDEX_SYMLINK;
		links = 1;
		objsize = strlen(target);
		parent = SLIST_FIRST(&arg->dirs)->objid;
		break;
	default:
		assert(0);
	}

	daclcount = nitems(aces);
	flags = ZFS_ACL_TRIVIAL | ZFS_ACL_AUTO_INHERIT | ZFS_NO_EXECS_DENIED |
	    ZFS_ARCHIVE | ZFS_AV_MODIFIED; /* XXX-MJ */
	gen = 1;
	gid = sb->st_gid;
	mode = sb->st_mode;
	uid = sb->st_uid;

	memset(aces, 0, sizeof(aces));
	aces[0].z_flags = ACE_OWNER;
	aces[0].z_type = ACE_ACCESS_ALLOWED_ACE_TYPE;
	aces[0].z_access_mask = ACE_WRITE_ATTRIBUTES | ACE_WRITE_OWNER |
	    ACE_WRITE_ACL | ACE_WRITE_NAMED_ATTRS | ACE_READ_ACL |
	    ACE_READ_ATTRIBUTES | ACE_READ_NAMED_ATTRS | ACE_SYNCHRONIZE;
	if ((mode & S_IRUSR) != 0)
		aces[0].z_access_mask |= ACE_READ_DATA;
	if ((mode & S_IWUSR) != 0)
		aces[0].z_access_mask |= ACE_WRITE_DATA | ACE_APPEND_DATA;
	if ((mode & S_IXUSR) != 0)
		aces[0].z_access_mask |= ACE_EXECUTE;

	aces[1].z_flags = ACE_GROUP | ACE_IDENTIFIER_GROUP;
	aces[1].z_type = ACE_ACCESS_ALLOWED_ACE_TYPE;
	aces[1].z_access_mask = ACE_READ_ACL | ACE_READ_ATTRIBUTES |
	    ACE_READ_NAMED_ATTRS | ACE_SYNCHRONIZE;
	if ((mode & S_IRGRP) != 0)
		aces[1].z_access_mask |= ACE_READ_DATA;
	if ((mode & S_IWGRP) != 0)
		aces[1].z_access_mask |= ACE_WRITE_DATA | ACE_APPEND_DATA;
	if ((mode & S_IXGRP) != 0)
		aces[1].z_access_mask |= ACE_EXECUTE;

	aces[2].z_flags = ACE_EVERYONE;
	aces[2].z_type = ACE_ACCESS_ALLOWED_ACE_TYPE;
	aces[2].z_access_mask = ACE_READ_ACL | ACE_READ_ATTRIBUTES |
	    ACE_READ_NAMED_ATTRS | ACE_SYNCHRONIZE;
	if ((mode & S_IROTH) != 0)
		aces[2].z_access_mask |= ACE_READ_DATA;
	if ((mode & S_IWOTH) != 0)
		aces[2].z_access_mask |= ACE_WRITE_DATA | ACE_APPEND_DATA;
	if ((mode & S_IXOTH) != 0)
		aces[2].z_access_mask |= ACE_EXECUTE;

	switch (layout) {
	case SA_LAYOUT_INDEX_DEFAULT:
		/* At most one variable-length attribute. */
		hdrsz = sizeof(uint64_t);
		break;
	case SA_LAYOUT_INDEX_SYMLINK:
		/* At most five variable-length attributes. */
		hdrsz = sizeof(uint64_t) * 2;
		break;
	default:
		assert(0);
	}

	sahdr = (sa_hdr_phys_t *)DN_BONUS(dnode);
	sahdr->sa_magic = SA_MAGIC;
	SA_HDR_LAYOUT_INFO_ENCODE(sahdr->sa_layout_info, layout, hdrsz);

	bonussz = SA_HDR_SIZE(sahdr);
	attrbuf = (char *)sahdr + SA_HDR_SIZE(sahdr);

	fs_populate_attr(fs, attrbuf, &daclcount, ZPL_DACL_COUNT, &bonussz);
	fs_populate_attr(fs, attrbuf, &flags, ZPL_FLAGS, &bonussz);
	fs_populate_attr(fs, attrbuf, &gen, ZPL_GEN, &bonussz);
	fs_populate_attr(fs, attrbuf, &gid, ZPL_GID, &bonussz);
	fs_populate_attr(fs, attrbuf, &links, ZPL_LINKS, &bonussz);
	fs_populate_attr(fs, attrbuf, &mode, ZPL_MODE, &bonussz);
	fs_populate_attr(fs, attrbuf, &parent, ZPL_PARENT, &bonussz);
	fs_populate_attr(fs, attrbuf, &objsize, ZPL_SIZE, &bonussz);
	fs_populate_attr(fs, attrbuf, &uid, ZPL_UID, &bonussz);

	/*
	 * We deliberately set atime = mtime here to ensure that images are
	 * reproducible.
	 */
	fs_populate_time(fs, attrbuf, &sb->st_mtim, ZPL_ATIME, &bonussz);
	fs_populate_time(fs, attrbuf, &sb->st_ctim, ZPL_CTIME, &bonussz);
	fs_populate_time(fs, attrbuf, &sb->st_mtim, ZPL_MTIME, &bonussz);
#ifdef __linux__
	/* Linux has no st_birthtim; approximate with st_ctim */
	fs_populate_time(fs, attrbuf, &sb->st_ctim, ZPL_CRTIME, &bonussz);
#else
	fs_populate_time(fs, attrbuf, &sb->st_birthtim, ZPL_CRTIME, &bonussz);
#endif

	fs_populate_varszattr(fs, attrbuf, aces, sizeof(aces), 0,
	    ZPL_DACL_ACES, &bonussz);
	sahdr->sa_lengths[0] = sizeof(aces);

	if (cur->type == S_IFLNK) {
		assert(layout == SA_LAYOUT_INDEX_SYMLINK);
		/* Need to use a spill block pointer if the target is long. */
		assert(bonussz + objsize <= DN_OLD_MAX_BONUSLEN);
		fs_populate_varszattr(fs, attrbuf, target, objsize,
		    sahdr->sa_lengths[0], ZPL_SYMLINK, &bonussz);
		sahdr->sa_lengths[1] = (uint16_t)objsize;
	}

	dnode->dn_bonuslen = bonussz;
}

static void
fs_populate_file(fsnode *cur, struct fs_populate_arg *arg)
{
	struct dnode_cursor *c;
	dnode_phys_t *dnode;
	zfs_opt_t *zfs;
	char *buf;
	uint64_t dnid;
	ssize_t n;
	size_t bufsz;
	off_t size, target;
	int fd;

	assert(cur->type == S_IFREG);
	assert((cur->inode->flags & FI_ROOT) == 0);

	zfs = arg->zfs;

	assert(cur->inode->ino != 0);
	if ((cur->inode->flags & FI_ALLOCATED) != 0) {
		/*
		 * This is a hard link of an existing file.
		 *
		 * XXX-MJ need to check whether it crosses datasets, add a test
		 * case for that
		 */
		fs_populate_dirent(arg, cur, cur->inode->ino);
		return;
	}

	dnode = objset_dnode_bonus_alloc(arg->fs->os,
	    DMU_OT_PLAIN_FILE_CONTENTS, DMU_OT_SA, 0, &dnid);
	cur->inode->ino = dnid;
	cur->inode->flags |= FI_ALLOCATED;

	fd = fs_open(cur, arg, O_RDONLY);

	buf = zfs->filebuf;
	bufsz = sizeof(zfs->filebuf);
	size = cur->inode->st.st_size;
	c = dnode_cursor_init(zfs, arg->fs->os, dnode, size, 0);
	for (off_t foff = 0; foff < size; foff += target) {
		off_t loc, sofar;

		/*
		 * Fill up our buffer, handling partial reads.
		 *
		 * It might be profitable to use copy_file_range(2) here.
		 */
		sofar = 0;
		target = MIN(size - foff, (off_t)bufsz);
		do {
			n = read(fd, buf + sofar, target);
			if (n < 0)
				err(1, "reading from '%s'", cur->name);
			if (n == 0)
				errx(1, "unexpected EOF reading '%s'",
				    cur->name);
			sofar += n;
		} while (sofar < target);

		if (target < (off_t)bufsz)
			memset(buf + target, 0, bufsz - target);

		loc = objset_space_alloc(zfs, arg->fs->os, &target);
		vdev_pwrite_dnode_indir(zfs, dnode, 0, 1, buf, target, loc,
		    dnode_cursor_next(zfs, c, foff));
	}
	eclose(fd);
	dnode_cursor_finish(zfs, c);

	fs_populate_sattrs(arg, cur, dnode);
	fs_populate_dirent(arg, cur, dnid);
}

static void
fs_populate_dir(fsnode *cur, struct fs_populate_arg *arg)
{
	dnode_phys_t *dnode;
	zfs_objset_t *os;
	uint64_t dnid;
	int dirfd;

	assert(cur->type == S_IFDIR);
	assert((cur->inode->flags & FI_ALLOCATED) == 0);

	os = arg->fs->os;

	dnode = objset_dnode_bonus_alloc(os, DMU_OT_DIRECTORY_CONTENTS,
	    DMU_OT_SA, 0, &dnid);

	/*
	 * Add an entry to the parent directory and open this directory.
	 */
	if (!SLIST_EMPTY(&arg->dirs)) {
		fs_populate_dirent(arg, cur, dnid);
		dirfd = fs_open(cur, arg, O_DIRECTORY | O_RDONLY);
	} else {
		arg->rootdirid = dnid;
		dirfd = arg->rootdirfd;
		arg->rootdirfd = -1;
	}

	/*
	 * Set ZPL attributes.
	 */
	fs_populate_sattrs(arg, cur, dnode);

	/*
	 * If this is a root directory, then its children belong to a different
	 * dataset and this directory remains empty in the current objset.
	 */
	if ((cur->inode->flags & FI_ROOT) == 0) {
		struct fs_populate_dir *dir;

		dir = ecalloc(1, sizeof(*dir));
		dir->dirfd = dirfd;
		dir->objid = dnid;
		dir->zap = zap_alloc(os, dnode);
		SLIST_INSERT_HEAD(&arg->dirs, dir, next);
	} else {
		zap_write(arg->zfs, zap_alloc(os, dnode));
		fs_build_one(arg->zfs, cur->inode->param, cur->child, dirfd);
	}
}

static void
fs_populate_symlink(fsnode *cur, struct fs_populate_arg *arg)
{
	dnode_phys_t *dnode;
	uint64_t dnid;

	assert(cur->type == S_IFLNK);
	assert((cur->inode->flags & (FI_ALLOCATED | FI_ROOT)) == 0);

	dnode = objset_dnode_bonus_alloc(arg->fs->os,
	    DMU_OT_PLAIN_FILE_CONTENTS, DMU_OT_SA, 0, &dnid);

	fs_populate_dirent(arg, cur, dnid);

	fs_populate_sattrs(arg, cur, dnode);
}

static int
fs_foreach_populate(fsnode *cur, void *_arg)
{
	struct fs_populate_arg *arg;
	struct fs_populate_dir *dir;
	int ret;

	arg = _arg;
	switch (cur->type) {
	case S_IFREG:
		fs_populate_file(cur, arg);
		break;
	case S_IFDIR:
		if (fsnode_isroot(cur))
			break;
		fs_populate_dir(cur, arg);
		break;
	case S_IFLNK:
		fs_populate_symlink(cur, arg);
		break;
	default:
		assert(0);
	}

	ret = (cur->inode->flags & FI_ROOT) != 0 ? 0 : 1;

	if (cur->next == NULL &&
	    (cur->child == NULL || (cur->inode->flags & FI_ROOT) != 0)) {
		/*
		 * We reached a terminal node in a subtree.  Walk back up and
		 * write out directories.  We're done once we hit the root of a
		 * dataset or find a level where we're not on the edge of the
		 * tree.
		 */
		do {
			dir = SLIST_FIRST(&arg->dirs);
			SLIST_REMOVE_HEAD(&arg->dirs, next);
			zap_write(arg->zfs, dir->zap);
			if (dir->dirfd != -1)
				eclose(dir->dirfd);
			free(dir);
			cur = cur->parent;
		} while (cur != NULL && cur->next == NULL &&
		    (cur->inode->flags & FI_ROOT) == 0);
	}

	return (ret);
}

static void
fs_add_zpl_attr_layout(zfs_zap_t *zap, unsigned int index,
    const sa_attr_type_t layout[], size_t sacnt)
{
	char ti[16];

	assert(sizeof(layout[0]) == 2);

	snprintf(ti, sizeof(ti), "%u", index);
	zap_add(zap, ti, sizeof(sa_attr_type_t), sacnt,
	    (const uint8_t *)layout);
}

/*
 * Initialize system attribute tables.
 *
 * There are two elements to this.  First, we write the zpl_attrs[] and
 * zpl_attr_layout[] tables to disk.  Then we create a lookup table which
 * allows us to set file attributes quickly.
 */
static uint64_t
fs_set_zpl_attrs(zfs_opt_t *zfs, zfs_fs_t *fs)
{
	zfs_zap_t *sazap, *salzap, *sarzap;
	zfs_objset_t *os;
	dnode_phys_t *saobj, *salobj, *sarobj;
	uint64_t saobjid, salobjid, sarobjid;
	uint16_t offset;

	os = fs->os;

	/*
	 * The on-disk tables are stored in two ZAP objects, the registry object
	 * and the layout object.  Individual attributes are described by
	 * entries in the registry object; for example, the value for the
	 * "ZPL_SIZE" key gives the size and encoding of the ZPL_SIZE attribute.
	 * The attributes of a file are ordered according to one of the layouts
	 * defined in the layout object.  The master node object is simply used
	 * to locate the registry and layout objects.
	 */
	saobj = objset_dnode_alloc(os, DMU_OT_SA_MASTER_NODE, &saobjid);
	salobj = objset_dnode_alloc(os, DMU_OT_SA_ATTR_LAYOUTS, &salobjid);
	sarobj = objset_dnode_alloc(os, DMU_OT_SA_ATTR_REGISTRATION, &sarobjid);

	sarzap = zap_alloc(os, sarobj);
	for (size_t i = 0; i < nitems(zpl_attrs); i++) {
		const zfs_sattr_t *sa;
		uint64_t attr;

		attr = 0;
		sa = &zpl_attrs[i];
		SA_ATTR_ENCODE(attr, (uint64_t)i, sa->size, sa->bs);
		zap_add_uint64(sarzap, sa->name, attr);
	}
	zap_write(zfs, sarzap);

	/*
	 * Layouts are arrays of indices into the registry.  We define two
	 * layouts for use by the ZPL, one for non-symlinks and one for
	 * symlinks.  They are identical except that the symlink layout includes
	 * ZPL_SYMLINK as its final attribute.
	 */
	salzap = zap_alloc(os, salobj);
	assert(zpl_attr_layout[nitems(zpl_attr_layout) - 1] == ZPL_SYMLINK);
	fs_add_zpl_attr_layout(salzap, SA_LAYOUT_INDEX_DEFAULT,
	    zpl_attr_layout, nitems(zpl_attr_layout) - 1);
	fs_add_zpl_attr_layout(salzap, SA_LAYOUT_INDEX_SYMLINK,
	    zpl_attr_layout, nitems(zpl_attr_layout));
	zap_write(zfs, salzap);

	sazap = zap_alloc(os, saobj);
	zap_add_uint64(sazap, SA_LAYOUTS, salobjid);
	zap_add_uint64(sazap, SA_REGISTRY, sarobjid);
	zap_write(zfs, sazap);

	/* Sanity check. */
	for (size_t i = 0; i < nitems(zpl_attrs); i++)
		assert(i == zpl_attrs[i].id);

	/*
	 * Build the offset table used when setting file attributes.  File
	 * attributes are stored in the object's bonus buffer; this table
	 * provides the buffer offset of attributes referenced by the layout
	 * table.
	 */
	fs->sacnt = nitems(zpl_attrs);
	fs->saoffs = ecalloc(fs->sacnt, sizeof(*fs->saoffs));
	for (size_t i = 0; i < fs->sacnt; i++)
		fs->saoffs[i] = 0xffff;
	offset = 0;
	for (size_t i = 0; i < nitems(zpl_attr_layout); i++) {
		uint16_t size;

		assert(zpl_attr_layout[i] < fs->sacnt);

		fs->saoffs[zpl_attr_layout[i]] = offset;
		size = zpl_attrs[zpl_attr_layout[i]].size;
		offset += size;
	}
	fs->satab = zpl_attrs;

	return (saobjid);
}

static void
fs_layout_one(zfs_opt_t *zfs, zfs_dsl_dir_t *dsldir, void *arg)
{
	char *mountpoint, *origmountpoint, *name, *next;
	fsnode *cur, *root;
	uint64_t canmount;

	if (!dsl_dir_has_dataset(dsldir))
		return;

	if (dsl_dir_get_canmount(dsldir, &canmount) == 0 && canmount == 0)
		return;
	mountpoint = dsl_dir_get_mountpoint(zfs, dsldir);
	if (mountpoint == NULL)
		return;

	/*
	 * If we were asked to specify a bootfs, set it here.
	 */
	if (zfs->bootfs != NULL && strcmp(zfs->bootfs,
	    dsl_dir_fullname(dsldir)) == 0) {
		zap_add_uint64(zfs->poolprops, "bootfs",
		    dsl_dir_dataset_id(dsldir));
	}

	origmountpoint = mountpoint;

	/*
	 * Figure out which fsnode corresponds to our mountpoint.
	 */
	root = arg;
	cur = root;
	if (strcmp(mountpoint, zfs->rootpath) != 0) {
		mountpoint += strlen(zfs->rootpath);

		/*
		 * Look up the directory in the staged tree.  For example, if
		 * the dataset's mount point is /foo/bar/baz, we'll search the
		 * root directory for "foo", search "foo" for "baz", and so on.
		 * Each intermediate name must refer to a directory; the final
		 * component need not exist.
		 */
		cur = root;
		for (next = name = mountpoint; next != NULL;) {
			for (; *next == '/'; next++)
				;
			name = strsep(&next, "/");

			for (; cur != NULL && strcmp(cur->name, name) != 0;
			    cur = cur->next)
				;
			if (cur == NULL) {
				if (next == NULL)
					break;
				errx(1, "missing mountpoint directory for `%s'",
				    dsl_dir_fullname(dsldir));
			}
			if (cur->type != S_IFDIR) {
				errx(1,
				    "mountpoint for `%s' is not a directory",
				    dsl_dir_fullname(dsldir));
			}
			if (next != NULL)
				cur = cur->child;
		}
	}

	if (cur != NULL) {
		assert(cur->type == S_IFDIR);

		/*
		 * Multiple datasets shouldn't share a mountpoint.  It's
		 * technically allowed, but it's not clear what makefs should do
		 * in that case.
		 */
		assert((cur->inode->flags & FI_ROOT) == 0);
		if (cur != root)
			cur->inode->flags |= FI_ROOT;
		assert(cur->inode->param == NULL);
		cur->inode->param = dsldir;
	}

	free(origmountpoint);
}

static int
fs_foreach_mark(fsnode *cur, void *arg)
{
	uint64_t *countp;

	countp = arg;
	if (cur->type == S_IFDIR && fsnode_isroot(cur))
		return (1);

	if (cur->inode->ino == 0) {
		cur->inode->ino = ++(*countp);
		cur->inode->nlink = 1;
	} else {
		cur->inode->nlink++;
	}

	return ((cur->inode->flags & FI_ROOT) != 0 ? 0 : 1);
}

/*
 * Create a filesystem dataset.  More specifically:
 * - create an object set for the dataset,
 * - add required metadata (SA tables, property definitions, etc.) to that
 *   object set,
 * - optionally populate the object set with file objects, using "root" as the
 *   root directory.
 *
 * "dirfd" is a directory descriptor for the directory referenced by "root".  It
 * is closed before returning.
 */
static void
fs_build_one(zfs_opt_t *zfs, zfs_dsl_dir_t *dsldir, fsnode *root, int dirfd)
{
	struct fs_populate_arg arg;
	zfs_fs_t fs;
	zfs_zap_t *masterzap;
	zfs_objset_t *os;
	dnode_phys_t *deleteq, *masterobj;
	uint64_t deleteqid, dnodecount, moid, rootdirid, saobjid;
	bool fakedroot;

	/*
	 * This dataset's mountpoint doesn't exist in the staging tree, or the
	 * dataset doesn't have a mountpoint at all.  In either case we still
	 * need a root directory.  Fake up a root fsnode to handle this case.
	 */
	fakedroot = root == NULL;
	if (fakedroot) {
		struct stat *stp;

		assert(dirfd == -1);

		root = ecalloc(1, sizeof(*root));
		root->inode = ecalloc(1, sizeof(*root->inode));
		root->name = estrdup(".");
		root->type = S_IFDIR;

		stp = &root->inode->st;
		stp->st_uid = 0;
		stp->st_gid = 0;
		stp->st_mode = S_IFDIR | 0755;
	}
	assert(root->type == S_IFDIR);
	assert(fsnode_isroot(root));

	/*
	 * Initialize the object set for this dataset.
	 */
	os = objset_alloc(zfs, DMU_OST_ZFS);
	masterobj = objset_dnode_alloc(os, DMU_OT_MASTER_NODE, &moid);
	assert(moid == MASTER_NODE_OBJ);

	memset(&fs, 0, sizeof(fs));
	fs.os = os;

	/*
	 * Create the ZAP SA layout now since filesystem object dnodes will
	 * refer to those attributes.
	 */
	saobjid = fs_set_zpl_attrs(zfs, &fs);

	/*
	 * Make a pass over the staged directory to detect hard links and assign
	 * virtual dnode numbers.
	 */
	dnodecount = 1; /* root directory */
	fsnode_foreach(root, fs_foreach_mark, &dnodecount);

	/*
	 * Make a second pass to populate the dataset with files from the
	 * staged directory.  Most of our runtime is spent here.
	 */
	arg.rootdirfd = dirfd;
	arg.zfs = zfs;
	arg.fs = &fs;
	SLIST_INIT(&arg.dirs);
	fs_populate_dir(root, &arg);
	assert(!SLIST_EMPTY(&arg.dirs));
	fsnode_foreach(root, fs_foreach_populate, &arg);
	assert(SLIST_EMPTY(&arg.dirs));
	rootdirid = arg.rootdirid;

	/*
	 * Create an empty delete queue.  We don't do anything with it, but
	 * OpenZFS will refuse to mount filesystems that don't have one.
	 */
	deleteq = objset_dnode_alloc(os, DMU_OT_UNLINKED_SET, &deleteqid);
	zap_write(zfs, zap_alloc(os, deleteq));

	/*
	 * Populate and write the master node object.  This is a ZAP object
	 * containing various dataset properties and the object IDs of the root
	 * directory and delete queue.
	 */
	masterzap = zap_alloc(os, masterobj);
	zap_add_uint64(masterzap, ZFS_ROOT_OBJ, rootdirid);
	zap_add_uint64(masterzap, ZFS_UNLINKED_SET, deleteqid);
	zap_add_uint64(masterzap, ZFS_SA_ATTRS, saobjid);
	zap_add_uint64(masterzap, ZPL_VERSION_OBJ, 5 /* ZPL_VERSION_SA */);
	zap_add_uint64(masterzap, "normalization", 0 /* off */);
	zap_add_uint64(masterzap, "utf8only", 0 /* off */);
	zap_add_uint64(masterzap, "casesensitivity", 0 /* case sensitive */);
	zap_add_uint64(masterzap, "acltype", 2 /* NFSv4 */);
	zap_write(zfs, masterzap);

	/*
	 * All finished with this object set, we may as well write it now.
	 * The DSL layer will sum up the bytes consumed by each dataset using
	 * information stored in the object set, so it can't be freed just yet.
	 */
	dsl_dir_dataset_write(zfs, os, dsldir);

	if (fakedroot) {
		free(root->inode);
		free(root->name);
		free(root);
	}
	free(fs.saoffs);
}

/*
 * Create an object set for each DSL directory which has a dataset and doesn't
 * already have an object set.
 */
static void
fs_build_unmounted(zfs_opt_t *zfs, zfs_dsl_dir_t *dsldir, void *arg __unused)
{
	if (dsl_dir_has_dataset(dsldir) && !dsl_dir_dataset_has_objset(dsldir))
		fs_build_one(zfs, dsldir, NULL, -1);
}

/*
 * Create our datasets and populate them with files.
 */
void
fs_build(zfs_opt_t *zfs, int dirfd, fsnode *root)
{
	/*
	 * Run through our datasets and find the root fsnode for each one.  Each
	 * root fsnode is flagged so that we can figure out which dataset it
	 * belongs to.
	 */
	dsl_dir_foreach(zfs, zfs->rootdsldir, fs_layout_one, root);

	/*
	 * Did we find our boot filesystem?
	 */
	if (zfs->bootfs != NULL && !zap_entry_exists(zfs->poolprops, "bootfs"))
		errx(1, "no mounted dataset matches bootfs property `%s'",
		    zfs->bootfs);

	/*
	 * Traverse the file hierarchy starting from the root fsnode.  One
	 * dataset, not necessarily the root dataset, must "own" the root
	 * directory by having its mountpoint be equal to the root path.
	 *
	 * As roots of other datasets are encountered during the traversal,
	 * fs_build_one() recursively creates the corresponding object sets and
	 * populates them.  Once this function has returned, all datasets will
	 * have been fully populated.
	 */
	fs_build_one(zfs, root->inode->param, root, dirfd);

	/*
	 * Now create object sets for datasets whose mountpoints weren't found
	 * in the staging directory, either because there is no mountpoint, or
	 * because the mountpoint doesn't correspond to an existing directory.
	 */
	dsl_dir_foreach(zfs, zfs->rootdsldir, fs_build_unmounted, NULL);
}
