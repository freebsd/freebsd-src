/*
 * Copyright (c) 1980, 1986 The Regents of the University of California.
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
static char sccsid[] = "@(#)utilities.c	5.30 (Berkeley) 7/26/91";
static char rcsid[] = "$Header: /a/cvs/386BSD/src/sbin/fsck/utilities.c,v 1.2 1993/07/22 16:52:01 jkh Exp $";
#endif /* not lint */

#include <sys/param.h>
#include <ufs/dinode.h>
#include <ufs/fs.h>
#include <ufs/dir.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "fsck.h"

long	diskreads, totalreads;	/* Disk cache statistics */
long	lseek();

ftypeok(dp)
	struct dinode *dp;
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

reply(question)
	char *question;
{
	int persevere;
	char c;

	if (preen)
		pfatal("INTERNAL ERROR: GOT TO reply()");
	persevere = !strcmp(question, "CONTINUE");
	printf("\n");
	if (!persevere && (nflag || fswritefd < 0)) {
		printf("%s? no\n\n", question);
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
		while (c != '\n' && getc(stdin) != '\n')
			if (feof(stdin))
				return (0);
	} while (c != 'y' && c != 'Y' && c != 'n' && c != 'N');
	printf("\n");
	if (c == 'y' || c == 'Y')
		return (1);
	return (0);
}

/*
 * Malloc buffers and set up cache.
 */
