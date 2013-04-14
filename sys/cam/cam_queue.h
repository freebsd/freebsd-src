/*-
 * CAM request queue management definitions.
 *
 * Copyright (c) 1997 Justin T. Gibbs.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions, and the following disclaimer,
 *    without modification, immediately at the beginning of the file.
 * 2. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#ifndef _CAM_CAM_QUEUE_H
#define _CAM_CAM_QUEUE_H 1

#ifdef _KERNEL

#include <sys/queue.h>
#include <cam/cam.h>

/*
 * This structure implements a heap based priority queue.  The queue
 * assumes that the objects stored in it begin with a cam_qentry
 * structure holding the priority information used to sort the objects.
 * This structure is opaque to clients (outside of the XPT layer) to allow
 * the implementation to change without affecting them.
 */
struct camq {
	cam_pinfo **queue_array;
	int	   array_size;
	int	   entries;
	u_int32_t  generation;
	u_int32_t  qfrozen_cnt;
};

TAILQ_HEAD(ccb_hdr_tailq, ccb_hdr);
LIST_HEAD(ccb_hdr_list, ccb_hdr);
SLIST_HEAD(ccb_hdr_slist, ccb_hdr);

struct cam_ccbq {
	struct	camq queue;
	int	devq_openings;
	int	devq_allocating;
	int	dev_openings;
	int	dev_active;
	int	held;
};

struct cam_ed;

struct cam_devq {
	struct	camq send_queue;
	int	send_openings;
	int	send_active;
};


struct cam_devq *cam_devq_alloc(int devices, int openings);

int		 cam_devq_init(struct cam_devq *devq, int devices,
			       int openings);

void		 cam_devq_free(struct cam_devq *devq);

u_int32_t	 cam_devq_resize(struct cam_devq *camq, int openings);
	
/*
 * Allocate a cam_ccb_queue structure and initialize it.
 */
struct cam_ccbq	*cam_ccbq_alloc(int openings);

u_int32_t	cam_ccbq_resize(struct cam_ccbq *ccbq, int devices);

int		cam_ccbq_init(struct cam_ccbq *ccbq, int openings);

void		cam_ccbq_free(struct cam_ccbq *ccbq);

void		cam_ccbq_fini(struct cam_ccbq *ccbq);

/*
 * Allocate and initialize a cam_queue structure.
 */
struct camq	*camq_alloc(int size);

/*
 * Resize a cam queue
 */
u_int32_t	camq_resize(struct camq *queue, int new_size);

/* 
 * Initialize a camq structure.  Return 0 on success, 1 on failure.
 */
int		camq_init(struct camq *camq, int size);

/*
 * Free a cam_queue structure.  This should only be called if a controller
 * driver failes somehow during its attach routine or is unloaded and has
 * obtained a cam_queue structure.
 */
void		camq_free(struct camq *queue);

/*
 * Finialize any internal storage or state of a cam_queue.
 */
void		camq_fini(struct camq *queue);

/*
 * cam_queue_insert: Given a CAM queue with at least one open spot,
 * insert the new entry maintaining order.
 */
void		camq_insert(struct camq *queue, cam_pinfo *new_entry);

/*
 * camq_remove: Remove and arbitrary entry from the queue maintaining
 * queue order.
 */
cam_pinfo	*camq_remove(struct camq *queue, int index);
#define CAMQ_HEAD 1	/* Head of queue index */

/* Index the first element in the heap */
#define CAMQ_GET_HEAD(camq) ((camq)->queue_array[CAMQ_HEAD])

/* Get the first element priority. */
#define CAMQ_GET_PRIO(camq) (((camq)->entries > 0) ?			\
			    ((camq)->queue_array[CAMQ_HEAD]->priority) : 0)

/*
 * camq_change_priority: Raise or lower the priority of an entry
 * maintaining queue order.
 */
void		camq_change_priority(struct camq *queue, int index,
				     u_int32_t new_priority);

static __inline int
cam_ccbq_pending_ccb_count(struct cam_ccbq *ccbq);

static __inline void
cam_ccbq_take_opening(struct cam_ccbq *ccbq);

static __inline int
cam_ccbq_insert_ccb(struct cam_ccbq *ccbq, union ccb *new_ccb);

static __inline int
cam_ccbq_remove_ccb(struct cam_ccbq *ccbq, union ccb *ccb);

static __inline union ccb *
cam_ccbq_peek_ccb(struct cam_ccbq *ccbq, int index);

static __inline void
cam_ccbq_send_ccb(struct cam_ccbq *queue, union ccb *send_ccb);

static __inline void
cam_ccbq_ccb_done(struct cam_ccbq *ccbq, union ccb *done_ccb);

static __inline void
cam_ccbq_release_opening(struct cam_ccbq *ccbq);


static __inline int
cam_ccbq_pending_ccb_count(struct cam_ccbq *ccbq)
{
	return (ccbq->queue.entries);
}

static __inline void
cam_ccbq_take_opening(struct cam_ccbq *ccbq)
{
	ccbq->devq_openings--;
	ccbq->held++;
}

static __inline int
cam_ccbq_insert_ccb(struct cam_ccbq *ccbq, union ccb *new_ccb)
{
	ccbq->held--;
	camq_insert(&ccbq->queue, &new_ccb->ccb_h.pinfo);
	if (ccbq->queue.qfrozen_cnt > 0) {
		ccbq->devq_openings++;
		ccbq->held++;
		return (1);
	} else
		return (0);
}

static __inline int
cam_ccbq_remove_ccb(struct cam_ccbq *ccbq, union ccb *ccb)
{
	camq_remove(&ccbq->queue, ccb->ccb_h.pinfo.index);
	if (ccbq->queue.qfrozen_cnt > 0) {
		ccbq->devq_openings--;
		ccbq->held--;
		return (1);
	} else
		return (0);
}

static __inline union ccb *
cam_ccbq_peek_ccb(struct cam_ccbq *ccbq, int index)
{
	return((union ccb *)ccbq->queue.queue_array[index]);
}

static __inline void
cam_ccbq_send_ccb(struct cam_ccbq *ccbq, union ccb *send_ccb)
{

	send_ccb->ccb_h.pinfo.index = CAM_ACTIVE_INDEX;
	ccbq->dev_active++;
	ccbq->dev_openings--;		
}

static __inline void
cam_ccbq_ccb_done(struct cam_ccbq *ccbq, union ccb *done_ccb)
{

	ccbq->dev_active--;
	ccbq->dev_openings++;	
	ccbq->held++;
}

static __inline void
cam_ccbq_release_opening(struct cam_ccbq *ccbq)
{
	ccbq->held--;
	ccbq->devq_openings++;
}

#endif /* _KERNEL */
#endif  /* _CAM_CAM_QUEUE_H */
