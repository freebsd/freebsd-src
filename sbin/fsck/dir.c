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
static const char sccsid[] = "@(#)dir.c	8.8 (Berkeley) 4/28/95";
#endif
static const char rcsid[] =
  "$FreeBSD$";
#endif /* not lint */

#include <sys/param.h>
#include <sys/time.h>

#include <ufs/ufs/dinode.h>
#include <ufs/ufs/dir.h>
#include <ufs/ffs/fs.h>

#include <err.h>
#include <string.h>

#include "fsck.h"

char	*lfname = "lost+found";
int	lfmode = 01777;
struct	dirtemplate emptydir = { 0, DIRBLKSIZ };
struct	dirtemplate dirhead = {
	0, 12, DT_DIR, 1, ".",
	0, DIRBLKSIZ - 12, DT_DIR, 2, ".."
};
struct	odirtemplate odirhead = {
	0, 12, 1, ".",
	0, DIRBLKSIZ - 12, 2, ".."
};

static int chgino __P((struct inodesc *));
static int dircheck __P((struct inodesc *, struct direct *));
static int expanddir __P((struct dinode *dp, char *name));
static void freedir __P((ino_t ino, ino_t parent));
static struct direct *fsck_readdir __P((struct inodesc *));
static struct bufarea *getdirblk __P((ufs_daddr_t blkno, long size));
static int lftempname __P((char *bufp, ino_t ino));
static int mkentry __P((struct inodesc *));

/*
 * Propagate connected state through the tree.
 */
void
propagate()
{
	register struct inoinfo **inpp, *inp;
	struct inoinfo **inpend;
	long change;

	inpend = &inpsort[inplast];
	do {
		change = 0;
		for (inpp = inpsort; inpp < inpend; inpp++) {
			inp = *inpp;
			if (inp->i_parent == 0)
				continue;
			if (inoinfo(inp->i_parent)->ino_state == DFOUND &&
			    inoinfo(inp->i_number)->ino_state == DSTATE) {
				inoinfo(inp->i_number)->ino_state = DFOUND;
				change++;
			}
		}
	} while (change > 0);
}

/*
 * Scan each entry in a directory block.
 */
int
dirscan(idesc)
	register struct inodesc *idesc;
{
	register struct direct *dp;
	register struct bufarea *bp;
	int dsize, n;
	long blksiz;
	char dbuf[DIRBLKSIZ];

	if (idesc->id_type != DATA)
		errx(EEXIT, "wrong type to dirscan %d", idesc->id_type);
	if (idesc->id_entryno == 0 &&
	    (idesc->id_filesize & (DIRBLKSIZ - 1)) != 0)
		idesc->id_filesize = roundup(idesc->id_filesize, DIRBLKSIZ);
	blksiz = idesc->id_numfrags * sblock.fs_fsize;
	if (chkrange(idesc->id_blkno, idesc->id_numfrags)) {
		idesc->id_filesize -= blksiz;
		return (SKIP);
	}
	idesc->id_loc = 0;
	for (dp = fsck_readdir(idesc); dp != NULL; dp = fsck_readdir(idesc)) {
		dsize = dp->d_reclen;
		if (dsize > sizeof(dbuf))
			dsize = sizeof(dbuf);
		memmove(dbuf, dp, (size_t)dsize);
#		if (BYTE_ORDER == LITTLE_ENDIAN)
			if (!newinofmt) {
				struct direct *tdp = (struct direct *)dbuf;
				u_char tmp;

				tmp = tdp->d_namlen;
				tdp->d_namlen = tdp->d_type;
				tdp->d_type = tmp;
			}
#		endif
		idesc->id_dirp = (struct direct *)dbuf;
		if ((n = (*idesc->id_func)(idesc)) & ALTERED) {
#			if (BYTE_ORDER == LITTLE_ENDIAN)
				if (!newinofmt && !doinglevel2) {
					struct direct *tdp;
					u_char tmp;

					tdp = (struct direct *)dbuf;
					tmp = tdp->d_namlen;
					tdp->d_namlen = tdp->d_type;
					tdp->d_type = tmp;
				}
#			endif
			bp = getdirblk(idesc->id_blkno, blksiz);
			memmove(bp->b_un.b_buf + idesc->id_loc - dsize, dbuf,
			    (size_t)dsize);
			dirty(bp);
			sbdirty();
		}
		if (n & STOP)
			return (n);
	}
	return (idesc->id_filesize > 0 ? KEEPON : STOP);
}

