/*-
 * Copyright (c) 1993
 *	The Regents of the University of California.  All rights reserved.
 * Modifications/enhancements:
 * 	Copyright (c) 1995 John S. Dyson.  All rights reserved.
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
 *	@(#)vfs_cluster.c	8.7 (Berkeley) 2/13/94
 * $FreeBSD$
 */

#include "opt_debug_cluster.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/buf.h>
#include <sys/vnode.h>
#include <sys/malloc.h>
#include <sys/mount.h>
#include <sys/resourcevar.h>
#include <sys/vmmeter.h>
#include <vm/vm.h>
#include <vm/vm_object.h>
#include <vm/vm_page.h>
#include <sys/sysctl.h>

#if defined(CLUSTERDEBUG)
#include <sys/sysctl.h>
static int	rcluster= 0;
SYSCTL_INT(_debug, OID_AUTO, rcluster, CTLFLAG_RW, &rcluster, 0, "");
#endif

static MALLOC_DEFINE(M_SEGMENT, "cluster_save buffer", "cluster_save buffer");

static struct cluster_save *
	cluster_collectbufs __P((struct vnode *vp, struct buf *last_bp));
static struct buf *
	cluster_rbuild __P((struct vnode *vp, u_quad_t filesize, daddr_t lbn,
			    daddr_t blkno, long size, int run, struct buf *fbp));

static int write_behind = 1;
SYSCTL_INT(_vfs, OID_AUTO, write_behind, CTLFLAG_RW, &write_behind, 0, "");

extern vm_page_t	bogus_page;

extern int cluster_pbuf_freecnt;

/*
 * Maximum number of blocks for read-ahead.
 */
#define MAXRA 32

/*
 * This replaces bread.
 */
int
cluster_read(vp, filesize, lblkno, size, cred, totread, seqcount, bpp)
	struct vnode *vp;
	u_quad_t filesize;
	daddr_t lblkno;
	long size;
	struct ucred *cred;
	long totread;
	int seqcount;
	struct buf **bpp;
{
	struct buf *bp, *rbp, *reqbp;
	daddr_t blkno, origblkno;
	int error, num_ra;
	int i;
	int maxra, racluster;
	long origtotread;

	error = 0;

	/*
	 * Try to limit the amount of read-ahead by a few
	 * ad-hoc parameters.  This needs work!!!
	 */
	racluster = vp->v_mount->mnt_iosize_max / size;
	maxra = 2 * racluster + (totread / size);
	if (maxra > MAXRA)
		maxra = MAXRA;
	if (maxra > nbuf/8)
		maxra = nbuf/8;

	/*
	 * get the requested block
	 */
	*bpp = reqbp = bp = getblk(vp, lblkno, size, 0, 0);
	origblkno = lblkno;
	origtotread = totread;

	/*
	 * if it is in the cache, then check to see if the reads have been
	 * sequential.  If they have, then try some read-ahead, otherwise
	 * back-off on prospective read-aheads.
	 */
	if (bp->b_flags & B_CACHE) {
		if (!seqcount) {
			return 0;
		} else if ((bp->b_flags & B_RAM) == 0) {
			return 0;
		} else {
			int s;
			struct buf *tbp;
			bp->b_flags &= ~B_RAM;
			/*
			 * We do the spl here so that there is no window
			 * between the incore and the b_usecount increment
			 * below.  We opt to keep the spl out of the loop
			 * for efficiency.
			 */
			s = splbio();
			for (i = 1; i < maxra; i++) {

				if (!(tbp = incore(vp, lblkno+i))) {
					break;
				}

				/*
				 * Set another read-ahead mark so we know 
				 * to check again.
				 */
				if (((i % racluster) == (racluster - 1)) ||
					(i == (maxra - 1)))
					tbp->b_flags |= B_RAM;
			}
			splx(s);
			if (i >= maxra) {
				return 0;
			}
			lblkno += i;
		}
		reqbp = bp = NULL;
	} else {
		off_t firstread = bp->b_offset;

		KASSERT(bp->b_offset != NOOFFSET,
		    ("cluster_read: no buffer offset"));
		if (firstread + totread > filesize)
			totread = filesize - firstread;
		if (totread > size) {
			int nblks = 0;
			int ncontigafter;
			while (totread > 0) {
				nblks++;
				totread -= size;
			}
			if (nblks == 1)
				goto single_block_read;
			if (nblks > racluster)
				nblks = racluster;

	    		error = VOP_BMAP(vp, lblkno, NULL,
				&blkno, &ncontigafter, NULL);
			if (error)
				goto single_block_read;
			if (blkno == -1)
				goto single_block_read;
			if (ncontigafter == 0)
				goto single_block_read;
			if (ncontigafter + 1 < nblks)
				nblks = ncontigafter + 1;

			bp = cluster_rbuild(vp, filesize, lblkno,
				blkno, size, nblks, bp);
			lblkno += (bp->b_bufsize / size);
		} else {
single_block_read:
			/*
			 * if it isn't in the cache, then get a chunk from
			 * disk if sequential, otherwise just get the block.
			 */
			bp->b_flags |= B_READ | B_RAM;
			lblkno += 1;
		}
	}

