/*-
 * Copyright (c) 1993
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
 *	@(#)vfs_cluster.c	8.10 (Berkeley) 3/28/95
 */

#include <sys/param.h>
#include <sys/proc.h>
#include <sys/buf.h>
#include <sys/vnode.h>
#include <sys/mount.h>
#include <sys/trace.h>
#include <sys/malloc.h>
#include <sys/resourcevar.h>
#include <libkern/libkern.h>

/*
 * Local declarations
 */
struct buf *cluster_newbuf __P((struct vnode *, struct buf *, long, daddr_t,
	    daddr_t, long, int));
struct buf *cluster_rbuild __P((struct vnode *, u_quad_t, struct buf *,
	    daddr_t, daddr_t, long, int, long));
void	    cluster_wbuild __P((struct vnode *, struct buf *, long,
	    daddr_t, int, daddr_t));
struct cluster_save *cluster_collectbufs __P((struct vnode *, struct buf *));

#ifdef DIAGNOSTIC
/*
 * Set to 1 if reads of block zero should cause readahead to be done.
 * Set to 0 treats a read of block zero as a non-sequential read.
 *
 * Setting to one assumes that most reads of block zero of files are due to
 * sequential passes over the files (e.g. cat, sum) where additional blocks
 * will soon be needed.  Setting to zero assumes that the majority are
 * surgical strikes to get particular info (e.g. size, file) where readahead
 * blocks will not be used and, in fact, push out other potentially useful
 * blocks from the cache.  The former seems intuitive, but some quick tests
 * showed that the latter performed better from a system-wide point of view.
 */
int	doclusterraz = 0;
#define ISSEQREAD(vp, blk) \
	(((blk) != 0 || doclusterraz) && \
	 ((blk) == (vp)->v_lastr + 1 || (blk) == (vp)->v_lastr))
#else
#define ISSEQREAD(vp, blk) \
	((blk) != 0 && ((blk) == (vp)->v_lastr + 1 || (blk) == (vp)->v_lastr))
#endif

/*
 * This replaces bread.  If this is a bread at the beginning of a file and
 * lastr is 0, we assume this is the first read and we'll read up to two
 * blocks if they are sequential.  After that, we'll do regular read ahead
 * in clustered chunks.
 *
 * There are 4 or 5 cases depending on how you count:
 *	Desired block is in the cache:
 *	    1 Not sequential access (0 I/Os).
 *	    2 Access is sequential, do read-ahead (1 ASYNC).
 *	Desired block is not in cache:
 *	    3 Not sequential access (1 SYNC).
 *	    4 Sequential access, next block is contiguous (1 SYNC).
 *	    5 Sequential access, next block is not contiguous (1 SYNC, 1 ASYNC)
 *
 * There are potentially two buffers that require I/O.
 * 	bp is the block requested.
 *	rbp is the read-ahead block.
 *	If either is NULL, then you don't have to do the I/O.
 */
cluster_read(vp, filesize, lblkno, size, cred, bpp)
	struct vnode *vp;
	u_quad_t filesize;
	daddr_t lblkno;
	long size;
	struct ucred *cred;
	struct buf **bpp;
{
	struct buf *bp, *rbp;
	daddr_t blkno, ioblkno;
	long flags;
	int error, num_ra, alreadyincore;

#ifdef DIAGNOSTIC
	if (size == 0)
		panic("cluster_read: size = 0");
#endif