/*
 * get next entry in a directory.
 */
static struct direct *
fsck_readdir(idesc)
	register struct inodesc *idesc;
{
	register struct direct *dp, *ndp;
	register struct bufarea *bp;
	long size, blksiz, fix, dploc;

	blksiz = idesc->id_numfrags * sblock.fs_fsize;
	bp = getdirblk(idesc->id_blkno, blksiz);
	if (idesc->id_loc % DIRBLKSIZ == 0 && idesc->id_filesize > 0 &&
	    idesc->id_loc < blksiz) {
		dp = (struct direct *)(bp->b_un.b_buf + idesc->id_loc);
		if (dircheck(idesc, dp))
			goto dpok;
		if (idesc->id_fix == IGNORE)
			return (0);
		fix = dofix(idesc, "DIRECTORY CORRUPTED");
		bp = getdirblk(idesc->id_blkno, blksiz);
		dp = (struct direct *)(bp->b_un.b_buf + idesc->id_loc);
		dp->d_reclen = DIRBLKSIZ;
		dp->d_ino = 0;
		dp->d_type = 0;
		dp->d_namlen = 0;
		dp->d_name[0] = '\0';
		if (fix)
			dirty(bp);
		idesc->id_loc += DIRBLKSIZ;
		idesc->id_filesize -= DIRBLKSIZ;
		return (dp);
	}
dpok:
	if (idesc->id_filesize <= 0 || idesc->id_loc >= blksiz)
		return NULL;
	dploc = idesc->id_loc;
	dp = (struct direct *)(bp->b_un.b_buf + dploc);
	idesc->id_loc += dp->d_reclen;
	idesc->id_filesize -= dp->d_reclen;
	if ((idesc->id_loc % DIRBLKSIZ) == 0)
		return (dp);
	ndp = (struct direct *)(bp->b_un.b_buf + idesc->id_loc);
	if (idesc->id_loc < blksiz && idesc->id_filesize > 0 &&
	    dircheck(idesc, ndp) == 0) {
		size = DIRBLKSIZ - (idesc->id_loc % DIRBLKSIZ);
		idesc->id_loc += size;
		idesc->id_filesize -= size;
		if (idesc->id_fix == IGNORE)
			return (0);
		fix = dofix(idesc, "DIRECTORY CORRUPTED");
		bp = getdirblk(idesc->id_blkno, blksiz);
		dp = (struct direct *)(bp->b_un.b_buf + dploc);
		dp->d_reclen += size;
		if (fix)
			dirty(bp);
	}
	return (dp);
}

/*
 * Verify that a directory entry is valid.
 * This is a superset of the checks made in the kernel.
 */
static int
dircheck(idesc, dp)
	struct inodesc *idesc;
	register struct direct *dp;
{
	register int size;
	register char *cp;
	u_char namlen, type;
	int spaceleft;

	spaceleft = DIRBLKSIZ - (idesc->id_loc % DIRBLKSIZ);
	if (dp->d_reclen == 0 ||
	    dp->d_reclen > spaceleft ||
	    (dp->d_reclen & 0x3) != 0)
		return (0);
	if (dp->d_ino == 0)
		return (1);
	size = DIRSIZ(!newinofmt, dp);
#	if (BYTE_ORDER == LITTLE_ENDIAN)
		if (!newinofmt) {
			type = dp->d_namlen;
			namlen = dp->d_type;
		} else {
			namlen = dp->d_namlen;
			type = dp->d_type;
		}
#	else
		namlen = dp->d_namlen;
		type = dp->d_type;
#	endif
	if (dp->d_reclen < size ||
	    idesc->id_filesize < size ||
	    namlen > MAXNAMLEN ||
	    type > 15)
		return (0);
	for (cp = dp->d_name, size = 0; size < namlen; size++)
		if (*cp == '\0' || (*cp++ == '/'))
			return (0);
	if (*cp != '\0')
		return (0);
	return (1);
}

