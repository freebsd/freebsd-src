/*
 * Copyright (c) 1982, 1986, 1989 Regents of the University of California.
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
 *
 *	from: @(#)ufs_bmap.c	7.13 (Berkeley) 5/8/91
 *	$Id: bmap.c,v 1.2 1993/10/16 18:49:22 rgrimes Exp $
 */

#include "param.h"
#include "dinode.h"
#include "fs.h"
#include "errno.h"

/*
 * Bmap converts a the logical block number of a file
 * to its physical block number on the disk. The conversion
 * is done by using the logical block number to index into
 * the array of block pointers described by the dinode.
 */
extern struct fs *fs;
extern int bdev;

static daddr_t bap[2*1024];
static daddr_t bnobap;

bmap(dip, bn, bnp)
	register struct dinode *dip;
	register daddr_t bn;
	daddr_t	*bnp;
{
	register daddr_t nb;
	int i, j, sh;
	int error;

/*fprintf(stderr, "bmap %d ", bn);*/
	if (bn < 0)
		return (EFBIG);

	/*
	 * The first NDADDR blocks are direct blocks
	 */
	if (bn < NDADDR) {
		nb = dip->di_db[bn];
		if (nb == 0) {
			*bnp = (daddr_t)-1;
/*fprintf(stderr, "%d\n", *bnp);*/
			return (0);
		}
		*bnp = fsbtodb(fs, nb);
/*fprintf(stderr, "%d\n", *bnp);*/
		return (0);
	}
	/*
	 * Determine the number of levels of indirection.
	 */
	sh = 1;
	bn -= NDADDR;
	for (j = NIADDR; j > 0; j--) {
		sh *= NINDIR(fs);
		if (bn < sh)
			break;
		bn -= sh;
	}
	if (j == 0)
		return (EFBIG);
	/*
	 * Fetch through the indirect blocks.
	 */
	nb = dip->di_ib[NIADDR - j];
	if (nb == 0) {
		*bnp = (daddr_t)-1;
/*fprintf(stderr, "%d\n", *bnp);*/
		return (0);
	}
	for (; j <= NIADDR; j++) {
		daddr_t bno = fsbtodb(fs, nb);

		if (bnobap != bno &&
(error = bread(bdev, bno, &bap,
			(int)fs->fs_bsize))) {
			return (error);
		}
		bnobap = bno;
		sh /= NINDIR(fs);
		i = (bn / sh) % NINDIR(fs);
		nb = bap[i];
		if (nb == 0) {
			*bnp = (daddr_t)-1;
/*fprintf(stderr, "%d\n", *bnp);*/
			return (0);
		}
	}
	*bnp = fsbtodb(fs, nb);
/*fprintf(stderr, "%d\n", *bnp);*/
	return (0);
}
