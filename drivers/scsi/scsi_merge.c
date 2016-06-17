/*
 *  scsi_merge.c Copyright (C) 1999 Eric Youngdale
 *
 *  SCSI queueing library.
 *      Initial versions: Eric Youngdale (eric@andante.org).
 *                        Based upon conversations with large numbers
 *                        of people at Linux Expo.
 *	Support for dynamic DMA mapping: Jakub Jelinek (jakub@redhat.com).
 *	Support for highmem I/O: Jens Axboe <axboe@suse.de>
 */

/*
 * This file contains queue management functions that are used by SCSI.
 * Typically this is used for several purposes.   First, we need to ensure
 * that commands do not grow so large that they cannot be handled all at
 * once by a host adapter.   The various flavors of merge functions included
 * here serve this purpose.
 *
 * Note that it would be quite trivial to allow the low-level driver the
 * flexibility to define it's own queue handling functions.  For the time
 * being, the hooks are not present.   Right now we are just using the
 * data in the host template as an indicator of how we should be handling
 * queues, and we select routines that are optimized for that purpose.
 *
 * Some hosts do not impose any restrictions on the size of a request.
 * In such cases none of the merge functions in this file are called,
 * and we allow ll_rw_blk to merge requests in the default manner.
 * This isn't guaranteed to be optimal, but it should be pretty darned
 * good.   If someone comes up with ideas of better ways of managing queues
 * to improve on the default behavior, then certainly fit it into this
 * scheme in whatever manner makes the most sense.   Please note that
 * since each device has it's own queue, we have considerable flexibility
 * in queue management.
 */

#define __NO_VERSION__
#include <linux/config.h>
#include <linux/module.h>

#include <linux/sched.h>
#include <linux/timer.h>
#include <linux/string.h>
#include <linux/slab.h>
#include <linux/ioport.h>
#include <linux/kernel.h>
#include <linux/stat.h>
#include <linux/blk.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/smp_lock.h>

#define __KERNEL_SYSCALLS__

#include <linux/unistd.h>

#include <asm/system.h>
#include <asm/irq.h>
#include <asm/dma.h>
#include <asm/io.h>

#include "scsi.h"
#include "hosts.h"
#include "constants.h"
#include <scsi/scsi_ioctl.h>

/*
 * scsi_malloc() can only dish out items of PAGE_SIZE or less, so we cannot
 * build a request that requires an sg table allocation of more than that.
 */
static const int scsi_max_sg = PAGE_SIZE / sizeof(struct scatterlist);

/*
 * This means that bounce buffers cannot be allocated in chunks > PAGE_SIZE.
 * Ultimately we should get away from using a dedicated DMA bounce buffer
 * pool, and we should instead try and use kmalloc() instead.  If we can
 * eliminate this pool, then this restriction would no longer be needed.
 */
#define DMA_SEGMENT_SIZE_LIMITED

#ifdef CONFIG_SCSI_DEBUG_QUEUES
/*
 * Enable a bunch of additional consistency checking.   Turn this off
 * if you are benchmarking.
 */
static int dump_stats(struct request *req,
		      int use_clustering,
		      int dma_host,
		      int segments)
{
	struct buffer_head *bh;

	/*
	 * Dump the information that we have.  We know we have an
	 * inconsistency.
	 */
	printk("nr_segments is %x\n", req->nr_segments);
	printk("counted segments is %x\n", segments);
	printk("Flags %d %d\n", use_clustering, dma_host);
	for (bh = req->bh; bh->b_reqnext != NULL; bh = bh->b_reqnext) 
	{
		printk("Segment 0x%p, blocks %d, addr 0x%lx\n",
		       bh,
		       bh->b_size >> 9,
		       bh_phys(bh) - 1);
	}
	panic("Ththththaats all folks.  Too dangerous to continue.\n");
}


/*
 * Simple sanity check that we will use for the first go around
 * in order to ensure that we are doing the counting correctly.
 * This can be removed for optimization.
 */
#define SANITY_CHECK(req, _CLUSTER, _DMA)				\
    if( req->nr_segments != __count_segments(req, _CLUSTER, _DMA, NULL) )	\
    {									\
	printk("Incorrect segment count at 0x%p", current_text_addr());	\
	dump_stats(req, _CLUSTER, _DMA, __count_segments(req, _CLUSTER, _DMA, NULL)); \
    }
#else
#define SANITY_CHECK(req, _CLUSTER, _DMA)
#endif

static void dma_exhausted(Scsi_Cmnd * SCpnt, int i)
{
	int jj;
	struct scatterlist *sgpnt;
	void **bbpnt;
	int consumed = 0;

	sgpnt = (struct scatterlist *) SCpnt->request_buffer;
	bbpnt = SCpnt->bounce_buffers;

	/*
	 * Now print out a bunch of stats.  First, start with the request
	 * size.
	 */
	printk("dma_free_sectors:%d\n", scsi_dma_free_sectors);
	printk("use_sg:%d\ti:%d\n", SCpnt->use_sg, i);
	printk("request_bufflen:%d\n", SCpnt->request_bufflen);
	/*
	 * Now dump the scatter-gather table, up to the point of failure.
	 */
	for(jj=0; jj < SCpnt->use_sg; jj++)
	{
		printk("[%d]\tlen:%d\taddr:%p\tbounce:%p\n",
		       jj,
		       sgpnt[jj].length,
		       sgpnt[jj].address,
		       (bbpnt ? bbpnt[jj] : NULL));
		if (bbpnt && bbpnt[jj])
			consumed += sgpnt[jj].length;
	}
	printk("Total %d sectors consumed\n", consumed);
	panic("DMA pool exhausted");
}