void
direrror(ino, errmesg)
	ino_t ino;
	char *errmesg;
{

	fileerror(ino, ino, errmesg);
}

void
fileerror(cwd, ino, errmesg)
	ino_t cwd, ino;
	char *errmesg;
{
	register struct dinode *dp;
	char pathbuf[MAXPATHLEN + 1];

	pwarn("%s ", errmesg);
	pinode(ino);
	printf("\n");
	getpathname(pathbuf, cwd, ino);
	if (ino < ROOTINO || ino > maxino) {
		pfatal("NAME=%s\n", pathbuf);
		return;
	}
	dp = ginode(ino);
	if (ftypeok(dp))
		pfatal("%s=%s\n",
		    (dp->di_mode & IFMT) == IFDIR ? "DIR" : "FILE", pathbuf);
	else
		pfatal("NAME=%s\n", pathbuf);
}

void
adjust(idesc, lcnt)
	register struct inodesc *idesc;
	int lcnt;
{
	struct dinode *dp;
	int saveresolved;

	dp = ginode(idesc->id_number);
	if (dp->di_nlink == lcnt) {
		/*
		 * If we have not hit any unresolved problems, are running
		 * in preen mode, and are on a filesystem using soft updates,
		 * then just toss any partially allocated files.
		 */
		if (resolved && preen && usedsoftdep) {
			clri(idesc, "UNREF", 1);
			return;
		} else {
			/*
			 * The filesystem can be marked clean even if
			 * a file is not linked up, but is cleared.
			 * Hence, resolved should not be cleared when
			 * linkup is answered no, but clri is answered yes.
			 */
			saveresolved = resolved;
			if (linkup(idesc->id_number, (ino_t)0, NULL) == 0) {
				resolved = saveresolved;
				clri(idesc, "UNREF", 0);
				return;
			}
			/*
			 * Account for the new reference created by linkup().
			 */
			dp = ginode(idesc->id_number);
			lcnt--;
		}
	}
	if (lcnt != 0) {
		pwarn("LINK COUNT %s", (lfdir == idesc->id_number) ? lfname :
			((dp->di_mode & IFMT) == IFDIR ? "DIR" : "FILE"));
		pinode(idesc->id_number);
		printf(" COUNT %d SHOULD BE %d",
			dp->di_nlink, dp->di_nlink - lcnt);
		if (preen || usedsoftdep) {
			if (lcnt < 0) {
				printf("\n");
				pfatal("LINK COUNT INCREASING");
			}
			if (preen)
				printf(" (ADJUSTED)\n");
		}
		if (preen || reply("ADJUST") == 1) {
			dp->di_nlink -= lcnt;
			inodirty();
		}
	}
}

static int
mkentry(idesc)
	struct inodesc *idesc;
{
	register struct direct *dirp = idesc->id_dirp;
	struct direct newent;
	int newlen, oldlen;

	newent.d_namlen = strlen(idesc->id_name);
	newlen = DIRSIZ(0, &newent);
	if (dirp->d_ino != 0)
		oldlen = DIRSIZ(0, dirp);
	else
		oldlen = 0;
	if (dirp->d_reclen - oldlen < newlen)
		return (KEEPON);
	newent.d_reclen = dirp->d_reclen - oldlen;
	dirp->d_reclen = oldlen;
	dirp = (struct direct *)(((char *)dirp) + oldlen);
	dirp->d_ino = idesc->id_parent;	/* ino to be entered is in id_parent */
	dirp->d_reclen = newent.d_reclen;
	if (newinofmt)
		dirp->d_type = inoinfo(idesc->id_parent)->ino_type;
	else
		dirp->d_type = 0;
	dirp->d_namlen = newent.d_namlen;
	memmove(dirp->d_name, idesc->id_name, (size_t)newent.d_namlen + 1);
#	if (BYTE_ORDER == LITTLE_ENDIAN)
		/*
		 * If the entry was split, dirscan() will only reverse the byte
		 * order of the original entry, and not the new one, before
		 * writing it back out.  So, we reverse the byte order here if
		 * necessary.
		 */
		if (oldlen != 0 && !newinofmt && !doinglevel2) {
			u_char tmp;

			tmp = dirp->d_namlen;
			dirp->d_namlen = dirp->d_type;
			dirp->d_type = tmp;
		}
#	endif
	return (ALTERED|STOP);
}

