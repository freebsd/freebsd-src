/*-
 * Copyright (c) 1980, 1991 The Regents of the University of California.
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
static char sccsid[] = "@(#)dumptape.c	5.18 (Berkeley) 4/24/91";
static char rcsid[] = "$Header: /a/cvs/386BSD/src/sbin/dump/dumptape.c,v 1.2 1993/07/22 16:49:19 jkh Exp $";
#endif /* not lint */

#include <sys/param.h>
#include <sys/wait.h>
#include <ufs/dir.h>
#include <ufs/dinode.h>
#include <ufs/fs.h>
#include <signal.h>
#include <fcntl.h>
#include <protocols/dumprestore.h>
#include <errno.h>
#ifdef __STDC__
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#endif
#include "dump.h"
#include "pathnames.h"

char	(*tblock)[TP_BSIZE];	/* pointer to malloc()ed buffer for tape */
int	writesize;		/* size of malloc()ed buffer for tape */
long	lastspclrec = -1;	/* tape block number of last written header */
int	trecno = 0;		/* next record to write in current block */
extern	long blocksperfile;	/* number of blocks per output file */
long	blocksthisvol;		/* number of blocks on current output file */
extern	int ntrec;		/* blocking factor on tape */
extern	int cartridge;
char	*nexttape;
#ifdef RDUMP
extern char *host;
int	rmtopen(), rmtwrite();
void	rmtclose();
#endif RDUMP

int	atomic();
void	doslave(), enslave(), flushtape(), killall();

/*
 * Concurrent dump mods (Caltech) - disk block reading and tape writing
 * are exported to several slave processes.  While one slave writes the
 * tape, the others read disk blocks; they pass control of the tape in
 * a ring via flock().	The parent process traverses the filesystem and
 * sends writeheader()'s and lists of daddr's to the slaves via pipes.
 */
struct req {			/* instruction packets sent to slaves */
	daddr_t dblk;
	int count;
} *req;
int reqsiz;

#define SLAVES 3		/* 1 slave writing, 1 reading, 1 for slack */
int slavefd[SLAVES];		/* pipes from master to each slave */
int slavepid[SLAVES];		/* used by killall() */
int rotor;			/* next slave to be instructed */
int master;			/* pid of master, for sending error signals */
int tenths;			/* length of tape used per block written */

int
alloctape()
{
	int pgoff = getpagesize() - 1;

	writesize = ntrec * TP_BSIZE;
	reqsiz = ntrec * sizeof(struct req);
	/*
	 * CDC 92181's and 92185's make 0.8" gaps in 1600-bpi start/stop mode
	 * (see DEC TU80 User's Guide).  The shorter gaps of 6250-bpi require
	 * repositioning after stopping, i.e, streaming mode, where the gap is
	 * variable, 0.30" to 0.45".  The gap is maximal when the tape stops.
	 */
	if (blocksperfile == 0)
		tenths = writesize / density +
		    (cartridge ? 16 : density == 625 ? 5 : 8);
	/*
	 * Allocate tape buffer contiguous with the array of instruction
	 * packets, so flushtape() can write them together with one write().
	 * Align tape buffer on page boundary to speed up tape write().
	 */
	req = (struct req *)malloc(reqsiz + writesize + pgoff);
	if (req == NULL)
		return(0);
	tblock = (char (*)[TP_BSIZE]) (((long)&req[ntrec] + pgoff) &~ pgoff);
	req = (struct req *)tblock - ntrec;
	return(1);
}


void
writerec(dp)
	char *dp;
{
	req[trecno].dblk = (daddr_t)0;
	req[trecno].count = 1;
	*(union u_spcl *)(*tblock++) = *(union u_spcl *)dp;	/* movc3 */
	lastspclrec = spcl.c_tapea;
	trecno++;
	spcl.c_tapea++;
	if (trecno >= ntrec)
		flushtape();
}

void
dumpblock(blkno, size)
	daddr_t blkno;
	int size;
{
	int avail, tpblks, dblkno;

	dblkno = fsbtodb(sblock, blkno);
	tpblks = size >> tp_bshift;
	while ((avail = MIN(tpblks, ntrec - trecno)) > 0) {
		req[trecno].dblk = dblkno;
		req[trecno].count = avail;
		trecno += avail;
		spcl.c_tapea += avail;
		if (trecno >= ntrec)
			flushtape();
		dblkno += avail << (tp_bshift - dev_bshift);
		tpblks -= avail;
	}
}

