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
static const char sccsid[] = "@(#)pass2.c	8.9 (Berkeley) 4/28/95";
#endif
static const char rcsid[] =
  "$FreeBSD$";
#endif /* not lint */

#include <sys/param.h>

#include <ufs/ufs/dinode.h>
#include <ufs/ufs/dir.h>
#include <ufs/ffs/fs.h>

#include <err.h>
#include <stdint.h>
#include <string.h>

#include "fsck.h"

#define MINDIRSIZE	(sizeof (struct dirtemplate))

static int blksort(const void *, const void *);
static int pass2check(struct inodesc *);

void
pass2(void)
{
	union dinode *dp;
	struct inoinfo **inpp, *inp;
	struct inoinfo **inpend;
	struct inodesc curino;
	union dinode dino;
	int i;
	char pathbuf[MAXPATHLEN + 1];

	switch (inoinfo(ROOTINO)->ino_state) {

	case USTATE:
		pfatal("ROOT INODE UNALLOCATED");
		if (reply("ALLOCATE") == 0) {
			ckfini(0);
			exit(EEXIT);
		}
		if (allocdir(ROOTINO, ROOTINO, 0755) != ROOTINO)
			errx(EEXIT, "CANNOT ALLOCATE ROOT INODE");
		break;

	case DCLEAR:
		pfatal("DUPS/BAD IN ROOT INODE");
		if (reply("REALLOCATE")) {
			freeino(ROOTINO);
			if (allocdir(ROOTINO, ROOTINO, 0755) != ROOTINO)
				errx(EEXIT, "CANNOT ALLOCATE ROOT INODE");
			break;
		}
		if (reply("CONTINUE") == 0) {
			ckfini(0);
			exit(EEXIT);
		}
		break;

	case FSTATE:
	case FCLEAR:
		pfatal("ROOT INODE NOT DIRECTORY");
		if (reply("REALLOCATE")) {
			freeino(ROOTINO);
			if (allocdir(ROOTINO, ROOTINO, 0755) != ROOTINO)
				errx(EEXIT, "CANNOT ALLOCATE ROOT INODE");
			break;
		}
		if (reply("FIX") == 0) {
			ckfini(0);
			exit(EEXIT);
		}
		dp = ginode(ROOTINO);
		DIP(dp, di_mode) &= ~IFMT;
		DIP(dp, di_mode) |= IFDIR;
		inodirty();
		break;

	case DSTATE:
		break;

	default:
		errx(EEXIT, "BAD STATE %d FOR ROOT INODE",
		    inoinfo(ROOTINO)->ino_state);
	}
	inoinfo(ROOTINO)->ino_state = DFOUND;
	inoinfo(WINO)->ino_state = FSTATE;
	inoinfo(WINO)->ino_type = DT_WHT;
	/*
	 * Sort the directory list into disk block order.
	 */
	qsort((char *)inpsort, (size_t)inplast, sizeof *inpsort, blksort);
	/*
	 * Check the integrity of each directory.
	 */
	memset(&curino, 0, sizeof(struct inodesc));
	curino.id_type = DATA;
	curino.id_func = pass2check;
	inpend = &inpsort[inplast];
	for (inpp = inpsort; inpp < inpend; inpp++) {
		if (got_siginfo) {
			printf("%s: phase 2: dir %td of %d (%d%%)\n", cdevname,
			    inpp - inpsort, (int)inplast,
			    (int)((inpp - inpsort) * 100 / inplast));
			got_siginfo = 0;
		}
		inp = *inpp;
		if (inp->i_isize == 0)
			continue;
		if (inp->i_isize < MINDIRSIZE) {
			direrror(inp->i_number, "DIRECTORY TOO SHORT");
			inp->i_isize = roundup(MINDIRSIZE, DIRBLKSIZ);
			if (reply("FIX") == 1) {
				dp = ginode(inp->i_number);
				DIP(dp, di_size) = inp->i_isize;
				inodirty();
			}
		} else if ((inp->i_isize & (DIRBLKSIZ - 1)) != 0) {
			getpathname(pathbuf, inp->i_number, inp->i_number);
			if (usedsoftdep)
				pfatal("%s %s: LENGTH %jd NOT MULTIPLE OF %d",
					"DIRECTORY", pathbuf,
					(intmax_t)inp->i_isize, DIRBLKSIZ);
			else
				pwarn("%s %s: LENGTH %jd NOT MULTIPLE OF %d",
					"DIRECTORY", pathbuf,
					(intmax_t)inp->i_isize, DIRBLKSIZ);
			if (preen)
				printf(" (ADJUSTED)\n");
			inp->i_isize = roundup(inp->i_isize, DIRBLKSIZ);
			if (preen || reply("ADJUST") == 1) {
				dp = ginode(inp->i_number);
				DIP(dp, di_size) =
				    roundup(inp->i_isize, DIRBLKSIZ);
				inodirty();
			}
		}
		dp = &dino;
		memset(dp, 0, sizeof(struct ufs2_dinode));
		DIP(dp, di_mode) = IFDIR;
		DIP(dp, di_size) = inp->i_isize;
		for (i = 0;
		     i < (inp->i_numblks<NDADDR ? inp->i_numblks : NDADDR);
		     i++)
			DIP(dp, di_db[i]) = inp->i_blks[i];
		if (inp->i_numblks > NDADDR)
			for (i = 0; i < NIADDR; i++)
				DIP(dp, di_ib[i]) = inp->i_blks[NDADDR + i];
		curino.id_number = inp->i_number;
		curino.id_parent = inp->i_parent;
		(void)ckinode(dp, &curino);
	}
	/*
	 * Now that the parents of all directories have been found,
	 * make another pass to verify the value of `..'
	 */
	for (inpp = inpsort; inpp < inpend; inpp++) {
		inp = *inpp;
		if (inp->i_parent == 0 || inp->i_isize == 0)
			continue;
		if (inoinfo(inp->i_parent)->ino_state == DFOUND &&
		    inoinfo(inp->i_number)->ino_state == DSTATE)
			inoinfo(inp->i_number)->ino_state = DFOUND;
		if (inp->i_dotdot == inp->i_parent ||
		    inp->i_dotdot == (ino_t)-1)
			continue;
		if (inp->i_dotdot == 0) {
			inp->i_dotdot = inp->i_parent;
			fileerror(inp->i_parent, inp->i_number, "MISSING '..'");
			if (reply("FIX") == 0)
				continue;
			(void)makeentry(inp->i_number, inp->i_parent, "..");
			inoinfo(inp->i_parent)->ino_linkcnt--;
			continue;
		}
		fileerror(inp->i_parent, inp->i_number,
		    "BAD INODE NUMBER FOR '..'");
		if (reply("FIX") == 0)
			continue;
		inoinfo(inp->i_dotdot)->ino_linkcnt++;
		inoinfo(inp->i_parent)->ino_linkcnt--;
		inp->i_dotdot = inp->i_parent;
		(void)changeino(inp->i_number, "..", inp->i_parent);
	}
	/*
	 * Mark all the directories that can be found from the root.
	 */
	propagate();
}

