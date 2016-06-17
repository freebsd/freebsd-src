#ifndef _LINUX_BLKDEV_H
#define _LINUX_BLKDEV_H

#include <linux/major.h>
#include <linux/sched.h>
#include <linux/genhd.h>
#include <linux/tqueue.h>
#include <linux/list.h>
#include <linux/mm.h>

#include <asm/io.h>

struct request_queue;
typedef struct request_queue request_queue_t;
struct elevator_s;
typedef struct elevator_s elevator_t;

/*
 * Ok, this is an expanded form so that we can use the same
 * request for paging requests.
 */
struct request {
	struct list_head queue;
	int elevator_sequence;

	volatile int rq_status;	/* should split this into a few status bits */
#define RQ_INACTIVE		(-1)
#define RQ_ACTIVE		1
#define RQ_SCSI_BUSY		0xffff
#define RQ_SCSI_DONE		0xfffe
#define RQ_SCSI_DISCONNECTING	0xffe0

	kdev_t rq_dev;
	int cmd;		/* READ or WRITE */
	int errors;
	unsigned long start_time;
	unsigned long sector;
	unsigned long nr_sectors;
	unsigned long hard_sector, hard_nr_sectors;
	unsigned int nr_segments;
	unsigned int nr_hw_segments;
	unsigned long current_nr_sectors, hard_cur_sectors;
	void * special;
	char * buffer;
	struct completion * waiting;
	struct buffer_head * bh;
	struct buffer_head * bhtail;
	request_queue_t *q;
};

#include <linux/elevator.h>

typedef int (merge_request_fn) (request_queue_t *q, 
				struct request  *req,
				struct buffer_head *bh,
				int);
typedef int (merge_requests_fn) (request_queue_t *q, 
				 struct request  *req,
				 struct request  *req2,
				 int);
typedef void (request_fn_proc) (request_queue_t *q);
typedef request_queue_t * (queue_proc) (kdev_t dev);
typedef int (make_request_fn) (request_queue_t *q, int rw, struct buffer_head *bh);
typedef void (plug_device_fn) (request_queue_t *q, kdev_t device);
typedef void (unplug_device_fn) (void *q);

struct request_list {
	unsigned int count;
	unsigned int pending[2];
	struct list_head free;
};

struct request_queue
{
	/*
	 * the queue request freelist, one for reads and one for writes
	 */
	struct request_list	rq;

	/*
	 * The total number of requests on each queue
	 */
	int nr_requests;

	/*
	 * Batching threshold for sleep/wakeup decisions
	 */
	int batch_requests;

	/*
	 * The total number of 512byte blocks on each queue
	 */
	atomic_t nr_sectors;

	/*
	 * Batching threshold for sleep/wakeup decisions
	 */
	int batch_sectors;

	/*
	 * The max number of 512byte blocks on each queue
	 */
	int max_queue_sectors;

	/*
	 * Together with queue_head for cacheline sharing
	 */
	struct list_head	queue_head;
	elevator_t		elevator;

	request_fn_proc		* request_fn;
	merge_request_fn	* back_merge_fn;
	merge_request_fn	* front_merge_fn;
	merge_requests_fn	* merge_requests_fn;
	make_request_fn		* make_request_fn;
	plug_device_fn		* plug_device_fn;
	/*
	 * The queue owner gets to use this for whatever they like.
	 * ll_rw_blk doesn't touch it.
	 */
	void			* queuedata;

	/*
	 * This is used to remove the plug when tq_disk runs.
	 */
	struct tq_struct	plug_tq;

	/*
	 * Boolean that indicates whether this queue is plugged or not.
	 */
	int			plugged:1;

	/*
	 * Boolean that indicates whether current_request is active or
	 * not.
	 */
	int			head_active:1;

	/*
	 * Boolean that indicates you will use blk_started_sectors
	 * and blk_finished_sectors in addition to blk_started_io
	 * and blk_finished_io.  It enables the throttling code to 
	 * help keep the sectors in flight to a reasonable value
	 */
	int			can_throttle:1;