#define CLUSTERABLE_DEVICE(SH,SD) (SH->use_clustering)

/*
 * This entire source file deals with the new queueing code.
 */

/*
 * Function:    __count_segments()
 *
 * Purpose:     Prototype for queue merge function.
 *
 * Arguments:   q       - Queue for which we are merging request.
 *              req     - request into which we wish to merge.
 *              use_clustering - 1 if this host wishes to use clustering
 *              dma_host - 1 if this host has ISA DMA issues (bus doesn't
 *                      expose all of the address lines, so that DMA cannot
 *                      be done from an arbitrary address).
 *		remainder - used to track the residual size of the last
 *			segment.  Comes in handy when we want to limit the 
 *			size of bounce buffer segments to PAGE_SIZE.
 *
 * Returns:     Count of the number of SG segments for the request.
 *
 * Lock status: 
 *
 * Notes:       This is only used for diagnostic purposes.
 */
__inline static int __count_segments(struct request *req,
				     int use_clustering,
				     int dma_host,
				     int * remainder)
{
	int ret = 1;
	int reqsize = 0;
	struct buffer_head *bh;
	struct buffer_head *bhnext;

	if( remainder != NULL ) {
		reqsize = *remainder;
	}

	/*
	 * Add in the size increment for the first buffer.
	 */
	bh = req->bh;
#ifdef DMA_SEGMENT_SIZE_LIMITED
	if( reqsize + bh->b_size > PAGE_SIZE ) {
		ret++;
		reqsize = bh->b_size;
	} else {
		reqsize += bh->b_size;
	}
#else
	reqsize += bh->b_size;
#endif

	for (bh = req->bh, bhnext = bh->b_reqnext; 
	     bhnext != NULL; 
	     bh = bhnext, bhnext = bh->b_reqnext) {
		if (use_clustering) {
			/* 
			 * See if we can do this without creating another
			 * scatter-gather segment.  In the event that this is a
			 * DMA capable host, make sure that a segment doesn't span
			 * the DMA threshold boundary.  
			 */
			if (dma_host && bh_phys(bhnext) - 1 == ISA_DMA_THRESHOLD) {
				ret++;
				reqsize = bhnext->b_size;
			} else if (blk_seg_merge_ok(bh, bhnext)) {
				/*
				 * This one is OK.  Let it go.
				 */ 
#ifdef DMA_SEGMENT_SIZE_LIMITED
				/* Note scsi_malloc is only able to hand out
				 * chunks of memory in sizes of PAGE_SIZE or
				 * less.  Thus we need to keep track of
				 * the size of the piece that we have
				 * seen so far, and if we have hit
				 * the limit of PAGE_SIZE, then we are
				 * kind of screwed and we need to start
				 * another segment.
				 */
				if( dma_host && bh_phys(bh) - 1 >= ISA_DMA_THRESHOLD
				    && reqsize + bhnext->b_size > PAGE_SIZE )
				{
					ret++;
					reqsize = bhnext->b_size;
					continue;
				}
#endif
				reqsize += bhnext->b_size;
				continue;
			}
			ret++;
			reqsize = bhnext->b_size;
		} else {
			ret++;
			reqsize = bhnext->b_size;
		}
	}
	if( remainder != NULL ) {
		*remainder = reqsize;
	}
	return ret;
}

/*
 * Function:    recount_segments()
 *
 * Purpose:     Recount the number of scatter-gather segments for this request.
 *
 * Arguments:   req     - request that needs recounting.
 *
 * Returns:     Count of the number of SG segments for the request.
 *
 * Lock status: Irrelevant.
 *
 * Notes:	This is only used when we have partially completed requests
 *		and the bit that is leftover is of an indeterminate size.
 *		This can come up if you get a MEDIUM_ERROR, for example,
 *		as we will have "completed" all of the sectors up to and
 *		including the bad sector, and the leftover bit is what
 *		we have to do now.  This tends to be a rare occurrence, so
 *		we aren't busting our butts to instantiate separate versions
 *		of this function for the 4 different flag values.  We
 *		probably should, however.
 */
void
recount_segments(Scsi_Cmnd * SCpnt)
{
	struct request *req;
	struct Scsi_Host *SHpnt;
	Scsi_Device * SDpnt;

	req   = &SCpnt->request;
	SHpnt = SCpnt->host;
	SDpnt = SCpnt->device;

	req->nr_segments = __count_segments(req, 
					    CLUSTERABLE_DEVICE(SHpnt, SDpnt),
					    SHpnt->unchecked_isa_dma, NULL);
}

#define MERGEABLE_BUFFERS(X,Y) \
(((((long)bh_phys((X))+(X)->b_size)|((long)bh_phys((Y)))) & \
  (DMA_CHUNK_SIZE - 1)) == 0)

