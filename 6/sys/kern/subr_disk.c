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
}

void
bioq_remove(struct bio_queue_head *head, struct bio *bp)
{
	if (bp == head->insert_point) {
		head->last_offset = bp->bio_offset;
		head->insert_point = TAILQ_NEXT(bp, bio_queue);
		if (head->insert_point == NULL) {
			head->last_offset = 0;
			head->insert_point = TAILQ_FIRST(&head->queue);
		}
	}
	TAILQ_REMOVE(&head->queue, bp, bio_queue);
}

void
bioq_flush(struct bio_queue_head *head, struct devstat *stp, int error)
{
	struct bio *bp;

	while ((bp = bioq_takefirst(head)) != NULL)
		biofinish(bp, stp, error);
}

void
bioq_insert_head(struct bio_queue_head *head, struct bio *bp)
{

	if (TAILQ_FIRST(&head->queue) == NULL)
		head->insert_point = bp;
	TAILQ_INSERT_HEAD(&head->queue, bp, bio_queue);
}

void
bioq_insert_tail(struct bio_queue_head *head, struct bio *bp)
{

	if (TAILQ_FIRST(&head->queue) == NULL)
		head->insert_point = bp;
	TAILQ_INSERT_TAIL(&head->queue, bp, bio_queue);
}

struct bio *
bioq_first(struct bio_queue_head *head)
{

	return (TAILQ_FIRST(&head->queue));
}

struct bio *
bioq_takefirst(struct bio_queue_head *head)
{
	struct bio *bp;

	bp = TAILQ_FIRST(&head->queue);
	if (bp != NULL)
		bioq_remove(head, bp);
	return (bp);
}

/*
 * Seek sort for disks.
 *
 * The disksort algorithm sorts all requests in a single queue while keeping
 * track of the current position of the disk with insert_point and
 * last_offset.  last_offset is the offset of the last block sent to disk, or
 * 0 once we reach the end.  insert_point points to the first buf after
 * last_offset, and is used to slightly speed up insertions.  Blocks are
 * always sorted in ascending order and the queue always restarts at 0.
 * This implements the one-way scan which optimizes disk seek times.
 */
void
bioq_disksort(bioq, bp)
	struct bio_queue_head *bioq;
	struct bio *bp;
{
	struct bio *bq;
	struct bio *bn;

	/*
	 * If the queue is empty then it's easy.
	 */
	if (bioq_first(bioq) == NULL) {
		bioq_insert_tail(bioq, bp);
		return;
	}
	/*
	 * Optimize for sequential I/O by seeing if we go at the tail.
	 */
	bq = TAILQ_LAST(&bioq->queue, bio_queue);
	if (bp->bio_offset > bq->bio_offset) {
		TAILQ_INSERT_AFTER(&bioq->queue, bq, bp, bio_queue);
		return;
	}
	/*
	 * Pick our scan start based on the last request.  A poor man's
	 * binary search.
	 */
	if (bp->bio_offset >= bioq->last_offset) { 
		bq = bioq->insert_point;
		/*
		 * If we're before the next bio and after the last offset,
		 * update insert_point;
		 */
		if (bp->bio_offset < bq->bio_offset) {
			bioq->insert_point = bp;
			TAILQ_INSERT_BEFORE(bq, bp, bio_queue);
			return;
		}
	} else
		bq = TAILQ_FIRST(&bioq->queue);
	if (bp->bio_offset < bq->bio_offset) {
		TAILQ_INSERT_BEFORE(bq, bp, bio_queue);
		return;
	}
	/* Insertion sort */
	while ((bn = TAILQ_NEXT(bq, bio_queue)) != NULL) {
		if (bp->bio_offset < bn->bio_offset)
			break;
		bq = bn;
	}
	TAILQ_INSERT_AFTER(&bioq->queue, bq, bp, bio_queue);
}
