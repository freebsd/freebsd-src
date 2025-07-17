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

#include <sys/param.h>
#include <sys/stat.h>
#include <sys/sysctl.h>

#include <ufs/ufs/dinode.h>
#include <ufs/ufs/dir.h>
#include <ufs/ffs/fs.h>

#include <err.h>
#include <limits.h>
#include <stdint.h>
#include <string.h>

#include "fsck.h"

static ufs2_daddr_t badblk;
static ufs2_daddr_t dupblk;
static ino_t lastino;		/* last inode in use */

static int checkinode(ino_t inumber, struct inodesc *, int rebuiltcg);

void
pass1(void)
{
	struct inostat *info;
	struct inodesc idesc;
	struct bufarea *cgbp;
	struct cg *cgp;
	ino_t inumber, inosused, mininos;
	ufs2_daddr_t i, cgd;
	u_int8_t *cp;
	int c, rebuiltcg;

	badblk = dupblk = lastino = 0;

	/*
	 * Set file system reserved blocks in used block map.
	 */
	for (c = 0; c < sblock.fs_ncg; c++) {
		cgd = cgdmin(&sblock, c);
		if (c == 0) {
			i = cgbase(&sblock, c);
		} else
			i = cgsblock(&sblock, c);
		for (; i < cgd; i++)
			setbmap(i);
	}
	i = sblock.fs_csaddr;
	cgd = i + howmany(sblock.fs_cssize, sblock.fs_fsize);
	for (; i < cgd; i++)
		setbmap(i);

	/*
	 * Find all allocated blocks.
	 */
	memset(&idesc, 0, sizeof(struct inodesc));
	idesc.id_func = pass1check;
	n_files = n_blks = 0;
	for (c = 0; c < sblock.fs_ncg; c++) {
		inumber = c * sblock.fs_ipg;
		cgbp = cglookup(c);
		cgp = cgbp->b_un.b_cg;
		rebuiltcg = 0;
		if (!check_cgmagic(c, cgbp)) {
			if (!reply("REBUILD CYLINDER GROUP")) {
				cgheader_corrupt = 1;
				if (!nflag) {
					pwarn("YOU WILL NEED TO RERUN FSCK.\n");
					rerun = 1;
				}
			} else {
				rebuild_cg(c, cgbp);
				rebuiltcg = 1;
			}
		}
		if (!rebuiltcg && sblock.fs_magic == FS_UFS2_MAGIC) {
			inosused = cgp->cg_initediblk;
			if (inosused > sblock.fs_ipg) {
				pfatal("Too many initialized inodes (%ju > %d) "
				    "in cylinder group %d\nReset to %d\n",
				    (uintmax_t)inosused, sblock.fs_ipg, c,
				    sblock.fs_ipg);
				inosused = sblock.fs_ipg;
			}
		} else {
			inosused = sblock.fs_ipg;
		}
		if (got_siginfo) {
			printf("%s: phase 1: cyl group %d of %d (%d%%)\n",
			    cdevname, c, sblock.fs_ncg,
			    c * 100 / sblock.fs_ncg);
			got_siginfo = 0;
		}
		if (got_sigalarm) {
			setproctitle("%s p1 %d%%", cdevname,
			     c * 100 / sblock.fs_ncg);
			got_sigalarm = 0;
		}
		/*
		 * If we are using soft updates, then we can trust the
		 * cylinder group inode allocation maps to tell us which
		 * inodes are allocated. We will scan the used inode map
		 * to find the inodes that are really in use, and then
		 * read only those inodes in from disk.
		 */
		if ((preen || inoopt) && usedsoftdep && !rebuiltcg) {
			cp = &cg_inosused(cgp)[(inosused - 1) / CHAR_BIT];
			for ( ; inosused != 0; cp--) {
				if (*cp == 0) {
					if (inosused > CHAR_BIT)
						inosused -= CHAR_BIT;
					else
						inosused = 0;
					continue;
				}
				for (i = 1 << (CHAR_BIT - 1); i > 0; i >>= 1) {
					if (*cp & i)
						break;
					inosused--;
				}
				break;
			}
		}
		/*
		 * Allocate inoinfo structures for the allocated inodes.
		 */
		inostathead[c].il_numalloced = inosused;
		if (inosused == 0) {
			inostathead[c].il_stat = NULL;
			continue;
		}
		info = Calloc((unsigned)inosused, sizeof(struct inostat));
		if (info == NULL)
			errx(EEXIT, "cannot alloc %u bytes for inoinfo",
			    (unsigned)(sizeof(struct inostat) * inosused));
		inostathead[c].il_stat = info;
		/*
		 * Scan the allocated inodes.
		 */
		setinodebuf(c, inosused);
		for (i = 0; i < inosused; i++, inumber++) {
			if (inumber < UFS_ROOTINO) {
				(void)getnextinode(inumber, rebuiltcg);
				continue;
			}
			/*
			 * NULL return indicates probable end of allocated
			 * inodes during cylinder group rebuild attempt.
			 * We always keep trying until we get to the minimum
			 * valid number for this cylinder group.
			 */
			if (checkinode(inumber, &idesc, rebuiltcg) == 0 &&
			    i > cgp->cg_initediblk)
				break;
		}
		/*
		 * This optimization speeds up future runs of fsck
		 * by trimming down the number of inodes in cylinder
		 * groups that formerly had many inodes but now have
		 * fewer in use.
		 */
		mininos = roundup(inosused + INOPB(&sblock), INOPB(&sblock));
		if (inoopt && !preen && !rebuiltcg &&
		    sblock.fs_magic == FS_UFS2_MAGIC &&
		    cgp->cg_initediblk > 2 * INOPB(&sblock) &&
		    mininos < cgp->cg_initediblk) {
			i = cgp->cg_initediblk;
			if (mininos < 2 * INOPB(&sblock))
				cgp->cg_initediblk = 2 * INOPB(&sblock);
			else
				cgp->cg_initediblk = mininos;
			pwarn("CYLINDER GROUP %d: RESET FROM %ju TO %d %s\n",
			    c, i, cgp->cg_initediblk, "VALID INODES");
			cgdirty(cgbp);
		}
		if (inosused < sblock.fs_ipg)
			continue;
		lastino += 1;
		if (lastino < (c * sblock.fs_ipg))
			inosused = 0;
		else
			inosused = lastino - (c * sblock.fs_ipg);
		if (rebuiltcg && inosused > cgp->cg_initediblk &&
		    sblock.fs_magic == FS_UFS2_MAGIC) {
			cgp->cg_initediblk = roundup(inosused, INOPB(&sblock));
			pwarn("CYLINDER GROUP %d: FOUND %d VALID INODES\n", c,
			    cgp->cg_initediblk);
		}
		/*
		 * If we were not able to determine in advance which inodes
		 * were in use, then reduce the size of the inoinfo structure
		 * to the size necessary to describe the inodes that we
		 * really found. Always leave map space in the first cylinder
		 * group in case we need to a root or lost+found directory.
		 */
		if (inumber == lastino || c == 0)
			continue;
		inostathead[c].il_numalloced = inosused;
		if (inosused == 0) {
			free(inostathead[c].il_stat);
			inostathead[c].il_stat = NULL;
			continue;
		}
		info = Calloc((unsigned)inosused, sizeof(struct inostat));
		if (info == NULL)
			errx(EEXIT, "cannot alloc %u bytes for inoinfo",
			    (unsigned)(sizeof(struct inostat) * inosused));
		memmove(info, inostathead[c].il_stat, inosused * sizeof(*info));
		free(inostathead[c].il_stat);
		inostathead[c].il_stat = info;
	}
	freeinodebuf();
}