#ifdef DMA_CHUNK_SIZE
static inline int scsi_new_mergeable(request_queue_t * q,
				     struct request * req,
				     struct Scsi_Host *SHpnt,
				     int max_segments)
{
	/*
	 * pci_map_sg will be able to merge these two
	 * into a single hardware sg entry, check if
	 * we'll have enough memory for the sg list.
	 * scsi.c allocates for this purpose
	 * min(64,sg_tablesize) entries.
	 */
	if (req->nr_segments >= max_segments ||
	    req->nr_segments >= SHpnt->sg_tablesize)
		return 0;
	req->nr_segments++;
	return 1;
}

static inline int scsi_new_segment(request_queue_t * q,
				   struct request * req,
				   struct Scsi_Host *SHpnt,
				   int max_segments)
{
	/*
	 * pci_map_sg won't be able to map these two
	 * into a single hardware sg entry, so we have to
	 * check if things fit into sg_tablesize.
	 */
	if (req->nr_hw_segments >= SHpnt->sg_tablesize ||
	     req->nr_segments >= SHpnt->sg_tablesize)
		return 0;
	req->nr_hw_segments++;
	req->nr_segments++;
	return 1;
}
#else
static inline int scsi_new_segment(request_queue_t * q,
				   struct request * req,
				   struct Scsi_Host *SHpnt,
				   int max_segments)
{
	if (req->nr_segments < SHpnt->sg_tablesize &&
	    req->nr_segments < max_segments) {
		/*
		 * This will form the start of a new segment.  Bump the 
		 * counter.
		 */
		req->nr_segments++;
		return 1;
	} else {
		return 0;
	}
}
#endif

/*
 * Function:    __scsi_merge_fn()
 *
 * Purpose:     Prototype for queue merge function.
 *
 * Arguments:   q       - Queue for which we are merging request.
 *              req     - request into which we wish to merge.
 *              bh      - Block which we may wish to merge into request
 *              use_clustering - 1 if this host wishes to use clustering
 *              dma_host - 1 if this host has ISA DMA issues (bus doesn't
 *                      expose all of the address lines, so that DMA cannot
 *                      be done from an arbitrary address).
 *
 * Returns:     1 if it is OK to merge the block into the request.  0
 *              if it is not OK.
 *
 * Lock status: io_request_lock is assumed to be held here.
 *
 * Notes:       Some drivers have limited scatter-gather table sizes, and
 *              thus they cannot queue an infinitely large command.  This
 *              function is called from ll_rw_blk before it attempts to merge
 *              a new block into a request to make sure that the request will
 *              not become too large.
 *
 *              This function is not designed to be directly called.  Instead
 *              it should be referenced from other functions where the
 *              use_clustering and dma_host parameters should be integer
 *              constants.  The compiler should thus be able to properly
 *              optimize the code, eliminating stuff that is irrelevant.
 *              It is more maintainable to do this way with a single function
 *              than to have 4 separate functions all doing roughly the
 *              same thing.
 */
__inline static int __scsi_back_merge_fn(request_queue_t * q,
					 struct request *req,
					 struct buffer_head *bh,
					 int max_segments,
					 int use_clustering,
					 int dma_host)
{
	unsigned int count;
	unsigned int segment_size = 0;
	Scsi_Device *SDpnt = q->queuedata;
	struct Scsi_Host *SHpnt = SDpnt->host;

	if (max_segments > scsi_max_sg)
		max_segments = scsi_max_sg;

#ifdef DMA_CHUNK_SIZE
	if (max_segments > 64)
		max_segments = 64;
#endif

	if ((req->nr_sectors + (bh->b_size >> 9)) > SHpnt->max_sectors)
		return 0;

	if (!BH_PHYS_4G(req->bhtail, bh))
		return 0;

	if (use_clustering) {
		/* 
		 * See if we can do this without creating another
		 * scatter-gather segment.  In the event that this is a
		 * DMA capable host, make sure that a segment doesn't span
		 * the DMA threshold boundary.  
		 */
		if (dma_host && bh_phys(req->bhtail) - 1 == ISA_DMA_THRESHOLD)
			goto new_end_segment;
		if (BH_CONTIG(req->bhtail, bh)) {
#ifdef DMA_SEGMENT_SIZE_LIMITED
			if (dma_host && bh_phys(bh) - 1 >= ISA_DMA_THRESHOLD) {
				segment_size = 0;
				count = __count_segments(req, use_clustering, dma_host, &segment_size);
				if( segment_size + bh->b_size > PAGE_SIZE ) {
					goto new_end_segment;
				}
			}
#endif
			/*
			 * This one is OK.  Let it go.
			 */
			return 1;
		}
	}
 new_end_segment:
#ifdef DMA_CHUNK_SIZE
	if (MERGEABLE_BUFFERS(req->bhtail, bh))
		return scsi_new_mergeable(q, req, SHpnt, max_segments);
#endif
	return scsi_new_segment(q, req, SHpnt, max_segments);
}

