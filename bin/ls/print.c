/*
 * Copyright (c) 1989 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Michael Fischbein.
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
static char sccsid[] = "@(#)print.c	5.24 (Berkeley) 10/19/90";
static char rcsid[] = "$Header: /a/cvs/386BSD/src/bin/ls/print.c,v 1.3 1993/10/14 17:26:38 jtc Exp $";
#endif /* not lint */

#include <sys/param.h>
#include <sys/stat.h>
#include <stdio.h>
#include <grp.h>
#include <pwd.h>
#include <utmp.h>
#include <tzfile.h>
#include "ls.h"

printscol(stats, num)
	register LS *stats;
	register int num;
{
	for (; num--; ++stats) {
		(void)printaname(stats);
		(void)putchar('\n');
	}
}

printlong(stats, num)
	LS *stats;
	register int num;
{
	extern int errno;
	char modep[15], *user_from_uid(), *group_from_gid(), *strerror();

	if (f_total)
		(void)printf("total %lu\n", f_kblocks ?
		    howmany(stats[0].lstat.st_btotal, 2) :
		    stats[0].lstat.st_btotal);
	for (; num--; ++stats) {
		if (f_inode)
			(void)printf("%6lu ", stats->lstat.st_ino);
		if (f_size)
			(void)printf("%4ld ", f_kblocks ?
			    howmany(stats->lstat.st_blocks, 2) :
			    stats->lstat.st_blocks);
		(void)strmode(stats->lstat.st_mode, modep);
		(void)printf("%s %3u %-*s ", modep, stats->lstat.st_nlink,
		    UT_NAMESIZE, user_from_uid(stats->lstat.st_uid, 0));
		if (f_group)
			(void)printf("%-*s ", UT_NAMESIZE,
			    group_from_gid(stats->lstat.st_gid, 0));
		if (S_ISCHR(stats->lstat.st_mode) ||
		    S_ISBLK(stats->lstat.st_mode))
			(void)printf("%3d, %3d ", major(stats->lstat.st_rdev),
			    minor(stats->lstat.st_rdev));
		else
			(void)printf("%8ld ", stats->lstat.st_size);
		if (f_accesstime)
			printtime(stats->lstat.st_atime);
		else if (f_statustime)
			printtime(stats->lstat.st_ctime);
		else
			printtime(stats->lstat.st_mtime);
		(void)printf("%s", stats->name);
		if (f_type)
			(void)printtype(stats->lstat.st_mode);
		if (S_ISLNK(stats->lstat.st_mode))
			printlink(stats->name);
		(void)putchar('\n');
	}
}

#define	TAB	8

printcol(stats, num)
	LS *stats;
	int num;
{
	extern int termwidth;
	register int base, chcnt, cnt, col, colwidth;
	int endcol, numcols, numrows, row;

	colwidth = stats[0].lstat.st_maxlen;
	if (f_inode)
		colwidth += 6;
	if (f_size)
		colwidth += 5;
	if (f_type)
		colwidth += 1;

	colwidth = (colwidth + TAB) & ~(TAB - 1);
	if (termwidth < 2 * colwidth) {
		printscol(stats, num);
		return;
	}

	numcols = termwidth / colwidth;
	numrows = num / numcols;
	if (num % numcols)
		++numrows;

	if (f_size && f_total)
		(void)printf("total %lu\n", f_kblocks ?
		    howmany(stats[0].lstat.st_btotal, 2) :
		    stats[0].lstat.st_btotal);
	for (row = 0; row < numrows; ++row) {
		endcol = colwidth;
		for (base = row, chcnt = col = 0; col < numcols; ++col) {
			chcnt += printaname(stats + base);
			if ((base += numrows) >= num)
				break;
			while ((cnt = (chcnt + TAB & ~(TAB - 1))) <= endcol) {
				(void)putchar('\t');
				chcnt = cnt;
			}
			endcol += colwidth;
		}
		putchar('\n');
	}
}

/*
 * print [inode] [size] name
 * return # of characters printed, no trailing characters
 */
printaname(lp)
	LS *lp;
{
	int chcnt;

	chcnt = 0;
	if (f_inode)
		chcnt += printf("%5lu ", lp->lstat.st_ino);
	if (f_size)
		chcnt += printf("%4ld ", f_kblocks ?
		    howmany(lp->lstat.st_blocks, 2) : lp->lstat.st_blocks);
	chcnt += printf("%s", lp->name);
	if (f_type)
		chcnt += printtype(lp->lstat.st_mode);
	return(chcnt);
}

printtime(ftime)
	time_t ftime;
{
	int i;
	char *longstring, *ctime();
	time_t time();

	longstring = ctime((long *)&ftime);
	for (i = 4; i < 11; ++i)
		(void)putchar(longstring[i]);

#define	SIXMONTHS	((DAYSPERNYEAR / 2) * SECSPERDAY)
	if (f_sectime)
		for (i = 11; i < 24; i++)
			(void)putchar(longstring[i]);
	else if (ftime + SIXMONTHS > time((time_t *)NULL))
		for (i = 11; i < 16; ++i)
			(void)putchar(longstring[i]);
	else {
		(void)putchar(' ');
		for (i = 20; i < 24; ++i)
			(void)putchar(longstring[i]);
	}
	(void)putchar(' ');
}

printtype(mode)
	mode_t mode;
{
	switch(mode & S_IFMT) {
	case S_IFDIR:
		(void)putchar('/');
		return(1);
	case S_IFLNK:
		(void)putchar('@');
		return(1);
	case S_IFSOCK:
		(void)putchar('=');
		return(1);
	case S_IFIFO:
		(void)putchar('|');
		return(1);
	}
	if (mode & (S_IXUSR | S_IXGRP | S_IXOTH)) {
		(void)putchar('*');
		return(1);
	}
	return(0);
}

printlink(name)
	char *name;
{
	int lnklen;
	char path[MAXPATHLEN + 1], *strerror();

	if ((lnklen = readlink(name, path, MAXPATHLEN)) == -1) {
		(void)fprintf(stderr, "\nls: %s: %s\n", name, strerror(errno));
		return;
	}
	path[lnklen] = '\0';
	(void)printf(" -> %s", path);
}
