/*-
 * Copyright (c) 1980, 1988, 1991 The Regents of the University of California.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
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
 */

#ifndef lint
static char sccsid[] = "@(#)dumptraverse.c	5.11 (Berkeley) 3/7/91";
static char rcsid[] = "$Header: /a/cvs/386BSD/src/sbin/dump/dumptraverse.c,v 1.2 1993/07/22 16:49:20 jkh Exp $";
#endif /* not lint */

#include <sys/param.h>
#include <ufs/dir.h>
#include <ufs/dinode.h>
#include <ufs/fs.h>
#include <protocols/dumprestore.h>
#ifdef __STDC__
#include <unistd.h>
#include <string.h>
#endif
#include "dump.h"

void	dmpindir();
#define	HASDUMPEDFILE	0x1
#define	HASSUBDIRS	0x2

/*
 * This is an estimation of the number of TP_BSIZE blocks in the file.
 * It estimates the number of blocks in files with holes by assuming
 * that all of the blocks accounted for by di_blocks are data blocks
 * (when some of the blocks are usually used for indirect pointers);
 * hence the estimate may be high.
 */
long
blockest(dp)
	register struct dinode *dp;
{
	long blkest, sizeest;

	/*
	 * dp->di_size is the size of the file in bytes.
	 * dp->di_blocks stores the number of sectors actually in the file.
	 * If there are more sectors than the size would indicate, this just
	 *	means that there are indirect blocks in the file or unused
	 *	sectors in the last file block; we can safely ignore these
	 *	(blkest = sizeest below).
	 * If the file is bigger than the number of sectors would indicate,
	 *	then the file has holes in it.	In this case we must use the
	 *	block count to estimate the number of data blocks used, but
	 *	we use the actual size for estimating the number of indirect
	 *	dump blocks (sizeest vs. blkest in the indirect block
	 *	calculation).
	 */
	blkest = howmany(dbtob(dp->di_blocks), TP_BSIZE);
	sizeest = howmany(dp->di_size, TP_BSIZE);
	if (blkest > sizeest)
		blkest = sizeest;
	if (dp->di_size > sblock->fs_bsize * NDADDR) {
		/* calculate the number of indirect blocks on the dump tape */
		blkest +=
			howmany(sizeest - NDADDR * sblock->fs_bsize / TP_BSIZE,
			TP_NINDIR);
	}
	return (blkest + 1);
}

/*
 * Dump pass 1.
 *
 * Walk the inode list for a filesystem to find all allocated inodes
 * that have been modified since the previous dump time. Also, find all
 * the directories in the filesystem.
 */
mapfiles(maxino, tapesize)
	ino_t maxino;
	long *tapesize;
{
	register int mode;
	register ino_t ino;
	register struct dinode *dp;
	int anydirskipped = 0;

	for (ino = 0; ino <= maxino; ino++) {
		dp = getino(ino);
		if ((mode = (dp->di_mode & IFMT)) == 0)
			continue;
		SETINO(ino, usedinomap);
		if (mode == IFDIR)
			SETINO(ino, dumpdirmap);
		if (dp->di_mtime >= spcl.c_ddate ||
		    dp->di_ctime >= spcl.c_ddate) {
			SETINO(ino, dumpinomap);
			if (mode != IFREG && mode != IFDIR && mode != IFLNK) {
				*tapesize += 1;
				continue;
			}
			*tapesize += blockest(dp);
			continue;
		}
		if (mode == IFDIR)
			anydirskipped = 1;
	}
	/*
	 * Restore gets very upset if the root is not dumped,
	 * so ensure that it always is dumped.
	 */
	SETINO(ROOTINO, usedinomap);
	return (anydirskipped);
}

/*
 * Dump pass 2.
 *
 * Scan each directory on the filesystem to see if it has any modified
 * files in it. If it does, and has not already been added to the dump
 * list (because it was itself modified), then add it. If a directory
 * has not been modified itself, contains no modified files and has no
 * subdirectories, then it can be deleted from the dump list and from
 * the list of directories. By deleting it from the list of directories,
 * its parent may now qualify for the same treatment on this or a later
 * pass using this algorithm.
 */
