/*
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <phk@FreeBSD.ORG> wrote this file.  As long as you retain this notice you
 * can do whatever you want with this stuff. If we meet some day, and you think
 * this stuff is worth it, you can buy me a beer in return.   Poul-Henning Kamp
 * ----------------------------------------------------------------------------
 *
 * $FreeBSD$
 *
 */

#include "opt_geom.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/stdint.h>
#include <sys/bio.h>
#include <sys/conf.h>
#include <sys/disk.h>
#include <sys/disklabel.h>

/*-
 * Disk error is the preface to plaintive error messages
 * about failing disk transfers.  It prints messages of the form
 * 	"hp0g: BLABLABLA cmd=read fsbn 12345 of 12344-12347"
 * blkdone should be -1 if the position of the error is unknown.
 * The message is printed with printf.
 */
void
disk_err(struct bio *bp, const char *what, int blkdone, int nl)
{
	daddr_t sn;

	printf("%s: %s ", devtoname(bp->bio_dev), what);
	switch(bp->bio_cmd) {
	case BIO_READ:		printf("cmd=read "); break;
	case BIO_WRITE:		printf("cmd=write "); break;
	case BIO_DELETE:	printf("cmd=delete "); break;
	case BIO_GETATTR:	printf("cmd=getattr "); break;
	case BIO_SETATTR:	printf("cmd=setattr "); break;
	default:		printf("cmd=%x ", bp->bio_cmd); break;
	}
	sn = bp->bio_blkno;
	if (bp->bio_bcount <= DEV_BSIZE) {
		printf("fsbn %jd%s", (intmax_t)sn, nl ? "\n" : "");
		return;
	}
	if (blkdone >= 0) {
		sn += blkdone;
		printf("fsbn %jd of ", (intmax_t)sn);
	}
	printf("%jd-%jd", (intmax_t)bp->bio_blkno,
	    (intmax_t)(bp->bio_blkno + (bp->bio_bcount - 1) / DEV_BSIZE));
	if (nl)
		printf("\n");
}

/*
 * Seek sort for disks.
 *
 * The buf_queue keep two queues, sorted in ascending block order.  The first
 * queue holds those requests which are positioned after the current block
 * (in the first request); the second, which starts at queue->switch_point,
 * holds requests which came in after their block number was passed.  Thus
 * we implement a one way scan, retracting after reaching the end of the drive
 * to the first request on the second queue, at which time it becomes the
 * first queue.
 *
 * A one-way scan is natural because of the way UNIX read-ahead blocks are
 * allocated.
 */

void
bioq_disksort(bioq, bp)
	struct bio_queue_head *bioq;
	struct bio *bp;
{
	struct bio *bq;
	struct bio *bn;
	struct bio *be;

	if (!atomic_cmpset_int(&bioq->busy, 0, 1))
		panic("Recursing in bioq_disksort()");
	be = TAILQ_LAST(&bioq->queue, bio_queue);
	/*
	 * If the queue is empty or we are an
	 * ordered transaction, then it's easy.
	 */
	if ((bq = bioq_first(bioq)) == NULL) {
		bioq_insert_tail(bioq, bp);
		bioq->busy = 0;
		return;
	} else if (bioq->insert_point != NULL) {

		/*
		 * A certain portion of the list is
		 * "locked" to preserve ordering, so
		 * we can only insert after the insert
		 * point.
		 */
		bq = bioq->insert_point;
	} else {

		/*
		 * If we lie before the last removed (currently active)
		 * request, and are not inserting ourselves into the
		 * "locked" portion of the list, then we must add ourselves
		 * to the second request list.
		 */
		if (bp->bio_pblkno < bioq->last_pblkno) {

			bq = bioq->switch_point;
			/*
			 * If we are starting a new secondary list,
			 * then it's easy.
			 */
			if (bq == NULL) {
				bioq->switch_point = bp;
				bioq_insert_tail(bioq, bp);
				bioq->busy = 0;
				return;
			}
			/*
			 * If we lie ahead of the current switch point,
			 * insert us before the switch point and move
			 * the switch point.
			 */
			if (bp->bio_pblkno < bq->bio_pblkno) {
				bioq->switch_point = bp;
				TAILQ_INSERT_BEFORE(bq, bp, bio_queue);
				bioq->busy = 0;
				return;
			}
		} else {
			if (bioq->switch_point != NULL)
				be = TAILQ_PREV(bioq->switch_point,
						bio_queue, bio_queue);
			/*
			 * If we lie between last_pblkno and bq,
			 * insert before bq.
			 */
			if (bp->bio_pblkno < bq->bio_pblkno) {
				TAILQ_INSERT_BEFORE(bq, bp, bio_queue);
				bioq->busy = 0;
				return;
			}
		}
	}

	/*
	 * Request is at/after our current position in the list.
	 * Optimize for sequential I/O by seeing if we go at the tail.
	 */
	if (bp->bio_pblkno > be->bio_pblkno) {
		TAILQ_INSERT_AFTER(&bioq->queue, be, bp, bio_queue);
		bioq->busy = 0;
		return;
	}

	/* Otherwise, insertion sort */
	while ((bn = TAILQ_NEXT(bq, bio_queue)) != NULL) {
		
		/*
		 * We want to go after the current request if it is the end
		 * of the first request list, or if the next request is a
		 * larger cylinder than our request.
		 */
		if (bn == bioq->switch_point
		 || bp->bio_pblkno < bn->bio_pblkno)
			break;
		bq = bn;
	}
	TAILQ_INSERT_AFTER(&bioq->queue, bq, bp, bio_queue);
	bioq->busy = 0;
}