int	nogripe = 0;

void
tperror()
{
	if (pipeout) {
		msg("write error on %s\n", tape);
		quit("Cannot recover\n");
		/* NOTREACHED */
	}
	msg("write error %d blocks into volume %d\n", blocksthisvol, tapeno);
	broadcast("DUMP WRITE ERROR!\n");
	if (!query("Do you want to restart?"))
		dumpabort();
	msg("Closing this volume.  Prepare to restart with new media;\n");
	msg("this dump volume will be rewritten.\n");
	killall();
	nogripe = 1;
	close_rewind();
	Exit(X_REWRITE);
}

void
sigpipe()
{

	quit("Broken pipe\n");
}

void
flushtape()
{
#ifndef __STDC__
	int write();
#endif

	int siz = (char *)tblock - (char *)req;

	if (atomic(write, slavefd[rotor], req, siz) != siz)
		quit("error writing command pipe: %s\n", strerror(errno));
	if (++rotor >= SLAVES)
		rotor = 0;
	tblock = (char (*)[TP_BSIZE]) &req[ntrec];
	trecno = 0;
	asize += tenths;
	blockswritten += ntrec;
	blocksthisvol += ntrec;
	if (!pipeout && (blocksperfile ?
	    (blocksthisvol >= blocksperfile) : (asize > tsize))) {
		close_rewind();
		startnewtape();
	}
	timeest();
}

void
trewind()
{
	int f;

	if (pipeout)
		return;
	for (f = 0; f < SLAVES; f++)
		close(slavefd[f]);
	while (wait((int *)NULL) >= 0)	/* wait for any signals from slaves */
		/* void */;
	msg("Closing %s\n", tape);
#ifdef RDUMP
	if (host) {
		rmtclose();
		while (rmtopen(tape, 0) < 0)
			sleep(10);
		rmtclose();
		return;
	}
#endif RDUMP
	close(tapefd);
	while ((f = open(tape, 0)) < 0)
		sleep (10);
	close(f);
}

void
close_rewind()
{
	trewind();
	if (nexttape)
		return;
	if (!nogripe) {
		msg("Change Volumes: Mount volume #%d\n", tapeno+1);
		broadcast("CHANGE DUMP VOLUMES!\7\7\n");
	}
	while (!query("Is the new volume mounted and ready to go?"))
		if (query("Do you want to abort?")) {
			dumpabort();
			/*NOTREACHED*/
		}
}

/*
 *	We implement taking and restoring checkpoints on the tape level.
 *	When each tape is opened, a new process is created by forking; this
 *	saves all of the necessary context in the parent.  The child
 *	continues the dump; the parent waits around, saving the context.
 *	If the child returns X_REWRITE, then it had problems writing that tape;
 *	this causes the parent to fork again, duplicating the context, and
 *	everything continues as if nothing had happened.
 */