mapdirs(maxino, tapesize)
	ino_t maxino;
	long *tapesize;
{
	register struct	dinode *dp;
	register int i, dirty;
	register char *map;
	register ino_t ino;
	long filesize, blkcnt = 0;
	int ret, change = 0;

	for (map = dumpdirmap, ino = 0; ino < maxino; ) {
		if ((ino % NBBY) == 0)
			dirty = *map++;
		else
			dirty >>= 1;
		ino++;
		if ((dirty & 1) == 0 || TSTINO(ino, dumpinomap))
			continue;
		dp = getino(ino);
		filesize = dp->di_size;
		for (ret = 0, i = 0; filesize > 0 && i < NDADDR; i++) {
			if (dp->di_db[i] != 0)
				ret |= searchdir(ino, dp->di_db[i],
					dblksize(sblock, dp, i), filesize);
			if (ret & HASDUMPEDFILE)
				filesize = 0;
			else
				filesize -= sblock->fs_bsize;
		}
		for (i = 0; filesize > 0 && i < NIADDR; i++) {
			if (dp->di_ib[i] == 0)
				continue;
			ret |= dirindir(ino, dp->di_ib[i], i, &filesize);
		}
		if (ret & HASDUMPEDFILE) {
			if (!TSTINO(ino, dumpinomap)) {
				SETINO(ino, dumpinomap);
				*tapesize += blockest(dp);
			}
			change = 1;
			continue;
		}
		if ((ret & HASSUBDIRS) == 0) {
			if (!TSTINO(ino, dumpinomap)) {
				CLRINO(ino, dumpdirmap);
				change = 1;
			}
		}
	}
	return (change);
}

/*
 * Read indirect blocks, and pass the data blocks to be searched
 * as directories. Quit as soon as any entry is found that will
 * require the directory to be dumped.
 */
dirindir(ino, blkno, level, filesize)
	ino_t ino;
	daddr_t blkno;
	int level, *filesize;
{
	int ret = 0;
	register int i;
	daddr_t	idblk[MAXNINDIR];

	bread(fsbtodb(sblock, blkno), (char *)idblk, sblock->fs_bsize);
	if (level <= 0) {
		for (i = 0; *filesize > 0 && i < NINDIR(sblock); i++) {
			blkno = idblk[i];
			if (blkno != 0)
				ret |= searchdir(ino, blkno, sblock->fs_bsize,
					filesize);
			if (ret & HASDUMPEDFILE)
				*filesize = 0;
			else
				*filesize -= sblock->fs_bsize;
		}
		return (ret);
	}
	level--;
	for (i = 0; *filesize > 0 && i < NINDIR(sblock); i++) {
		blkno = idblk[i];
		if (blkno != 0)
			ret |= dirindir(ino, blkno, level, filesize);
	}
	return (ret);
}

/*
 * Scan a disk block containing directory information looking to see if
 * any of the entries are on the dump list and to see if the directory
 * contains any subdirectories.
 */
searchdir(ino, blkno, size, filesize)
	ino_t ino;
	daddr_t blkno;
	register int size;
	int filesize;
{
	register struct direct *dp;
	register long loc;
	char dblk[MAXBSIZE];

	bread(fsbtodb(sblock, blkno), dblk, size);
	if (filesize < size)
		size = filesize;
	for (loc = 0; loc < size; ) {
		dp = (struct direct *)(dblk + loc);
		if (dp->d_reclen == 0) {
			msg("corrupted directory, inumber %d\n", ino);
			break;
		}
		loc += dp->d_reclen;
		if (dp->d_ino == 0)
			continue;
		if (dp->d_name[0] == '.') {
			if (dp->d_name[1] == '\0')
				continue;
			if (dp->d_name[1] == '.' && dp->d_name[2] == '\0')
				continue;
		}
		if (TSTINO(dp->d_ino, dumpinomap))
			return (HASDUMPEDFILE);
		if (TSTINO(dp->d_ino, dumpdirmap))
			return (HASSUBDIRS);
	}
	return (0);
}