	/*
	 * if we have been doing sequential I/O, then do some read-ahead
	 */
	rbp = NULL;
	if (seqcount && (lblkno < (origblkno + seqcount))) {
		/*
		 * we now build the read-ahead buffer if it is desirable.
		 */
		if (((u_quad_t)(lblkno + 1) * size) <= filesize &&
		    !(error = VOP_BMAP(vp, lblkno, NULL, &blkno, &num_ra, NULL)) &&
		    blkno != -1) {
			int nblksread;
			int ntoread = num_ra + 1;
			nblksread = (origtotread + size - 1) / size;
			if (seqcount < nblksread)
				seqcount = nblksread;
			if (seqcount < ntoread)
				ntoread = seqcount;
			if (num_ra) {
				rbp = cluster_rbuild(vp, filesize, lblkno,
					blkno, size, ntoread, NULL);
			} else {
				rbp = getblk(vp, lblkno, size, 0, 0);
				rbp->b_flags |= B_READ | B_ASYNC | B_RAM;
				rbp->b_blkno = blkno;
			}
		}
	}

	/*
	 * handle the synchronous read
	 */
	if (bp) {
#if defined(CLUSTERDEBUG)
		if (rcluster)
			printf("S(%ld,%ld,%d) ",
			    (long)bp->b_lblkno, bp->b_bcount, seqcount);
#endif
		if ((bp->b_flags & B_CLUSTER) == 0) {
			vfs_busy_pages(bp, 0);
		}
		bp->b_flags &= ~(B_ERROR|B_INVAL);
		if (bp->b_flags & (B_ASYNC|B_CALL))
			BUF_KERNPROC(bp);
		error = VOP_STRATEGY(vp, bp);
		curproc->p_stats->p_ru.ru_inblock++;
	}

	/*
	 * and if we have read-aheads, do them too
	 */
	if (rbp) {
		if (error) {
			rbp->b_flags &= ~(B_ASYNC | B_READ);
			brelse(rbp);
		} else if (rbp->b_flags & B_CACHE) {
			rbp->b_flags &= ~(B_ASYNC | B_READ);
			bqrelse(rbp);
		} else {
#if defined(CLUSTERDEBUG)
			if (rcluster) {
				if (bp)
					printf("A+(%ld,%ld,%ld,%d) ",
					    (long)rbp->b_lblkno, rbp->b_bcount,
					    (long)(rbp->b_lblkno - origblkno),
					    seqcount);
				else
					printf("A(%ld,%ld,%ld,%d) ",
					    (long)rbp->b_lblkno, rbp->b_bcount,
					    (long)(rbp->b_lblkno - origblkno),
					    seqcount);
			}
#endif

			if ((rbp->b_flags & B_CLUSTER) == 0) {
				vfs_busy_pages(rbp, 0);
			}
			rbp->b_flags &= ~(B_ERROR|B_INVAL);
			if (rbp->b_flags & (B_ASYNC|B_CALL))
				BUF_KERNPROC(rbp);
			(void) VOP_STRATEGY(vp, rbp);
			curproc->p_stats->p_ru.ru_inblock++;
		}
	}
	if (reqbp)
		return (biowait(reqbp));
	else
		return (error);
}