__inline static int __scsi_front_merge_fn(request_queue_t * q,
					  struct request *req,
					  struct buffer_head *bh,
					  int max_segments,
					  int use_clustering,
					  int dma_host)
{
	unsigned int count;
	unsigned int segment_size = 0;
	Scsi_Device *SDpnt = q->queuedata;
	struct Scsi_Host *SHpnt = SDpnt->host;

	if (max_segments > scsi_max_sg)
		max_segments = scsi_max_sg;

#ifdef DMA_CHUNK_SIZE
	if (max_segments > 64)
		max_segments = 64;
#endif

	if ((req->nr_sectors + (bh->b_size >> 9)) > SHpnt->max_sectors)
		return 0;

	if (!BH_PHYS_4G(bh, req->bh))
		return 0;

	if (use_clustering) {
		/* 
		 * See if we can do this without creating another
		 * scatter-gather segment.  In the event that this is a
		 * DMA capable host, make sure that a segment doesn't span
		 * the DMA threshold boundary. 
		 */
		if (dma_host && bh_phys(bh) - 1 == ISA_DMA_THRESHOLD) {
			goto new_start_segment;
		}
		if (BH_CONTIG(bh, req->bh)) {
#ifdef DMA_SEGMENT_SIZE_LIMITED
			if (dma_host && bh_phys(bh) - 1 >= ISA_DMA_THRESHOLD) {
				segment_size = bh->b_size;
				count = __count_segments(req, use_clustering, dma_host, &segment_size);
				if( count != req->nr_segments ) {
					goto new_start_segment;
				}
			}
#endif
			/*
			 * This one is OK.  Let it go.
			 */
			return 1;
		}
	}
 new_start_segment:
#ifdef DMA_CHUNK_SIZE
	if (MERGEABLE_BUFFERS(bh, req->bh))
		return scsi_new_mergeable(q, req, SHpnt, max_segments);
#endif
	return scsi_new_segment(q, req, SHpnt, max_segments);
}

/*
 * Function:    scsi_merge_fn_()
 *
 * Purpose:     queue merge function.
 *
 * Arguments:   q       - Queue for which we are merging request.
 *              req     - request into which we wish to merge.
 *              bh      - Block which we may wish to merge into request
 *
 * Returns:     1 if it is OK to merge the block into the request.  0
 *              if it is not OK.
 *
 * Lock status: io_request_lock is assumed to be held here.
 *
 * Notes:       Optimized for different cases depending upon whether
 *              ISA DMA is in use and whether clustering should be used.
 */
#define MERGEFCT(_FUNCTION, _BACK_FRONT, _CLUSTER, _DMA)		\
static int _FUNCTION(request_queue_t * q,				\
		     struct request * req,				\
		     struct buffer_head * bh,				\
		     int max_segments)					\
{									\
    int ret;								\
    SANITY_CHECK(req, _CLUSTER, _DMA);					\
    ret =  __scsi_ ## _BACK_FRONT ## _merge_fn(q,			\
					       req,			\
					       bh,			\
					       max_segments,		\
					       _CLUSTER,		\
					       _DMA);			\
    return ret;								\
}

/* Version with use_clustering 0 and dma_host 1 is not necessary,
 * since the only use of dma_host above is protected by use_clustering.
 */
MERGEFCT(scsi_back_merge_fn_, back, 0, 0)
MERGEFCT(scsi_back_merge_fn_c, back, 1, 0)
MERGEFCT(scsi_back_merge_fn_dc, back, 1, 1)

MERGEFCT(scsi_front_merge_fn_, front, 0, 0)
MERGEFCT(scsi_front_merge_fn_c, front, 1, 0)
MERGEFCT(scsi_front_merge_fn_dc, front, 1, 1)

/*
 * Function:    __scsi_merge_requests_fn()
 *
 * Purpose:     Prototype for queue merge function.
 *
 * Arguments:   q       - Queue for which we are merging request.
 *              req     - request into which we wish to merge.
 *              next    - 2nd request that we might want to combine with req
 *              use_clustering - 1 if this host wishes to use clustering
 *              dma_host - 1 if this host has ISA DMA issues (bus doesn't
 *                      expose all of the address lines, so that DMA cannot
 *                      be done from an arbitrary address).
 *
 * Returns:     1 if it is OK to merge the two requests.  0
 *              if it is not OK.
 *
 * Lock status: io_request_lock is assumed to be held here.
 *
 * Notes:       Some drivers have limited scatter-gather table sizes, and
 *              thus they cannot queue an infinitely large command.  This
 *              function is called from ll_rw_blk before it attempts to merge
 *              a new block into a request to make sure that the request will
 *              not become too large.
 *
 *              This function is not designed to be directly called.  Instead
 *              it should be referenced from other functions where the
 *              use_clustering and dma_host parameters should be integer
 *              constants.  The compiler should thus be able to properly
 *              optimize the code, eliminating stuff that is irrelevant.
 *              It is more maintainable to do this way with a single function
 *              than to have 4 separate functions all doing roughly the
 *              same thing.
 */