/*
 * Dump passes 3 and 4.
 *
 * Dump the contents of an inode to tape.
 */
void
dumpino(dp, ino)
	register struct dinode *dp;
	ino_t ino;
{
	int mode, level, cnt;
	long size;

	if (newtape) {
		newtape = 0;
		dumpmap(dumpinomap, TS_BITS, ino);
	}
	CLRINO(ino, dumpinomap);
	spcl.c_dinode = *dp;
	spcl.c_type = TS_INODE;
	spcl.c_count = 0;
	/*
	 * Check for freed inode.
	 */
	if ((mode = (dp->di_mode & IFMT)) == 0)
		return;
	if ((mode != IFDIR && mode != IFREG && mode != IFLNK) ||
	    dp->di_size == 0) {
		writeheader(ino);
		return;
	}
	if (DFASTLINK(*dp)) {
		spcl.c_addr[0] = 1;
		spcl.c_count = 1;
		writeheader(ino);
		writerec(dp->di_symlink);
		return;
	}
	if (dp->di_size > NDADDR * sblock->fs_bsize)
		cnt = NDADDR * sblock->fs_frag;
	else
		cnt = howmany(dp->di_size, sblock->fs_fsize);
	blksout(&dp->di_db[0], cnt, ino);
	if ((size = dp->di_size - NDADDR * sblock->fs_bsize) <= 0)
		return;
	for (level = 0; level < NIADDR; level++) {
		dmpindir(ino, dp->di_ib[level], level, &size);
		if (size <= 0)
			return;
	}
}

/*
 * Read indirect blocks, and pass the data blocks to be dumped.
 */
void
dmpindir(ino, blk, level, size)
	ino_t ino;
	daddr_t blk;
	int level;
	long *size;
{
	int i, cnt;
	daddr_t idblk[MAXNINDIR];

	if (blk != 0)
		bread(fsbtodb(sblock, blk), (char *)idblk, sblock->fs_bsize);
	else
		bzero((char *)idblk, sblock->fs_bsize);
	if (level <= 0) {
		if (*size < NINDIR(sblock) * sblock->fs_bsize)
			cnt = howmany(*size, sblock->fs_fsize);
		else
			cnt = NINDIR(sblock) * sblock->fs_frag;
		*size -= NINDIR(sblock) * sblock->fs_bsize;
		blksout(&idblk[0], cnt, ino);
		return;
	}
	level--;
	for (i = 0; i < NINDIR(sblock); i++) {
		dmpindir(ino, idblk[i], level, size);
		if (*size <= 0)
			return;
	}
}

/*
 * Collect up the data into tape record sized buffers and output them.
 */
void
blksout(blkp, frags, ino)
	daddr_t *blkp;
	int frags;
	ino_t ino;
{
	register daddr_t *bp;
	int i, j, count, blks, tbperdb;

	blks = howmany(frags * sblock->fs_fsize, TP_BSIZE);
	tbperdb = sblock->fs_bsize >> tp_bshift;
	for (i = 0; i < blks; i += TP_NINDIR) {
		if (i + TP_NINDIR > blks)
			count = blks;
		else
			count = i + TP_NINDIR;
		for (j = i; j < count; j++)
			if (blkp[j / tbperdb] != 0)
				spcl.c_addr[j - i] = 1;
			else
				spcl.c_addr[j - i] = 0;
		spcl.c_count = count - i;
		writeheader(ino);
		bp = &blkp[i / tbperdb];
		for (j = i; j < count; j += tbperdb, bp++)
			if (*bp != 0)
				if (j + tbperdb <= count)
					dumpblock(*bp, sblock->fs_bsize);
				else
					dumpblock(*bp, (count - j) * TP_BSIZE);
		spcl.c_type = TS_ADDR;
	}
}