/*
 * If blocks are contiguous on disk, use this to provide clustered
 * read ahead.  We will read as many blocks as possible sequentially
 * and then parcel them up into logical blocks in the buffer hash table.
 */
static struct buf *
cluster_rbuild(vp, filesize, lbn, blkno, size, run, fbp)
	struct vnode *vp;
	u_quad_t filesize;
	daddr_t lbn;
	daddr_t blkno;
	long size;
	int run;
	struct buf *fbp;
{
	struct buf *bp, *tbp;
	daddr_t bn;
	int i, inc, j;

	KASSERT(size == vp->v_mount->mnt_stat.f_iosize,
	    ("cluster_rbuild: size %ld != filesize %ld\n",
	    size, vp->v_mount->mnt_stat.f_iosize));

	/*
	 * avoid a division
	 */
	while ((u_quad_t) size * (lbn + run) > filesize) {
		--run;
	}

	if (fbp) {
		tbp = fbp;
		tbp->b_flags |= B_READ; 
	} else {
		tbp = getblk(vp, lbn, size, 0, 0);
		if (tbp->b_flags & B_CACHE)
			return tbp;
		tbp->b_flags |= B_ASYNC | B_READ | B_RAM;
	}

	tbp->b_blkno = blkno;
	if( (tbp->b_flags & B_MALLOC) ||
		((tbp->b_flags & B_VMIO) == 0) || (run <= 1) )
		return tbp;

	bp = trypbuf(&cluster_pbuf_freecnt);
	if (bp == 0)
		return tbp;

	/*
	 * We are synthesizing a buffer out of vm_page_t's, but
	 * if the block size is not page aligned then the starting
	 * address may not be either.  Inherit the b_data offset
	 * from the original buffer.
	 */
	bp->b_data = (char *)((vm_offset_t)bp->b_data |
	    ((vm_offset_t)tbp->b_data & PAGE_MASK));
	bp->b_flags = B_ASYNC | B_READ | B_CALL | B_CLUSTER | B_VMIO;
	bp->b_iodone = cluster_callback;
	bp->b_blkno = blkno;
	bp->b_lblkno = lbn;
	bp->b_offset = tbp->b_offset;
	KASSERT(bp->b_offset != NOOFFSET, ("cluster_rbuild: no buffer offset"));
	pbgetvp(vp, bp);

	TAILQ_INIT(&bp->b_cluster.cluster_head);

	bp->b_bcount = 0;
	bp->b_bufsize = 0;
	bp->b_npages = 0;