static int
chgino(idesc)
	struct inodesc *idesc;
{
	register struct direct *dirp = idesc->id_dirp;

	if (memcmp(dirp->d_name, idesc->id_name, (int)dirp->d_namlen + 1))
		return (KEEPON);
	dirp->d_ino = idesc->id_parent;
	if (newinofmt)
		dirp->d_type = inoinfo(idesc->id_parent)->ino_type;
	else
		dirp->d_type = 0;
	return (ALTERED|STOP);
}

int
linkup(orphan, parentdir, name)
	ino_t orphan;
	ino_t parentdir;
	char *name;
{
	register struct dinode *dp;
	int lostdir;
	ino_t oldlfdir;
	struct inodesc idesc;
	char tempname[BUFSIZ];

	memset(&idesc, 0, sizeof(struct inodesc));
	dp = ginode(orphan);
	lostdir = (dp->di_mode & IFMT) == IFDIR;
	pwarn("UNREF %s ", lostdir ? "DIR" : "FILE");
	pinode(orphan);
	if (preen && dp->di_size == 0)
		return (0);
	if (preen)
		printf(" (RECONNECTED)\n");
	else
		if (reply("RECONNECT") == 0)
			return (0);
	if (lfdir == 0) {
		dp = ginode(ROOTINO);
		idesc.id_name = lfname;
		idesc.id_type = DATA;
		idesc.id_func = findino;
		idesc.id_number = ROOTINO;
		if ((ckinode(dp, &idesc) & FOUND) != 0) {
			lfdir = idesc.id_parent;
		} else {
			pwarn("NO lost+found DIRECTORY");
			if (preen || reply("CREATE")) {
				lfdir = allocdir(ROOTINO, (ino_t)0, lfmode);
				if (lfdir != 0) {
					if (makeentry(ROOTINO, lfdir, lfname) != 0) {
						numdirs++;
						if (preen)
							printf(" (CREATED)\n");
					} else {
						freedir(lfdir, ROOTINO);
						lfdir = 0;
						if (preen)
							printf("\n");
					}
				}
			}
		}
		if (lfdir == 0) {
			pfatal("SORRY. CANNOT CREATE lost+found DIRECTORY");
			printf("\n\n");
			return (0);
		}
	}
	dp = ginode(lfdir);
	if ((dp->di_mode & IFMT) != IFDIR) {
		pfatal("lost+found IS NOT A DIRECTORY");
		if (reply("REALLOCATE") == 0)
			return (0);
		oldlfdir = lfdir;
		if ((lfdir = allocdir(ROOTINO, (ino_t)0, lfmode)) == 0) {
			pfatal("SORRY. CANNOT CREATE lost+found DIRECTORY\n\n");
			return (0);
		}
		if ((changeino(ROOTINO, lfname, lfdir) & ALTERED) == 0) {
			pfatal("SORRY. CANNOT CREATE lost+found DIRECTORY\n\n");
			return (0);
		}
		inodirty();
		idesc.id_type = ADDR;
		idesc.id_func = pass4check;
		idesc.id_number = oldlfdir;
		adjust(&idesc, inoinfo(oldlfdir)->ino_linkcnt + 1);
		inoinfo(oldlfdir)->ino_linkcnt = 0;
		dp = ginode(lfdir);
	}
	if (inoinfo(lfdir)->ino_state != DFOUND) {
		pfatal("SORRY. NO lost+found DIRECTORY\n\n");
		return (0);
	}
	(void)lftempname(tempname, orphan);
	if (makeentry(lfdir, orphan, (name ? name : tempname)) == 0) {
		pfatal("SORRY. NO SPACE IN lost+found DIRECTORY");
		printf("\n\n");
		return (0);
	}
	inoinfo(orphan)->ino_linkcnt--;
	if (lostdir) {
		if ((changeino(orphan, "..", lfdir) & ALTERED) == 0 &&
		    parentdir != (ino_t)-1)
			(void)makeentry(orphan, lfdir, "..");
		dp = ginode(lfdir);
		dp->di_nlink++;
		inodirty();
		inoinfo(lfdir)->ino_linkcnt++;
		pwarn("DIR I=%lu CONNECTED. ", orphan);
		if (parentdir != (ino_t)-1) {
			printf("PARENT WAS I=%lu\n", (u_long)parentdir);
			/*
			 * The parent directory, because of the ordering
			 * guarantees, has had the link count incremented
			 * for the child, but no entry was made.  This
			 * fixes the parent link count so that fsck does
			 * not need to be rerun.
			 */
			inoinfo(parentdir)->ino_linkcnt++;
		}
		if (preen == 0)
			printf("\n");
	}
	return (1);
}

