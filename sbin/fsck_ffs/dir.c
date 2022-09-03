/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
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
 * 3. Neither the name of the University nor the names of its contributors
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

#if 0
#ifndef lint
static const char sccsid[] = "@(#)dir.c	8.8 (Berkeley) 4/28/95";
#endif /* not lint */
#endif
#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/sysctl.h>

#include <ufs/ufs/dinode.h>
#include <ufs/ufs/dir.h>
#include <ufs/ffs/fs.h>

#include <err.h>
#include <string.h>

#include "fsck.h"

static struct	dirtemplate emptydir = {
	0, DIRBLKSIZ, DT_UNKNOWN, 0, "",
	0, 0, DT_UNKNOWN, 0, ""
};
static struct	dirtemplate dirhead = {
	0, 12, DT_DIR, 1, ".",
	0, DIRBLKSIZ - 12, DT_DIR, 2, ".."
};

static int chgino(struct inodesc *);
static int dircheck(struct inodesc *, struct bufarea *, struct direct *);
static int expanddir(struct inode *ip, char *name);
static void freedir(ino_t ino, ino_t parent);
static struct direct *fsck_readdir(struct inodesc *);
static struct bufarea *getdirblk(ufs2_daddr_t blkno, long size);
static int lftempname(char *bufp, ino_t ino);
static int mkentry(struct inodesc *);

/*
 * Propagate connected state through the tree.
 */