	unsigned long		bounce_pfn;

	/*
	 * Is meant to protect the queue in the future instead of
	 * io_request_lock
	 */
	spinlock_t		queue_lock;

	/*
	 * Tasks wait here for free read and write requests
	 */
	wait_queue_head_t	wait_for_requests;
};

#define blk_queue_plugged(q)	(q)->plugged
#define blk_fs_request(rq)	((rq)->cmd == READ || (rq)->cmd == WRITE)
#define blk_queue_empty(q)	list_empty(&(q)->queue_head)

extern inline int rq_data_dir(struct request *rq)
{
	if (rq->cmd == READ)
		return READ;
	else if (rq->cmd == WRITE)
		return WRITE;
	else {
		BUG();
		return -1; /* ahem */
	}
}

extern unsigned long blk_max_low_pfn, blk_max_pfn;

#define BLK_BOUNCE_HIGH		((u64)blk_max_low_pfn << PAGE_SHIFT)
#define BLK_BOUNCE_ANY		((u64)blk_max_pfn << PAGE_SHIFT)

extern void blk_queue_bounce_limit(request_queue_t *, u64);

#ifdef CONFIG_HIGHMEM
extern struct buffer_head *create_bounce(int, struct buffer_head *);
extern inline struct buffer_head *blk_queue_bounce(request_queue_t *q, int rw,
						   struct buffer_head *bh)
{
	struct page *page = bh->b_page;

#ifndef CONFIG_DISCONTIGMEM
	if (page - mem_map <= q->bounce_pfn)
#else
	if ((page - page_zone(page)->zone_mem_map) + (page_zone(page)->zone_start_paddr >> PAGE_SHIFT) <= q->bounce_pfn)
#endif
		return bh;

	return create_bounce(rw, bh);
}
#else
#define blk_queue_bounce(q, rw, bh)	(bh)
#endif

#define bh_phys(bh)		(page_to_phys((bh)->b_page) + bh_offset((bh)))

#define BH_CONTIG(b1, b2)	(bh_phys((b1)) + (b1)->b_size == bh_phys((b2)))
#define BH_PHYS_4G(b1, b2)	((bh_phys((b1)) | 0xffffffff) == ((bh_phys((b2)) + (b2)->b_size - 1) | 0xffffffff))

struct blk_dev_struct {
	/*
	 * queue_proc has to be atomic
	 */
	request_queue_t		request_queue;
	queue_proc		*queue;
	void			*data;
};

struct sec_size {
	unsigned block_size;
	unsigned block_size_bits;
};

/*
 * Used to indicate the default queue for drivers that don't bother
 * to implement multiple queues.  We have this access macro here
 * so as to eliminate the need for each and every block device
 * driver to know about the internal structure of blk_dev[].
 */
#define BLK_DEFAULT_QUEUE(_MAJOR)  &blk_dev[_MAJOR].request_queue

extern struct sec_size * blk_sec[MAX_BLKDEV];
extern struct blk_dev_struct blk_dev[MAX_BLKDEV];
extern void grok_partitions(struct gendisk *dev, int drive, unsigned minors, long size);
extern void register_disk(struct gendisk *dev, kdev_t first, unsigned minors, struct block_device_operations *ops, long size);
extern void generic_make_request(int rw, struct buffer_head * bh);
extern inline request_queue_t *blk_get_queue(kdev_t dev);
extern void blkdev_release_request(struct request *);

/*
 * Access functions for manipulating queue properties
 */
extern int blk_grow_request_list(request_queue_t *q, int nr_requests, int max_queue_sectors);
extern void blk_init_queue(request_queue_t *, request_fn_proc *);
extern void blk_cleanup_queue(request_queue_t *);
extern void blk_queue_headactive(request_queue_t *, int);
extern void blk_queue_throttle_sectors(request_queue_t *, int);
extern void blk_queue_make_request(request_queue_t *, make_request_fn *);
extern void generic_unplug_device(void *);
extern inline int blk_seg_merge_ok(struct buffer_head *, struct buffer_head *);