static int
pass2check(struct inodesc *idesc)
{
	struct direct *dirp = idesc->id_dirp;
	struct inoinfo *inp;
	int n, entrysize, ret = 0;
	union dinode *dp;
	const char *errmsg;
	struct direct proto;
	char namebuf[MAXPATHLEN + 1];
	char pathbuf[MAXPATHLEN + 1];

	/*
	 * check for "."
	 */
	if (idesc->id_entryno != 0)
		goto chk1;
	if (dirp->d_ino != 0 && strcmp(dirp->d_name, ".") == 0) {
		if (dirp->d_ino != idesc->id_number) {
			direrror(idesc->id_number, "BAD INODE NUMBER FOR '.'");
			dirp->d_ino = idesc->id_number;
			if (reply("FIX") == 1)
				ret |= ALTERED;
		}
		if (dirp->d_type != DT_DIR) {
			direrror(idesc->id_number, "BAD TYPE VALUE FOR '.'");
			dirp->d_type = DT_DIR;
			if (reply("FIX") == 1)
				ret |= ALTERED;
		}
		goto chk1;
	}
	direrror(idesc->id_number, "MISSING '.'");
	proto.d_ino = idesc->id_number;
	proto.d_type = DT_DIR;
	proto.d_namlen = 1;
	(void)strcpy(proto.d_name, ".");
	entrysize = DIRSIZ(0, &proto);
	if (dirp->d_ino != 0 && strcmp(dirp->d_name, "..") != 0) {
		pfatal("CANNOT FIX, FIRST ENTRY IN DIRECTORY CONTAINS %s\n",
			dirp->d_name);
	} else if (dirp->d_reclen < entrysize) {
		pfatal("CANNOT FIX, INSUFFICIENT SPACE TO ADD '.'\n");
	} else if (dirp->d_reclen < 2 * entrysize) {
		proto.d_reclen = dirp->d_reclen;
		memmove(dirp, &proto, (size_t)entrysize);
		if (reply("FIX") == 1)
			ret |= ALTERED;
	} else {
		n = dirp->d_reclen - entrysize;
		proto.d_reclen = entrysize;
		memmove(dirp, &proto, (size_t)entrysize);
		idesc->id_entryno++;
		inoinfo(dirp->d_ino)->ino_linkcnt--;
		dirp = (struct direct *)((char *)(dirp) + entrysize);
		memset(dirp, 0, (size_t)n);
		dirp->d_reclen = n;
		if (reply("FIX") == 1)
			ret |= ALTERED;
	}
chk1:
	if (idesc->id_entryno > 1)
		goto chk2;
	inp = getinoinfo(idesc->id_number);
	proto.d_ino = inp->i_parent;
	proto.d_type = DT_DIR;
	proto.d_namlen = 2;
	(void)strcpy(proto.d_name, "..");
	entrysize = DIRSIZ(0, &proto);
	if (idesc->id_entryno == 0) {
		n = DIRSIZ(0, dirp);
		if (dirp->d_reclen < n + entrysize)
			goto chk2;
		proto.d_reclen = dirp->d_reclen - n;
		dirp->d_reclen = n;
		idesc->id_entryno++;
		inoinfo(dirp->d_ino)->ino_linkcnt--;
		dirp = (struct direct *)((char *)(dirp) + n);
		memset(dirp, 0, (size_t)proto.d_reclen);
		dirp->d_reclen = proto.d_reclen;
	}
	if (dirp->d_ino != 0 && strcmp(dirp->d_name, "..") == 0) {
		inp->i_dotdot = dirp->d_ino;
		if (dirp->d_type != DT_DIR) {
			direrror(idesc->id_number, "BAD TYPE VALUE FOR '..'");
			dirp->d_type = DT_DIR;
			if (reply("FIX") == 1)
				ret |= ALTERED;
		}
		goto chk2;
	}
	if (dirp->d_ino != 0 && strcmp(dirp->d_name, ".") != 0) {
		fileerror(inp->i_parent, idesc->id_number, "MISSING '..'");
		pfatal("CANNOT FIX, SECOND ENTRY IN DIRECTORY CONTAINS %s\n",
			dirp->d_name);
		inp->i_dotdot = (ino_t)-1;
	} else if (dirp->d_reclen < entrysize) {
		fileerror(inp->i_parent, idesc->id_number, "MISSING '..'");
		pfatal("CANNOT FIX, INSUFFICIENT SPACE TO ADD '..'\n");
		inp->i_dotdot = (ino_t)-1;
	} else if (inp->i_parent != 0) {
		/*
		 * We know the parent, so fix now.
		 */
		inp->i_dotdot = inp->i_parent;
		fileerror(inp->i_parent, idesc->id_number, "MISSING '..'");
		proto.d_reclen = dirp->d_reclen;
		memmove(dirp, &proto, (size_t)entrysize);
		if (reply("FIX") == 1)
			ret |= ALTERED;
	}
	idesc->id_entryno++;
	if (dirp->d_ino != 0)
		inoinfo(dirp->d_ino)->ino_linkcnt--;
	return (ret|KEEPON);
chk2:
	if (dirp->d_ino == 0)
		return (ret|KEEPON);
	if (dirp->d_namlen <= 2 &&
	    dirp->d_name[0] == '.' &&
	    idesc->id_entryno >= 2) {
		if (dirp->d_namlen == 1) {
			direrror(idesc->id_number, "EXTRA '.' ENTRY");
			dirp->d_ino = 0;
			if (reply("FIX") == 1)
				ret |= ALTERED;
			return (KEEPON | ret);
		}
		if (dirp->d_name[1] == '.') {
			direrror(idesc->id_number, "EXTRA '..' ENTRY");
			dirp->d_ino = 0;
			if (reply("FIX") == 1)
				ret |= ALTERED;
			return (KEEPON | ret);
		}
	}
	idesc->id_entryno++;
	n = 0;
	if (dirp->d_ino > maxino) {
		fileerror(idesc->id_number, dirp->d_ino, "I OUT OF RANGE");
		n = reply("REMOVE");
	} else if (((dirp->d_ino == WINO && dirp->d_type != DT_WHT) ||
		    (dirp->d_ino != WINO && dirp->d_type == DT_WHT))) {
		fileerror(idesc->id_number, dirp->d_ino, "BAD WHITEOUT ENTRY");
		dirp->d_ino = WINO;
		dirp->d_type = DT_WHT;
		if (reply("FIX") == 1)
			ret |= ALTERED;
	} else {
again:
		switch (inoinfo(dirp->d_ino)->ino_state) {
		case USTATE:
			if (idesc->id_entryno <= 2)
				break;
			fileerror(idesc->id_number, dirp->d_ino, "UNALLOCATED");
			n = reply("REMOVE");
			break;

		case DCLEAR:
		case FCLEAR:
			if (idesc->id_entryno <= 2)
				break;
			if (inoinfo(dirp->d_ino)->ino_state == FCLEAR)
				errmsg = "DUP/BAD";
			else if (!preen && !usedsoftdep)
				errmsg = "ZERO LENGTH DIRECTORY";
			else {
				n = 1;
				break;
			}
			fileerror(idesc->id_number, dirp->d_ino, errmsg);
			if ((n = reply("REMOVE")) == 1)
				break;
			dp = ginode(dirp->d_ino);
			inoinfo(dirp->d_ino)->ino_state =
			   (DIP(dp, di_mode) & IFMT) == IFDIR ? DSTATE : FSTATE;
			inoinfo(dirp->d_ino)->ino_linkcnt = DIP(dp, di_nlink);
			goto again;

		case DSTATE:
			if (inoinfo(idesc->id_number)->ino_state == DFOUND)
				inoinfo(dirp->d_ino)->ino_state = DFOUND;
			/* fall through */

		case DFOUND:
			inp = getinoinfo(dirp->d_ino);
			if (inp->i_parent != 0 && idesc->id_entryno > 2) {
				getpathname(pathbuf, idesc->id_number,
				    idesc->id_number);
				getpathname(namebuf, dirp->d_ino, dirp->d_ino);
				pwarn("%s%s%s %s %s\n", pathbuf,
				    (strcmp(pathbuf, "/") == 0 ? "" : "/"),
				    dirp->d_name,
				    "IS AN EXTRANEOUS HARD LINK TO DIRECTORY",
				    namebuf);
				if (cursnapshot != 0)
					break;
				if (preen) {
					printf(" (REMOVED)\n");
					n = 1;
					break;
				}
				if ((n = reply("REMOVE")) == 1)
					break;
			}
			if (idesc->id_entryno > 2)
				inp->i_parent = idesc->id_number;
			/* fall through */

		case FSTATE:
			if (dirp->d_type != inoinfo(dirp->d_ino)->ino_type) {
				fileerror(idesc->id_number, dirp->d_ino,
				    "BAD TYPE VALUE");
				dirp->d_type = inoinfo(dirp->d_ino)->ino_type;
				if (reply("FIX") == 1)
					ret |= ALTERED;
			}
			inoinfo(dirp->d_ino)->ino_linkcnt--;
			break;

		default:
			errx(EEXIT, "BAD STATE %d FOR INODE I=%d",
			    inoinfo(dirp->d_ino)->ino_state, dirp->d_ino);
		}
	}
	if (n == 0)
		return (ret|KEEPON);
	dirp->d_ino = 0;
	return (ret|KEEPON|ALTERED);
}

/*
 * Routine to sort disk blocks.
 */
static int
blksort(const void *arg1, const void *arg2)
{

	return ((*(struct inoinfo * const *)arg1)->i_blks[0] -
		(*(struct inoinfo * const *)arg2)->i_blks[0]);
}