	error = 0;
	flags = B_READ;
	*bpp = bp = getblk(vp, lblkno, size, 0, 0);
	if (bp->b_flags & B_CACHE) {
		/*
		 * Desired block is in cache; do any readahead ASYNC.
		 * Case 1, 2.
		 */
		trace(TR_BREADHIT, pack(vp, size), lblkno);
		flags |= B_ASYNC;
		ioblkno = lblkno + (vp->v_ralen ? vp->v_ralen : 1);
		alreadyincore = incore(vp, ioblkno) != NULL;
		bp = NULL;
	} else {
		/* Block wasn't in cache, case 3, 4, 5. */
		trace(TR_BREADMISS, pack(vp, size), lblkno);
		bp->b_flags |= B_READ;
		ioblkno = lblkno;
		alreadyincore = 0;
		curproc->p_stats->p_ru.ru_inblock++;		/* XXX */
	}
	/*
	 * XXX
	 * Replace 1 with a window size based on some permutation of
	 * maxcontig and rot_delay.  This will let you figure out how
	 * many blocks you should read-ahead (case 2, 4, 5).
	 *
	 * If the access isn't sequential, reset the window to 1.
	 * Note that a read to the same block is considered sequential.
	 * This catches the case where the file is being read sequentially,
	 * but at smaller than the filesystem block size.
	 */
	rbp = NULL;
	if (!ISSEQREAD(vp, lblkno)) {
		vp->v_ralen = 0;
		vp->v_maxra = lblkno;
	} else if ((ioblkno + 1) * size <= filesize && !alreadyincore &&
	    !(error = VOP_BMAP(vp, ioblkno, NULL, &blkno, &num_ra)) &&
	    blkno != -1) {
		/*
		 * Reading sequentially, and the next block is not in the
		 * cache.  We are going to try reading ahead.
		 */
		if (num_ra) {
			/*
			 * If our desired readahead block had been read
			 * in a previous readahead but is no longer in
			 * core, then we may be reading ahead too far
			 * or are not using our readahead very rapidly.
			 * In this case we scale back the window.
			 */
			if (!alreadyincore && ioblkno <= vp->v_maxra)
				vp->v_ralen = max(vp->v_ralen >> 1, 1);
			/*
			 * There are more sequential blocks than our current
			 * window allows, scale up.  Ideally we want to get
			 * in sync with the filesystem maxcontig value.
			 */
			else if (num_ra > vp->v_ralen && lblkno != vp->v_lastr)
				vp->v_ralen = vp->v_ralen ?
					min(num_ra, vp->v_ralen << 1) : 1;

			if (num_ra > vp->v_ralen)
				num_ra = vp->v_ralen;
		}

		if (num_ra)				/* case 2, 4 */
			rbp = cluster_rbuild(vp, filesize,
			    bp, ioblkno, blkno, size, num_ra, flags);
		else if (ioblkno == lblkno) {
			bp->b_blkno = blkno;
			/* Case 5: check how many blocks to read ahead */
			++ioblkno;
			if ((ioblkno + 1) * size > filesize ||
			    incore(vp, ioblkno) || (error = VOP_BMAP(vp,
			     ioblkno, NULL, &blkno, &num_ra)) || blkno == -1)
				goto skip_readahead;
			/*
			 * Adjust readahead as above.
			 * Don't check alreadyincore, we know it is 0 from
			 * the previous conditional.
			 */
			if (num_ra) {
				if (ioblkno <= vp->v_maxra)
					vp->v_ralen = max(vp->v_ralen >> 1, 1);
				else if (num_ra > vp->v_ralen &&
					 lblkno != vp->v_lastr)
					vp->v_ralen = vp->v_ralen ?
						min(num_ra,vp->v_ralen<<1) : 1;
				if (num_ra > vp->v_ralen)
					num_ra = vp->v_ralen;
			}
			flags |= B_ASYNC;
			if (num_ra)
				rbp = cluster_rbuild(vp, filesize,
				    NULL, ioblkno, blkno, size, num_ra, flags);
			else {
				rbp = getblk(vp, ioblkno, size, 0, 0);
				rbp->b_flags |= flags;
				rbp->b_blkno = blkno;
			}
		} else {
			/* case 2; read ahead single block */
			rbp = getblk(vp, ioblkno, size, 0, 0);
			rbp->b_flags |= flags;
			rbp->b_blkno = blkno;
		}

		if (rbp == bp)			/* case 4 */
			rbp = NULL;
		else if (rbp) {			/* case 2, 5 */
			trace(TR_BREADMISSRA,
			    pack(vp, (num_ra + 1) * size), ioblkno);
			curproc->p_stats->p_ru.ru_inblock++;	/* XXX */
		}
	}