	inc = btodb(size);
	for (bn = blkno, i = 0; i < run; ++i, bn += inc) {
		if (i != 0) {
			if ((bp->b_npages * PAGE_SIZE) +
			    round_page(size) > vp->v_mount->mnt_iosize_max) {
				break;
			}

			/*
			 * Shortcut some checks and try to avoid buffers that
			 * would block in the lock.  The same checks have to
			 * be made again after we officially get the buffer.
			 */
			if ((tbp = incore(vp, lbn + i)) != NULL) {
				if (BUF_LOCK(tbp, LK_EXCLUSIVE | LK_NOWAIT))
					break;
				BUF_UNLOCK(tbp);

				for (j = 0; j < tbp->b_npages; j++) {
					if (tbp->b_pages[j]->valid)
						break;
				}
				
				if (j != tbp->b_npages)
					break;
	
				if (tbp->b_bcount != size)
					break;
			}

			tbp = getblk(vp, lbn + i, size, 0, 0);

			/*
			 * Stop scanning if the buffer is fuly valid 
			 * (marked B_CACHE), or locked (may be doing a
			 * background write), or if the buffer is not
			 * VMIO backed.  The clustering code can only deal
			 * with VMIO-backed buffers.
			 */
			if ((tbp->b_flags & (B_CACHE|B_LOCKED)) ||
			    (tbp->b_flags & B_VMIO) == 0) {
				bqrelse(tbp);
				break;
			}

			/*
			 * The buffer must be completely invalid in order to
			 * take part in the cluster.  If it is partially valid
			 * then we stop.
			 */
			for (j = 0;j < tbp->b_npages; j++) {
				if (tbp->b_pages[j]->valid)
					break;
			}
			if (j != tbp->b_npages) {
				bqrelse(tbp);
				break;
			}

			/*
			 * Set a read-ahead mark as appropriate
			 */
			if ((fbp && (i == 1)) || (i == (run - 1)))
				tbp->b_flags |= B_RAM;

			/*
			 * Set the buffer up for an async read (XXX should
			 * we do this only if we do not wind up brelse()ing?).
			 * Set the block number if it isn't set, otherwise
			 * if it is make sure it matches the block number we
			 * expect.
			 */
			tbp->b_flags |= B_READ | B_ASYNC;
			if (tbp->b_blkno == tbp->b_lblkno) {
				tbp->b_blkno = bn;
			} else if (tbp->b_blkno != bn) {
				brelse(tbp);
				break;
			}
		}
		/*
		 * XXX fbp from caller may not be B_ASYNC, but we are going
		 * to biodone() it in cluster_callback() anyway
		 */
		BUF_KERNPROC(tbp);
		TAILQ_INSERT_TAIL(&bp->b_cluster.cluster_head,
			tbp, b_cluster.cluster_entry);
		for (j = 0; j < tbp->b_npages; j += 1) {
			vm_page_t m;
			m = tbp->b_pages[j];
			vm_page_io_start(m);
			vm_object_pip_add(m->object, 1);
			if ((bp->b_npages == 0) ||
				(bp->b_pages[bp->b_npages-1] != m)) {
				bp->b_pages[bp->b_npages] = m;
				bp->b_npages++;
			}
			if ((m->valid & VM_PAGE_BITS_ALL) == VM_PAGE_BITS_ALL)
				tbp->b_pages[j] = bogus_page;
		}
		bp->b_bcount += tbp->b_bcount;
		bp->b_bufsize += tbp->b_bufsize;
	}

	/*
	 * Fully valid pages in the cluster are already good and do not need
	 * to be re-read from disk.  Replace the page with bogus_page
	 */
	for (j = 0; j < bp->b_npages; j++) {
		if ((bp->b_pages[j]->valid & VM_PAGE_BITS_ALL) ==
		    VM_PAGE_BITS_ALL) {
			bp->b_pages[j] = bogus_page;
		}
	}
	if (bp->b_bufsize > bp->b_kvasize)
		panic("cluster_rbuild: b_bufsize(%ld) > b_kvasize(%d)\n",
		    bp->b_bufsize, bp->b_kvasize);
	bp->b_kvasize = bp->b_bufsize;

	pmap_qenter(trunc_page((vm_offset_t) bp->b_data),
		(vm_page_t *)bp->b_pages, bp->b_npages);
	return (bp);
}

/*
 * Cleanup after a clustered read or write.
 * This is complicated by the fact that any of the buffers might have
 * extra memory (if there were no empty buffer headers at allocbuf time)
 * that we will need to shift around.
 */
void
cluster_callback(bp)
	struct buf *bp;
{
	struct buf *nbp, *tbp;
	int error = 0;

	/*
	 * Must propogate errors to all the components.
	 */
	if (bp->b_flags & B_ERROR)
		error = bp->b_error;

