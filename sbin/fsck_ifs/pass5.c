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
static const char sccsid[] = "@(#)pass5.c	8.9 (Berkeley) 4/28/95";
#endif
static const char rcsid[] =
  "$FreeBSD$";
#endif /* not lint */

#include <sys/param.h>

#include <ufs/ufs/dinode.h>
#include <ufs/ffs/fs.h>

#include <err.h>
#include <string.h>

#include "fsck.h"

void
pass5(void)
{
	int c, blk, frags, basesize, sumsize, mapsize, savednrpos = 0;
	int inomapsize, blkmapsize, astart, aend, ustart, uend;
	struct fs *fs = &sblock;
	struct cg *cg = &cgrp;
	ufs_daddr_t dbase, dmax;
	ufs_daddr_t d;
	long i, j, k, l, m, n;
	struct csum *cs;
	struct csum cstotal;
	struct inodesc idesc[3];
	char buf[MAXBSIZE];
	struct cg *newcg = (struct cg *)buf;
	struct ocg *ocg = (struct ocg *)buf;

	inoinfo(WINO)->ino_state = USTATE;
	memset(newcg, 0, (size_t)fs->fs_cgsize);
	/*
	 * Note: cg_niblk is 16 bits and may overflow, so it must never
	 * be used except for comparing with the old value.
	 */
	newcg->cg_niblk = fs->fs_ipg;
	if (cvtlevel >= 3) {
		if (fs->fs_maxcontig < 2 && fs->fs_contigsumsize > 0) {
			if (preen)
				pwarn("DELETING CLUSTERING MAPS\n");
			if (preen || reply("DELETE CLUSTERING MAPS")) {
				fs->fs_contigsumsize = 0;
				doinglevel1 = 1;
				sbdirty();
			}
		}
		if (fs->fs_maxcontig > 1) {
			char *doit = 0;

			if (fs->fs_contigsumsize < 1) {
				doit = "CREAT";
			} else if (fs->fs_contigsumsize < fs->fs_maxcontig &&
				   fs->fs_contigsumsize < FS_MAXCONTIG) {
				doit = "EXPAND";
			}
			if (doit) {
				i = fs->fs_contigsumsize;
				fs->fs_contigsumsize =
				    MIN(fs->fs_maxcontig, FS_MAXCONTIG);
				if (CGSIZE(fs) > fs->fs_bsize) {
					pwarn("CANNOT %s CLUSTER MAPS\n", doit);
					fs->fs_contigsumsize = i;
				} else if (preen ||
				    reply("CREATE CLUSTER MAPS")) {
					if (preen)
						pwarn("%sING CLUSTER MAPS\n",
						    doit);
					fs->fs_cgsize =
					    fragroundup(fs, CGSIZE(fs));
					doinglevel1 = 1;
					sbdirty();
				}
			}
		}
	}
	switch ((int)fs->fs_postblformat) {

	case FS_42POSTBLFMT:
		basesize = (char *)(&ocg->cg_btot[0]) -
		    (char *)(&ocg->cg_firstfield);
		sumsize = &ocg->cg_iused[0] - (u_int8_t *)(&ocg->cg_btot[0]);
		mapsize = &ocg->cg_free[howmany(fs->fs_fpg, NBBY)] -
			(u_char *)&ocg->cg_iused[0];
		blkmapsize = howmany(fs->fs_fpg, NBBY);
		inomapsize = &ocg->cg_free[0] - (u_char *)&ocg->cg_iused[0];
		ocg->cg_magic = CG_MAGIC;
		savednrpos = fs->fs_nrpos;
		fs->fs_nrpos = 8;
		break;

	case FS_DYNAMICPOSTBLFMT:
		newcg->cg_btotoff =
		     &newcg->cg_space[0] - (u_char *)(&newcg->cg_firstfield);
		newcg->cg_boff =
		    newcg->cg_btotoff + fs->fs_cpg * sizeof(int32_t);
		newcg->cg_iusedoff = newcg->cg_boff +
		    fs->fs_cpg * fs->fs_nrpos * sizeof(u_int16_t);
		newcg->cg_freeoff =
		    newcg->cg_iusedoff + howmany(fs->fs_ipg, NBBY);
		inomapsize = newcg->cg_freeoff - newcg->cg_iusedoff;
		newcg->cg_nextfreeoff = newcg->cg_freeoff +
		    howmany(fs->fs_cpg * fs->fs_spc / NSPF(fs), NBBY);
		blkmapsize = newcg->cg_nextfreeoff - newcg->cg_freeoff;
		if (fs->fs_contigsumsize > 0) {
			newcg->cg_clustersumoff = newcg->cg_nextfreeoff -
			    sizeof(u_int32_t);
			newcg->cg_clustersumoff =
			    roundup(newcg->cg_clustersumoff, sizeof(u_int32_t));
			newcg->cg_clusteroff = newcg->cg_clustersumoff +
			    (fs->fs_contigsumsize + 1) * sizeof(u_int32_t);
			newcg->cg_nextfreeoff = newcg->cg_clusteroff +
			    howmany(fs->fs_cpg * fs->fs_spc / NSPB(fs), NBBY);
		}
		newcg->cg_magic = CG_MAGIC;
		basesize = &newcg->cg_space[0] -
		    (u_char *)(&newcg->cg_firstfield);
		sumsize = newcg->cg_iusedoff - newcg->cg_btotoff;
		mapsize = newcg->cg_nextfreeoff - newcg->cg_iusedoff;
		break;

	default:
		inomapsize = blkmapsize = sumsize = 0;	/* keep lint happy */
		errx(EEXIT, "UNKNOWN ROTATIONAL TABLE FORMAT %d",
			fs->fs_postblformat);
	}
	memset(&idesc[0], 0, sizeof idesc);
	for (i = 0; i < 3; i++) {
		idesc[i].id_type = ADDR;
		if (doinglevel2)
			idesc[i].id_fix = FIX;
	}
	memset(&cstotal, 0, sizeof(struct csum));
	j = blknum(fs, fs->fs_size + fs->fs_frag - 1);
	for (i = fs->fs_size; i < j; i++)
		setbmap(i);
	for (c = 0; c < fs->fs_ncg; c++) {
		getblk(&cgblk, cgtod(fs, c), fs->fs_cgsize);
		if (!cg_chkmagic(cg))
			pfatal("CG %d: BAD MAGIC NUMBER\n", c);
		dbase = cgbase(fs, c);
		dmax = dbase + fs->fs_fpg;
		if (dmax > fs->fs_size)
			dmax = fs->fs_size;
		newcg->cg_time = cg->cg_time;
		newcg->cg_cgx = c;
		if (c == fs->fs_ncg - 1)
			newcg->cg_ncyl = fs->fs_ncyl % fs->fs_cpg;
		else
			newcg->cg_ncyl = fs->fs_cpg;
		newcg->cg_ndblk = dmax - dbase;
		if (fs->fs_contigsumsize > 0)
			newcg->cg_nclusterblks = newcg->cg_ndblk / fs->fs_frag;
		newcg->cg_cs.cs_ndir = 0;
		newcg->cg_cs.cs_nffree = 0;
		newcg->cg_cs.cs_nbfree = 0;
		newcg->cg_cs.cs_nifree = fs->fs_ipg;
		if (cg->cg_rotor < newcg->cg_ndblk)
			newcg->cg_rotor = cg->cg_rotor;
		else
			newcg->cg_rotor = 0;
		if (cg->cg_frotor < newcg->cg_ndblk)
			newcg->cg_frotor = cg->cg_frotor;
		else
			newcg->cg_frotor = 0;
		if (cg->cg_irotor < fs->fs_ipg)
			newcg->cg_irotor = cg->cg_irotor;
		else
			newcg->cg_irotor = 0;
		memset(&newcg->cg_frsum[0], 0, sizeof newcg->cg_frsum);
		memset(&cg_blktot(newcg)[0], 0,
		      (size_t)(sumsize + mapsize));
		if (fs->fs_postblformat == FS_42POSTBLFMT)
			ocg->cg_magic = CG_MAGIC;
		j = fs->fs_ipg * c;
		for (i = 0; i < inostathead[c].il_numalloced; j++, i++) {
			switch (inoinfo(j)->ino_state) {

			case USTATE:
				break;

			case DSTATE:
			case DCLEAR:
			case DFOUND:
				newcg->cg_cs.cs_ndir++;
				/* fall through */

			case FSTATE:
			case FCLEAR:
				newcg->cg_cs.cs_nifree--;
				setbit(cg_inosused(newcg), i);
				break;

			default:
				if (j < ROOTINO)
					break;
				errx(EEXIT, "BAD STATE %d FOR INODE I=%ld",
				    inoinfo(j)->ino_state, j);
			}
		}
		if (c == 0)
			for (i = 0; i < ROOTINO; i++) {
				setbit(cg_inosused(newcg), i);
				newcg->cg_cs.cs_nifree--;
			}
		for (i = 0, d = dbase;
		     d < dmax;
		     d += fs->fs_frag, i += fs->fs_frag) {
			frags = 0;
			for (j = 0; j < fs->fs_frag; j++) {
				if (testbmap(d + j))
					continue;
				setbit(cg_blksfree(newcg), i + j);
				frags++;
			}
			if (frags == fs->fs_frag) {
				newcg->cg_cs.cs_nbfree++;
				j = cbtocylno(fs, i);
				cg_blktot(newcg)[j]++;
				cg_blks(fs, newcg, j)[cbtorpos(fs, i)]++;
				if (fs->fs_contigsumsize > 0)
					setbit(cg_clustersfree(newcg),
					    i / fs->fs_frag);
			} else if (frags > 0) {
				newcg->cg_cs.cs_nffree += frags;
				blk = blkmap(fs, cg_blksfree(newcg), i);
				ffs_fragacct(fs, blk, newcg->cg_frsum, 1);
			}
		}
		if (fs->fs_contigsumsize > 0) {
			int32_t *sump = cg_clustersum(newcg);
			u_char *mapp = cg_clustersfree(newcg);
			int map = *mapp++;
			int bit = 1;
			int run = 0;

			for (i = 0; i < newcg->cg_nclusterblks; i++) {
				if ((map & bit) != 0) {
					run++;
				} else if (run != 0) {
					if (run > fs->fs_contigsumsize)
						run = fs->fs_contigsumsize;
					sump[run]++;
					run = 0;
				}
				if ((i & (NBBY - 1)) != (NBBY - 1)) {
					bit <<= 1;
				} else {
					map = *mapp++;
					bit = 1;
				}
			}
			if (run != 0) {
				if (run > fs->fs_contigsumsize)
					run = fs->fs_contigsumsize;
				sump[run]++;
			}
		}
		cstotal.cs_nffree += newcg->cg_cs.cs_nffree;
		cstotal.cs_nbfree += newcg->cg_cs.cs_nbfree;
		cstotal.cs_nifree += newcg->cg_cs.cs_nifree;
		cstotal.cs_ndir += newcg->cg_cs.cs_ndir;
		cs = &fs->fs_cs(fs, c);
		if (memcmp(&newcg->cg_cs, cs, sizeof *cs) != 0 &&
		    dofix(&idesc[0], "FREE BLK COUNT(S) WRONG IN SUPERBLK")) {
			memmove(cs, &newcg->cg_cs, sizeof *cs);
			sbdirty();
		}
		if (doinglevel1) {
			memmove(cg, newcg, (size_t)fs->fs_cgsize);
			cgdirty();
			continue;
		}
		if ((memcmp(newcg, cg, basesize) != 0 ||
		     memcmp(&cg_blktot(newcg)[0],
			  &cg_blktot(cg)[0], sumsize) != 0) &&
		    dofix(&idesc[2], "SUMMARY INFORMATION BAD")) {
			memmove(cg, newcg, (size_t)basesize);
			memmove(&cg_blktot(cg)[0],
			       &cg_blktot(newcg)[0], (size_t)sumsize);
			cgdirty();
		}
		if (debug) {
			for (i = 0; i < inomapsize; i++) {
				j = cg_inosused(newcg)[i];
				k = cg_inosused(cg)[i];
				if (j == k)
					continue;
				for (m = 0, l = 1; m < NBBY; m++, l <<= 1) {
					if ((j & l) == (k & l))
						continue;
					n = c * fs->fs_ipg + i * NBBY + m;
					if ((j & l) != 0)
						pwarn("%s INODE %d MARKED %s\n",
						    "ALLOCATED", n, "FREE");
					else
						pwarn("%s INODE %d MARKED %s\n",
						    "UNALLOCATED", n, "USED");
				}
			}
			astart = ustart = -1;
			for (i = 0; i < blkmapsize; i++) {
				j = cg_blksfree(cg)[i];
				k = cg_blksfree(newcg)[i];
				if (j == k)
					continue;
				for (m = 0, l = 1; m < NBBY; m++, l <<= 1) {
					if ((j & l) == (k & l))
						continue;
					n = c * fs->fs_fpg + i * NBBY + m;
					if ((j & l) != 0) {
						if (astart == -1) {
							astart = aend = n;
							continue;
						}
						if (aend + 1 == n) {
							aend = n;
							continue;
						}
						pwarn("%s FRAGS %d-%d %s\n",
						    "ALLOCATED", astart, aend,
						    "MARKED FREE");
						astart = aend = n;
					} else {
						if (ustart == -1) {
							ustart = uend = n;
							continue;
						}
						if (uend + 1 == n) {
							uend = n;
							continue;
						}
						pwarn("%s FRAGS %d-%d %s\n",
						    "UNALLOCATED", ustart, uend,
						    "MARKED USED");
						ustart = uend = n;
					}
				}
			}
			if (astart != -1)
				pwarn("%s FRAGS %d-%d %s\n",
				    "ALLOCATED", astart, aend,
				    "MARKED FREE");
			if (ustart != -1)
				pwarn("%s FRAGS %d-%d %s\n",
				    "UNALLOCATED", ustart, uend,
				    "MARKED USED");
		}
		if (usedsoftdep) {
			for (i = 0; i < inomapsize; i++) {
				j = cg_inosused(newcg)[i];
				if ((cg_inosused(cg)[i] & j) == j)
					continue;
				for (k = 0; k < NBBY; k++) {
					if ((j & (1 << k)) == 0)
						continue;
					if (cg_inosused(cg)[i] & (1 << k))
						continue;
					pwarn("ALLOCATED INODE %d MARKED FREE\n",
					    c * fs->fs_ipg + i * NBBY + k);
				}
			}
			for (i = 0; i < blkmapsize; i++) {
				j = cg_blksfree(cg)[i];
				if ((cg_blksfree(newcg)[i] & j) == j)
					continue;
				for (k = 0; k < NBBY; k++) {
					if ((j & (1 << k)) == 0)
						continue;
					if (cg_blksfree(newcg)[i] & (1 << k))
						continue;
					pwarn("ALLOCATED FRAG %d MARKED FREE\n",
					    c * fs->fs_fpg + i * NBBY + k);
				}
			}
		}
		if (memcmp(cg_inosused(newcg), cg_inosused(cg), mapsize) != 0 &&
		    dofix(&idesc[1], "BLK(S) MISSING IN BIT MAPS")) {
			memmove(cg_inosused(cg), cg_inosused(newcg),
			      (size_t)mapsize);
			cgdirty();
		}
	}
	if (fs->fs_postblformat == FS_42POSTBLFMT)
		fs->fs_nrpos = savednrpos;
	if (memcmp(&cstotal, &fs->fs_cstotal, sizeof *cs) != 0
	    && dofix(&idesc[0], "FREE BLK COUNT(S) WRONG IN SUPERBLK")) {
		memmove(&fs->fs_cstotal, &cstotal, sizeof *cs);
		fs->fs_ronly = 0;
		fs->fs_fmod = 0;
		sbdirty();
	}
}