	/* XXX Kirk, do we need to make sure the bp has creds? */
skip_readahead:
	if (bp)
		if (bp->b_flags & (B_DONE | B_DELWRI))
			panic("cluster_read: DONE bp");
		else 
			error = VOP_STRATEGY(bp);

	if (rbp)
		if (error || rbp->b_flags & (B_DONE | B_DELWRI)) {
			rbp->b_flags &= ~(B_ASYNC | B_READ);
			brelse(rbp);
		} else
			(void) VOP_STRATEGY(rbp);

	/*
	 * Recalculate our maximum readahead
	 */
	if (rbp == NULL)
		rbp = bp;
	if (rbp)
		vp->v_maxra = rbp->b_lblkno + (rbp->b_bufsize / size) - 1;

	if (bp)
		return(biowait(bp));
	return(error);
}

/*
 * If blocks are contiguous on disk, use this to provide clustered
 * read ahead.  We will read as many blocks as possible sequentially
 * and then parcel them up into logical blocks in the buffer hash table.
 */
struct buf *
cluster_rbuild(vp, filesize, bp, lbn, blkno, size, run, flags)
	struct vnode *vp;
	u_quad_t filesize;
	struct buf *bp;
	daddr_t lbn;
	daddr_t blkno;
	long size;
	int run;
	long flags;
{
	struct cluster_save *b_save;
	struct buf *tbp;
	daddr_t bn;
	int i, inc;

#ifdef DIAGNOSTIC
	if (size != vp->v_mount->mnt_stat.f_iosize)
		panic("cluster_rbuild: size %d != filesize %d\n",
			size, vp->v_mount->mnt_stat.f_iosize);
#endif
	if (size * (lbn + run + 1) > filesize)
		--run;
	if (run == 0) {
		if (!bp) {
			bp = getblk(vp, lbn, size, 0, 0);
			bp->b_blkno = blkno;
			bp->b_flags |= flags;
		}
		return(bp);
	}

	bp = cluster_newbuf(vp, bp, flags, blkno, lbn, size, run + 1);
	if (bp->b_flags & (B_DONE | B_DELWRI))
		return (bp);

	b_save = malloc(sizeof(struct buf *) * run + sizeof(struct cluster_save),
	    M_SEGMENT, M_WAITOK);
	b_save->bs_bufsize = b_save->bs_bcount = size;
	b_save->bs_nchildren = 0;
	b_save->bs_children = (struct buf **)(b_save + 1);
	b_save->bs_saveaddr = bp->b_saveaddr;
	bp->b_saveaddr = (caddr_t) b_save;

	inc = btodb(size);
	for (bn = blkno + inc, i = 1; i <= run; ++i, bn += inc) {
		/*
		 * A component of the cluster is already in core,
		 * terminate the cluster early.
		 */
		if (incore(vp, lbn + i))
			break;
		tbp = getblk(vp, lbn + i, 0, 0, 0);
		/*
		 * getblk may return some memory in the buffer if there were
		 * no empty buffers to shed it to.  If there is currently
		 * memory in the buffer, we move it down size bytes to make
		 * room for the valid pages that cluster_callback will insert.
		 * We do this now so we don't have to do it at interrupt time
		 * in the callback routine.
		 */
		if (tbp->b_bufsize != 0) {
			caddr_t bdata = (char *)tbp->b_data;

			/*
			 * No room in the buffer to add another page,
			 * terminate the cluster early.
			 */
			if (tbp->b_bufsize + size > MAXBSIZE) {
#ifdef DIAGNOSTIC
				if (tbp->b_bufsize != MAXBSIZE)
					panic("cluster_rbuild: too much memory");
#endif
				brelse(tbp);
				break;
			}
			if (tbp->b_bufsize > size) {
				/*
				 * XXX if the source and destination regions
				 * overlap we have to copy backward to avoid
				 * clobbering any valid pages (i.e. pagemove
				 * implementations typically can't handle
				 * overlap).
				 */
				bdata += tbp->b_bufsize;
				while (bdata > (char *)tbp->b_data) {
					bdata -= CLBYTES;
					pagemove(bdata, bdata + size, CLBYTES);
				}
			} else 
				pagemove(bdata, bdata + size, tbp->b_bufsize);
		}
		tbp->b_blkno = bn;
		tbp->b_flags |= flags | B_READ | B_ASYNC;
		++b_save->bs_nchildren;
		b_save->bs_children[i - 1] = tbp;
	}
	/*
	 * The cluster may have been terminated early, adjust the cluster
	 * buffer size accordingly.  If no cluster could be formed,
	 * deallocate the cluster save info.
	 */
	if (i <= run) {
		if (i == 1) {
			bp->b_saveaddr = b_save->bs_saveaddr;
			bp->b_flags &= ~B_CALL;
			bp->b_iodone = NULL;
			free(b_save, M_SEGMENT);
		}
		allocbuf(bp, size * i);
	}
	return(bp);
}

