/*
 * Copyright (c) 1980, 1986, 1993
 *	The Regents of the University of California.  All rights reserved.
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
#if 0
static const char sccsid[] = "@(#)utilities.c	8.6 (Berkeley) 5/19/95";
#endif
static const char rcsid[] =
  "$FreeBSD$";
#endif /* not lint */

#include <sys/param.h>
#include <sys/types.h>
#include <sys/sysctl.h>
#include <sys/stat.h>

#include <ufs/ufs/dinode.h>
#include <ufs/ufs/dir.h>
#include <ufs/ffs/fs.h>

#include <err.h>
#include <errno.h>
#include <string.h>
#include <ctype.h>
#include <fstab.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "fsck.h"

long	diskreads, totalreads;	/* Disk cache statistics */

int
ftypeok(struct dinode *dp)
{
	switch (dp->di_mode & IFMT) {

	case IFDIR:
	case IFREG:
	case IFBLK:
	case IFCHR:
	case IFLNK:
	case IFSOCK:
	case IFIFO:
		return (1);

	default:
		if (debug)
			printf("bad file type 0%o\n", dp->di_mode);
		return (0);
	}
}

int
reply(char *question)
{
	int persevere;
	char c;

	if (preen)
		pfatal("INTERNAL ERROR: GOT TO reply()");
	persevere = !strcmp(question, "CONTINUE");
	printf("\n");
	if (!persevere && (nflag || (fswritefd < 0 && bkgrdflag == 0))) {
		printf("%s? no\n\n", question);
		resolved = 0;
		return (0);
	}
	if (yflag || (persevere && nflag)) {
		printf("%s? yes\n\n", question);
		return (1);
	}
	do	{
		printf("%s? [yn] ", question);
		(void) fflush(stdout);
		c = getc(stdin);
		while (c != '\n' && getc(stdin) != '\n') {
			if (feof(stdin)) {
				resolved = 0;
				return (0);
			}
		}
	} while (c != 'y' && c != 'Y' && c != 'n' && c != 'N');
	printf("\n");
	if (c == 'y' || c == 'Y')
		return (1);
	resolved = 0;
	return (0);
}

/*
 * Look up state information for an inode.
 */
struct inostat *
inoinfo(ino_t inum)
{
	static struct inostat unallocated = { USTATE, 0, 0 };
	struct inostatlist *ilp;
	int iloff;

	if (inum > maxino)
		errx(EEXIT, "inoinfo: inumber %d out of range", inum);
	ilp = &inostathead[inum / sblock.fs_ipg];
	iloff = inum % sblock.fs_ipg;
	if (iloff >= ilp->il_numalloced)
		return (&unallocated);
	return (&ilp->il_stat[iloff]);
}

/*
 * Malloc buffers and set up cache.
 */
void
bufinit(void)
{
	struct bufarea *bp;
	long bufcnt, i;
	char *bufp;

	pbp = pdirbp = (struct bufarea *)0;
	bufp = malloc((unsigned int)sblock.fs_bsize);
	if (bufp == 0)
		errx(EEXIT, "cannot allocate buffer pool");
	cgblk.b_un.b_buf = bufp;
	initbarea(&cgblk);
	bufhead.b_next = bufhead.b_prev = &bufhead;
	bufcnt = MAXBUFSPACE / sblock.fs_bsize;
	if (bufcnt < MINBUFS)
		bufcnt = MINBUFS;
	for (i = 0; i < bufcnt; i++) {
		bp = (struct bufarea *)malloc(sizeof(struct bufarea));
		bufp = malloc((unsigned int)sblock.fs_bsize);
		if (bp == NULL || bufp == NULL) {
			if (i >= MINBUFS)
				break;
			errx(EEXIT, "cannot allocate buffer pool");
		}
		bp->b_un.b_buf = bufp;
		bp->b_prev = &bufhead;
		bp->b_next = bufhead.b_next;
		bufhead.b_next->b_prev = bp;
		bufhead.b_next = bp;
		initbarea(bp);
	}
	bufhead.b_size = i;	/* save number of buffers */
}

