/*
 *  modified for Lites 1.1
 *
 *  Aug 1995, Godmar Back (gback@cs.utah.edu)
 *  University of Utah, Department of Computer Science
 */
/*
 * Copyright (c) 1982, 1986, 1989, 1993
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
 *
 *	@(#)ext2_subr.c	8.2 (Berkeley) 9/21/93
 */

#include <sys/param.h>
#include <gnu/ext2fs/ext2_fs.h>
#include <gnu/ext2fs/ext2_fs_sb.h>
#include <gnu/ext2fs/fs.h>

#include <sys/systm.h>
#include <sys/vnode.h>
#include <gnu/ext2fs/ext2_extern.h>
#include <sys/buf.h>
#include <ufs/ufs/quota.h>
#include <ufs/ufs/inode.h>

/*
 * Return buffer with the contents of block "offset" from the beginning of
 * directory "ip".  If "res" is non-zero, fill it in with a pointer to the
 * remaining space in the directory.
 */
int
ext2_blkatoff(ap)
	struct vop_blkatoff_args /* {
		struct vnode *a_vp;
		off_t a_offset;
		char **a_res;
		struct buf **a_bpp;
	} */ *ap;
{
	struct inode *ip;
	register struct ext2_sb_info *fs;
	struct buf *bp;
	daddr_t lbn;
	int bsize, error;

	ip = VTOI(ap->a_vp);
	fs = ip->i_e2fs;
	lbn = lblkno(fs, ap->a_offset);
	bsize = blksize(fs, ip, lbn);

	*ap->a_bpp = NULL;
	if (error = bread(ap->a_vp, lbn, bsize, NOCRED, &bp)) {
		brelse(bp);
		return (error);
	}
	if (ap->a_res)
		*ap->a_res = (char *)bp->b_data + blkoff(fs, ap->a_offset);
	*ap->a_bpp = bp;
	return (0);
}

#if defined(KERNEL) && defined(DIAGNOSTIC)
void
ext2_checkoverlap(bp, ip)
	struct buf *bp;
	struct inode *ip;
{
	register struct buf *ebp, *ep;
	register daddr_t start, last;
	struct vnode *vp;

	ebp = &buf[nbuf];
	start = bp->b_blkno;
	last = start + btodb(bp->b_bcount) - 1;
	for (ep = buf; ep < ebp; ep++) {
		if (ep == bp || (ep->b_flags & B_INVAL) ||
		    ep->b_vp == NULLVP)
			continue;
#if !defined(__FreeBSD__)
		if (VOP_BMAP(ep->b_vp, (daddr_t)0, &vp, (daddr_t)0, NULL))
			continue;
#else
		if (VOP_BMAP(ep->b_vp, (daddr_t)0, &vp, (daddr_t)0, NULL, NULL))
			continue;
#endif
		if (vp != ip->i_devvp)
			continue;
		/* look for overlap */
		if (ep->b_bcount == 0 || ep->b_blkno > last ||
		    ep->b_blkno + btodb(ep->b_bcount) <= start)
			continue;
		vprint("Disk overlap", vp);
		(void)printf("\tstart %d, end %d overlap start %d, end %d\n",
			start, last, ep->b_blkno,
			ep->b_blkno + btodb(ep->b_bcount) - 1);
		panic("Disk buffer overlap");
	}
}
#endif /* DIAGNOSTIC */

