/*
 * Copyright (c) 1998 Robert Nordier
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms are freely
 * permitted provided that the above copyright notice and this
 * paragraph and the following disclaimer are duplicated in all
 * such forms.
 *
 * This software is provided "AS IS" and without any express or
 * implied warranties, including, without limitation, the implied
 * warranties of merchantability and fitness for a particular
 * purpose.
 */

/*
 * $FreeBSD$
 */

#include <ufs/ffs/fs.h>
#include <ufs/ufs/dinode.h>

/*
 * We use 4k `virtual' blocks for filesystem data, whatever the actual
 * filesystem block size. FFS blocks are always a multiple of 4k.
 */
#define VBLKSIZE	4096
#define VBLKMASK	(VBLKSIZE - 1)
#define DBPERVBLK	(VBLKSIZE / DEV_BSIZE)
#define IPERVBLK	(VBLKSIZE / sizeof(struct dinode))
#define INDIRPERVBLK	(VBLKSIZE / sizeof(ufs_daddr_t))
#define INO_TO_VBA(fs, x) (fsbtodb(fs, ino_to_fsba(fs, x)) + \
    (ino_to_fsbo(fs, x) / IPERVBLK) * DBPERVBLK)
#define INO_TO_VBO(fs, x) (ino_to_fsbo(fs, x) % IPERVBLK)
#define FS_TO_VBA(fs, fsb, off) (fsbtodb(fs, fsb) + \
    ((off) / VBLKSIZE) * DBPERVBLK)
#define FS_TO_VBO(fs, fsb, off) ((off) & VBLKMASK)

/* Buffers that must not span a 64k boundary. */
static struct dmadat {
	char blkbuf[VBLKSIZE];				/* filesystem blocks */
	ufs_daddr_t indbuf[VBLKSIZE / sizeof(ufs_daddr_t)]; /* indir blocks */
	char sbbuf[SBSIZE];				/* superblock */
	char secbuf[DEV_BSIZE];				/* for MBR/disklabel */
} *dmadat;

static ino_t lookup(const char *);
static ssize_t fsread(ino_t, void *, size_t);

static int ls, dsk_meta;
static uint32_t fs_off;

static inline int
fsfind(const char *name, ino_t * ino)
{
	char buf[DEV_BSIZE];
	struct dirent *d;
	char *s;
	ssize_t n;

	fs_off = 0;
	while ((n = fsread(*ino, buf, DEV_BSIZE)) > 0)
	for (s = buf; s < buf + DEV_BSIZE;) {
		d = (void *)s;
		if (ls)
			printf("%s ", d->d_name);
		else if (!strcmp(name, d->d_name)) {
			*ino = d->d_fileno;
			return d->d_type;
		}
		s += d->d_reclen;
	}
	if (n != -1 && ls)
		putchar('\n');
	return 0;
}

static ino_t
lookup(const char *path)
{
	char name[MAXNAMLEN + 1];
	const char *s;
	ino_t ino;
	ssize_t n;
	int dt;

	ino = ROOTINO;
	dt = DT_DIR;
	for (;;) {
		if (*path == '/')
			path++;
		if (!*path)
			break;
		for (s = path; *s && *s != '/'; s++);
			if ((n = s - path) > MAXNAMLEN)
				return 0;
		ls = *path == '?' && n == 1 && !*s;
		memcpy(name, path, n);
		name[n] = 0;
		if ((dt = fsfind(name, &ino)) <= 0)
			break;
		path = s;
	}
	return dt == DT_REG ? ino : 0;
}

static ssize_t
fsread(ino_t inode, void *buf, size_t nbyte)
{
	static struct dinode din;
	static ino_t inomap;
	static daddr_t blkmap, indmap;
	char *blkbuf;
	ufs_daddr_t *indbuf;
	struct fs *fs;
	char *s;
	ufs_daddr_t lbn, addr;
	daddr_t vbaddr;
	size_t n, nb, off, vboff;

	blkbuf = dmadat->blkbuf;
	indbuf = dmadat->indbuf;
	fs = (struct fs *)dmadat->sbbuf;
	if (!dsk_meta) {
		inomap = 0;
		if (dskread(fs, SBOFF / DEV_BSIZE, SBSIZE / DEV_BSIZE))
			return -1;
		if (fs->fs_magic != FS_MAGIC) {
			printf("Not ufs\n");
			return -1;
		}
		dsk_meta++;
	}
	if (!inode)
		return 0;
	if (inomap != inode) {
		if (dskread(blkbuf, INO_TO_VBA(fs, inode), DBPERVBLK))
			return -1;
		din = ((struct dinode *)blkbuf)[INO_TO_VBO(fs, inode)];
		inomap = inode;
		fs_off = 0;
		blkmap = indmap = 0;
	}
	s = buf;
	if (nbyte > (n = din.di_size - fs_off))
		nbyte = n;
	nb = nbyte;
	while (nb) {
		lbn = lblkno(fs, fs_off);
		off = blkoff(fs, fs_off);
		if (lbn < NDADDR)
			addr = din.di_db[lbn];
		else {
			vbaddr = FS_TO_VBA(fs, din.di_ib[0], sizeof(indbuf[0]) *
			((lbn - NDADDR) % NINDIR(fs)));
			if (indmap != vbaddr) {
				if (dskread(indbuf, vbaddr, DBPERVBLK))
					return -1;
				indmap = vbaddr;
			}
			addr = indbuf[(lbn - NDADDR) % INDIRPERVBLK];
		}
		vbaddr = FS_TO_VBA(fs, addr, off);
		vboff = FS_TO_VBO(fs, addr, off);
		n = dblksize(fs, &din, lbn) - (off & ~VBLKMASK);
		if (n > VBLKSIZE)
			n = VBLKSIZE;
		if (blkmap != vbaddr) {
			if (dskread(blkbuf, vbaddr, n >> DEV_BSHIFT))
				return -1;
			blkmap = vbaddr;
		}
		n -= vboff;
		if (n > nb)
			n = nb;
		memcpy(s, blkbuf + vboff, n);
		s += n;
		fs_off += n;
		nb -= n;
	}
	return nbyte;
}