/*
 * Either get a new buffer or grow the existing one.
 */
struct buf *
cluster_newbuf(vp, bp, flags, blkno, lblkno, size, run)
	struct vnode *vp;
	struct buf *bp;
	long flags;
	daddr_t blkno;
	daddr_t lblkno;
	long size;
	int run;
{
	if (!bp) {
		bp = getblk(vp, lblkno, size, 0, 0);
		if (bp->b_flags & (B_DONE | B_DELWRI)) {
			bp->b_blkno = blkno;
			return(bp);
		}
	}
	allocbuf(bp, run * size);
	bp->b_blkno = blkno;
	bp->b_iodone = cluster_callback;
	bp->b_flags |= flags | B_CALL;
	return(bp);
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
	struct cluster_save *b_save;
	struct buf **bpp, *tbp;
	long bsize;
	caddr_t cp;
	int error = 0;

	/*
	 * Must propogate errors to all the components.
	 */
	if (bp->b_flags & B_ERROR)
		error = bp->b_error;

	b_save = (struct cluster_save *)(bp->b_saveaddr);
	bp->b_saveaddr = b_save->bs_saveaddr;

	bsize = b_save->bs_bufsize;
	cp = (char *)bp->b_data + bsize;
	/*
	 * Move memory from the large cluster buffer into the component
	 * buffers and mark IO as done on these.
	 */
	for (bpp = b_save->bs_children; b_save->bs_nchildren--; ++bpp) {
		tbp = *bpp;
		pagemove(cp, tbp->b_data, bsize);
		tbp->b_bufsize += bsize;
		tbp->b_bcount = bsize;
		if (error) {
			tbp->b_flags |= B_ERROR;
			tbp->b_error = error;
		}
		biodone(tbp);
		bp->b_bufsize -= bsize;
		cp += bsize;
	}
	/*
	 * If there was excess memory in the cluster buffer,
	 * slide it up adjacent to the remaining valid data.
	 */
	if (bp->b_bufsize != bsize) {
		if (bp->b_bufsize < bsize)
			panic("cluster_callback: too little memory");
		pagemove(cp, (char *)bp->b_data + bsize, bp->b_bufsize - bsize);
	}
	bp->b_bcount = bsize;
	bp->b_iodone = NULL;
	free(b_save, M_SEGMENT);
	if (bp->b_flags & B_ASYNC)
		brelse(bp);
	else {
		bp->b_flags &= ~B_WANTED;
		wakeup((caddr_t)bp);
	}
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
cluster_write(bp, filesize)
        struct buf *bp;
	u_quad_t filesize;
{
        struct vnode *vp;
        daddr_t lbn;
        int maxclen, cursize;

        vp = bp->b_vp;
        lbn = bp->b_lblkno;

	/* Initialize vnode to beginning of file. */
	if (lbn == 0)
		vp->v_lasta = vp->v_clen = vp->v_cstart = vp->v_lastw = 0;

        if (vp->v_clen == 0 || lbn != vp->v_lastw + 1 ||
	    (bp->b_blkno != vp->v_lasta + btodb(bp->b_bcount))) {
		maxclen = MAXBSIZE / vp->v_mount->mnt_stat.f_iosize - 1;
		if (vp->v_clen != 0) {
			/*
			 * Next block is not sequential.
			 *
			 * If we are not writing at end of file, the process
			 * seeked to another point in the file since its
			 * last write, or we have reached our maximum
			 * cluster size, then push the previous cluster.
			 * Otherwise try reallocating to make it sequential.
			 */
			cursize = vp->v_lastw - vp->v_cstart + 1;
			if ((lbn + 1) * bp->b_bcount != filesize ||
			    lbn != vp->v_lastw + 1 || vp->v_clen <= cursize) {
				cluster_wbuild(vp, NULL, bp->b_bcount,
				    vp->v_cstart, cursize, lbn);
			} else {
				struct buf **bpp, **endbp;
				struct cluster_save *buflist;

				buflist = cluster_collectbufs(vp, bp);
				endbp = &buflist->bs_children
				    [buflist->bs_nchildren - 1];
				if (VOP_REALLOCBLKS(vp, buflist)) {
					/*
					 * Failed, push the previous cluster.
					 */
					for (bpp = buflist->bs_children;
					     bpp < endbp; bpp++)
						brelse(*bpp);
					free(buflist, M_SEGMENT);
					cluster_wbuild(vp, NULL, bp->b_bcount,
					    vp->v_cstart, cursize, lbn);
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
		 * Consider beginning a cluster.
		 * If at end of file, make cluster as large as possible,
		 * otherwise find size of existing cluster.
		 */
		if ((lbn + 1) * bp->b_bcount != filesize &&
		    (VOP_BMAP(vp, lbn, NULL, &bp->b_blkno, &maxclen) ||
		     bp->b_blkno == -1)) {
			bawrite(bp);
			vp->v_clen = 0;
			vp->v_lasta = bp->b_blkno;
			vp->v_cstart = lbn + 1;
			vp->v_lastw = lbn;
			return;
		}
                vp->v_clen = maxclen;
                if (maxclen == 0) {		/* I/O not contiguous */
			vp->v_cstart = lbn + 1;
                        bawrite(bp);
                } else {			/* Wait for rest of cluster */
			vp->v_cstart = lbn;
                        bdwrite(bp);
		}
	} else if (lbn == vp->v_cstart + vp->v_clen) {
		/*
		 * At end of cluster, write it out.
		 */
		cluster_wbuild(vp, bp, bp->b_bcount, vp->v_cstart,
		    vp->v_clen + 1, lbn);
		vp->v_clen = 0;
		vp->v_cstart = lbn + 1;
	} else
		/*
		 * In the middle of a cluster, so just delay the
		 * I/O for now.
		 */
		bdwrite(bp);
	vp->v_lastw = lbn;
	vp->v_lasta = bp->b_blkno;
}


/*
 * This is an awful lot like cluster_rbuild...wish they could be combined.
 * The last lbn argument is the current block on which I/O is being
 * performed.  Check to see that it doesn't fall in the middle of
 * the current block (if last_bp == NULL).
 */
void
cluster_wbuild(vp, last_bp, size, start_lbn, len, lbn)
	struct vnode *vp;
	struct buf *last_bp;
	long size;
	daddr_t start_lbn;
	int len;
	daddr_t	lbn;
{
	struct cluster_save *b_save;
	struct buf *bp, *tbp;
	caddr_t	cp;
	int i, s;

#ifdef DIAGNOSTIC
	if (size != vp->v_mount->mnt_stat.f_iosize)
		panic("cluster_wbuild: size %d != filesize %d\n",
			size, vp->v_mount->mnt_stat.f_iosize);
#endif
redo:
	while ((!incore(vp, start_lbn) || start_lbn == lbn) && len) {
		++start_lbn;
		--len;
	}

	/* Get more memory for current buffer */
	if (len <= 1) {
		if (last_bp) {
			bawrite(last_bp);
		} else if (len) {
			bp = getblk(vp, start_lbn, size, 0, 0);
			bawrite(bp);
		}
		return;
	}

	bp = getblk(vp, start_lbn, size, 0, 0);
	if (!(bp->b_flags & B_DELWRI)) {
		++start_lbn;
		--len;
		brelse(bp);
		goto redo;
	}

	/*
	 * Extra memory in the buffer, punt on this buffer.
	 * XXX we could handle this in most cases, but we would have to
	 * push the extra memory down to after our max possible cluster
	 * size and then potentially pull it back up if the cluster was
	 * terminated prematurely--too much hassle.
	 */
	if (bp->b_bcount != bp->b_bufsize) {
		++start_lbn;
		--len;
		bawrite(bp);
		goto redo;
	}

	--len;
	b_save = malloc(sizeof(struct buf *) * len + sizeof(struct cluster_save),
	    M_SEGMENT, M_WAITOK);
	b_save->bs_bcount = bp->b_bcount;
	b_save->bs_bufsize = bp->b_bufsize;
	b_save->bs_nchildren = 0;
	b_save->bs_children = (struct buf **)(b_save + 1);
	b_save->bs_saveaddr = bp->b_saveaddr;
	bp->b_saveaddr = (caddr_t) b_save;

	bp->b_flags |= B_CALL;
	bp->b_iodone = cluster_callback;
	cp = (char *)bp->b_data + size;
	for (++start_lbn, i = 0; i < len; ++i, ++start_lbn) {
		/*
		 * Block is not in core or the non-sequential block
		 * ending our cluster was part of the cluster (in which
		 * case we don't want to write it twice).
		 */
		if (!incore(vp, start_lbn) ||
		    last_bp == NULL && start_lbn == lbn)
			break;

		/*
		 * Get the desired block buffer (unless it is the final
		 * sequential block whose buffer was passed in explictly
		 * as last_bp).
		 */
		if (last_bp == NULL || start_lbn != lbn) {
			tbp = getblk(vp, start_lbn, size, 0, 0);
			if (!(tbp->b_flags & B_DELWRI)) {
				brelse(tbp);
				break;
			}
		} else
			tbp = last_bp;

		++b_save->bs_nchildren;

		/* Move memory from children to parent */
		if (tbp->b_blkno != (bp->b_blkno + btodb(bp->b_bufsize))) {
			printf("Clustered Block: %d addr %x bufsize: %d\n",
			    bp->b_lblkno, bp->b_blkno, bp->b_bufsize);
			printf("Child Block: %d addr: %x\n", tbp->b_lblkno,
			    tbp->b_blkno);
			panic("Clustered write to wrong blocks");
		}

		pagemove(tbp->b_data, cp, size);
		bp->b_bcount += size;
		bp->b_bufsize += size;

		tbp->b_bufsize -= size;
		tbp->b_flags &= ~(B_READ | B_DONE | B_ERROR | B_DELWRI);
		tbp->b_flags |= (B_ASYNC | B_AGE);
		s = splbio();
		reassignbuf(tbp, tbp->b_vp);		/* put on clean list */
		++tbp->b_vp->v_numoutput;
		splx(s);
		b_save->bs_children[i] = tbp;

		cp += size;
	}

	if (i == 0) {
		/* None to cluster */
		bp->b_saveaddr = b_save->bs_saveaddr;
		bp->b_flags &= ~B_CALL;
		bp->b_iodone = NULL;
		free(b_save, M_SEGMENT);
	}
	bawrite(bp);
	if (i < len) {
		len -= i + 1;
		start_lbn += 1;
		goto redo;
	}
}

/*
 * Collect together all the buffers in a cluster.
 * Plus add one additional buffer.
 */
struct cluster_save *
cluster_collectbufs(vp, last_bp)
	struct vnode *vp;
	struct buf *last_bp;
{
	struct cluster_save *buflist;
	daddr_t	lbn;
	int i, len;

	len = vp->v_lastw - vp->v_cstart + 1;
	buflist = malloc(sizeof(struct buf *) * (len + 1) + sizeof(*buflist),
	    M_SEGMENT, M_WAITOK);
	buflist->bs_nchildren = 0;
	buflist->bs_children = (struct buf **)(buflist + 1);
	for (lbn = vp->v_cstart, i = 0; i < len; lbn++, i++)
		    (void)bread(vp, lbn, last_bp->b_bcount, NOCRED,
			&buflist->bs_children[i]);
	buflist->bs_children[i] = last_bp;
	buflist->bs_nchildren = i + 1;
	return (buflist);
}