void
startnewtape()
{
	int	parentpid;
	int	childpid;
	int	status;
	int	waitpid;
	sig_t	interrupt;
	int	blks, i;
	char	*p;

	interrupt = signal(SIGINT, SIG_IGN);
	parentpid = getpid();

    restore_check_point:
	(void)signal(SIGINT, interrupt);
	/*
	 *	All signals are inherited...
	 */
	childpid = fork();
	if (childpid < 0) {
		msg("Context save fork fails in parent %d\n", parentpid);
		Exit(X_ABORT);
	}
	if (childpid != 0) {
		/*
		 *	PARENT:
		 *	save the context by waiting
		 *	until the child doing all of the work returns.
		 *	don't catch the interrupt
		 */
		signal(SIGINT, SIG_IGN);
#ifdef TDEBUG
		msg("Tape: %d; parent process: %d child process %d\n",
			tapeno+1, parentpid, childpid);
#endif TDEBUG
		while ((waitpid = wait(&status)) != childpid)
			msg("Parent %d waiting for child %d has another child %d return\n",
				parentpid, childpid, waitpid);
		if (status & 0xFF) {
			msg("Child %d returns LOB status %o\n",
				childpid, status&0xFF);
		}
		status = (status >> 8) & 0xFF;
#ifdef TDEBUG
		switch(status) {
			case X_FINOK:
				msg("Child %d finishes X_FINOK\n", childpid);
				break;
			case X_ABORT:
				msg("Child %d finishes X_ABORT\n", childpid);
				break;
			case X_REWRITE:
				msg("Child %d finishes X_REWRITE\n", childpid);
				break;
			default:
				msg("Child %d finishes unknown %d\n",
					childpid, status);
				break;
		}
#endif TDEBUG
		switch(status) {
			case X_FINOK:
				Exit(X_FINOK);
			case X_ABORT:
				Exit(X_ABORT);
			case X_REWRITE:
				goto restore_check_point;
			default:
				msg("Bad return code from dump: %d\n", status);
				Exit(X_ABORT);
		}
		/*NOTREACHED*/
	} else {	/* we are the child; just continue */
#ifdef TDEBUG
		sleep(4);	/* allow time for parent's message to get out */
		msg("Child on Tape %d has parent %d, my pid = %d\n",
			tapeno+1, parentpid, getpid());
#endif TDEBUG
		/*
		 * If we have a name like "/dev/rmt0,/dev/rmt1",
		 * use the name before the comma first, and save
		 * the remaining names for subsequent volumes.
		 */
		tapeno++;		/* current tape sequence */
		if (nexttape || index(tape, ',')) {
			if (nexttape && *nexttape)
				tape = nexttape;
			if (p = index(tape, ',')) {
				*p = '\0';
				nexttape = p + 1;
			} else
				nexttape = NULL;
			msg("Dumping volume %d on %s\n", tapeno, tape);
		}
#ifdef RDUMP
		while ((tapefd = (host ? rmtopen(tape, 2) :
			pipeout ? 1 : open(tape, O_WRONLY|O_CREAT, 0666))) < 0)
#else RDUMP
		while ((tapefd =
			pipeout ? 1 : open(tape, O_WRONLY|O_CREAT, 0666)) < 0)
#endif RDUMP
		    {
			msg("Cannot open output \"%s\".\n", tape);
			if (!query("Do you want to retry the open?"))
				dumpabort();
		}

		enslave();  /* Share open tape file descriptor with slaves */

		asize = 0;
		blocksthisvol = 0;
		newtape++;		/* new tape signal */
		blks = 0;
		if (spcl.c_type != TS_END)
			for (i = 0; i < spcl.c_count; i++)
				if (spcl.c_addr[i] != 0)
					blks++;
		spcl.c_count = blks + 1 - spcl.c_tapea + lastspclrec;
		spcl.c_volume++;
		spcl.c_type = TS_TAPE;
		spcl.c_flags |= DR_NEWHEADER;
		writeheader(curino);
		spcl.c_flags &=~ DR_NEWHEADER;
		if (tapeno > 1)
			msg("Volume %d begins with blocks from inode %d\n",
				tapeno, curino);
	}
}

void
dumpabort()
{
	if (master != 0 && master != getpid())
		kill(master, SIGTERM);	/* Signals master to call dumpabort */
	else {
		killall();
		msg("The ENTIRE dump is aborted.\n");
	}
	Exit(X_ABORT);
}

void
Exit(status)
	int status;
{
#ifdef TDEBUG
	msg("pid = %d exits with status %d\n", getpid(), status);
#endif TDEBUG
	exit(status);
}

/*
 * could use pipe() for this if flock() worked on pipes
 */
void
lockfile(fd)
	int fd[2];
{
	char tmpname[20];

	strcpy(tmpname, _PATH_LOCK);
	mktemp(tmpname);
	if ((fd[1] = creat(tmpname, 0400)) < 0)
		quit("cannot create lockfile %s: %s\n",
		    tmpname, strerror(errno));
	if ((fd[0] = open(tmpname, 0)) < 0)
		quit("cannot reopen lockfile %s: %s\n",
		    tmpname, strerror(errno));
	(void) unlink(tmpname);
}