	pmap_qremove(trunc_page((vm_offset_t) bp->b_data), bp->b_npages);
	/*
	 * Move memory from the large cluster buffer into the component
	 * buffers and mark IO as done on these.
	 */
	for (tbp = TAILQ_FIRST(&bp->b_cluster.cluster_head);
		tbp; tbp = nbp) {
		nbp = TAILQ_NEXT(&tbp->b_cluster, cluster_entry);
		if (error) {
			tbp->b_flags |= B_ERROR;
			tbp->b_error = error;
		} else {
			tbp->b_dirtyoff = tbp->b_dirtyend = 0;
			tbp->b_flags &= ~(B_ERROR|B_INVAL);
			/*
			 * XXX the bdwrite()/bqrelse() issued during
			 * cluster building clears B_RELBUF (see bqrelse()
			 * comment).  If direct I/O was specified, we have
			 * to restore it here to allow the buffer and VM
			 * to be freed.
			 */
			if (tbp->b_flags & B_DIRECT)
				tbp->b_flags |= B_RELBUF;
		}
		biodone(tbp);
	}
	relpbuf(bp, &cluster_pbuf_freecnt);
}

/*
 *	cluster_wbuild_wb:
 *
 *	Implement modified write build for cluster.
 *
 *		write_behind = 0	write behind disabled
 *		write_behind = 1	write behind normal (default)
 *		write_behind = 2	write behind backed-off
 */

static __inline int
cluster_wbuild_wb(struct vnode *vp, long size, daddr_t start_lbn, int len)
{
	int r = 0;

	switch(write_behind) {
	case 2:
		if (start_lbn < len)
			break;
		start_lbn -= len;
		/* fall through */
	case 1:
		r = cluster_wbuild(vp, size, start_lbn, len);
		/* fall through */
	default:
		/* fall through */
		break;
	}
	return(r);
}

/*
 * Do clustered write for FFS.
 *
 * Three cases:
 *	1. Write is not sequential (write asynchronously)
 *	Write is sequential:
 *	2.	beginning of cluster - begin cluster
 *	3.	middle of a cluster - add to cluster
 *	4.	end of a cluster - asynchronously write cluster
 */
