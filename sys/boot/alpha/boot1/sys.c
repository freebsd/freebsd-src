/*
 * Mach Operating System
 * Copyright (c) 1992, 1991 Carnegie Mellon University
 * All Rights Reserved.
 *
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND FOR
 * ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie Mellon
 * the rights to redistribute these changes.
 *
 *	from: Mach, Revision 2.2  92/04/04  11:36:34  rpd
 *	fromL Id: sys.c,v 1.21 1997/06/09 05:10:56 bde Exp
 * $FreeBSD$
 */

#include <string.h>
#include <sys/param.h>
#include <sys/time.h>

#include <ufs/ffs/fs.h>
#include <ufs/ufs/quota.h>
#include <ufs/ufs/inode.h>

#include <sys/dirent.h>

#define COMPAT_UFS

struct fs *fs;
struct inode inode;
int boff = 0;

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

static char fsbuf[SBSIZE], iobuf[VBLKSIZE];
ufs_daddr_t mapbuf[VBLKSIZE / sizeof(ufs_daddr_t)];
static int mapblock;

int poff;

#ifdef RAWBOOT
#define STARTBYTE	8192	/* Where on the media the kernel starts */
#endif

static int block_map(int file_block);
static int find(char *path);

int
readit(char *buffer, int count)
{
    int logno, off, size;
    int cnt2, fsblk, bnum2;
    struct fs *fs_copy;
    int n = 0;

    if (poff + count > inode.i_size)
	count = inode.i_size - poff;
    while (count > 0 && poff < inode.i_size) {
	fs_copy = fs;
	off = blkoff(fs_copy, poff);
	logno = lblkno(fs_copy, poff);
	fsblk = block_map(logno);
	cnt2 = blksize(fs_copy, &inode, logno) - (off & ~VBLKMASK);
	if (cnt2 > VBLKSIZE)
		cnt2 = VBLKSIZE;
	size = cnt2;
	bnum2 = FS_TO_VBA(fs_copy, fsblk, off) + boff;
	off = FS_TO_VBO(fs_copy, fsblk, off);
	if (	(!off)  && (size <= count)) {
	    devread(buffer, bnum2, cnt2);
	} else {
	    size -= off;
	    if (size > count)
		size = count;
	    devread(iobuf, bnum2, cnt2);
	    bcopy(iobuf+off, buffer, size);
	}
	buffer += size;
	count -= size;
	poff += size;
	n += size;
    }
    return n;
}

static int
find(char *path)
{
    char *rest, ch;
    int block, blklen, fsboff, off, loc, ino = ROOTINO;
    struct dirent *dp;
    char list_only;

    list_only = (path[0] == '?' && path[1] == '\0');
 loop:
    devread(iobuf, INO_TO_VBA(fs, ino) + boff, VBLKSIZE);
    bcopy((void *)&((struct dinode *)iobuf)[INO_TO_VBO(fs, ino)],
	  (void *)&inode.i_din,
	  sizeof (struct dinode));
    if (!*path)
	return 1;
    while (*path == '/')
	path++;
    if (!inode.i_size || ((inode.i_mode&IFMT) != IFDIR))
	return 0;
    for (rest = path; (ch = *rest) && ch != '/'; rest++) ;
    *rest = 0;
    loc = 0;
    do {
	if (loc >= inode.i_size) {
	    if (list_only) {
		putchar('\n');
		return -1;
	    } else {
		return 0;
	    }
	}
	if (!(off = (loc & VBLKMASK))) {
	    block = lblkno(fs, loc);
	    fsboff = blkoff(fs, loc);
	    blklen = blksize(fs, &inode, block) - fsboff;
	    if (blklen > VBLKSIZE)
		blklen = VBLKSIZE;
	    devread(iobuf, FS_TO_VBA(fs, block_map(block), fsboff) + boff,
		blklen);
	}
	dp = (struct dirent *)(iobuf + off);
	loc += dp->d_reclen;
	if (dp->d_fileno && list_only) {
	    puts(dp->d_name);
	    putchar(' ');
	}
    } while (!dp->d_fileno || strcmp(path, dp->d_name));
    ino = dp->d_fileno;
    *(path = rest) = ch;
    goto loop;
}


static int
block_map(int file_block)
{
	int bnum;
	if (file_block < NDADDR)
		return(inode.i_db[file_block]);
	bnum = FS_TO_VBA(fs, inode.i_ib[0], sizeof(mapbuf[0]) *
	    ((file_block - NDADDR) % NINDIR(fs))) + boff;
	if (bnum != mapblock) {
		devread(mapbuf, bnum, VBLKSIZE);
		mapblock = bnum;
	}
	return (mapbuf[(file_block - NDADDR) % INDIRPERVBLK]);
}

#ifdef COMPAT_UFS

#define max(a, b)	((a) > (b) ? (a) : (b))

/*
 * Sanity checks for old file systems.
 *
 * XXX - goes away some day.
 */
static void
ffs_oldfscompat(fs)
	struct fs *fs;
{
	int i;

	fs->fs_npsect = max(fs->fs_npsect, fs->fs_nsect);	/* XXX */
	fs->fs_interleave = max(fs->fs_interleave, 1);		/* XXX */
	if (fs->fs_postblformat == FS_42POSTBLFMT)		/* XXX */
		fs->fs_nrpos = 8;				/* XXX */
	if (fs->fs_inodefmt < FS_44INODEFMT) {			/* XXX */
		quad_t sizepb = fs->fs_bsize;			/* XXX */
								/* XXX */
		fs->fs_maxfilesize = fs->fs_bsize * NDADDR - 1;	/* XXX */
		for (i = 0; i < NIADDR; i++) {			/* XXX */
			sizepb *= NINDIR(fs);			/* XXX */
			fs->fs_maxfilesize += sizepb;		/* XXX */
		}						/* XXX */
		fs->fs_qbmask = ~fs->fs_bmask;			/* XXX */
		fs->fs_qfmask = ~fs->fs_fmask;			/* XXX */
	}							/* XXX */
}
#endif

int
openrd(char *name)
{
    int ret;
    char namecopy[128];

    if (devopen())
	return 1;

    /*
     * Load Filesystem info (mount the device).
     */
    devread((char *)(fs = (struct fs *)fsbuf), SBLOCK + boff, SBSIZE);

#ifdef COMPAT_UFS
    ffs_oldfscompat(fs);
#endif

    /*
     * Find the actual FILE on the mounted device.
     * Make a copy of the name since find() is destructive.
     */
    strcpy(namecopy, name);
    ret = find(namecopy);
    if (ret == 0)
	return 1;
    if (ret < 0)
	return -1;
    poff = 0;
    return 0;
}

void
bcopy(const void *src, void *dst, size_t len) 
{
	const char *s;
	char *d;
		 
	for (d = dst, s = src; len; len--)
		*d++ = *s++;
}
