/*
 * Copyright (c) 2002 Networks Associates Technology, Inc.
 * All rights reserved.
 *
 * This software was developed for the FreeBSD Project by Marshall
 * Kirk McKusick and Network Associates Laboratories, the Security
 * Research Division of Network Associates, Inc. under DARPA/SPAWAR
 * contract N66001-01-C-8035 ("CBOSS"), as part of the DARPA CHATS
 * research program
 *
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

#include <ufs/ufs/dinode.h>
#include <ufs/ffs/fs.h>

/*
 * We use 4k `virtual' blocks for filesystem data, whatever the actual
 * filesystem block size. FFS blocks are always a multiple of 4k.
 */
#define VBLKSHIFT	12
#define VBLKSIZE	(1 << VBLKSHIFT)
#define VBLKMASK	(VBLKSIZE - 1)
#define DBPERVBLK	(VBLKSIZE / DEV_BSIZE)
#define INDIRPERVBLK(fs) (NINDIR(fs) / ((fs)->fs_bsize >> VBLKSHIFT))
#define IPERVBLK(fs)	(INOPB(fs) / ((fs)->fs_bsize >> VBLKSHIFT))
#define INO_TO_VBA(fs, ipervblk, x) \
    (fsbtodb(fs, cgimin(fs, ino_to_cg(fs, x))) + \
    (((x) % (fs)->fs_ipg) / (ipervblk) * DBPERVBLK))
#define INO_TO_VBO(ipervblk, x) ((x) % ipervblk)
#define FS_TO_VBA(fs, fsb, off) (fsbtodb(fs, fsb) + \
    ((off) / VBLKSIZE) * DBPERVBLK)
#define FS_TO_VBO(fs, fsb, off) ((off) & VBLKMASK)

/* Buffers that must not span a 64k boundary. */
struct dmadat {
	char blkbuf[VBLKSIZE];	/* filesystem blocks */
	char indbuf[VBLKSIZE];	/* indir blocks */
	char sbbuf[SBLOCKSIZE];	/* superblock */
	char secbuf[DEV_BSIZE];	/* for MBR/disklabel */
};
static struct dmadat *dmadat;

static ino_t lookup(const char *);
static ssize_t fsread(ino_t, void *, size_t);

static int ls, dsk_meta;
static uint32_t fs_off;

static __inline__ int
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
		printf("\n");
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
	name[0] = '/';
	name[1] = '\0';
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
		if (dt != DT_DIR) {
			printf("%s: not a directory.\n", name);
			return (0);
		}
		if ((dt = fsfind(name, &ino)) <= 0)
			break;
		path = s;
	}
	return dt == DT_REG ? ino : 0;
}

#ifdef UFS1_ONLY