void
cluster_write(bp, filesize, seqcount)
	struct buf *bp;
	u_quad_t filesize;
	int seqcount;
{
	struct vnode *vp;
	daddr_t lbn;
	int maxclen, cursize;
	int lblocksize;
	int async;

	vp = bp->b_vp;
	if (vp->v_type == VREG) {
		async = vp->v_mount->mnt_flag & MNT_ASYNC;
		lblocksize = vp->v_mount->mnt_stat.f_iosize;
	} else {
		async = 0;
		lblocksize = bp->b_bufsize;
	}
	lbn = bp->b_lblkno;
	KASSERT(bp->b_offset != NOOFFSET, ("cluster_write: no buffer offset"));

	/* Initialize vnode to beginning of file. */
	if (lbn == 0)
		vp->v_lasta = vp->v_clen = vp->v_cstart = vp->v_lastw = 0;

	if (vp->v_clen == 0 || lbn != vp->v_lastw + 1 ||
	    (bp->b_blkno != vp->v_lasta + btodb(lblocksize))) {
		maxclen = vp->v_mount->mnt_iosize_max / lblocksize - 1;
		if (vp->v_clen != 0) {
			/*
			 * Next block is not sequential.
			 *
			 * If we are not writing at end of file, the process
			 * seeked to another point in the file since its last
			 * write, or we have reached our maximum cluster size,
			 * then push the previous cluster. Otherwise try
			 * reallocating to make it sequential.
			 *
			 * Change to algorithm: only push previous cluster if
			 * it was sequential from the point of view of the
			 * seqcount heuristic, otherwise leave the buffer 
			 * intact so we can potentially optimize the I/O
			 * later on in the buf_daemon or update daemon
			 * flush.
			 */
			cursize = vp->v_lastw - vp->v_cstart + 1;
			if (((u_quad_t) bp->b_offset + lblocksize) != filesize ||
			    lbn != vp->v_lastw + 1 || vp->v_clen <= cursize) {
				if (!async && seqcount > 0) {
					cluster_wbuild_wb(vp, lblocksize,
						vp->v_cstart, cursize);
				}
			} else {
				struct buf **bpp, **endbp;
				struct cluster_save *buflist;

				buflist = cluster_collectbufs(vp, bp);
				endbp = &buflist->bs_children
				    [buflist->bs_nchildren - 1];
				if (VOP_REALLOCBLKS(vp, buflist)) {
					/*
					 * Failed, push the previous cluster
					 * if *really* writing sequentially
					 * in the logical file (seqcount > 1),
					 * otherwise delay it in the hopes that
					 * the low level disk driver can
					 * optimize the write ordering.
					 */
					for (bpp = buflist->bs_children;
					     bpp < endbp; bpp++)
						brelse(*bpp);
					free(buflist, M_SEGMENT);
					if (seqcount > 1) {
						cluster_wbuild_wb(vp, 
						    lblocksize, vp->v_cstart, 
						    cursize);
					}
				} else {
					/*
					 * Succeeded, keep building cluster.
					 */
					for (bpp = buflist->bs_children;
					     bpp <= endbp; bpp++)
						bdwrite(*bpp);
					free(buflist, M_SEGMENT);
					vp->v_lastw = lbn;
					vp->v_lasta = bp->b_blkno;
					return;
				}
			}
		}
		/*
		 * Consider beginning a cluster. If at end of file, make
		 * cluster as large as possible, otherwise find size of
		 * existing cluster.
		 */
		if ((vp->v_type == VREG) &&
			((u_quad_t) bp->b_offset + lblocksize) != filesize &&
		    (bp->b_blkno == bp->b_lblkno) &&
		    (VOP_BMAP(vp, lbn, NULL, &bp->b_blkno, &maxclen, NULL) ||
		     bp->b_blkno == -1)) {
			bawrite(bp);
			vp->v_clen = 0;
			vp->v_lasta = bp->b_blkno;
			vp->v_cstart = lbn + 1;
			vp->v_lastw = lbn;
			return;
		}
		vp->v_clen = maxclen;
		if (!async && maxclen == 0) {	/* I/O not contiguous */
			vp->v_cstart = lbn + 1;
			bawrite(bp);
		} else {	/* Wait for rest of cluster */
			vp->v_cstart = lbn;
			bdwrite(bp);
		}
	} else if (lbn == vp->v_cstart + vp->v_clen) {
		/*
		 * At end of cluster, write it out if seqcount tells us we
		 * are operating sequentially, otherwise let the buf or
		 * update daemon handle it.
		 */
		bdwrite(bp);
		if (seqcount > 1)
			cluster_wbuild_wb(vp, lblocksize, vp->v_cstart, vp->v_clen + 1);
		vp->v_clen = 0;
		vp->v_cstart = lbn + 1;
	} else if (vm_page_count_severe()) {
		/*
		 * We are low on memory, get it going NOW
		 */
		bawrite(bp);
	} else {
		/*
		 * In the middle of a cluster, so just delay the I/O for now.
		 */
		bdwrite(bp);
	}
	vp->v_lastw = lbn;
	vp->v_lasta = bp->b_blkno;
}


/*
 * This is an awful lot like cluster_rbuild...wish they could be combined.
 * The last lbn argument is the current block on which I/O is being
 * performed.  Check to see that it doesn't fall in the middle of
 * the current block (if last_bp == NULL).
 */