/*
 * Dump a map to the tape.
 */
void
dumpmap(map, type, ino)
	char *map;
	int type;
	ino_t ino;
{
	register int i;
	char *cp;

	spcl.c_type = type;
	spcl.c_count = howmany(mapsize * sizeof(char), TP_BSIZE);
	writeheader(ino);
	for (i = 0, cp = map; i < spcl.c_count; i++, cp += TP_BSIZE)
		writerec(cp);
}

/*
 * Write a header record to the dump tape.
 */
void
writeheader(ino)
	ino_t ino;
{
	register long sum, cnt, *lp;

	spcl.c_inumber = ino;
	spcl.c_magic = NFS_MAGIC;
	spcl.c_checksum = 0;
	lp = (long *)&spcl;
	sum = 0;
	cnt = sizeof(union u_spcl) / (4 * sizeof(long));
	while (--cnt >= 0) {
		sum += *lp++;
		sum += *lp++;
		sum += *lp++;
		sum += *lp++;
	}
	spcl.c_checksum = CHECKSUM - sum;
	writerec((char *)&spcl);
}

struct dinode *
getino(inum)
	ino_t inum;
{
	static daddr_t minino, maxino;
	static struct dinode inoblock[MAXINOPB];

	curino = inum;
	if (inum >= minino && inum < maxino)
		return (&inoblock[inum - minino]);
	bread(fsbtodb(sblock, itod(sblock, inum)), inoblock, sblock->fs_bsize);
	minino = inum - (inum % INOPB(sblock));
	maxino = minino + INOPB(sblock);
	return (&inoblock[inum - minino]);
}

/*
 * Read a chunk of data from the disk.
 * Try to recover from hard errors by reading in sector sized pieces.
 * Error recovery is attempted at most BREADEMAX times before seeking
 * consent from the operator to continue.
 */
int	breaderrors = 0;		
#define	BREADEMAX 32

void
bread(blkno, buf, size)
	daddr_t blkno;
	char *buf;
	int size;	
{
	int cnt, i;
	extern int errno;

loop:
	if (lseek(diskfd, (long)(blkno << dev_bshift), 0) < 0)
		msg("bread: lseek fails\n");
	if ((cnt = read(diskfd, buf, size)) == size)
		return;
	if (blkno + (size / dev_bsize) > fsbtodb(sblock, sblock->fs_size)) {
		/*
		 * Trying to read the final fragment.
		 *
		 * NB - dump only works in TP_BSIZE blocks, hence
		 * rounds `dev_bsize' fragments up to TP_BSIZE pieces.
		 * It should be smarter about not actually trying to
		 * read more than it can get, but for the time being
		 * we punt and scale back the read only when it gets
		 * us into trouble. (mkm 9/25/83)
		 */
		size -= dev_bsize;
		goto loop;
	}
	msg("read error from %s [block %d]: count=%d, got=%d, errno=%d (%s)\n",
		disk, blkno, size, cnt, errno, strerror(errno));
	if (++breaderrors > BREADEMAX) {
		msg("More than %d block read errors from %d\n",
			BREADEMAX, disk);
		broadcast("DUMP IS AILING!\n");
		msg("This is an unrecoverable error.\n");
		if (!query("Do you want to attempt to continue?")){
			dumpabort();
			/*NOTREACHED*/
		} else
			breaderrors = 0;
	}
	/*
	 * Zero buffer, then try to read each sector of buffer separately.
	 */
	bzero(buf, size);
	for (i = 0; i < size; i += dev_bsize, buf += dev_bsize, blkno++) {
		if (lseek(diskfd, (long)(blkno << dev_bshift), 0) < 0)
			msg("bread: lseek2 fails!\n");
		if ((cnt = read(diskfd, buf, dev_bsize)) != dev_bsize)
			msg("    read error from %s [sector %d, errno %d]\n",
			    disk, blkno, errno);
	}
}