static int
checkinode(ino_t inumber, struct inodesc *idesc, int rebuiltcg)
{
	struct inode ip;
	union dinode *dp;
	ufs2_daddr_t ndb;
	mode_t mode;
	intmax_t size, fixsize;
	int j, ret, offset;

	if ((dp = getnextinode(inumber, rebuiltcg)) == NULL) {
		pfatal("INVALID INODE");
		goto unknown;
	}
	mode = DIP(dp, di_mode) & IFMT;
	if (mode == 0) {
		if ((sblock.fs_magic == FS_UFS1_MAGIC &&
		     (memcmp(dp->dp1.di_db, zino.dp1.di_db,
			UFS_NDADDR * sizeof(ufs1_daddr_t)) ||
		      memcmp(dp->dp1.di_ib, zino.dp1.di_ib,
			UFS_NIADDR * sizeof(ufs1_daddr_t)) ||
		      dp->dp1.di_mode || dp->dp1.di_size)) ||
		    (sblock.fs_magic == FS_UFS2_MAGIC &&
		     (memcmp(dp->dp2.di_db, zino.dp2.di_db,
			UFS_NDADDR * sizeof(ufs2_daddr_t)) ||
		      memcmp(dp->dp2.di_ib, zino.dp2.di_ib,
			UFS_NIADDR * sizeof(ufs2_daddr_t)) ||
		      dp->dp2.di_mode || dp->dp2.di_size))) {
			pfatal("PARTIALLY ALLOCATED INODE I=%lu",
			    (u_long)inumber);
			if (reply("CLEAR") == 1) {
				ginode(inumber, &ip);
				clearinode(ip.i_dp);
				inodirty(&ip);
				irelse(&ip);
			}
		}
		inoinfo(inumber)->ino_state = USTATE;
		return (1);
	}
	lastino = inumber;
	if (chkfilesize(mode, DIP(dp, di_size)) == 0) {
		pfatal("BAD FILE SIZE");
		goto unknown;
	}
	if (!preen && mode == IFMT && reply("HOLD BAD BLOCK") == 1) {
		ginode(inumber, &ip);
		dp = ip.i_dp;
		DIP_SET(dp, di_size, sblock.fs_fsize);
		DIP_SET(dp, di_mode, IFREG|0600);
		inodirty(&ip);
		irelse(&ip);
	}
	if ((mode == IFBLK || mode == IFCHR || mode == IFIFO ||
	     mode == IFSOCK) && DIP(dp, di_size) != 0) {
		if (debug)
			printf("bad special-file size %ju:",
			    (uintmax_t)DIP(dp, di_size));
		pfatal("BAD SPECIAL-FILE SIZE");
		goto unknown;
	}
	if ((mode == IFBLK || mode == IFCHR) &&
	    (dev_t)DIP(dp, di_rdev) == NODEV) {
		if (debug)
			printf("bad special-file rdev NODEV:");
		pfatal("BAD SPECIAL-FILE RDEV");
		goto unknown;
	}
	ndb = howmany(DIP(dp, di_size), sblock.fs_bsize);
	if (ndb < 0) {
		if (debug)
			printf("negative size %ju ndb %ju:",
				(uintmax_t)DIP(dp, di_size), (uintmax_t)ndb);
		pfatal("NEGATIVE FILE SIZE");
		goto unknown;
	}
	if (mode == IFBLK || mode == IFCHR)
		ndb++;
	if (mode == IFLNK) {
		/*
		 * Fake ndb value so direct/indirect block checks below
		 * will detect any garbage after symlink string.
		 */
		if (DIP(dp, di_size) < (off_t)sblock.fs_maxsymlinklen) {
			if (sblock.fs_magic == FS_UFS1_MAGIC)
				ndb = howmany(DIP(dp, di_size),
				    sizeof(ufs1_daddr_t));
			else
				ndb = howmany(DIP(dp, di_size),
				    sizeof(ufs2_daddr_t));
			if (ndb > UFS_NDADDR) {
				j = ndb - UFS_NDADDR;
				for (ndb = 1; j > 1; j--)
					ndb *= NINDIR(&sblock);
				ndb += UFS_NDADDR;
			}
		}
	}
	for (j = ndb; ndb < UFS_NDADDR && j < UFS_NDADDR; j++) {
		if (DIP(dp, di_db[j]) == 0)
			continue;
		if (debug)
			printf("invalid direct addr[%d]: %ju\n", j,
			    (uintmax_t)DIP(dp, di_db[j]));
		pfatal("INVALID DIRECT BLOCK");
		ginode(inumber, &ip);
		prtinode(&ip);
		if (reply("CLEAR") == 1) {
			DIP_SET(ip.i_dp, di_db[j], 0);
			inodirty(&ip);
		}
		irelse(&ip);
	}
	for (j = 0, ndb -= UFS_NDADDR; ndb > 0; j++)
		ndb /= NINDIR(&sblock);
	for (; j < UFS_NIADDR; j++) {
		if (DIP(dp, di_ib[j]) == 0)
			continue;
		if (debug)
			printf("invalid indirect addr: %ju\n",
			    (uintmax_t)DIP(dp, di_ib[j]));
		pfatal("INVALID INDIRECT BLOCK");
		ginode(inumber, &ip);
		prtinode(&ip);
		if (reply("CLEAR") == 1) {
			DIP_SET(ip.i_dp, di_ib[j], 0);
			inodirty(&ip);
		}
		irelse(&ip);
	}
	if (ftypeok(dp) == 0) {
		pfatal("UNKNOWN FILE TYPE");
		goto unknown;
	}
	n_files++;
	inoinfo(inumber)->ino_linkcnt = DIP(dp, di_nlink);
	if (mode == IFDIR) {
		if (DIP(dp, di_size) == 0) {
			inoinfo(inumber)->ino_state = DCLEAR;
		} else if (DIP(dp, di_nlink) == 0) {
			inoinfo(inumber)->ino_state = DZLINK;
		} else {
			inoinfo(inumber)->ino_state = DSTATE;
		}
		cacheino(dp, inumber);
		countdirs++;
	} else if (DIP(dp, di_nlink) <= 0)
		inoinfo(inumber)->ino_state = FZLINK;
	else
		inoinfo(inumber)->ino_state = FSTATE;
	inoinfo(inumber)->ino_type = IFTODT(mode);
	badblk = dupblk = 0;
	idesc->id_number = inumber;
	if (DIP(dp, di_flags) & SF_SNAPSHOT)
		inoinfo(inumber)->ino_idtype = SNAP;
	else
		inoinfo(inumber)->ino_idtype = ADDR;
	idesc->id_type = inoinfo(inumber)->ino_idtype;
	(void)ckinode(dp, idesc);
	if (sblock.fs_magic == FS_UFS2_MAGIC && dp->dp2.di_extsize > 0) {
		ndb = howmany(dp->dp2.di_extsize, sblock.fs_bsize);
		for (j = 0; j < UFS_NXADDR; j++) {
			if (--ndb == 0 &&
			    (offset = blkoff(&sblock, dp->dp2.di_extsize)) != 0)
				idesc->id_numfrags = numfrags(&sblock,
				    fragroundup(&sblock, offset));
			else
				idesc->id_numfrags = sblock.fs_frag;
			if (dp->dp2.di_extb[j] == 0)
				continue;
			idesc->id_blkno = dp->dp2.di_extb[j];
			ret = (*idesc->id_func)(idesc);
			if (ret & STOP)
				break;
		}
	}
	if (sblock.fs_magic == FS_UFS2_MAGIC)
		eascan(idesc, &dp->dp2);
	idesc->id_entryno *= btodb(sblock.fs_fsize);
	if (DIP(dp, di_blocks) != idesc->id_entryno) {
		pwarn("INCORRECT BLOCK COUNT I=%lu (%ju should be %ju)",
		    (u_long)inumber, (uintmax_t)DIP(dp, di_blocks),
		    (uintmax_t)idesc->id_entryno);
		if (preen)
			printf(" (CORRECTED)\n");
		else if (reply("CORRECT") == 0)
			return (1);
		if (bkgrdflag == 0) {
			ginode(inumber, &ip);
			DIP_SET(ip.i_dp, di_blocks, idesc->id_entryno);
			inodirty(&ip);
			irelse(&ip);
		} else {
			cmd.value = idesc->id_number;
			cmd.size = idesc->id_entryno - DIP(dp, di_blocks);
			if (debug)
				printf("adjblkcnt ino %ju amount %lld\n",
				    (uintmax_t)cmd.value, (long long)cmd.size);
			if (sysctl(adjblkcnt, MIBSIZE, 0, 0,
			    &cmd, sizeof cmd) == -1)
				rwerror("ADJUST INODE BLOCK COUNT", cmd.value);
		}
	}
	/*
	 * UFS does not allow files to end with a hole; it requires that
	 * the last block of a file be allocated. The last allocated block
	 * in a file is tracked in id_lballoc. Here, we check for a size
	 * past the last allocated block of the file and if that is found,
	 * shorten the file to reference the last allocated block to avoid
	 * having it reference a hole at its end.
	 * 
	 * Soft updates will always ensure that the file size is correct
	 * for files that contain only direct block pointers. However
	 * soft updates does not roll back sizes for files with indirect
	 * blocks that it has set to unallocated because their contents
	 * have not yet been written to disk. Hence, the file can appear
	 * to have a hole at its end because the block pointer has been
	 * rolled back to zero. Thus finding a hole at the end of a file
	 * that is located in an indirect block receives only a warning
	 * while finding a hole at the end of a file in a direct block
	 * receives a fatal error message.
	 */
	size = DIP(dp, di_size);
	if (idesc->id_lballoc < lblkno(&sblock, size - 1) &&
	    /* exclude embedded symbolic links */
	    ((mode != IFLNK) || size >= sblock.fs_maxsymlinklen)) {
 		fixsize = lblktosize(&sblock, idesc->id_lballoc + 1);
		if (size > UFS_NDADDR * sblock.fs_bsize)
			pwarn("INODE %lu: FILE SIZE %ju BEYOND END OF "
			      "ALLOCATED FILE, SIZE SHOULD BE %ju",
			      (u_long)inumber, size, fixsize);
		else
			pfatal("INODE %lu: FILE SIZE %ju BEYOND END OF "
			      "ALLOCATED FILE, SIZE SHOULD BE %ju",
			      (u_long)inumber, size, fixsize);
		if (preen)
			printf(" (ADJUSTED)\n");
		else if (reply("ADJUST") == 0)
			return (1);
		if (bkgrdflag == 0) {
			ginode(inumber, &ip);
			DIP_SET(ip.i_dp, di_size, fixsize);
			inodirty(&ip);
			irelse(&ip);
		} else {
			cmd.value = idesc->id_number;
			cmd.size = fixsize;
			if (debug)
				printf("setsize ino %ju size set to %ju\n",
				    (uintmax_t)cmd.value, (uintmax_t)cmd.size);
			if (sysctl(setsize, MIBSIZE, 0, 0,
			    &cmd, sizeof cmd) == -1)
				rwerror("SET INODE SIZE", cmd.value);
		}

	}
	return (1);
unknown:
	ginode(inumber, &ip);
	prtinode(&ip);
	inoinfo(inumber)->ino_state = USTATE;
	if (reply("CLEAR") == 1) {
		clearinode(ip.i_dp);
		inodirty(&ip);
	}
	irelse(&ip);
	return (1);
}