int
cluster_wbuild(vp, size, start_lbn, len)
	struct vnode *vp;
	long size;
	daddr_t start_lbn;
	int len;
{
	struct buf *bp, *tbp;
	int i, j, s;
	int totalwritten = 0;
	int dbsize = btodb(size);

	while (len > 0) {
		s = splbio();
		/*
		 * If the buffer is not delayed-write (i.e. dirty), or it 
		 * is delayed-write but either locked or inval, it cannot 
		 * partake in the clustered write.
		 */
		if (((tbp = gbincore(vp, start_lbn)) == NULL) ||
		  ((tbp->b_flags & (B_LOCKED | B_INVAL | B_DELWRI)) != B_DELWRI) ||
		  BUF_LOCK(tbp, LK_EXCLUSIVE | LK_NOWAIT)) {
			++start_lbn;
			--len;
			splx(s);
			continue;
		}
		bremfree(tbp);
		tbp->b_flags &= ~B_DONE;
		splx(s);

		/*
		 * Extra memory in the buffer, punt on this buffer.
		 * XXX we could handle this in most cases, but we would
		 * have to push the extra memory down to after our max
		 * possible cluster size and then potentially pull it back
		 * up if the cluster was terminated prematurely--too much
		 * hassle.
		 */
		if (((tbp->b_flags & (B_CLUSTEROK|B_MALLOC)) != B_CLUSTEROK) ||
		  (tbp->b_bcount != tbp->b_bufsize) ||
		  (tbp->b_bcount != size) ||
		  (len == 1) ||
		  ((bp = getpbuf(&cluster_pbuf_freecnt)) == NULL)) {
			totalwritten += tbp->b_bufsize;
			bawrite(tbp);
			++start_lbn;
			--len;
			continue;
		}

		/*
		 * We got a pbuf to make the cluster in.
		 * so initialise it.
		 */
		TAILQ_INIT(&bp->b_cluster.cluster_head);
		bp->b_bcount = 0;
		bp->b_bufsize = 0;
		bp->b_npages = 0;
		if (tbp->b_wcred != NOCRED) {
		    bp->b_wcred = tbp->b_wcred;
		    crhold(bp->b_wcred);
		}

		bp->b_blkno = tbp->b_blkno;
		bp->b_lblkno = tbp->b_lblkno;
		bp->b_offset = tbp->b_offset;

		/*
		 * We are synthesizing a buffer out of vm_page_t's, but
		 * if the block size is not page aligned then the starting
		 * address may not be either.  Inherit the b_data offset
		 * from the original buffer.
		 */
		bp->b_data = (char *)((vm_offset_t)bp->b_data |
		    ((vm_offset_t)tbp->b_data & PAGE_MASK));
		bp->b_flags |= B_CALL | B_CLUSTER |
				(tbp->b_flags & (B_VMIO | B_NEEDCOMMIT));
		bp->b_iodone = cluster_callback;
		pbgetvp(vp, bp);
		/*
		 * From this location in the file, scan forward to see
		 * if there are buffers with adjacent data that need to
		 * be written as well.
		 */
		for (i = 0; i < len; ++i, ++start_lbn) {
			if (i != 0) { /* If not the first buffer */
				s = splbio();
				/*
				 * If the adjacent data is not even in core it
				 * can't need to be written.
				 */
				if ((tbp = gbincore(vp, start_lbn)) == NULL) {
					splx(s);
					break;
				}

				/*
				 * If it IS in core, but has different
				 * characteristics, or is locked (which
				 * means it could be undergoing a background
				 * I/O or be in a weird state), then don't
				 * cluster with it.
				 */
				if ((tbp->b_flags & (B_VMIO | B_CLUSTEROK |
				    B_INVAL | B_DELWRI | B_NEEDCOMMIT))
				  != (B_DELWRI | B_CLUSTEROK |
				    (bp->b_flags & (B_VMIO | B_NEEDCOMMIT))) ||
				    (tbp->b_flags & B_LOCKED) ||
				    tbp->b_wcred != bp->b_wcred ||
				    BUF_LOCK(tbp, LK_EXCLUSIVE | LK_NOWAIT)) {
					splx(s);
					break;
				}

				/*
				 * Check that the combined cluster
				 * would make sense with regard to pages
				 * and would not be too large
				 */
				if ((tbp->b_bcount != size) ||
				  ((bp->b_blkno + (dbsize * i)) !=
				    tbp->b_blkno) ||
				  ((tbp->b_npages + bp->b_npages) >
				    (vp->v_mount->mnt_iosize_max / PAGE_SIZE))) {
					BUF_UNLOCK(tbp);
					splx(s);
					break;
				}
				/*
				 * Ok, it's passed all the tests,
				 * so remove it from the free list
				 * and mark it busy. We will use it.
				 */
				bremfree(tbp);
				tbp->b_flags &= ~B_DONE;
				splx(s);
			} /* end of code for non-first buffers only */
			/* check for latent dependencies to be handled */
			if ((LIST_FIRST(&tbp->b_dep)) != NULL &&
			    bioops.io_start)
				(*bioops.io_start)(tbp);
			/*
			 * If the IO is via the VM then we do some
			 * special VM hackery (yuck).  Since the buffer's
			 * block size may not be page-aligned it is possible
			 * for a page to be shared between two buffers.  We
			 * have to get rid of the duplication when building
			 * the cluster.
			 */
			if (tbp->b_flags & B_VMIO) {
				vm_page_t m;

				if (i != 0) { /* if not first buffer */
					for (j = 0; j < tbp->b_npages; j += 1) {
						m = tbp->b_pages[j];
						if (m->flags & PG_BUSY) {
							bqrelse(tbp);
							goto finishcluster;
						}
					}
				}
					
				for (j = 0; j < tbp->b_npages; j += 1) {
					m = tbp->b_pages[j];
					vm_page_io_start(m);
					vm_object_pip_add(m->object, 1);
					if ((bp->b_npages == 0) ||
					  (bp->b_pages[bp->b_npages - 1] != m)) {
						bp->b_pages[bp->b_npages] = m;
						bp->b_npages++;
					}
				}
			}
			bp->b_bcount += size;
			bp->b_bufsize += size;

			s = splbio();
			bundirty(tbp);
			tbp->b_flags &= ~(B_READ | B_DONE | B_ERROR);
			tbp->b_flags |= B_ASYNC;
			reassignbuf(tbp, tbp->b_vp);	/* put on clean list */
			++tbp->b_vp->v_numoutput;
			splx(s);
			BUF_KERNPROC(tbp);
			TAILQ_INSERT_TAIL(&bp->b_cluster.cluster_head,
				tbp, b_cluster.cluster_entry);
		}
	finishcluster:
		pmap_qenter(trunc_page((vm_offset_t) bp->b_data),
			(vm_page_t *) bp->b_pages, bp->b_npages);
		if (bp->b_bufsize > bp->b_kvasize)
			panic(
			    "cluster_wbuild: b_bufsize(%ld) > b_kvasize(%d)\n",
			    bp->b_bufsize, bp->b_kvasize);
		bp->b_kvasize = bp->b_bufsize;
		totalwritten += bp->b_bufsize;
		bp->b_dirtyoff = 0;
		bp->b_dirtyend = bp->b_bufsize;
		bawrite(bp);

		len -= i;
	}
	return totalwritten;
}

