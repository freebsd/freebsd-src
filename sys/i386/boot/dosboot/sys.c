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
 * $FreeBSD: src/sys/i386/boot/dosboot/sys.c,v 1.5 1999/08/28 00:43:25 peter Exp $
 */
#include <stdio.h>
#include <string.h>
#include <memory.h>

#define bcopy(a,b,c)	memcpy(b,a,c)

#include "protmod.h"
#include "boot.h"
#include "dir.h"

#define BUFSIZE 4096
#undef MAXBSIZE
#define MAXBSIZE 8192

void ufs_read(char *buffer, long count);
static long block_map(long file_block);

char buf[BUFSIZE], fsbuf[SBSIZE], iobuf[MAXBSIZE];
char mapbuf[MAXBSIZE];
long mapblock = 0;

void xread(unsigned long addr, long size)
{
	long count = BUFSIZE;
	while (size > 0l) {
		if (BUFSIZE > size)
			count = size;
		ufs_read(buf, count);
		pm_copy(buf, addr, count);
		size -= count;
		addr += count;
	}
}

void ufs_read(char *buffer, long count)
{
	long logno, off, size;
	long cnt2, bnum2;

	while (count) {
		off = blkoff(fs, poff);
		logno = lblkno(fs, poff);
		cnt2 = size = blksize(fs, &inode, logno);
		bnum2 = fsbtodb(fs, block_map(logno)) + boff;
		cnt = cnt2;
		bnum = bnum2;
		if (	(!off)  && (size <= count))
		{
			iodest = buffer;
			devread();
		}
		else
		{
			iodest = iobuf;
			size -= off;
			if (size > count)
				size = count;
			devread();
			bcopy(iodest+off,buffer,size);
		}
		buffer += size;
		count -= size;
		poff += size;
	}
}

static int find(char *path)
{
	char *rest, ch;
	long block, off, loc, ino = ROOTINO;
	struct direct *dp;
loop:	iodest = iobuf;
	cnt = fs->fs_bsize;
	bnum = fsbtodb(fs,itod(fs,ino)) + boff;
	devread();
	bcopy(&((struct dinode *)iodest)[ino % fs->fs_inopb],
	      &inode.i_din,
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
		if (loc >= inode.i_size)
			return 0;
		if (!(off = blkoff(fs, loc))) {
			block = lblkno(fs, loc);
			cnt = blksize(fs, &inode, block);
			bnum = fsbtodb(fs, block_map(block)) + boff;
			iodest = iobuf;
			devread();
		}
		dp = (struct direct *)(iodest + off);
		loc += dp->d_reclen;
	} while (!dp->d_ino || strcmp(path, dp->d_name));
	ino = dp->d_ino;
	*(path = rest) = ch;
	goto loop;
}

static long block_map(long file_block)
{
	if (file_block < NDADDR)
		return(inode.i_db[file_block]);
	if ((bnum=fsbtodb(fs, inode.i_ib[0])+boff) != mapblock) {
		iodest = mapbuf;
		cnt = fs->fs_bsize;
		devread();
		mapblock = bnum;
	}
	return (((long *)mapbuf)[(file_block - NDADDR) % NINDIR(fs)]);
}

int openrd(char *name)
{
	char *cp = name;

	dosdev = 0x80;				/* only 1st HD supported yet */
	inode.i_dev = dosdev;
	/***********************************************\
	* Now we know the disk unit and part,			*
	* Load disk info, (open the device)				*
	\***********************************************/
	if (devopen()) return 1;

	/***********************************************\
	* Load Filesystem info (mount the device)		*
	\***********************************************/
	iodest = (char *)(fs = (struct fs *)fsbuf);
	cnt = SBSIZE;
	bnum = SBLOCK + boff;
	devread();
	/***********************************************\
	* Find the actual FILE on the mounted device	*
	\***********************************************/
	if (!find(cp)) return 1;
	poff = 0;
	name = cp;
	return 0;
}