int
pass1check(struct inodesc *idesc)
{
	int res = KEEPON;
	int anyout, nfrags;
	ufs2_daddr_t blkno = idesc->id_blkno;
	struct dups *dlp;
	struct dups *new;

	if (idesc->id_type == SNAP) {
		if (blkno == BLK_NOCOPY)
			return (KEEPON);
		if (idesc->id_number == cursnapshot) {
			if (blkno == blkstofrags(&sblock, idesc->id_lbn))
				return (KEEPON);
			if (blkno == BLK_SNAP) {
				blkno = blkstofrags(&sblock, idesc->id_lbn);
				idesc->id_entryno -= idesc->id_numfrags;
			}
		} else {
			if (blkno == BLK_SNAP)
				return (KEEPON);
		}
	}
	if ((anyout = chkrange(blkno, idesc->id_numfrags)) != 0) {
		blkerror(idesc->id_number, "BAD", blkno);
		if (badblk++ >= MAXBAD) {
			pwarn("EXCESSIVE BAD BLKS I=%lu",
			    (u_long)idesc->id_number);
			if (preen)
				printf(" (SKIPPING)\n");
			else if (reply("CONTINUE") == 0) {
				ckfini(0);
				exit(EEXIT);
			}
			rerun = 1;
			return (STOP);
		}
	}
	for (nfrags = idesc->id_numfrags; nfrags > 0; blkno++, nfrags--) {
		if (anyout && chkrange(blkno, 1)) {
			res = SKIP;
		} else if (!testbmap(blkno)) {
			n_blks++;
			setbmap(blkno);
		} else {
			blkerror(idesc->id_number, "DUP", blkno);
			if (dupblk++ >= MAXDUP) {
				pwarn("EXCESSIVE DUP BLKS I=%lu",
					(u_long)idesc->id_number);
				if (preen)
					printf(" (SKIPPING)\n");
				else if (reply("CONTINUE") == 0) {
					ckfini(0);
					exit(EEXIT);
				}
				rerun = 1;
				return (STOP);
			}
			new = (struct dups *)Malloc(sizeof(struct dups));
			if (new == NULL) {
				pfatal("DUP TABLE OVERFLOW.");
				if (reply("CONTINUE") == 0) {
					ckfini(0);
					exit(EEXIT);
				}
				rerun = 1;
				return (STOP);
			}
			new->dup = blkno;
			if (muldup == NULL) {
				duplist = muldup = new;
				new->next = NULL;
			} else {
				new->next = muldup->next;
				muldup->next = new;
			}
			for (dlp = duplist; dlp != muldup; dlp = dlp->next)
				if (dlp->dup == blkno)
					break;
			if (dlp == muldup && dlp->dup != blkno)
				muldup = new;
		}
		/*
		 * count the number of blocks found in id_entryno
		 */
		idesc->id_entryno++;
	}
	if (idesc->id_level == 0 && idesc->id_lballoc < idesc->id_lbn)
		idesc->id_lballoc = idesc->id_lbn;
	return (res);
}