void
propagate(void)
{
	struct inoinfo **inpp, *inp;
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
			    INO_IS_DUNFOUND(inp->i_number)) {
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
dirscan(struct inodesc *idesc)
{
	struct direct *dp;
	struct bufarea *bp;
	u_int dsize, n;
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
		idesc->id_dirp = (struct direct *)dbuf;
		if ((n = (*idesc->id_func)(idesc)) & ALTERED) {
			bp = getdirblk(idesc->id_blkno, blksiz);
			if (bp->b_errs != 0)
				return (STOP);
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
 * Get and verify the next entry in a directory.
 * We also verify that if there is another entry in the block that it is
 * valid, so if it is not valid it can be subsumed into the current entry. 
 */
static struct direct *
fsck_readdir(struct inodesc *idesc)
{
	struct direct *dp, *ndp;
	struct bufarea *bp;
	long size, blksiz, subsume_ndp;

	subsume_ndp = 0;
	blksiz = idesc->id_numfrags * sblock.fs_fsize;
	if (idesc->id_filesize <= 0 || idesc->id_loc >= blksiz)
		return (NULL);
	bp = getdirblk(idesc->id_blkno, blksiz);
	if (bp->b_errs != 0)
		return (NULL);
	dp = (struct direct *)(bp->b_un.b_buf + idesc->id_loc);
	/*
	 * Only need to check current entry if it is the first in the
	 * the block, as later entries will have been checked in the
	 * previous call to this function.
	 */
	if (idesc->id_loc % DIRBLKSIZ != 0 || dircheck(idesc, bp, dp) != 0) {
		/*
		 * Current entry is good, update to point at next.
		 */
		idesc->id_loc += dp->d_reclen;
		idesc->id_filesize -= dp->d_reclen;
		/*
		 * If at end of directory block, just return this entry.
		 */
		if (idesc->id_filesize <= 0 || idesc->id_loc >= blksiz ||
		    idesc->id_loc % DIRBLKSIZ == 0)
			return (dp);
		/*
		 * If the next entry good, return this entry.
		 */
		ndp = (struct direct *)(bp->b_un.b_buf + idesc->id_loc);
		if (dircheck(idesc, bp, ndp) != 0)
			return (dp);
		/*
		 * The next entry is bad, so subsume it and the remainder
		 * of this directory block into this entry.
		 */
		subsume_ndp = 1;
	}
	/*
	 * Current or next entry is bad. Zap current entry or
	 * subsume next entry into current entry as appropriate.
	 */
	size = DIRBLKSIZ - (idesc->id_loc % DIRBLKSIZ);
	idesc->id_loc += size;
	idesc->id_filesize -= size;
	if (idesc->id_fix == IGNORE)
		return (NULL);
	if (subsume_ndp) {
		memset(ndp, 0, size);
		dp->d_reclen += size;
	} else {
		memset(dp, 0, size);
		dp->d_reclen = size;
	}
	if (dofix(idesc, "DIRECTORY CORRUPTED"))
		dirty(bp);
	return (dp);
}

/*
 * Verify that a directory entry is valid.
 * This is a superset of the checks made in the kernel.
 * Also optionally clears padding and unused directory space.
 *
 * Returns 0 if the entry is bad, 1 if the entry is good.
 */
static int
dircheck(struct inodesc *idesc, struct bufarea *bp, struct direct *dp)
{
	size_t size;
	char *cp;
	u_int8_t namlen;
	int spaceleft, modified, unused;

	spaceleft = DIRBLKSIZ - (idesc->id_loc % DIRBLKSIZ);
	size = DIRSIZ(0, dp);
	if (dp->d_reclen == 0 ||
	    dp->d_reclen > spaceleft ||
	    dp->d_reclen < size ||
	    idesc->id_filesize < size ||
	    (dp->d_reclen & (DIR_ROUNDUP - 1)) != 0)
		goto bad;
	modified = 0;
	if (dp->d_ino == 0) {
		if (!zflag || fswritefd < 0)
			return (1);
		/*
		 * Special case of an unused directory entry. Normally only
		 * occurs at the beginning of a directory block when the block
		 * contains no entries. Other than the first entry in a
		 * directory block, the kernel coalesces unused space with
		 * the previous entry by extending its d_reclen. However,
		 * when cleaning up a directory, fsck may set d_ino to zero
		 * in the middle of a directory block. If we're clearing out
		 * directory cruft (-z flag), then make sure that all directory
		 * space in entries with d_ino == 0 gets fully cleared.
		 */
		if (dp->d_type != 0) {
			dp->d_type = 0;
			modified = 1;
		}
		if (dp->d_namlen != 0) {
			dp->d_namlen = 0;
			modified = 1;
		}
		unused = dp->d_reclen - __offsetof(struct direct, d_name);
		for (cp = dp->d_name; unused > 0; unused--, cp++) {
			if (*cp != '\0') {
				*cp = '\0';
				modified = 1;
			}
		}
		if (modified)
			dirty(bp);
		return (1);
	}
	/*
	 * The d_type field should not be tested here. A bad type is an error
	 * in the entry itself but is not a corruption of the directory
	 * structure itself. So blowing away all the remaining entries in the
	 * directory block is inappropriate. Rather the type error should be
	 * checked in pass1 and fixed there.
	 *
	 * The name validation should also be done in pass1 although the
	 * check to see if the name is longer than fits in the space
	 * allocated for it (i.e., the *cp != '\0' fails after exiting the
	 * loop below) then it really is a structural error that requires
	 * the stronger action taken here.
	 */
	namlen = dp->d_namlen;
	if (namlen == 0 || dp->d_type > 15)
		goto bad;
	for (cp = dp->d_name, size = 0; size < namlen; size++) {
		if (*cp == '\0' || *cp++ == '/')
			goto bad;
	}
	if (*cp != '\0')
		goto bad;
	if (zflag && fswritefd >= 0) {
		/*
		 * Clear unused directory entry space, including the d_name
		 * padding.
		 */
		/* First figure the number of pad bytes. */
		unused = roundup2(namlen + 1, DIR_ROUNDUP) - (namlen + 1);

		/* Add in the free space to the end of the record. */
		unused += dp->d_reclen - DIRSIZ(0, dp);

		/*
		 * Now clear out the unused space, keeping track if we actually
		 * changed anything.
		 */
		for (cp = &dp->d_name[namlen + 1]; unused > 0; unused--, cp++) {
			if (*cp != '\0') {
				*cp = '\0';
				modified = 1;
			}
		}
		
		if (modified)
			dirty(bp);
	}
	return (1);

bad:
	if (debug)
		printf("Bad dir: ino %d reclen %d namlen %d type %d name %s\n",
		    dp->d_ino, dp->d_reclen, dp->d_namlen, dp->d_type,
		    dp->d_name);
	return (0);
}

void
direrror(ino_t ino, const char *errmesg)
{

	fileerror(ino, ino, errmesg);
}

void
fileerror(ino_t cwd, ino_t ino, const char *errmesg)
{
	struct inode ip;
	union dinode *dp;
	char pathbuf[MAXPATHLEN + 1];

	pwarn("%s ", errmesg);
	if (ino < UFS_ROOTINO || ino > maxino) {
		pfatal("out-of-range inode number %ju", (uintmax_t)ino);
		return;
	}
	ginode(ino, &ip);
	dp = ip.i_dp;
	prtinode(&ip);
	printf("\n");
	getpathname(pathbuf, cwd, ino);
	if (ftypeok(dp))
		pfatal("%s=%s\n",
		    (DIP(dp, di_mode) & IFMT) == IFDIR ? "DIR" : "FILE",
		    pathbuf);
	else
		pfatal("NAME=%s\n", pathbuf);
	irelse(&ip);
}

void
adjust(struct inodesc *idesc, int lcnt)
{
	struct inode ip;
	union dinode *dp;
	int saveresolved;

	ginode(idesc->id_number, &ip);
	dp = ip.i_dp;
	if (DIP(dp, di_nlink) == lcnt) {
		/*
		 * If we have not hit any unresolved problems, are running
		 * in preen mode, and are on a file system using soft updates,
		 * then just toss any partially allocated files.
		 */
		if (resolved && (preen || bkgrdflag) && usedsoftdep) {
			clri(idesc, "UNREF", 1);
			irelse(&ip);
			return;
		} else {
			/*
			 * The file system can be marked clean even if
			 * a file is not linked up, but is cleared.
			 * Hence, resolved should not be cleared when
			 * linkup is answered no, but clri is answered yes.
			 */
			saveresolved = resolved;
			if (linkup(idesc->id_number, (ino_t)0, NULL) == 0) {
				resolved = saveresolved;
				clri(idesc, "UNREF", 0);
				irelse(&ip);
				return;
			}
			/*
			 * Account for the new reference created by linkup().
			 */
			lcnt--;
		}
	}
	if (lcnt != 0) {
		pwarn("LINK COUNT %s", (lfdir == idesc->id_number) ? lfname :
			((DIP(dp, di_mode) & IFMT) == IFDIR ? "DIR" : "FILE"));
		prtinode(&ip);
		printf(" COUNT %d SHOULD BE %d",
			DIP(dp, di_nlink), DIP(dp, di_nlink) - lcnt);
		if (preen || usedsoftdep) {
			if (lcnt < 0) {
				printf("\n");
				pfatal("LINK COUNT INCREASING");
			}
			if (preen)
				printf(" (ADJUSTED)\n");
		}
		if (preen || reply("ADJUST") == 1) {
			if (bkgrdflag == 0) {
				DIP_SET(dp, di_nlink, DIP(dp, di_nlink) - lcnt);
				inodirty(&ip);
			} else {
				cmd.value = idesc->id_number;
				cmd.size = -lcnt;
				if (debug)
					printf("adjrefcnt ino %ld amt %lld\n",
					    (long)cmd.value,
					    (long long)cmd.size);
				if (sysctl(adjrefcnt, MIBSIZE, 0, 0,
				    &cmd, sizeof cmd) == -1)
					rwerror("ADJUST INODE", cmd.value);
			}
		}
	}
	irelse(&ip);
}

static int
mkentry(struct inodesc *idesc)
{
	struct direct *dirp = idesc->id_dirp;
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
	dirp->d_type = inoinfo(idesc->id_parent)->ino_type;
	dirp->d_namlen = newent.d_namlen;
	memmove(dirp->d_name, idesc->id_name, (size_t)newent.d_namlen + 1);
	return (ALTERED|STOP);
}

static int
chgino(struct inodesc *idesc)
{
	struct direct *dirp = idesc->id_dirp;

	if (memcmp(dirp->d_name, idesc->id_name, (int)dirp->d_namlen + 1))
		return (KEEPON);
	dirp->d_ino = idesc->id_parent;
	dirp->d_type = inoinfo(idesc->id_parent)->ino_type;
	return (ALTERED|STOP);
}

int
linkup(ino_t orphan, ino_t parentdir, char *name)
{
	struct inode ip;
	union dinode *dp;
	int lostdir;
	ino_t oldlfdir;
	struct inoinfo *inp;
	struct inodesc idesc;
	char tempname[BUFSIZ];

	memset(&idesc, 0, sizeof(struct inodesc));
	ginode(orphan, &ip);
	dp = ip.i_dp;
	lostdir = (DIP(dp, di_mode) & IFMT) == IFDIR;
	pwarn("UNREF %s ", lostdir ? "DIR" : "FILE");
	prtinode(&ip);
	printf("\n");
	if (preen && DIP(dp, di_size) == 0) {
		irelse(&ip);
		return (0);
	}
	irelse(&ip);
	if (cursnapshot != 0) {
		pfatal("FILE LINKUP IN SNAPSHOT");
		return (0);
	}
	if (preen)
		printf(" (RECONNECTED)\n");
	else if (reply("RECONNECT") == 0)
		return (0);
	if (lfdir == 0) {
		ginode(UFS_ROOTINO, &ip);
		idesc.id_name = strdup(lfname);
		idesc.id_type = DATA;
		idesc.id_func = findino;
		idesc.id_number = UFS_ROOTINO;
		if ((ckinode(ip.i_dp, &idesc) & FOUND) != 0) {
			lfdir = idesc.id_parent;
		} else {
			pwarn("NO lost+found DIRECTORY");
			if (preen || reply("CREATE")) {
				lfdir = allocdir(UFS_ROOTINO, (ino_t)0, lfmode);
				if (lfdir != 0) {
					if (makeentry(UFS_ROOTINO, lfdir,
					    lfname) != 0) {
						numdirs++;
						if (preen)
							printf(" (CREATED)\n");
					} else {
						freedir(lfdir, UFS_ROOTINO);
						lfdir = 0;
						if (preen)
							printf("\n");
					}
				}
			}
		}
		irelse(&ip);
		if (lfdir == 0) {
			pfatal("SORRY. CANNOT CREATE lost+found DIRECTORY");
			printf("\n\n");
			return (0);
		}
	}
	ginode(lfdir, &ip);
	dp = ip.i_dp;
	if ((DIP(dp, di_mode) & IFMT) != IFDIR) {
		pfatal("lost+found IS NOT A DIRECTORY");
		if (reply("REALLOCATE") == 0) {
			irelse(&ip);
			return (0);
		}
		oldlfdir = lfdir;
		if ((lfdir = allocdir(UFS_ROOTINO, (ino_t)0, lfmode)) == 0) {
			pfatal("SORRY. CANNOT CREATE lost+found DIRECTORY\n\n");
			irelse(&ip);
			return (0);
		}
		if ((changeino(UFS_ROOTINO, lfname, lfdir) & ALTERED) == 0) {
			pfatal("SORRY. CANNOT CREATE lost+found DIRECTORY\n\n");
			irelse(&ip);
			return (0);
		}
		idesc.id_type = inoinfo(oldlfdir)->ino_idtype;
		idesc.id_func = freeblock;
		idesc.id_number = oldlfdir;
		adjust(&idesc, inoinfo(oldlfdir)->ino_linkcnt + 1);
		inoinfo(oldlfdir)->ino_linkcnt = 0;
		inodirty(&ip);
		irelse(&ip);
		ginode(lfdir, &ip);
		dp = ip.i_dp;
	}
	if (inoinfo(lfdir)->ino_state != DFOUND) {
		pfatal("SORRY. NO lost+found DIRECTORY\n\n");
		irelse(&ip);
		return (0);
	}
	(void)lftempname(tempname, orphan);
	if (makeentry(lfdir, orphan, (name ? name : tempname)) == 0) {
		pfatal("SORRY. NO SPACE IN lost+found DIRECTORY");
		printf("\n\n");
		irelse(&ip);
		return (0);
	}
	inoinfo(orphan)->ino_linkcnt--;
	if (lostdir) {
		if ((changeino(orphan, "..", lfdir) & ALTERED) == 0 &&
		    parentdir != (ino_t)-1)
			(void)makeentry(orphan, lfdir, "..");
		DIP_SET(dp, di_nlink, DIP(dp, di_nlink) + 1);
		inodirty(&ip);
		inoinfo(lfdir)->ino_linkcnt++;
		pwarn("DIR I=%lu CONNECTED. ", (u_long)orphan);
		inp = getinoinfo(parentdir);
		if (parentdir != (ino_t)-1 && inp != NULL &&
		    (inp->i_flags & INFO_NEW) == 0) {
			printf("PARENT WAS I=%lu\n", (u_long)parentdir);
			/*
			 * If the parent directory did not have to
			 * be replaced then because of the ordering
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
	irelse(&ip);
	return (1);
}

/*
 * fix an entry in a directory.
 */
int
changeino(ino_t dir, const char *name, ino_t newnum)
{
	struct inodesc idesc;
	struct inode ip;
	int error;

	memset(&idesc, 0, sizeof(struct inodesc));
	idesc.id_type = DATA;
	idesc.id_func = chgino;
	idesc.id_number = dir;
	idesc.id_fix = DONTKNOW;
	idesc.id_name = strdup(name);
	idesc.id_parent = newnum;	/* new value for name */
	ginode(dir, &ip);
	error = ckinode(ip.i_dp, &idesc);
	irelse(&ip);
	return (error);
}

/*
 * make an entry in a directory
 */
int
makeentry(ino_t parent, ino_t ino, const char *name)
{
	struct inode ip;
	union dinode *dp;
	struct inodesc idesc;
	int retval;
	char pathbuf[MAXPATHLEN + 1];

	if (parent < UFS_ROOTINO || parent >= maxino ||
	    ino < UFS_ROOTINO || ino >= maxino)
		return (0);
	memset(&idesc, 0, sizeof(struct inodesc));
	idesc.id_type = DATA;
	idesc.id_func = mkentry;
	idesc.id_number = parent;
	idesc.id_parent = ino;	/* this is the inode to enter */
	idesc.id_fix = DONTKNOW;
	idesc.id_name = strdup(name);
	ginode(parent, &ip);
	dp = ip.i_dp;
	if (DIP(dp, di_size) % DIRBLKSIZ) {
		DIP_SET(dp, di_size, roundup(DIP(dp, di_size), DIRBLKSIZ));
		inodirty(&ip);
	}
	if ((ckinode(dp, &idesc) & ALTERED) != 0) {
		irelse(&ip);
		return (1);
	}
	getpathname(pathbuf, parent, parent);
	if (expanddir(&ip, pathbuf) == 0) {
		irelse(&ip);
		return (0);
	}
	retval = ckinode(dp, &idesc) & ALTERED;
	irelse(&ip);
	return (retval);
}

/*
 * Attempt to expand the size of a directory
 */
static int
expanddir(struct inode *ip, char *name)
{
	ufs2_daddr_t lastlbn, oldblk, newblk, indirblk;
	size_t filesize, lastlbnsize;
	struct bufarea *bp, *nbp;
	struct inodesc idesc;
	union dinode *dp;
	int indiralloced;
	char *cp;

	nbp = NULL;
	indiralloced = newblk = indirblk = 0;
	pwarn("NO SPACE LEFT IN %s", name);
	if (!preen && reply("EXPAND") == 0)
		return (0);
	dp = ip->i_dp;
	filesize = DIP(dp, di_size);
	lastlbn = lblkno(&sblock, filesize);
	/*
	 * We only expand lost+found to a single indirect block.
	 */
	if ((DIP(dp, di_mode) & IFMT) != IFDIR || filesize == 0 ||
	    lastlbn >= UFS_NDADDR + NINDIR(&sblock))
		goto bad;
	/*
	 * If last block is a fragment, expand it to a full size block.
	 */
	lastlbnsize = sblksize(&sblock, filesize, lastlbn);
	if (lastlbnsize > 0 && lastlbnsize < sblock.fs_bsize) {
		oldblk = DIP(dp, di_db[lastlbn]);
		bp = getdirblk(oldblk, lastlbnsize);
		if (bp->b_errs)
			goto bad;
		if ((newblk = allocblk(sblock.fs_frag)) == 0)
			goto bad;
		nbp = getdatablk(newblk, sblock.fs_bsize, BT_DIRDATA);
		if (nbp->b_errs)
			goto bad;
		DIP_SET(dp, di_db[lastlbn], newblk);
		DIP_SET(dp, di_size, filesize + sblock.fs_bsize - lastlbnsize);
		DIP_SET(dp, di_blocks, DIP(dp, di_blocks) +
		    btodb(sblock.fs_bsize - lastlbnsize));
		inodirty(ip);
		memmove(nbp->b_un.b_buf, bp->b_un.b_buf, lastlbnsize);
		memset(&nbp->b_un.b_buf[lastlbnsize], 0,
		    sblock.fs_bsize - lastlbnsize);
		for (cp = &nbp->b_un.b_buf[lastlbnsize];
		     cp < &nbp->b_un.b_buf[sblock.fs_bsize];
		     cp += DIRBLKSIZ)
			memmove(cp, &emptydir, sizeof emptydir);
		dirty(nbp);
		brelse(nbp);
		idesc.id_blkno = oldblk;
		idesc.id_numfrags = numfrags(&sblock, lastlbnsize);
		(void)freeblock(&idesc);
		if (preen)
			printf(" (EXPANDED)\n");
		return (1);
	}
	if ((newblk = allocblk(sblock.fs_frag)) == 0)
		goto bad;
	bp = getdirblk(newblk, sblock.fs_bsize);
	if (bp->b_errs)
		goto bad;
	memset(bp->b_un.b_buf, 0, sblock.fs_bsize);
	for (cp = bp->b_un.b_buf;
	     cp < &bp->b_un.b_buf[sblock.fs_bsize];
	     cp += DIRBLKSIZ)
		memmove(cp, &emptydir, sizeof emptydir);
	dirty(bp);
	if (lastlbn < UFS_NDADDR) {
		DIP_SET(dp, di_db[lastlbn], newblk);
	} else {
		/*
		 * Allocate indirect block if needed.
		 */
		if ((indirblk = DIP(dp, di_ib[0])) == 0) {
			if ((indirblk = allocblk(sblock.fs_frag)) == 0)
				goto bad;
			indiralloced = 1;
		}
		nbp = getdatablk(indirblk, sblock.fs_bsize, BT_LEVEL1);
		if (nbp->b_errs)
			goto bad;
		if (indiralloced) {
			memset(nbp->b_un.b_buf, 0, sblock.fs_bsize);
			DIP_SET(dp, di_ib[0], indirblk);
			DIP_SET(dp, di_blocks,
			    DIP(dp, di_blocks) + btodb(sblock.fs_bsize));
		}
		IBLK_SET(nbp, lastlbn - UFS_NDADDR, newblk);
		dirty(nbp);
		brelse(nbp);
	}
	DIP_SET(dp, di_size, filesize + sblock.fs_bsize);
	DIP_SET(dp, di_blocks, DIP(dp, di_blocks) + btodb(sblock.fs_bsize));
	inodirty(ip);
	if (preen)
		printf(" (EXPANDED)\n");
	return (1);
bad:
	pfatal(" (EXPANSION FAILED)\n");
	if (nbp != NULL)
		brelse(nbp);
	if (newblk != 0) {
		idesc.id_blkno = newblk;
		idesc.id_numfrags = sblock.fs_frag;
		(void)freeblock(&idesc);
	}
	if (indiralloced) {
		idesc.id_blkno = indirblk;
		idesc.id_numfrags = sblock.fs_frag;
		(void)freeblock(&idesc);
	}
	return (0);
}

/*
 * allocate a new directory
 */
ino_t
allocdir(ino_t parent, ino_t request, int mode)
{
	ino_t ino;
	char *cp;
	struct inode ip;
	union dinode *dp;
	struct bufarea *bp;
	struct inoinfo *inp;
	struct dirtemplate *dirp;

	ino = allocino(request, IFDIR|mode);
	if (ino == 0)
		return (0);
	dirp = &dirhead;
	dirp->dot_ino = ino;
	dirp->dotdot_ino = parent;
	ginode(ino, &ip);
	dp = ip.i_dp;
	bp = getdirblk(DIP(dp, di_db[0]), sblock.fs_fsize);
	if (bp->b_errs) {
		freeino(ino);
		irelse(&ip);
		return (0);
	}
	memmove(bp->b_un.b_buf, dirp, sizeof(struct dirtemplate));
	for (cp = &bp->b_un.b_buf[DIRBLKSIZ];
	     cp < &bp->b_un.b_buf[sblock.fs_fsize];
	     cp += DIRBLKSIZ)
		memmove(cp, &emptydir, sizeof emptydir);
	dirty(bp);
	DIP_SET(dp, di_nlink, 2);
	inodirty(&ip);
	if (ino == UFS_ROOTINO) {
		inoinfo(ino)->ino_linkcnt = DIP(dp, di_nlink);
		if ((inp = getinoinfo(ino)) == NULL)
			inp = cacheino(dp, ino);
		else
			inp->i_flags = INFO_NEW;
		inp->i_parent = parent;
		inp->i_dotdot = parent;
		irelse(&ip);
		return(ino);
	}
	if (!INO_IS_DVALID(parent)) {
		freeino(ino);
		irelse(&ip);
		return (0);
	}
	inp = cacheino(dp, ino);
	inp->i_parent = parent;
	inp->i_dotdot = parent;
	inoinfo(ino)->ino_state = inoinfo(parent)->ino_state;
	if (inoinfo(ino)->ino_state == DSTATE) {
		inoinfo(ino)->ino_linkcnt = DIP(dp, di_nlink);
		inoinfo(parent)->ino_linkcnt++;
	}
	irelse(&ip);
	ginode(parent, &ip);
	dp = ip.i_dp;
	DIP_SET(dp, di_nlink, DIP(dp, di_nlink) + 1);
	inodirty(&ip);
	irelse(&ip);
	return (ino);
}

/*
 * free a directory inode
 */
static void
freedir(ino_t ino, ino_t parent)
{
	struct inode ip;
	union dinode *dp;

	if (ino != parent) {
		ginode(parent, &ip);
		dp = ip.i_dp;
		DIP_SET(dp, di_nlink, DIP(dp, di_nlink) - 1);
		inodirty(&ip);
		irelse(&ip);
	}
	freeino(ino);
}

/*
 * generate a temporary name for the lost+found directory.
 */
static int
lftempname(char *bufp, ino_t ino)
{
	ino_t in;
	char *cp;
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
getdirblk(ufs2_daddr_t blkno, long size)
{

	if (pdirbp != NULL && pdirbp->b_errs == 0)
		brelse(pdirbp);
	pdirbp = getdatablk(blkno, size, BT_DIRDATA);
	return (pdirbp);
}