/*
 * fix an entry in a directory.
 */
int
changeino(dir, name, newnum)
	ino_t dir;
	char *name;
	ino_t newnum;
{
	struct inodesc idesc;

	memset(&idesc, 0, sizeof(struct inodesc));
	idesc.id_type = DATA;
	idesc.id_func = chgino;
	idesc.id_number = dir;
	idesc.id_fix = DONTKNOW;
	idesc.id_name = name;
	idesc.id_parent = newnum;	/* new value for name */
	return (ckinode(ginode(dir), &idesc));
}

/*
 * make an entry in a directory
 */
int
makeentry(parent, ino, name)
	ino_t parent, ino;
	char *name;
{
	struct dinode *dp;
	struct inodesc idesc;
	char pathbuf[MAXPATHLEN + 1];

	if (parent < ROOTINO || parent >= maxino ||
	    ino < ROOTINO || ino >= maxino)
		return (0);
	memset(&idesc, 0, sizeof(struct inodesc));
	idesc.id_type = DATA;
	idesc.id_func = mkentry;
	idesc.id_number = parent;
	idesc.id_parent = ino;	/* this is the inode to enter */
	idesc.id_fix = DONTKNOW;
	idesc.id_name = name;
	dp = ginode(parent);
	if (dp->di_size % DIRBLKSIZ) {
		dp->di_size = roundup(dp->di_size, DIRBLKSIZ);
		inodirty();
	}
	if ((ckinode(dp, &idesc) & ALTERED) != 0)
		return (1);
	getpathname(pathbuf, parent, parent);
	dp = ginode(parent);
	if (expanddir(dp, pathbuf) == 0)
		return (0);
	return (ckinode(dp, &idesc) & ALTERED);
}

/*
 * Attempt to expand the size of a directory
 */
static int
expanddir(dp, name)
	register struct dinode *dp;
	char *name;
{
	ufs_daddr_t lastbn, newblk;
	register struct bufarea *bp;
	char *cp, firstblk[DIRBLKSIZ];

	lastbn = lblkno(&sblock, dp->di_size);
	if (lastbn >= NDADDR - 1 || dp->di_db[lastbn] == 0 || dp->di_size == 0)
		return (0);
	if ((newblk = allocblk(sblock.fs_frag)) == 0)
		return (0);
	dp->di_db[lastbn + 1] = dp->di_db[lastbn];
	dp->di_db[lastbn] = newblk;
	dp->di_size += sblock.fs_bsize;
	dp->di_blocks += btodb(sblock.fs_bsize);
	bp = getdirblk(dp->di_db[lastbn + 1],
		(long)dblksize(&sblock, dp, lastbn + 1));
	if (bp->b_errs)
		goto bad;
	memmove(firstblk, bp->b_un.b_buf, DIRBLKSIZ);
	bp = getdirblk(newblk, sblock.fs_bsize);
	if (bp->b_errs)
		goto bad;
	memmove(bp->b_un.b_buf, firstblk, DIRBLKSIZ);
	for (cp = &bp->b_un.b_buf[DIRBLKSIZ];
	     cp < &bp->b_un.b_buf[sblock.fs_bsize];
	     cp += DIRBLKSIZ)
		memmove(cp, &emptydir, sizeof emptydir);
	dirty(bp);
	bp = getdirblk(dp->di_db[lastbn + 1],
		(long)dblksize(&sblock, dp, lastbn + 1));
	if (bp->b_errs)
		goto bad;
	memmove(bp->b_un.b_buf, &emptydir, sizeof emptydir);
	pwarn("NO SPACE LEFT IN %s", name);
	if (preen)
		printf(" (EXPANDED)\n");
	else if (reply("EXPAND") == 0)
		goto bad;
	dirty(bp);
	inodirty();
	return (1);
bad:
	dp->di_db[lastbn] = dp->di_db[lastbn + 1];
	dp->di_db[lastbn + 1] = 0;
	dp->di_size -= sblock.fs_bsize;
	dp->di_blocks -= btodb(sblock.fs_bsize);
	freeblk(newblk, sblock.fs_frag);
	return (0);
}