void
enslave()
{
	int first[2], prev[2], next[2], cmd[2];     /* file descriptors */
	register int i, j;

	master = getpid();
	signal(SIGTERM, dumpabort); /* Slave sends SIGTERM on dumpabort() */
	signal(SIGPIPE, sigpipe);
	signal(SIGUSR1, tperror);    /* Slave sends SIGUSR1 on tape errors */
	lockfile(first);
	for (i = 0; i < SLAVES; i++) {
		if (i == 0) {
			prev[0] = first[1];
			prev[1] = first[0];
		} else {
			prev[0] = next[0];
			prev[1] = next[1];
			flock(prev[1], LOCK_EX);
		}
		if (i < SLAVES - 1) {
			lockfile(next);
		} else {
			next[0] = first[0];
			next[1] = first[1];	    /* Last slave loops back */
		}
		if (pipe(cmd) < 0 || (slavepid[i] = fork()) < 0)
			quit("too many slaves, %d (recompile smaller): %s\n",
			    i, strerror(errno));
		slavefd[i] = cmd[1];
		if (slavepid[i] == 0) { 	    /* Slave starts up here */
			for (j = 0; j <= i; j++)
				close(slavefd[j]);
			signal(SIGINT, SIG_IGN);    /* Master handles this */
			doslave(cmd[0], prev, next);
			Exit(X_FINOK);
		}
		close(cmd[0]);
		if (i > 0) {
			close(prev[0]);
			close(prev[1]);
		}
	}
	close(first[0]);
	close(first[1]);
	master = 0; rotor = 0;
}

void
killall()
{
	register int i;

	for (i = 0; i < SLAVES; i++)
		if (slavepid[i] > 0)
			kill(slavepid[i], SIGKILL);
}

/*
 * Synchronization - each process has a lockfile, and shares file
 * descriptors to the following process's lockfile.  When our write
 * completes, we release our lock on the following process's lock-
 * file, allowing the following process to lock it and proceed. We
 * get the lock back for the next cycle by swapping descriptors.
 */
void
doslave(cmd, prev, next)
	register int cmd, prev[2], next[2];
{
	register int nread, toggle = 0;
	int nwrite;
#ifndef __STDC__
	int read();
#endif

	/*
	 * Need our own seek pointer.
	 */
	close(diskfd);
	if ((diskfd = open(disk, O_RDONLY)) < 0)
		quit("slave couldn't reopen disk: %s\n", strerror(errno));
	/*
	 * Get list of blocks to dump, read the blocks into tape buffer
	 */
	while ((nread = atomic(read, cmd, req, reqsiz)) == reqsiz) {
		register struct req *p = req;
		for (trecno = 0; trecno < ntrec; trecno += p->count, p += p->count) {
			if (p->dblk) {
				bread(p->dblk, tblock[trecno],
					p->count * TP_BSIZE);
			} else {
				if (p->count != 1 || atomic(read, cmd,
				    tblock[trecno], TP_BSIZE) != TP_BSIZE)
					quit("master/slave protocol botched.\n");
			}
		}
		flock(prev[toggle], LOCK_EX);	/* Wait our turn */

#ifdef RDUMP
		if ((nwrite = (host ? rmtwrite(tblock[0], writesize)
			: write(tapefd, tblock[0], writesize))) != writesize) {
#else RDUMP
		if ((nwrite = write(tapefd, tblock[0], writesize))
		    != writesize) {
#endif RDUMP
			if (nwrite == -1) 
				perror("write");
			else
				msg("short write: got %d instead of %d\n",
				    nwrite, writesize);
			kill(master, SIGUSR1);
			for (;;)
				sigpause(0);
		}
		toggle ^= 1;
		flock(next[toggle], LOCK_UN);	/* Next slave's turn */
	}					/* Also jolts him awake */
	if (nread != 0)
		quit("error reading command pipe: %s\n", strerror(errno));
}

/*
 * Since a read from a pipe may not return all we asked for,
 * or a write may not write all we ask if we get a signal,
 * loop until the count is satisfied (or error).
 */
int
atomic(func, fd, buf, count)
	int (*func)(), fd, count;
	char *buf;
{
	int got, need = count;

	while ((got = (*func)(fd, buf, need)) > 0 && (need -= got) > 0)
		buf += got;
	return (got < 0 ? got : count - need);
}