__inline static int __scsi_merge_requests_fn(request_queue_t * q,
					     struct request *req,
					     struct request *next,
					     int max_segments,
					     int use_clustering,
					     int dma_host)
{
	Scsi_Device *SDpnt = q->queuedata;
	struct Scsi_Host *SHpnt = SDpnt->host;

	/*
	 * First check if the either of the requests are re-queued
	 * requests.  Can't merge them if they are.
	 */
	if (req->special || next->special)
		return 0;

	if (max_segments > scsi_max_sg)
		max_segments = scsi_max_sg;

#ifdef DMA_CHUNK_SIZE
	if (max_segments > 64)
		max_segments = 64;

	/* If it would not fit into prepared memory space for sg chain,
	 * then don't allow the merge.
	 */
	if (req->nr_segments + next->nr_segments - 1 > max_segments ||
	    req->nr_segments + next->nr_segments - 1 > SHpnt->sg_tablesize) {
		return 0;
	}
	if (req->nr_hw_segments + next->nr_hw_segments - 1 > SHpnt->sg_tablesize) {
		return 0;
	}
#else
	/*
	 * If the two requests together are too large (even assuming that we
	 * can merge the boundary requests into one segment, then don't
	 * allow the merge.
	 */
	if (req->nr_segments + next->nr_segments - 1 > SHpnt->sg_tablesize) {
		return 0;
	}
#endif

	if ((req->nr_sectors + next->nr_sectors) > SHpnt->max_sectors)
		return 0;

	if (!BH_PHYS_4G(req->bhtail, next->bh))
		return 0;

	/*
	 * The main question is whether the two segments at the boundaries
	 * would be considered one or two.
	 */
	if (use_clustering) {
		/* 
		 * See if we can do this without creating another
		 * scatter-gather segment.  In the event that this is a
		 * DMA capable host, make sure that a segment doesn't span
		 * the DMA threshold boundary.  
		 */
		if (dma_host && bh_phys(req->bhtail) - 1 == ISA_DMA_THRESHOLD)
			goto dont_combine;
#ifdef DMA_SEGMENT_SIZE_LIMITED
		/*
		 * We currently can only allocate scatter-gather bounce
		 * buffers in chunks of PAGE_SIZE or less.
		 */
		if (dma_host && BH_CONTIG(req->bhtail, next->bh)
		    && bh_phys(req->bhtail) - 1 >= ISA_DMA_THRESHOLD)
		{
			int segment_size = 0;
			int count = 0;

			count = __count_segments(req, use_clustering, dma_host, &segment_size);
			count += __count_segments(next, use_clustering, dma_host, &segment_size);
			if( count != req->nr_segments + next->nr_segments ) {
				goto dont_combine;
			}
		}
#endif
		if (BH_CONTIG(req->bhtail, next->bh)) {
			/*
			 * This one is OK.  Let it go.
			 */
			req->nr_segments += next->nr_segments - 1;
#ifdef DMA_CHUNK_SIZE
			req->nr_hw_segments += next->nr_hw_segments - 1;
#endif
			return 1;
		}
	}
      dont_combine:
#ifdef DMA_CHUNK_SIZE
	if (req->nr_segments + next->nr_segments > max_segments ||
	    req->nr_segments + next->nr_segments > SHpnt->sg_tablesize) {
		return 0;
	}
	/* If dynamic DMA mapping can merge last segment in req with
	 * first segment in next, then the check for hw segments was
	 * done above already, so we can always merge.
	 */
	if (MERGEABLE_BUFFERS (req->bhtail, next->bh)) {
		req->nr_hw_segments += next->nr_hw_segments - 1;
	} else if (req->nr_hw_segments + next->nr_hw_segments > SHpnt->sg_tablesize) {
		return 0;
	} else {
		req->nr_hw_segments += next->nr_hw_segments;
	}
	req->nr_segments += next->nr_segments;
	return 1;
#else
	/*
	 * We know that the two requests at the boundary should not be combined.
	 * Make sure we can fix something that is the sum of the two.
	 * A slightly stricter test than we had above.
	 */
	if (req->nr_segments + next->nr_segments > max_segments ||
	    req->nr_segments + next->nr_segments > SHpnt->sg_tablesize) {
		return 0;
	} else {
		/*
		 * This will form the start of a new segment.  Bump the 
		 * counter.
		 */
		req->nr_segments += next->nr_segments;
		return 1;
	}
#endif
}

/*
 * Function:    scsi_merge_requests_fn_()
 *
 * Purpose:     queue merge function.
 *
 * Arguments:   q       - Queue for which we are merging request.
 *              req     - request into which we wish to merge.
 *              bh      - Block which we may wish to merge into request
 *
 * Returns:     1 if it is OK to merge the block into the request.  0
 *              if it is not OK.
 *
 * Lock status: io_request_lock is assumed to be held here.
 *
 * Notes:       Optimized for different cases depending upon whether
 *              ISA DMA is in use and whether clustering should be used.
 */
#define MERGEREQFCT(_FUNCTION, _CLUSTER, _DMA)		\
static int _FUNCTION(request_queue_t * q,		\
		     struct request * req,		\
		     struct request * next,		\
		     int max_segments)			\
{							\
    int ret;						\
    SANITY_CHECK(req, _CLUSTER, _DMA);			\
    ret =  __scsi_merge_requests_fn(q, req, next, max_segments, _CLUSTER, _DMA); \
    return ret;						\
}

/* Version with use_clustering 0 and dma_host 1 is not necessary,
 * since the only use of dma_host above is protected by use_clustering.
 */