/*
 * Manage a cache of directory blocks.
 */
struct bufarea *
getdatablk(ufs_daddr_t blkno, long size)
{
	struct bufarea *bp;

	for (bp = bufhead.b_next; bp != &bufhead; bp = bp->b_next)
		if (bp->b_bno == fsbtodb(&sblock, blkno))
			goto foundit;
	for (bp = bufhead.b_prev; bp != &bufhead; bp = bp->b_prev)
		if ((bp->b_flags & B_INUSE) == 0)
			break;
	if (bp == &bufhead)
		errx(EEXIT, "deadlocked buffer pool");
	getblk(bp, blkno, size);
	/* fall through */
foundit:
	totalreads++;
	bp->b_prev->b_next = bp->b_next;
	bp->b_next->b_prev = bp->b_prev;
	bp->b_prev = &bufhead;
	bp->b_next = bufhead.b_next;
	bufhead.b_next->b_prev = bp;
	bufhead.b_next = bp;
	bp->b_flags |= B_INUSE;
	return (bp);
}

void
getblk(struct bufarea *bp, ufs_daddr_t blk, long size)
{
	ufs_daddr_t dblk;

	dblk = fsbtodb(&sblock, blk);
	if (bp->b_bno != dblk) {
		flush(fswritefd, bp);
		diskreads++;
		bp->b_errs = bread(fsreadfd, bp->b_un.b_buf, dblk, size);
		bp->b_bno = dblk;
		bp->b_size = size;
	}
}

void
flush(int fd, struct bufarea *bp)
{
	int i, j;

	if (!bp->b_dirty)
		return;
	bp->b_dirty = 0;
	if (fswritefd < 0) {
		pfatal("WRITING IN READ_ONLY MODE.\n");
		return;
	}
	if (bp->b_errs != 0)
		pfatal("WRITING %sZERO'ED BLOCK %d TO DISK\n",
		    (bp->b_errs == bp->b_size / dev_bsize) ? "" : "PARTIALLY ",
		    bp->b_bno);
	bp->b_errs = 0;
	bwrite(fd, bp->b_un.b_buf, bp->b_bno, (long)bp->b_size);
	if (bp != &sblk)
		return;
	for (i = 0, j = 0; i < sblock.fs_cssize; i += sblock.fs_bsize, j++) {
		bwrite(fswritefd, (char *)sblock.fs_csp + i,
		    fsbtodb(&sblock, sblock.fs_csaddr + j * sblock.fs_frag),
		    sblock.fs_cssize - i < sblock.fs_bsize ?
		    sblock.fs_cssize - i : sblock.fs_bsize);
	}
}

void
rwerror(char *mesg, ufs_daddr_t blk)
{

	if (bkgrdcheck)
		exit(EEXIT);
	if (preen == 0)
		printf("\n");
	pfatal("CANNOT %s: %ld", mesg, (long)blk);
	if (reply("CONTINUE") == 0)
		exit(EEXIT);
}

