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
 *	$Id$
 */

#include <sys/param.h>
#include <sys/time.h>

#include <ufs/ffs/fs.h>
#include <ufs/ufs/quota.h>
#include <ufs/ufs/inode.h>

#include <sys/dirent.h>

struct fs *fs;
struct inode inode;
int boff = 0;

#if 0
/* #define BUFSIZE 4096 */
#define BUFSIZE MAXBSIZE

static char buf[BUFSIZE], fsbuf[SBSIZE], iobuf[MAXBSIZE];
#endif

#define BUFSIZE 8192
#define MAPBUFSIZE BUFSIZE
static char buf[BUFSIZE], fsbuf[BUFSIZE], iobuf[BUFSIZE];

static char mapbuf[MAPBUFSIZE];
static int mapblock;

int poff;

#ifdef RAWBOOT
#define STARTBYTE	8192	/* Where on the media the kernel starts */
#endif

static int block_map(int file_block);
static int find(char *path);

int
read(char *buffer, int count)
{
    int logno, off, size;
    int cnt2, bnum2;
    struct fs *fs_copy;
    int n = 0;

    if (poff + count > inode.i_size)
	count = inode.i_size - poff;
    while (count > 0 && poff < inode.i_size) {
	fs_copy = fs;
	off = blkoff(fs_copy, poff);
	logno = lblkno(fs_copy, poff);
	cnt2 = size = blksize(fs_copy, &inode, logno);
	bnum2 = fsbtodb(fs_copy, block_map(logno)) + boff;
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
    int block, off, loc, ino = ROOTINO;
    struct dirent *dp;
    char list_only;

    list_only = (path[0] == '?' && path[1] == '\0');
 loop:
    devread(iobuf, fsbtodb(fs, ino_to_fsba(fs, ino)) + boff, fs->fs_bsize);
    bcopy((void *)&((struct dinode *)iobuf)[ino % fs->fs_inopb],
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
	if (!(off = blkoff(fs, loc))) {
	    block = lblkno(fs, loc);
	    devread(iobuf, fsbtodb(fs, block_map(block)) + boff,
		    blksize(fs, &inode, block));
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
	if ((bnum=fsbtodb(fs, inode.i_ib[0])+boff) != mapblock) {
		devread(mapbuf, bnum, fs->fs_bsize);
		mapblock = bnum;
	}
	return (((int *)mapbuf)[(file_block - NDADDR) % NINDIR(fs)]);
}


int
openrd(char *name)
{
    int ret;

    if (devopen())
	return 1;

    /*
     * Load Filesystem info (mount the device).
     */
    devread((char *)(fs = (struct fs *)fsbuf), SBLOCK + boff, SBSIZE);

    /*
     * Find the actual FILE on the mounted device.
     */
    ret = find(name);
    if (ret == 0)
	return 1;
    if (ret < 0)
	return -1;
    poff = 0;
    return 0;
}