static ssize_t
fsread(ino_t inode, void *buf, size_t nbyte)
{
	static struct ufs1_dinode dp1;
	static ino_t inomap;
	char *blkbuf;
	caddr_t indbuf;
	struct fs *fs;
	char *s;
	size_t n, nb, size, off, vboff;
	long lbn;
	ufs1_daddr_t addr, vbaddr;
	static ufs1_daddr_t blkmap, indmap;

	blkbuf = dmadat->blkbuf;
	indbuf = dmadat->indbuf;
	fs = (struct fs *)dmadat->sbbuf;
	if (!dsk_meta) {
		inomap = 0;
		if (dskread(fs, SBLOCK_UFS1 / DEV_BSIZE, SBLOCKSIZE / DEV_BSIZE))
			return -1;
		if (fs->fs_magic != FS_UFS1_MAGIC) {
			printf("Not ufs\n");
			return -1;
		}
		dsk_meta++;
	}
	if (!inode)
		return 0;
	if (inomap != inode) {
		n = IPERVBLK(fs);
		if (dskread(blkbuf, INO_TO_VBA(fs, n, inode), DBPERVBLK))
			return -1;
		dp1 = ((struct ufs1_dinode *)blkbuf)[INO_TO_VBO(n, inode)];
		inomap = inode;
		fs_off = 0;
		blkmap = indmap = 0;
	}
	s = buf;
	size = dp1.di_size;
	n = size - fs_off;
	if (nbyte > n)
		nbyte = n;
	nb = nbyte;
	while (nb) {
		lbn = lblkno(fs, fs_off);
		off = blkoff(fs, fs_off);
		if (lbn < NDADDR) {
			addr = dp1.di_db[lbn];
		} else {
			n = INDIRPERVBLK(fs);
			addr = dp1.di_ib[0];
			vbaddr = fsbtodb(fs, addr) +
			    (lbn - NDADDR) / (n * DBPERVBLK);
			if (indmap != vbaddr) {
				if (dskread(indbuf, vbaddr, DBPERVBLK))
					return -1;
				indmap = vbaddr;
			}
			addr = ((ufs1_daddr_t *)indbuf)[(lbn - NDADDR) % n];
		}
		vbaddr = fsbtodb(fs, addr) + (off >> VBLKSHIFT) * DBPERVBLK;
		vboff = off & VBLKMASK;
		n = sblksize(fs, size, lbn) - (off & ~VBLKMASK);
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

#else /* UFS1_AND_UFS2 */

/*
 * Possible superblock locations ordered from most to least likely.
 */
static int sblock_try[] = SBLOCKSEARCH;

#define DIP(field) fs->fs_magic == FS_UFS1_MAGIC ? dp1.field : dp2.field

static ssize_t
fsread(ino_t inode, void *buf, size_t nbyte)
{
	static struct ufs1_dinode dp1;
	static struct ufs2_dinode dp2;
	static ino_t inomap;
	char *blkbuf;
	void *indbuf;
	struct fs *fs;
	char *s;
	size_t n, nb, size, off, vboff;
	ufs_lbn_t lbn;
	ufs2_daddr_t addr, vbaddr;
	static ufs2_daddr_t blkmap, indmap;
	u_int u;


	blkbuf = dmadat->blkbuf;
	indbuf = dmadat->indbuf;
	fs = (struct fs *)dmadat->sbbuf;
	if (!dsk_meta) {
		inomap = 0;
		for (n = 0; sblock_try[n] != -1; n++) {
			if (dskread(fs, sblock_try[n] / DEV_BSIZE,
			    SBLOCKSIZE / DEV_BSIZE))
				return -1;
			if ((fs->fs_magic == FS_UFS1_MAGIC ||
			    (fs->fs_magic == FS_UFS2_MAGIC &&
			    fs->fs_sblockloc == sblock_try[n])) &&
			    fs->fs_bsize <= MAXBSIZE &&
			    fs->fs_bsize >= sizeof(struct fs))
				break;
		}
		if (sblock_try[n] == -1) {
			printf("Not ufs\n");
			return -1;
		}
		dsk_meta++;
	}
	if (!inode)
		return 0;
	if (inomap != inode) {
		n = IPERVBLK(fs);
		if (dskread(blkbuf, INO_TO_VBA(fs, n, inode), DBPERVBLK))
			return -1;
		n = INO_TO_VBO(n, inode);
		if (fs->fs_magic == FS_UFS1_MAGIC)
			dp1 = ((struct ufs1_dinode *)blkbuf)[n];
		else
			dp2 = ((struct ufs2_dinode *)blkbuf)[n];
		inomap = inode;
		fs_off = 0;
		blkmap = indmap = 0;
	}
	s = buf;
	size = DIP(di_size);
	n = size - fs_off;
	if (nbyte > n)
		nbyte = n;
	nb = nbyte;
	while (nb) {
		lbn = lblkno(fs, fs_off);
		off = blkoff(fs, fs_off);
		if (lbn < NDADDR) {
			addr = DIP(di_db[lbn]);
		} else if (lbn < NDADDR + NINDIR(fs)) {
			n = INDIRPERVBLK(fs);
			addr = DIP(di_ib[0]);
			u = (u_int)(lbn - NDADDR) / (n * DBPERVBLK);
			vbaddr = fsbtodb(fs, addr) + u;
			if (indmap != vbaddr) {
				if (dskread(indbuf, vbaddr, DBPERVBLK))
					return -1;
				indmap = vbaddr;
			}
			n = (lbn - NDADDR) & (n - 1);
			if (fs->fs_magic == FS_UFS1_MAGIC)
				addr = ((ufs1_daddr_t *)indbuf)[n];
			else
				addr = ((ufs2_daddr_t *)indbuf)[n];
		} else {
			printf("file too big\n");
			return -1;
		}
		vbaddr = fsbtodb(fs, addr) + (off >> VBLKSHIFT) * DBPERVBLK;
		vboff = off & VBLKMASK;
		n = sblksize(fs, size, lbn) - (off & ~VBLKMASK);
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

#endif /* UFS1_AND_UFS2 */