void
ckfini(int markclean)
{
	struct bufarea *bp, *nbp;
	int ofsmodified, cnt = 0;

	if (bkgrdflag) {
		unlink(snapname);
		if ((!(sblock.fs_flags & FS_UNCLEAN)) != markclean) {
			cmd.value = FS_UNCLEAN;
			cmd.size = markclean ? -1 : 1;
			if (sysctlbyname("vfs.ffs.setflags", 0, 0,
			    &cmd, sizeof cmd) == -1)
				rwerror("SET FILESYSTEM FLAGS", FS_UNCLEAN);
			if (!preen) {
				printf("\n***** FILE SYSTEM MARKED %s *****\n",
				    markclean ? "CLEAN" : "DIRTY");
				if (!markclean)
					rerun = 1;
			}
		} else if (!preen && !markclean) {
			printf("\n***** FILE SYSTEM STILL DIRTY *****\n");
			rerun = 1;
		}
	}
	if (fswritefd < 0) {
		(void)close(fsreadfd);
		return;
	}
	flush(fswritefd, &sblk);
	if (havesb && sblk.b_bno != SBOFF / dev_bsize && cursnapshot == 0 &&
	    !preen && reply("UPDATE STANDARD SUPERBLOCK")) {
		sblk.b_bno = SBOFF / dev_bsize;
		sbdirty();
		flush(fswritefd, &sblk);
	}
	flush(fswritefd, &cgblk);
	free(cgblk.b_un.b_buf);
	for (bp = bufhead.b_prev; bp && bp != &bufhead; bp = nbp) {
		cnt++;
		flush(fswritefd, bp);
		nbp = bp->b_prev;
		free(bp->b_un.b_buf);
		free((char *)bp);
	}
	if (bufhead.b_size != cnt)
		errx(EEXIT, "panic: lost %d buffers", bufhead.b_size - cnt);
	pbp = pdirbp = (struct bufarea *)0;
	if (cursnapshot == 0 && sblock.fs_clean != markclean) {
		if ((sblock.fs_clean = markclean) != 0)
			sblock.fs_flags &= ~(FS_UNCLEAN | FS_NEEDSFSCK);
		sbdirty();
		ofsmodified = fsmodified;
		flush(fswritefd, &sblk);
		fsmodified = ofsmodified;
		if (!preen) {
			printf("\n***** FILE SYSTEM MARKED %s *****\n",
			    markclean ? "CLEAN" : "DIRTY");
			if (!markclean)
				rerun = 1;
		}
	} else if (!preen && !markclean) {
		printf("\n***** FILE SYSTEM STILL DIRTY *****\n");
		rerun = 1;
	}
	if (debug && totalreads > 0)
		printf("cache missed %ld of %ld (%d%%)\n", diskreads,
		    totalreads, (int)(diskreads * 100 / totalreads));
	(void)close(fsreadfd);
	(void)close(fswritefd);
}

int
bread(int fd, char *buf, ufs_daddr_t blk, long size)
{
	char *cp;
	int i, errs;
	off_t offset;

	offset = blk;
	offset *= dev_bsize;
	if (lseek(fd, offset, 0) < 0)
		rwerror("SEEK BLK", blk);
	else if (read(fd, buf, (int)size) == size)
		return (0);
	rwerror("READ BLK", blk);
	if (lseek(fd, offset, 0) < 0)
		rwerror("SEEK BLK", blk);
	errs = 0;
	memset(buf, 0, (size_t)size);
	printf("THE FOLLOWING DISK SECTORS COULD NOT BE READ:");
	for (cp = buf, i = 0; i < size; i += secsize, cp += secsize) {
		if (read(fd, cp, (int)secsize) != secsize) {
			(void)lseek(fd, offset + i + secsize, 0);
			if (secsize != dev_bsize && dev_bsize != 1)
				printf(" %ld (%ld),",
				    (blk * dev_bsize + i) / secsize,
				    blk + i / dev_bsize);
			else
				printf(" %ld,", blk + i / dev_bsize);
			errs++;
		}
	}
	printf("\n");
	if (errs)
		resolved = 0;
	return (errs);
}