/*
 * Collect together all the buffers in a cluster.
 * Plus add one additional buffer.
 */
static struct cluster_save *
cluster_collectbufs(vp, last_bp)
	struct vnode *vp;
	struct buf *last_bp;
{
	struct cluster_save *buflist;
	struct buf *bp;
	daddr_t lbn;
	int i, len;

	len = vp->v_lastw - vp->v_cstart + 1;
	buflist = malloc(sizeof(struct buf *) * (len + 1) + sizeof(*buflist),
	    M_SEGMENT, M_WAITOK);
	buflist->bs_nchildren = 0;
	buflist->bs_children = (struct buf **) (buflist + 1);
	for (lbn = vp->v_cstart, i = 0; i < len; lbn++, i++) {
		(void) bread(vp, lbn, last_bp->b_bcount, NOCRED, &bp);
		buflist->bs_children[i] = bp;
		if (bp->b_blkno == bp->b_lblkno)
			VOP_BMAP(bp->b_vp, bp->b_lblkno, NULL, &bp->b_blkno,
				NULL, NULL);
	}
	buflist->bs_children[i] = bp = last_bp;
	if (bp->b_blkno == bp->b_lblkno)
		VOP_BMAP(bp->b_vp, bp->b_lblkno, NULL, &bp->b_blkno,
			NULL, NULL);
	buflist->bs_nchildren = i + 1;
	return (buflist);
}