MERGEREQFCT(scsi_merge_requests_fn_, 0, 0)
MERGEREQFCT(scsi_merge_requests_fn_c, 1, 0)
MERGEREQFCT(scsi_merge_requests_fn_dc, 1, 1)
/*
 * Function:    __init_io()
 *
 * Purpose:     Prototype for io initialize function.
 *
 * Arguments:   SCpnt   - Command descriptor we wish to initialize
 *              sg_count_valid  - 1 if the sg count in the req is valid.
 *              use_clustering - 1 if this host wishes to use clustering
 *              dma_host - 1 if this host has ISA DMA issues (bus doesn't
 *                      expose all of the address lines, so that DMA cannot
 *                      be done from an arbitrary address).
 *
 * Returns:     1 on success.
 *
 * Lock status: 
 *
 * Notes:       Only the SCpnt argument should be a non-constant variable.
 *              This function is designed in such a way that it will be
 *              invoked from a series of small stubs, each of which would
 *              be optimized for specific circumstances.
 *
 *              The advantage of this is that hosts that don't do DMA
 *              get versions of the function that essentially don't have
 *              any of the DMA code.  Same goes for clustering - in the
 *              case of hosts with no need for clustering, there is no point
 *              in a whole bunch of overhead.
 *
 *              Finally, in the event that a host has set can_queue to SG_ALL
 *              implying that there is no limit to the length of a scatter
 *              gather list, the sg count in the request won't be valid
 *              (mainly because we don't need queue management functions
 *              which keep the tally uptodate.
 */