/*
 * allocate a new directory
 */
ino_t
allocdir(parent, request, mode)
	ino_t parent, request;
	int mode;
{
	ino_t ino;
	char *cp;
	struct dinode *dp;
	register struct bufarea *bp;
	struct dirtemplate *dirp;

	ino = allocino(request, IFDIR|mode);
	if (newinofmt)
		dirp = &dirhead;
	else
		dirp = (struct dirtemplate *)&odirhead;
	dirp->dot_ino = ino;
	dirp->dotdot_ino = parent;
	dp = ginode(ino);
	bp = getdirblk(dp->di_db[0], sblock.fs_fsize);
	if (bp->b_errs) {
		freeino(ino);
		return (0);
	}
	memmove(bp->b_un.b_buf, dirp, sizeof(struct dirtemplate));
	for (cp = &bp->b_un.b_buf[DIRBLKSIZ];
	     cp < &bp->b_un.b_buf[sblock.fs_fsize];
	     cp += DIRBLKSIZ)
		memmove(cp, &emptydir, sizeof emptydir);
	dirty(bp);
	dp->di_nlink = 2;
	inodirty();
	if (ino == ROOTINO) {
		inoinfo(ino)->ino_linkcnt = dp->di_nlink;
		cacheino(dp, ino);
		return(ino);
	}
	if (inoinfo(parent)->ino_state != DSTATE &&
	    inoinfo(parent)->ino_state != DFOUND) {
		freeino(ino);
		return (0);
	}
	cacheino(dp, ino);
	inoinfo(ino)->ino_state = inoinfo(parent)->ino_state;
	if (inoinfo(ino)->ino_state == DSTATE) {
		inoinfo(ino)->ino_linkcnt = dp->di_nlink;
		inoinfo(parent)->ino_linkcnt++;
	}
	dp = ginode(parent);
	dp->di_nlink++;
	inodirty();
	return (ino);
}

/*
 * free a directory inode
 */
static void
freedir(ino, parent)
	ino_t ino, parent;
{
	struct dinode *dp;

	if (ino != parent) {
		dp = ginode(parent);
		dp->di_nlink--;
		inodirty();
	}
	freeino(ino);
}

/*
 * generate a temporary name for the lost+found directory.
 */
static int
lftempname(bufp, ino)
	char *bufp;
	ino_t ino;
{
	register ino_t in;
	register char *cp;
	int namlen;

	cp = bufp + 2;
	for (in = maxino; in > 0; in /= 10)
		cp++;
	*--cp = 0;
	namlen = cp - bufp;
	in = ino;
	while (cp > bufp) {
		*--cp = (in % 10) + '0';
		in /= 10;
	}
	*cp = '#';
	return (namlen);
}

/*
 * Get a directory block.
 * Insure that it is held until another is requested.
 */
static struct bufarea *
getdirblk(blkno, size)
	ufs_daddr_t blkno;
	long size;
{

	if (pdirbp != 0)
		pdirbp->b_flags &= ~B_INUSE;
	pdirbp = getdatablk(blkno, size);
	return (pdirbp);
}