extern int * blk_size[MAX_BLKDEV];

extern int * blksize_size[MAX_BLKDEV];

extern int * hardsect_size[MAX_BLKDEV];

extern int * max_readahead[MAX_BLKDEV];

extern int * max_sectors[MAX_BLKDEV];

extern int * max_segments[MAX_BLKDEV];

#define MAX_SEGMENTS 128
#define MAX_SECTORS 255
#define MAX_QUEUE_SECTORS (4 << (20 - 9)) /* 4 mbytes when full sized */
#define MAX_NR_REQUESTS 1024 /* 1024k when in 512 units, normally min is 1M in 1k units */

#define PageAlignSize(size) (((size) + PAGE_SIZE -1) & PAGE_MASK)

#define blkdev_entry_to_request(entry) list_entry((entry), struct request, queue)
#define blkdev_entry_next_request(entry) blkdev_entry_to_request((entry)->next)
#define blkdev_entry_prev_request(entry) blkdev_entry_to_request((entry)->prev)
#define blkdev_next_request(req) blkdev_entry_to_request((req)->queue.next)
#define blkdev_prev_request(req) blkdev_entry_to_request((req)->queue.prev)

extern void drive_stat_acct (kdev_t dev, int rw,
					unsigned long nr_sectors, int new_io);

static inline int get_hardsect_size(kdev_t dev)
{
	int retval = 512;
	int major = MAJOR(dev);

	if (hardsect_size[major]) {
		int minor = MINOR(dev);
		if (hardsect_size[major][minor])
			retval = hardsect_size[major][minor];
	}
	return retval;
}

static inline int blk_oversized_queue(request_queue_t * q)
{
	if (q->can_throttle)
		return atomic_read(&q->nr_sectors) > q->max_queue_sectors;
	return q->rq.count == 0;
}

static inline int blk_oversized_queue_reads(request_queue_t * q)
{
	if (q->can_throttle)
		return atomic_read(&q->nr_sectors) > q->max_queue_sectors + q->batch_sectors;
	return q->rq.count == 0;
}

static inline int blk_oversized_queue_batch(request_queue_t * q)
{
	return atomic_read(&q->nr_sectors) > q->max_queue_sectors - q->batch_sectors;
}

#define blk_finished_io(nsects)	do { } while (0)
#define blk_started_io(nsects)	do { } while (0)

static inline void blk_started_sectors(struct request *rq, int count)
{
	request_queue_t *q = rq->q;
	if (q && q->can_throttle) {
		atomic_add(count, &q->nr_sectors);
		if (atomic_read(&q->nr_sectors) < 0) {
			printk("nr_sectors is %d\n", atomic_read(&q->nr_sectors));
			BUG();
		}
	}
}

static inline void blk_finished_sectors(struct request *rq, int count)
{
	request_queue_t *q = rq->q;
	if (q && q->can_throttle) {
		atomic_sub(count, &q->nr_sectors);
		
		smp_mb();
		if (q->rq.count >= q->batch_requests && !blk_oversized_queue_batch(q)) {
			if (waitqueue_active(&q->wait_for_requests))
				wake_up(&q->wait_for_requests);
		}
		if (atomic_read(&q->nr_sectors) < 0) {
			printk("nr_sectors is %d\n", atomic_read(&q->nr_sectors));
			BUG();
		}
	}
}

static inline unsigned int blksize_bits(unsigned int size)
{
	unsigned int bits = 8;
	do {
		bits++;
		size >>= 1;
	} while (size > 256);
	return bits;
}

static inline unsigned int block_size(kdev_t dev)
{
	int retval = BLOCK_SIZE;
	int major = MAJOR(dev);

	if (blksize_size[major]) {
		int minor = MINOR(dev);
		if (blksize_size[major][minor])
			retval = blksize_size[major][minor];
	}
	return retval;
}

#endif