void
bwrite(int fd, char *buf, ufs_daddr_t blk, long size)
{
	int i;
	char *cp;
	off_t offset;

	if (fd < 0)
		return;
	offset = blk;
	offset *= dev_bsize;
	if (lseek(fd, offset, 0) < 0)
		rwerror("SEEK BLK", blk);
	else if (write(fd, buf, (int)size) == size) {
		fsmodified = 1;
		return;
	}
	resolved = 0;
	rwerror("WRITE BLK", blk);
	if (lseek(fd, offset, 0) < 0)
		rwerror("SEEK BLK", blk);
	printf("THE FOLLOWING SECTORS COULD NOT BE WRITTEN:");
	for (cp = buf, i = 0; i < size; i += dev_bsize, cp += dev_bsize)
		if (write(fd, cp, (int)dev_bsize) != dev_bsize) {
			(void)lseek(fd, offset + i + dev_bsize, 0);
			printf(" %ld,", blk + i / dev_bsize);
		}
	printf("\n");
	return;
}

/*
 * allocate a data block with the specified number of fragments
 */
ufs_daddr_t
allocblk(long frags)
{
	int i, j, k, cg, baseblk;
	struct cg *cgp = &cgrp;

	if (frags <= 0 || frags > sblock.fs_frag)
		return (0);
	for (i = 0; i < maxfsblock - sblock.fs_frag; i += sblock.fs_frag) {
		for (j = 0; j <= sblock.fs_frag - frags; j++) {
			if (testbmap(i + j))
				continue;
			for (k = 1; k < frags; k++)
				if (testbmap(i + j + k))
					break;
			if (k < frags) {
				j += k;
				continue;
			}
			cg = dtog(&sblock, i + j);
			getblk(&cgblk, cgtod(&sblock, cg), sblock.fs_cgsize);
			if (!cg_chkmagic(cgp))
				pfatal("CG %d: BAD MAGIC NUMBER\n", cg);
			baseblk = dtogd(&sblock, i + j);
			for (k = 0; k < frags; k++) {
				setbmap(i + j + k);
				clrbit(cg_blksfree(cgp), baseblk + k);
			}
			n_blks += frags;
			if (frags == sblock.fs_frag)
				cgp->cg_cs.cs_nbfree--;
			else
				cgp->cg_cs.cs_nffree -= frags;
			cgdirty();
			return (i + j);
		}
	}
	return (0);
}

/*
 * Free a previously allocated block
 */
void
freeblk(ufs_daddr_t blkno, long frags)
{
	struct inodesc idesc;

	idesc.id_blkno = blkno;
	idesc.id_numfrags = frags;
	(void)pass4check(&idesc);
}

/*
 * Find a pathname
 */
void
getpathname(char *namebuf, ino_t curdir, ino_t ino)
{
	int len;
	char *cp;
	struct inodesc idesc;
	static int busy = 0;

	if (curdir == ino && ino == ROOTINO) {
		(void)strcpy(namebuf, "/");
		return;
	}
	if (busy ||
	    (inoinfo(curdir)->ino_state != DSTATE &&
	     inoinfo(curdir)->ino_state != DFOUND)) {
		(void)strcpy(namebuf, "?");
		return;
	}
	busy = 1;
	memset(&idesc, 0, sizeof(struct inodesc));
	idesc.id_type = DATA;
	idesc.id_fix = IGNORE;
	cp = &namebuf[MAXPATHLEN - 1];
	*cp = '\0';
	if (curdir != ino) {
		idesc.id_parent = curdir;
		goto namelookup;
	}
	while (ino != ROOTINO) {
		idesc.id_number = ino;
		idesc.id_func = findino;
		idesc.id_name = "..";
		if ((ckinode(ginode(ino), &idesc) & FOUND) == 0)
			break;
	namelookup:
		idesc.id_number = idesc.id_parent;
		idesc.id_parent = ino;
		idesc.id_func = findname;
		idesc.id_name = namebuf;
		if ((ckinode(ginode(idesc.id_number), &idesc)&FOUND) == 0)
			break;
		len = strlen(namebuf);
		cp -= len;
		memmove(cp, namebuf, (size_t)len);
		*--cp = '/';
		if (cp < &namebuf[MAXNAMLEN])
			break;
		ino = idesc.id_number;
	}
	busy = 0;
	if (ino != ROOTINO)
		*--cp = '?';
	memmove(namebuf, cp, (size_t)(&namebuf[MAXPATHLEN] - cp));
}

