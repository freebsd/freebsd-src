/*-
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <phk@FreeBSD.ORG> wrote this file.  As long as you retain this notice you
 * can do whatever you want with this stuff. If we meet some day, and you think
 * this stuff is worth it, you can buy me a beer in return.   Poul-Henning Kamp
 * ----------------------------------------------------------------------------
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_geom.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bio.h>
#include <sys/conf.h>
#include <sys/disk.h>
#include <geom/geom_disk.h>

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

	if (bp->bio_dev != NULL)
		printf("%s: %s ", devtoname(bp->bio_dev), what);
	else if (bp->bio_disk != NULL)
		printf("%s%d: %s ",
		    bp->bio_disk->d_name, bp->bio_disk->d_unit, what);
	else
		printf("disk??: %s ", what);
	switch(bp->bio_cmd) {
	case BIO_READ:		printf("cmd=read "); break;
	case BIO_WRITE:		printf("cmd=write "); break;
	case BIO_DELETE:	printf("cmd=delete "); break;
	case BIO_GETATTR:	printf("cmd=getattr "); break;
	default:		printf("cmd=%x ", bp->bio_cmd); break;
	}
	sn = bp->bio_pblkno;
	if (bp->bio_bcount <= DEV_BSIZE) {
		printf("fsbn %jd%s", (intmax_t)sn, nl ? "\n" : "");
		return;
	}
	if (blkdone >= 0) {
		sn += blkdone;
		printf("fsbn %jd of ", (intmax_t)sn);
	}
	printf("%jd-%jd", (intmax_t)bp->bio_pblkno,
	    (intmax_t)(bp->bio_pblkno + (bp->bio_bcount - 1) / DEV_BSIZE));
	if (nl)
		printf("\n");
}

/*
 * BIO queue implementation
 */

void
bioq_init(struct bio_queue_head *head)
{
	TAILQ_INIT(&head->queue);
	head->last_offset = 0;
	head->insert_point = NULL;
	head->switch_point = NULL;
}

void
bioq_remove(struct bio_queue_head *head, struct bio *bp)
{
	if (bp == head->switch_point)
		head->switch_point = TAILQ_NEXT(bp, bio_queue);
	if (bp == head->insert_point) {
		head->insert_point = TAILQ_PREV(bp, bio_queue, bio_queue);
		if (head->insert_point == NULL)
			head->last_offset = 0;
	} else if (bp == TAILQ_FIRST(&head->queue))
		head->last_offset = bp->bio_offset;
	TAILQ_REMOVE(&head->queue, bp, bio_queue);
	if (TAILQ_FIRST(&head->queue) == head->switch_point)
		head->switch_point = NULL;
}

void
bioq_flush(struct bio_queue_head *head, struct devstat *stp, int error)
{
	struct bio *bp;

	for (;;) {
		bp = bioq_first(head);
		if (bp == NULL)
			break;
		bioq_remove(head, bp);
		biofinish(bp, stp, error);
	}
}

void
bioq_insert_head(struct bio_queue_head *head, struct bio *bp)
{

	TAILQ_INSERT_HEAD(&head->queue, bp, bio_queue);
}

void
bioq_insert_tail(struct bio_queue_head *head, struct bio *bp)
{

	TAILQ_INSERT_TAIL(&head->queue, bp, bio_queue);
}

struct bio *
bioq_first(struct bio_queue_head *head)
{

	return (TAILQ_FIRST(&head->queue));
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

	be = TAILQ_LAST(&bioq->queue, bio_queue);
	/*
	 * If the queue is empty or we are an
	 * ordered transaction, then it's easy.
	 */
	if ((bq = bioq_first(bioq)) == NULL) {
		bioq_insert_tail(bioq, bp);
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
		if (bp->bio_offset < bioq->last_offset) {

			bq = bioq->switch_point;
			/*
			 * If we are starting a new secondary list,
			 * then it's easy.
			 */
			if (bq == NULL) {
				bioq->switch_point = bp;
				bioq_insert_tail(bioq, bp);
				return;
			}
			/*
			 * If we lie ahead of the current switch point,
			 * insert us before the switch point and move
			 * the switch point.
			 */
			if (bp->bio_offset < bq->bio_offset) {
				bioq->switch_point = bp;
				TAILQ_INSERT_BEFORE(bq, bp, bio_queue);
				return;
			}
		} else {
			if (bioq->switch_point != NULL)
				be = TAILQ_PREV(bioq->switch_point,
						bio_queue, bio_queue);
			/*
			 * If we lie between last_offset and bq,
			 * insert before bq.
			 */
			if (bp->bio_offset < bq->bio_offset) {
				TAILQ_INSERT_BEFORE(bq, bp, bio_queue);
				return;
			}
		}
	}

	/*
	 * Request is at/after our current position in the list.
	 * Optimize for sequential I/O by seeing if we go at the tail.
	 */
	if (bp->bio_offset > be->bio_offset) {
		TAILQ_INSERT_AFTER(&bioq->queue, be, bp, bio_queue);
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
		 || bp->bio_offset < bn->bio_offset)
			break;
		bq = bn;
	}
	TAILQ_INSERT_AFTER(&bioq->queue, bq, bp, bio_queue);
}