__inline static int __init_io(Scsi_Cmnd * SCpnt,
			      int sg_count_valid,
			      int use_clustering,
			      int dma_host)
{
	struct buffer_head * bh;
	struct buffer_head * bhprev;
	char		   * buff;
	int		     count;
	int		     i;
	struct request     * req = &SCpnt->request;
	int		     sectors;
	struct scatterlist * sgpnt;
	int		     this_count;
	void		   ** bbpnt;

	/*
	 * First we need to know how many scatter gather segments are needed.
	 */
	if (!sg_count_valid) {
		count = __count_segments(req, use_clustering, dma_host, NULL);
	} else {
		count = req->nr_segments;
	}

	/*
	 * If the dma pool is nearly empty, then queue a minimal request
	 * with a single segment.  Typically this will satisfy a single
	 * buffer.
	 */
	if (dma_host && scsi_dma_free_sectors <= 10) {
		this_count = req->current_nr_sectors;
		goto single_segment;
	}
	/*
	 * we really want to use sg even for a single segment request,
	 * however some people just cannot be bothered to write decent
	 * driver code so we can't risk to break somebody making the
	 * assumption that sg requests will always contain at least 2
	 * segments. if the driver is 32-bit dma safe, then use sg for
	 * 1 entry anyways. if not, don't rely on the driver handling this
	 * case.
 	 */
	if (count == 1 && !SCpnt->host->highmem_io) {
		this_count = req->nr_sectors;
		goto single_segment;
	}

	/*
	 * for sane drivers, use sg even for 1 entry request
	 */
	SCpnt->use_sg = count;
	SCpnt->sglist_len = (SCpnt->use_sg * sizeof(struct scatterlist));

	/* If we could potentially require ISA bounce buffers, allocate
	 * space for this array here.
	 */
	if (dma_host)
		SCpnt->sglist_len += (SCpnt->use_sg * sizeof(void *));

	/* scsi_malloc can only allocate in chunks of 512 bytes so
	 * round it up.
	 */
	SCpnt->sglist_len = (SCpnt->sglist_len + 511) & ~511;

	sgpnt = (struct scatterlist *) scsi_malloc(SCpnt->sglist_len);

	/*
	 * Now fill the scatter-gather table.
	 */
	if (!sgpnt) {
#if 0
		/*
		 * If we cannot allocate the scatter-gather table, then
		 * simply write the first buffer all by itself.
		 */
		printk("Warning - running *really* short on DMA buffers\n");
		this_count = req->current_nr_sectors;
		goto single_segment;
#else
		/*
		 * it's probably better to simply always back off a little,
		 * and let some memory be returned to dma pool instead of
		 * always falling back to (slow) single segments
		 */
		return 0;
#endif
	}

	/*
	 * Next, walk the list, and fill in the addresses and sizes of
	 * each segment.
	 */
	memset(sgpnt, 0, SCpnt->sglist_len);
	SCpnt->request_buffer = (char *) sgpnt;
	SCpnt->request_bufflen = 0;
	bhprev = NULL;

	if (dma_host)
		bbpnt = (void **) ((char *)sgpnt +
			 (SCpnt->use_sg * sizeof(struct scatterlist)));
	else
		bbpnt = NULL;

	SCpnt->bounce_buffers = bbpnt;

	for (count = 0, bh = req->bh; bh; bh = bh->b_reqnext) {
		if (use_clustering && bhprev != NULL) {
			if (dma_host && bh_phys(bhprev) - 1 == ISA_DMA_THRESHOLD) {
				/* Nothing - fall through */
			} else if (blk_seg_merge_ok(bhprev, bh)) {
				/*
				 * This one is OK.  Let it go.  Note that we
				 * do not have the ability to allocate
				 * bounce buffer segments > PAGE_SIZE, so
				 * for now we limit the thing.
				 */
				if( dma_host ) {
#ifdef DMA_SEGMENT_SIZE_LIMITED
					if (bh_phys(bh) - 1 < ISA_DMA_THRESHOLD
					    || sgpnt[count - 1].length + bh->b_size <= PAGE_SIZE ) {
						sgpnt[count - 1].length += bh->b_size;
						bhprev = bh;
						continue;
					}
#else
					sgpnt[count - 1].length += bh->b_size;
					bhprev = bh;
					continue;
#endif
				} else {
					sgpnt[count - 1].length += bh->b_size;
					SCpnt->request_bufflen += bh->b_size;
					bhprev = bh;
					continue;
				}
			}
		}

		if (SCpnt->host->highmem_io) {
			sgpnt[count].page = bh->b_page;
			sgpnt[count].offset = bh_offset(bh);
			sgpnt[count].address = NULL;
		} else {
			if (PageHighMem(bh->b_page))
				BUG();

			sgpnt[count].page = NULL;
			sgpnt[count].address = bh->b_data;
		}
		
		sgpnt[count].length = bh->b_size;

		if (!dma_host)
			SCpnt->request_bufflen += bh->b_size;

		count++;
		bhprev = bh;
	}

	/*
	 * Verify that the count is correct.
	 */
	if (count != SCpnt->use_sg) {
		printk("Incorrect number of segments after building list\n");
#ifdef CONFIG_SCSI_DEBUG_QUEUES
		dump_stats(req, use_clustering, dma_host, count);
#endif
	}
	if (!dma_host) {
		return 1;
	}
	/*
	 * Now allocate bounce buffers, if needed.
	 */
	SCpnt->request_bufflen = 0;
	for (i = 0; i < count; i++) {
		sectors = (sgpnt[i].length >> 9);
		SCpnt->request_bufflen += sgpnt[i].length;
		/*
		 * only done for dma_host, in which case .page is not
		 * set since it's guarenteed to be a low memory page
		 */
		if (virt_to_phys(sgpnt[i].address) + sgpnt[i].length - 1 >
		    ISA_DMA_THRESHOLD) {
			if( scsi_dma_free_sectors - sectors <= 10  ) {
				/*
				 * If this would nearly drain the DMA
				 * pool empty, then let's stop here.
				 * Don't make this request any larger.
				 * This is kind of a safety valve that
				 * we use - we could get screwed later
				 * on if we run out completely.  
				 */
				SCpnt->request_bufflen -= sgpnt[i].length;
				SCpnt->use_sg = i;
				if (i == 0) {
					goto big_trouble;
				}
				break;
			}

			bbpnt[i] = sgpnt[i].address;
			sgpnt[i].address =
			    (char *) scsi_malloc(sgpnt[i].length);
			/*
			 * If we cannot allocate memory for this DMA bounce
			 * buffer, then queue just what we have done so far.
			 */
			if (sgpnt[i].address == NULL) {
				printk("Warning - running low on DMA memory\n");
				SCpnt->request_bufflen -= sgpnt[i].length;
				SCpnt->use_sg = i;
				if (i == 0) {
					goto big_trouble;
				}
				break;
			}
			if (req->cmd == WRITE) {
				memcpy(sgpnt[i].address, bbpnt[i],
				       sgpnt[i].length);
			}
		}
	}
	return 1;

      big_trouble:
	/*
	 * We come here in the event that we get one humongous
	 * request, where we need a bounce buffer, and the buffer is
	 * more than we can allocate in a single call to
	 * scsi_malloc().  In addition, we only come here when it is
	 * the 0th element of the scatter-gather table that gets us
	 * into this trouble.  As a fallback, we fall back to
	 * non-scatter-gather, and ask for a single segment.  We make
	 * a half-hearted attempt to pick a reasonably large request
	 * size mainly so that we don't thrash the thing with
	 * iddy-biddy requests.
	 */

	/*
	 * The original number of sectors in the 0th element of the
	 * scatter-gather table.  
	 */
	sectors = sgpnt[0].length >> 9;

	/* 
	 * Free up the original scatter-gather table.  Note that since
	 * it was the 0th element that got us here, we don't have to
	 * go in and free up memory from the other slots.  
	 */
	SCpnt->request_bufflen = 0;
	SCpnt->use_sg = 0;
	scsi_free(SCpnt->request_buffer, SCpnt->sglist_len);

	/*
	 * Make an attempt to pick up as much as we reasonably can.
	 * Just keep adding sectors until the pool starts running kind of
	 * low.  The limit of 30 is somewhat arbitrary - the point is that
	 * it would kind of suck if we dropped down and limited ourselves to
	 * single-block requests if we had hundreds of free sectors.
	 */
	if( scsi_dma_free_sectors > 30 ) {
		for (this_count = 0, bh = req->bh; bh; bh = bh->b_reqnext) {
			if( scsi_dma_free_sectors - this_count < 30 
			    || this_count == sectors )
			{
				break;
			}
			this_count += bh->b_size >> 9;
		}

	} else {
		/*
		 * Yow!   Take the absolute minimum here.
		 */
		this_count = req->current_nr_sectors;
	}

	/*
	 * Now drop through into the single-segment case.
	 */
	
single_segment:
	/*
	 * for highmem cases, we have to revert to bouncing for single
	 * segments. rather just give up now and let the device starvation
	 * path reinitiate this i/o later
	 */
	if (SCpnt->host->highmem_io)
		return 0;

	/*
	 * Come here if for any reason we choose to do this as a single
	 * segment.  Possibly the entire request, or possibly a small
	 * chunk of the entire request.
	 */
	bh = req->bh;
	buff = req->buffer = bh->b_data;

	if (PageHighMem(bh->b_page))
		BUG();

	if (dma_host) {
		/*
		 * Allocate a DMA bounce buffer.  If the allocation fails, fall
		 * back and allocate a really small one - enough to satisfy
		 * the first buffer.
		 */
		if (bh_phys(bh) + (this_count << 9) - 1 > ISA_DMA_THRESHOLD) {
			buff = (char *) scsi_malloc(this_count << 9);
			if (!buff) {
				printk("Warning - running low on DMA memory\n");
				this_count = req->current_nr_sectors;
				buff = (char *) scsi_malloc(this_count << 9);
				if (!buff) {
					dma_exhausted(SCpnt, 0);
				}
			}
			if (req->cmd == WRITE)
				memcpy(buff, (char *) req->buffer, this_count << 9);
		}
	}

	SCpnt->request_bufflen = this_count << 9;
	SCpnt->request_buffer = buff;
	SCpnt->use_sg = 0;
	return 1;
}

