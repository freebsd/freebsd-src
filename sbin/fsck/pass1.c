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
static char sccsid[] = "@(#)pass1.c	5.16 (Berkeley) 3/19/91";
static char rcsid[] = "$Header: /a/cvs/386BSD/src/sbin/fsck/pass1.c,v 1.2 1993/07/22 16:51:52 jkh Exp $";
#endif /* not lint */

#include <sys/param.h>
#include <ufs/dinode.h>
#include <ufs/fs.h>
#include <stdlib.h>
#include <string.h>
#include "fsck.h"

static daddr_t badblk;
static daddr_t dupblk;
int pass1check();
struct dinode *getnextinode();

pass1()
{
	register int c, i, j;
	register struct dinode *dp;
	struct zlncnt *zlnp;
	int ndb, cgd;
	struct inodesc idesc;
	ino_t inumber;

	/*
	 * Set file system reserved blocks in used block map.
	 */
	for (c = 0; c < sblock.fs_ncg; c++) {
		cgd = cgdmin(&sblock, c);
		if (c == 0) {
			i = cgbase(&sblock, c);
			cgd += howmany(sblock.fs_cssize, sblock.fs_fsize);
		} else
			i = cgsblock(&sblock, c);
		for (; i < cgd; i++)
			setbmap(i);
	}
	/*
	 * Find all allocated blocks.
	 */
	bzero((char *)&idesc, sizeof(struct inodesc));
	idesc.id_type = ADDR;
	idesc.id_func = pass1check;
	inumber = 0;
	n_files = n_blks = 0;
	resetinodebuf();
	for (c = 0; c < sblock.fs_ncg; c++) {
		for (i = 0; i < sblock.fs_ipg; i++, inumber++) {
			if (inumber < ROOTINO)
				continue;
			dp = getnextinode(inumber);
			if ((dp->di_mode & IFMT) == 0) {
				if (bcmp((char *)dp->di_db, (char *)zino.di_db,
					NDADDR * sizeof(daddr_t)) ||
				    bcmp((char *)dp->di_ib, (char *)zino.di_ib,
					NIADDR * sizeof(daddr_t)) ||
				    dp->di_mode || dp->di_size) {
					pfatal("PARTIALLY ALLOCATED INODE I=%lu",
						inumber);
					if (reply("CLEAR") == 1) {
						dp = ginode(inumber);
						clearinode(dp);
						inodirty();
					}
				}
				statemap[inumber] = USTATE;
				continue;
			}
			lastino = inumber;
			/* is fast symlink? */
			if (DFASTLINK(*dp)) {
				lncntp[inumber] = dp->di_nlink;
				statemap[inumber] = FSTATE;
				n_files++;
				continue;
			}
			if (/* dp->di_size < 0 || */
			    dp->di_size + sblock.fs_bsize - 1 < dp->di_size) {
				if (debug)
					printf("bad size %lu:", dp->di_size);
				goto unknown;
			}
			if (!preen && (dp->di_mode & IFMT) == IFMT &&
			    reply("HOLD BAD BLOCK") == 1) {
				dp = ginode(inumber);
				dp->di_size = sblock.fs_fsize;
				dp->di_mode = IFREG|0600;
				inodirty();
			}
			ndb = howmany(dp->di_size, sblock.fs_bsize);
			if (ndb < 0) {
				if (debug)
					printf("bad size %lu ndb %d:",
						dp->di_size, ndb);
				goto unknown;
			}
			if ((dp->di_mode & IFMT) == IFBLK ||
			    (dp->di_mode & IFMT) == IFCHR)
				ndb++;
			for (j = ndb; j < NDADDR; j++)
				if (dp->di_db[j] != 0) {
					if (debug)
						printf("bad direct addr: %ld\n",
							dp->di_db[j]);
					goto unknown;
				}
			for (j = 0, ndb -= NDADDR; ndb > 0; j++)
				ndb /= NINDIR(&sblock);
			for (; j < NIADDR; j++)
				if (dp->di_ib[j] != 0) {
					if (debug)
						printf("bad indirect addr: %ld\n",
							dp->di_ib[j]);
					goto unknown;
				}
			if (ftypeok(dp) == 0)
				goto unknown;
			n_files++;
			lncntp[inumber] = dp->di_nlink;
			if (dp->di_nlink <= 0) {
				zlnp = (struct zlncnt *)malloc(sizeof *zlnp);
				if (zlnp == NULL) {
					pfatal("LINK COUNT TABLE OVERFLOW");
					if (reply("CONTINUE") == 0)
						errexit("");
				} else {
					zlnp->zlncnt = inumber;
					zlnp->next = zlnhead;
					zlnhead = zlnp;
				}
			}
			if ((dp->di_mode & IFMT) == IFDIR) {
				if (dp->di_size == 0)
					statemap[inumber] = DCLEAR;
				else
					statemap[inumber] = DSTATE;
				cacheino(dp, inumber);
			} else
				statemap[inumber] = FSTATE;
			badblk = dupblk = 0;
			idesc.id_number = inumber;
			(void)ckinode(dp, &idesc);
			idesc.id_entryno *= btodb(sblock.fs_fsize);
			if (dp->di_blocks != idesc.id_entryno) {
				pwarn("INCORRECT BLOCK COUNT I=%lu (%ld should be %ld)",
				    inumber, dp->di_blocks, idesc.id_entryno);
				if (preen)
					printf(" (CORRECTED)\n");
				else if (reply("CORRECT") == 0)
					continue;
				dp = ginode(inumber);
				dp->di_blocks = idesc.id_entryno;
				inodirty();
			}
			continue;
	unknown:
			pfatal("UNKNOWN FILE TYPE I=%lu", inumber);
			statemap[inumber] = FCLEAR;
			if (reply("CLEAR") == 1) {
				statemap[inumber] = USTATE;
				dp = ginode(inumber);
				clearinode(dp);
				inodirty();
			}
		}
	}
	freeinodebuf();
}

pass1check(idesc)
	register struct inodesc *idesc;
{
	int res = KEEPON;
	int anyout, nfrags;
	daddr_t blkno = idesc->id_blkno;
	register struct dups *dlp;
	struct dups *new;

	if ((anyout = chkrange(blkno, idesc->id_numfrags)) != 0) {
		blkerror(idesc->id_number, "BAD", blkno);
		if (badblk++ >= MAXBAD) {
			pwarn("EXCESSIVE BAD BLKS I=%lu",
				idesc->id_number);
			if (preen)
				printf(" (SKIPPING)\n");
			else if (reply("CONTINUE") == 0)
				errexit("");
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
					idesc->id_number);
				if (preen)
					printf(" (SKIPPING)\n");
				else if (reply("CONTINUE") == 0)
					errexit("");
				return (STOP);
			}
			new = (struct dups *)malloc(sizeof(struct dups));
			if (new == NULL) {
				pfatal("DUP TABLE OVERFLOW.");
				if (reply("CONTINUE") == 0)
					errexit("");
				return (STOP);
			}
			new->dup = blkno;
			if (muldup == 0) {
				duplist = muldup = new;
				new->next = 0;
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
	return (res);
}