void
catch(int sig)
{
	if (!doinglevel2)
		ckfini(0);
	exit(12);
}

/*
 * When preening, allow a single quit to signal
 * a special exit after filesystem checks complete
 * so that reboot sequence may be interrupted.
 */
void
catchquit(int sig)
{
	printf("returning to single-user after filesystem check\n");
	returntosingle = 1;
	(void)signal(SIGQUIT, SIG_DFL);
}

/*
 * determine whether an inode should be fixed.
 */
int
dofix(struct inodesc *idesc, char *msg)
{

	switch (idesc->id_fix) {

	case DONTKNOW:
		if (idesc->id_type == DATA)
			direrror(idesc->id_number, msg);
		else
			pwarn("%s", msg);
		if (preen) {
			printf(" (SALVAGED)\n");
			idesc->id_fix = FIX;
			return (ALTERED);
		}
		if (reply("SALVAGE") == 0) {
			idesc->id_fix = NOFIX;
			return (0);
		}
		idesc->id_fix = FIX;
		return (ALTERED);

	case FIX:
		return (ALTERED);

	case NOFIX:
	case IGNORE:
		return (0);

	default:
		errx(EEXIT, "UNKNOWN INODESC FIX MODE %d", idesc->id_fix);
	}
	/* NOTREACHED */
	return (0);
}

#include <stdarg.h>

/*
 * An unexpected inconsistency occured.
 * Die if preening or filesystem is running with soft dependency protocol,
 * otherwise just print message and continue.
 */
void
pfatal(const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	if (!preen) {
		(void)vfprintf(stdout, fmt, ap);
		va_end(ap);
		if (usedsoftdep)
			(void)fprintf(stdout,
			    "\nUNEXPECTED SOFT UPDATE INCONSISTENCY\n");
		/*
		 * Force foreground fsck to clean up inconsistency.
		 */
		if (bkgrdflag) {
			cmd.value = FS_NEEDSFSCK;
			cmd.size = 1;
			if (sysctlbyname("vfs.ffs.setflags", 0, 0,
			    &cmd, sizeof cmd) == -1)
				pwarn("CANNOT SET FS_NEEDSFSCK FLAG\n");
			fprintf(stdout, "CANNOT RUN IN BACKGROUND\n");
			ckfini(0);
			exit(EEXIT);
		}
		return;
	}
	if (cdevname == NULL)
		cdevname = "fsck";
	(void)fprintf(stdout, "%s: ", cdevname);
	(void)vfprintf(stdout, fmt, ap);
	(void)fprintf(stdout,
	    "\n%s: UNEXPECTED%sINCONSISTENCY; RUN fsck MANUALLY.\n",
	    cdevname, usedsoftdep ? " SOFT UPDATE " : " ");
	/*
	 * Force foreground fsck to clean up inconsistency.
	 */
	if (bkgrdflag) {
		cmd.value = FS_NEEDSFSCK;
		cmd.size = 1;
		if (sysctlbyname("vfs.ffs.setflags", 0, 0,
		    &cmd, sizeof cmd) == -1)
			pwarn("CANNOT SET FS_NEEDSFSCK FLAG\n");
	}
	ckfini(0);
	exit(EEXIT);
}

/*
 * Pwarn just prints a message when not preening or running soft dependency
 * protocol, or a warning (preceded by filename) when preening.
 */
void
pwarn(const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	if (preen)
		(void)fprintf(stdout, "%s: ", cdevname);
	(void)vfprintf(stdout, fmt, ap);
	va_end(ap);
}

/*
 * Stub for routines from kernel.
 */
void
panic(const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	pfatal("INTERNAL INCONSISTENCY:");
	(void)vfprintf(stdout, fmt, ap);
	va_end(ap);
	exit(EEXIT);
}
