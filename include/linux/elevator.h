#ifndef _LINUX_ELEVATOR_H
#define _LINUX_ELEVATOR_H

typedef void (elevator_fn) (struct request *, elevator_t *,
			    struct list_head *,
			    struct list_head *, int);

typedef int (elevator_merge_fn) (request_queue_t *, struct request **, struct list_head *,
				 struct buffer_head *, int, int);

typedef void (elevator_merge_cleanup_fn) (request_queue_t *, struct request *, int);

typedef void (elevator_merge_req_fn) (struct request *, struct request *);

struct elevator_s
{
	int read_latency;
	int write_latency;

	elevator_merge_fn *elevator_merge_fn;
	elevator_merge_req_fn *elevator_merge_req_fn;

	unsigned int queue_ID;
};

int elevator_noop_merge(request_queue_t *, struct request **, struct list_head *, struct buffer_head *, int, int);
void elevator_noop_merge_cleanup(request_queue_t *, struct request *, int);
void elevator_noop_merge_req(struct request *, struct request *);

int elevator_linus_merge(request_queue_t *, struct request **, struct list_head *, struct buffer_head *, int, int);
void elevator_linus_merge_cleanup(request_queue_t *, struct request *, int);
void elevator_linus_merge_req(struct request *, struct request *);

typedef struct blkelv_ioctl_arg_s {
	int queue_ID;
	int read_latency;
	int write_latency;
	int max_bomb_segments;
} blkelv_ioctl_arg_t;

#define BLKELVGET   _IOR(0x12,106,sizeof(blkelv_ioctl_arg_t))
#define BLKELVSET   _IOW(0x12,107,sizeof(blkelv_ioctl_arg_t))

extern int blkelvget_ioctl(elevator_t *, blkelv_ioctl_arg_t *);
extern int blkelvset_ioctl(elevator_t *, const blkelv_ioctl_arg_t *);

extern void elevator_init(elevator_t *, elevator_t);

/*
 * Return values from elevator merger
 */
#define ELEVATOR_NO_MERGE	0
#define ELEVATOR_FRONT_MERGE	1
#define ELEVATOR_BACK_MERGE	2

/*
 * This is used in the elevator algorithm.  We don't prioritise reads
 * over writes any more --- although reads are more time-critical than
 * writes, by treating them equally we increase filesystem throughput.
 * This turns out to give better overall performance.  -- sct
 */
#define IN_ORDER(s1,s2)				\
	((((s1)->rq_dev == (s2)->rq_dev &&	\
	   (s1)->sector < (s2)->sector)) ||	\
	 (s1)->rq_dev < (s2)->rq_dev)

#define BHRQ_IN_ORDER(bh, rq)			\
	((((bh)->b_rdev == (rq)->rq_dev &&	\
	   (bh)->b_rsector < (rq)->sector)) ||	\
	 (bh)->b_rdev < (rq)->rq_dev)

static inline int elevator_request_latency(elevator_t * elevator, int rw)
{
	int latency;

	latency = elevator->read_latency;
	if (rw != READ)
		latency = elevator->write_latency;

	return latency;
}

#define ELV_LINUS_SEEK_COST	1

#define ELEVATOR_NOOP							\
((elevator_t) {								\
	0,				/* read_latency */		\
	0,				/* write_latency */		\
									\
	elevator_noop_merge,		/* elevator_merge_fn */		\
	elevator_noop_merge_req,	/* elevator_merge_req_fn */	\
	})

#define ELEVATOR_LINUS							\
((elevator_t) {								\
	128,				/* read passovers */		\
	512,				/* write passovers */		\
									\
	elevator_linus_merge,		/* elevator_merge_fn */		\
	elevator_linus_merge_req,	/* elevator_merge_req_fn */	\
	})

#endif
