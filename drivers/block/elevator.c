/*
 *  linux/drivers/block/elevator.c
 *
 *  Block device elevator/IO-scheduler.
 *
 *  Copyright (C) 2000 Andrea Arcangeli <andrea@suse.de> SuSE
 *
 * 30042000 Jens Axboe <axboe@suse.de> :
 *
 * Split the elevator a bit so that it is possible to choose a different
 * one or even write a new "plug in". There are three pieces:
 * - elevator_fn, inserts a new request in the queue list
 * - elevator_merge_fn, decides whether a new buffer can be merged with
 *   an existing request
 * - elevator_dequeue_fn, called when a request is taken off the active list
 *
 * 20082000 Dave Jones <davej@suse.de> :
 * Removed tests for max-bomb-segments, which was breaking elvtune
 *  when run without -bN
 *
 */

#include <linux/fs.h>
#include <linux/blkdev.h>
#include <linux/elevator.h>
#include <linux/blk.h>
#include <linux/module.h>
#include <asm/uaccess.h>

/*
 * This is a bit tricky. It's given that bh and rq are for the same
 * device, but the next request might of course not be. Run through
 * the tests below to check if we want to insert here if we can't merge
 * bh into an existing request
 */
inline int bh_rq_in_between(struct buffer_head *bh, struct request *rq,
			    struct list_head *head)
{
	struct list_head *next;
	struct request *next_rq;

	next = rq->queue.next;
	if (next == head)
		return 0;

	/*
	 * if the device is different (usually on a different partition),
	 * just check if bh is after rq
	 */
	next_rq = blkdev_entry_to_request(next);
	if (next_rq->rq_dev != rq->rq_dev)
		return bh->b_rsector > rq->sector;

	/*
	 * ok, rq, next_rq and bh are on the same device. if bh is in between
	 * the two, this is the sweet spot
	 */
	if (bh->b_rsector < next_rq->sector && bh->b_rsector > rq->sector)
		return 1;

	/*
	 * next_rq is ordered wrt rq, but bh is not in between the two
	 */
	if (next_rq->sector > rq->sector)
		return 0;

	/*
	 * next_rq and rq not ordered, if we happen to be either before
	 * next_rq or after rq insert here anyway
	 */
	if (bh->b_rsector > rq->sector || bh->b_rsector < next_rq->sector)
		return 1;

	return 0;
}


int elevator_linus_merge(request_queue_t *q, struct request **req,
			 struct list_head * head,
			 struct buffer_head *bh, int rw,
			 int max_sectors)
{
	struct list_head *entry = &q->queue_head;
	unsigned int count = bh->b_size >> 9, ret = ELEVATOR_NO_MERGE;
	struct request *__rq;
	int backmerge_only = 0;

	while (!backmerge_only && (entry = entry->prev) != head) {
		__rq = blkdev_entry_to_request(entry);

		/*
		 * we can't insert beyond a zero sequence point
		 */
		if (__rq->elevator_sequence <= 0)
			backmerge_only = 1;

		if (__rq->waiting)
			continue;
		if (__rq->rq_dev != bh->b_rdev)
			continue;
		if (!*req && bh_rq_in_between(bh, __rq, &q->queue_head) && !backmerge_only)
			*req = __rq;
		if (__rq->cmd != rw)
			continue;
		if (__rq->nr_sectors + count > max_sectors)
			continue;
		if (__rq->sector + __rq->nr_sectors == bh->b_rsector) {
			ret = ELEVATOR_BACK_MERGE;
			*req = __rq;
			break;
		} else if (__rq->sector - count == bh->b_rsector && !backmerge_only) {
			ret = ELEVATOR_FRONT_MERGE;
			__rq->elevator_sequence--;
			*req = __rq;
			break;
		}
	}

	/*
	 * account merge (ret != 0, cost is 1) or seeky insert (*req is set,
	 * cost is ELV_LINUS_SEEK_COST
	 */
	if (*req) {
		int scan_cost = ret ? 1 : ELV_LINUS_SEEK_COST;
		struct list_head *entry = &(*req)->queue;

		while ((entry = entry->next) != &q->queue_head) {
			__rq = blkdev_entry_to_request(entry);
			__rq->elevator_sequence -= scan_cost;
		}
	}

	return ret;
}

void elevator_linus_merge_req(struct request *req, struct request *next)
{
	if (next->elevator_sequence < req->elevator_sequence)
		req->elevator_sequence = next->elevator_sequence;
}

/*
 * See if we can find a request that this buffer can be coalesced with.
 */
int elevator_noop_merge(request_queue_t *q, struct request **req,
			struct list_head * head,
			struct buffer_head *bh, int rw,
			int max_sectors)
{
	struct list_head *entry;
	unsigned int count = bh->b_size >> 9;

	if (list_empty(&q->queue_head))
		return ELEVATOR_NO_MERGE;

	entry = &q->queue_head;
	while ((entry = entry->prev) != head) {
		struct request *__rq = blkdev_entry_to_request(entry);

		if (__rq->cmd != rw)
			continue;
		if (__rq->rq_dev != bh->b_rdev)
			continue;
		if (__rq->nr_sectors + count > max_sectors)
			continue;
		if (__rq->waiting)
			continue;
		if (__rq->sector + __rq->nr_sectors == bh->b_rsector) {
			*req = __rq;
			return ELEVATOR_BACK_MERGE;
		} else if (__rq->sector - count == bh->b_rsector) {
			*req = __rq;
			return ELEVATOR_FRONT_MERGE;
		}
	}

	*req = blkdev_entry_to_request(q->queue_head.prev);
	return ELEVATOR_NO_MERGE;
}

void elevator_noop_merge_req(struct request *req, struct request *next) {}

int blkelvget_ioctl(elevator_t * elevator, blkelv_ioctl_arg_t * arg)
{
	blkelv_ioctl_arg_t output;

	output.queue_ID			= elevator->queue_ID;
	output.read_latency		= elevator->read_latency;
	output.write_latency		= elevator->write_latency;
	output.max_bomb_segments	= 0;

	if (copy_to_user(arg, &output, sizeof(blkelv_ioctl_arg_t)))
		return -EFAULT;

	return 0;
}

int blkelvset_ioctl(elevator_t * elevator, const blkelv_ioctl_arg_t * arg)
{
	blkelv_ioctl_arg_t input;

	if (copy_from_user(&input, arg, sizeof(blkelv_ioctl_arg_t)))
		return -EFAULT;

	if (input.read_latency < 0)
		return -EINVAL;
	if (input.write_latency < 0)
		return -EINVAL;

	elevator->read_latency		= input.read_latency;
	elevator->write_latency		= input.write_latency;
	return 0;
}

void elevator_init(elevator_t * elevator, elevator_t type)
{
	static unsigned int queue_ID;

	*elevator = type;
	elevator->queue_ID = queue_ID++;
}