bufinit()
{
	register struct bufarea *bp;
	long bufcnt, i;
	char *bufp;

	pbp = pdirbp = (struct bufarea *)0;
	bufp = malloc((unsigned int)sblock.fs_bsize);
	if (bufp == 0)
		errexit("cannot allocate buffer pool\n");
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
			errexit("cannot allocate buffer pool\n");
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
getdatablk(blkno, size)
	daddr_t blkno;
	long size;
{
	register struct bufarea *bp;

	for (bp = bufhead.b_next; bp != &bufhead; bp = bp->b_next)
		if (bp->b_bno == fsbtodb(&sblock, blkno))
			goto foundit;
	for (bp = bufhead.b_prev; bp != &bufhead; bp = bp->b_prev)
		if ((bp->b_flags & B_INUSE) == 0)
			break;
	if (bp == &bufhead)
		errexit("deadlocked buffer pool\n");
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
getblk(bp, blk, size)
	register struct bufarea *bp;
	daddr_t blk;
	long size;
{
	daddr_t dblk;

	dblk = fsbtodb(&sblock, blk);
	if (bp->b_bno != dblk) {
		flush(fswritefd, bp);
		diskreads++;
		bp->b_errs = bread(fsreadfd, bp->b_un.b_buf, dblk, size);
		bp->b_bno = dblk;
		bp->b_size = size;
	}
}

flush(fd, bp)
	int fd;
	register struct bufarea *bp;
{
	register int i, j;

	if (!bp->b_dirty)
		return;
	if (bp->b_errs != 0)
		pfatal("WRITING %sZERO'ED BLOCK %d TO DISK\n",
		    (bp->b_errs == bp->b_size / dev_bsize) ? "" : "PARTIALLY ",
		    bp->b_bno);
	bp->b_dirty = 0;
	bp->b_errs = 0;
	bwrite(fd, bp->b_un.b_buf, bp->b_bno, (long)bp->b_size);
	if (bp != &sblk)
		return;
	for (i = 0, j = 0; i < sblock.fs_cssize; i += sblock.fs_bsize, j++) {
		bwrite(fswritefd, (char *)sblock.fs_csp[j],
		    fsbtodb(&sblock, sblock.fs_csaddr + j * sblock.fs_frag),
		    sblock.fs_cssize - i < sblock.fs_bsize ?
		    sblock.fs_cssize - i : sblock.fs_bsize);
	}
}

rwerror(mesg, blk)
	char *mesg;
	daddr_t blk;
{

	if (preen == 0)
		printf("\n");
	pfatal("CANNOT %s: BLK %ld", mesg, blk);
	if (reply("CONTINUE") == 0)
		errexit("Program terminated\n");
}

ckfini()
{
	register struct bufarea *bp, *nbp;
	int cnt = 0;

	flush(fswritefd, &sblk);
	if (havesb && sblk.b_bno != SBOFF / dev_bsize &&
	    !preen && reply("UPDATE STANDARD SUPERBLOCK")) {
		sblk.b_bno = SBOFF / dev_bsize;
		sbdirty();
		flush(fswritefd, &sblk);
	}
	flush(fswritefd, &cgblk);
	free(cgblk.b_un.b_buf);
	for (bp = bufhead.b_prev; bp != &bufhead; bp = nbp) {
		cnt++;
		flush(fswritefd, bp);
		nbp = bp->b_prev;
		free(bp->b_un.b_buf);
		free((char *)bp);
	}
	if (bufhead.b_size != cnt)
		errexit("Panic: lost %d buffers\n", bufhead.b_size - cnt);
	pbp = pdirbp = (struct bufarea *)0;
	if (debug)
		printf("cache missed %ld of %ld (%d%%)\n", diskreads,
		    totalreads, (int)(diskreads * 100 / totalreads));
	(void)close(fsreadfd);
	(void)close(fswritefd);
}

bread(fd, buf, blk, size)
	int fd;
	char *buf;
	daddr_t blk;
	long size;
{
	char *cp;
	int i, errs;

	if (lseek(fd, blk * dev_bsize, 0) < 0)
		rwerror("SEEK", blk);
	else if (read(fd, buf, (int)size) == size)
		return (0);
	rwerror("READ", blk);
	if (lseek(fd, blk * dev_bsize, 0) < 0)
		rwerror("SEEK", blk);
	errs = 0;
	bzero(buf, (size_t)size);
	printf("THE FOLLOWING DISK SECTORS COULD NOT BE READ:");
	for (cp = buf, i = 0; i < size; i += secsize, cp += secsize) {
		if (read(fd, cp, (int)secsize) != secsize) {
			(void)lseek(fd, blk * dev_bsize + i + secsize, 0);
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
	return (errs);
}

bwrite(fd, buf, blk, size)
	int fd;
	char *buf;
	daddr_t blk;
	long size;
{
	int i;
	char *cp;

	if (fd < 0)
		return;
	if (lseek(fd, blk * dev_bsize, 0) < 0)
		rwerror("SEEK", blk);
	else if (write(fd, buf, (int)size) == size) {
		fsmodified = 1;
		return;
	}
	rwerror("WRITE", blk);
	if (lseek(fd, blk * dev_bsize, 0) < 0)
		rwerror("SEEK", blk);
	printf("THE FOLLOWING SECTORS COULD NOT BE WRITTEN:");
	for (cp = buf, i = 0; i < size; i += dev_bsize, cp += dev_bsize)
		if (write(fd, cp, (int)dev_bsize) != dev_bsize) {
			(void)lseek(fd, blk * dev_bsize + i + dev_bsize, 0);
			printf(" %ld,", blk + i / dev_bsize);
		}
	printf("\n");
	return;
}

/*
 * allocate a data block with the specified number of fragments
 */
allocblk(frags)
	long frags;
{
	register int i, j, k;

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
			for (k = 0; k < frags; k++)
				setbmap(i + j + k);
			n_blks += frags;
			return (i + j);
		}
	}
	return (0);
}

/*
 * Free a previously allocated block
 */
freeblk(blkno, frags)
	daddr_t blkno;
	long frags;
{
	struct inodesc idesc;

	idesc.id_blkno = blkno;
	idesc.id_numfrags = frags;
	(void)pass4check(&idesc);
}

/*
 * Find a pathname
 */
getpathname(namebuf, curdir, ino)
	char *namebuf;
	ino_t curdir, ino;
{
	int len;
	register char *cp;
	struct inodesc idesc;
	static int busy = 0;
	extern int findname();

	if (busy ||
	    (statemap[curdir] != DSTATE && statemap[curdir] != DFOUND)) {
		(void)strcpy(namebuf, "?");
		return;
	}
	busy = 1;
	bzero((char *)&idesc, sizeof(struct inodesc));
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
		bcopy(namebuf, cp, (size_t)len);
		*--cp = '/';
		if (cp < &namebuf[MAXNAMLEN])
			break;
		ino = idesc.id_number;
	}
	busy = 0;
	if (ino != ROOTINO)
		*--cp = '?';
	bcopy(cp, namebuf, (size_t)(&namebuf[MAXPATHLEN] - cp));
}

void
catch()
{
	ckfini();
	exit(12);
}

/*
 * When preening, allow a single quit to signal
 * a special exit after filesystem checks complete
 * so that reboot sequence may be interrupted.
 */
void
catchquit()
{
	extern returntosingle;

	printf("returning to single-user after filesystem check\n");
	returntosingle = 1;
	(void)signal(SIGQUIT, SIG_DFL);
}

/*
 * Ignore a single quit signal; wait and flush just in case.
 * Used by child processes in preen.
 */
void
voidquit()
{

	sleep(1);
	(void)signal(SIGQUIT, SIG_IGN);
	(void)signal(SIGQUIT, SIG_DFL);
}

/*
 * determine whether an inode should be fixed.
 */
dofix(idesc, msg)
	register struct inodesc *idesc;
	char *msg;
{

	switch (idesc->id_fix) {

	case DONTKNOW:
		if (idesc->id_type == DATA)
			direrror(idesc->id_number, msg);
		else
			pwarn(msg);
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
		errexit("UNKNOWN INODESC FIX MODE %d\n", idesc->id_fix);
	}
	/* NOTREACHED */
}

/* VARARGS1 */
errexit(s1, s2, s3, s4)
	char *s1;
{
	printf(s1, s2, s3, s4);
	exit(8);
}

/*
 * An unexpected inconsistency occured.
 * Die if preening, otherwise just print message and continue.
 */
/* VARARGS1 */
pfatal(s, a1, a2, a3)
	char *s;
{

	if (preen) {
		printf("%s: ", devname);
		printf(s, a1, a2, a3);
		printf("\n");
		printf("%s: UNEXPECTED INCONSISTENCY; RUN fsck MANUALLY.\n",
			devname);
		exit(8);
	}
	printf(s, a1, a2, a3);
}

/*
 * Pwarn just prints a message when not preening,
 * or a warning (preceded by filename) when preening.
 */
/* VARARGS1 */
pwarn(s, a1, a2, a3, a4, a5, a6)
	char *s;
{

	if (preen)
		printf("%s: ", devname);
	printf(s, a1, a2, a3, a4, a5, a6);
}

#ifndef lint
/*
 * Stub for routines from kernel.
 */
panic(s)
	char *s;
{

	pfatal("INTERNAL INCONSISTENCY:");
	errexit(s);
}
#endif