#define INITIO(_FUNCTION, _VALID, _CLUSTER, _DMA)	\
static int _FUNCTION(Scsi_Cmnd * SCpnt)			\
{							\
    return __init_io(SCpnt, _VALID, _CLUSTER, _DMA);	\
}

/*
 * ll_rw_blk.c now keeps track of the number of segments in
 * a request.  Thus we don't have to do it any more here.
 * We always force "_VALID" to 1.  Eventually clean this up
 * and get rid of the extra argument.
 */
INITIO(scsi_init_io_v, 1, 0, 0)
INITIO(scsi_init_io_vd, 1, 0, 1)
INITIO(scsi_init_io_vc, 1, 1, 0)
INITIO(scsi_init_io_vdc, 1, 1, 1)

/*
 * Function:    initialize_merge_fn()
 *
 * Purpose:     Initialize merge function for a host
 *
 * Arguments:   SHpnt   - Host descriptor.
 *
 * Returns:     Nothing.
 *
 * Lock status: 
 *
 * Notes:
 */
void initialize_merge_fn(Scsi_Device * SDpnt)
{
	struct Scsi_Host *SHpnt = SDpnt->host;
	request_queue_t *q = &SDpnt->request_queue;
	dma64_addr_t bounce_limit;

	/*
	 * If this host has an unlimited tablesize, then don't bother with a
	 * merge manager.  The whole point of the operation is to make sure
	 * that requests don't grow too large, and this host isn't picky.
	 *
	 * Note that ll_rw_blk.c is effectively maintaining a segment
	 * count which is only valid if clustering is used, and it obviously
	 * doesn't handle the DMA case.   In the end, it
	 * is simply easier to do it ourselves with our own functions
	 * rather than rely upon the default behavior of ll_rw_blk.
	 */
	if (!CLUSTERABLE_DEVICE(SHpnt, SDpnt) && SHpnt->unchecked_isa_dma == 0) {
		q->back_merge_fn = scsi_back_merge_fn_;
		q->front_merge_fn = scsi_front_merge_fn_;
		q->merge_requests_fn = scsi_merge_requests_fn_;
		SDpnt->scsi_init_io_fn = scsi_init_io_v;
	} else if (!CLUSTERABLE_DEVICE(SHpnt, SDpnt) && SHpnt->unchecked_isa_dma != 0) {
		q->back_merge_fn = scsi_back_merge_fn_;
		q->front_merge_fn = scsi_front_merge_fn_;
		q->merge_requests_fn = scsi_merge_requests_fn_;
		SDpnt->scsi_init_io_fn = scsi_init_io_vd;
	} else if (CLUSTERABLE_DEVICE(SHpnt, SDpnt) && SHpnt->unchecked_isa_dma == 0) {
		q->back_merge_fn = scsi_back_merge_fn_c;
		q->front_merge_fn = scsi_front_merge_fn_c;
		q->merge_requests_fn = scsi_merge_requests_fn_c;
		SDpnt->scsi_init_io_fn = scsi_init_io_vc;
	} else if (CLUSTERABLE_DEVICE(SHpnt, SDpnt) && SHpnt->unchecked_isa_dma != 0) {
		q->back_merge_fn = scsi_back_merge_fn_dc;
		q->front_merge_fn = scsi_front_merge_fn_dc;
		q->merge_requests_fn = scsi_merge_requests_fn_dc;
		SDpnt->scsi_init_io_fn = scsi_init_io_vdc;
	}

	/*
	 * now enable highmem I/O, if appropriate
	 */
	bounce_limit = BLK_BOUNCE_HIGH;
	if (SHpnt->highmem_io && (SDpnt->type == TYPE_DISK)) {
		if (!PCI_DMA_BUS_IS_PHYS)
			/* Platforms with virtual-DMA translation
 			 * hardware have no practical limit.
			 */
			bounce_limit = BLK_BOUNCE_ANY;
		else
			bounce_limit = SHpnt->pci_dev->dma_mask;
	}

	blk_queue_bounce_limit(q, bounce_limit);
}
