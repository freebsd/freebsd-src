/*
 *
 * ===================================
 * HARP  |  Host ATM Research Platform
 * ===================================
 *
 *
 * This Host ATM Research Platform ("HARP") file (the "Software") is
 * made available by Network Computing Services, Inc. ("NetworkCS")
 * "AS IS".  NetworkCS does not provide maintenance, improvements or
 * support of any kind.
 *
 * NETWORKCS MAKES NO WARRANTIES OR REPRESENTATIONS, EXPRESS OR IMPLIED,
 * INCLUDING, BUT NOT LIMITED TO, IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE, AS TO ANY ELEMENT OF THE
 * SOFTWARE OR ANY SUPPORT PROVIDED IN CONNECTION WITH THIS SOFTWARE.
 * In no event shall NetworkCS be responsible for any damages, including
 * but not limited to consequential damages, arising from or relating to
 * any use of the Software or related support.
 *
 * Copyright 1994-1998 Network Computing Services, Inc.
 *
 * Copies of this Software may be made, however, the above copyright
 * notice must be reproduced on all copies.
 *
 *	@(#) $FreeBSD: src/sys/netatm/queue.h,v 1.2 1999/08/28 00:48:41 peter Exp $
 *
 */

/*
 * Core ATM Services
 * -----------------
 *
 * General queueing/linking definitions
 *
 */

#ifndef _NETATM_QUEUE_H
#define _NETATM_QUEUE_H

/*
 * Structure defining the queue controls for a doubly linked queue
 */
struct q_queue {
	caddr_t		q_head;		/* Head of queue */
	caddr_t		q_tail;		/* Tail of queue */
};
typedef struct q_queue Queue_t;

/*
 * Structure defining the queue elements of a doubly linked queue
 */
struct q_elem {
	caddr_t		q_forw;		/* Forward link */
	caddr_t		q_back;		/* Backward link */
};
typedef struct q_elem Qelem_t;

/*
 * Macro to add a control block onto the tail of a doubly linked queue
 *	e = control block to add
 *	t = control block structure type
 *	el = name of control block's q_elem field
 *	q = pointer to queue controls
 */
#define	ENQUEUE(e,t,el,q)					\
{								\
	(e)->el.q_forw = NULL;					\
	(e)->el.q_back = (q).q_tail;				\
	if ((q).q_head == NULL) {				\
		(q).q_head = (caddr_t)(e);			\
		(q).q_tail = (caddr_t)(e);			\
	} else {						\
		((t *)(q).q_tail)->el.q_forw = (caddr_t)(e);	\
		(q).q_tail = (caddr_t)(e);			\
	}							\
}

/*
 * Macro to remove a control block from a doubly linked queue
 *	e = control block to remove
 *	t = control block structure type
 *	el = name of control block's q_elem field
 *	q = pointer to queue controls
 */
#define	DEQUEUE(e,t,el,q)					\
{								\
	/* Ensure control block is on queue */			\
	if ((e)->el.q_forw || (q).q_tail == (caddr_t)(e)) {	\
		if ((e)->el.q_forw)				\
			((t *)(e)->el.q_forw)->el.q_back = (e)->el.q_back;\
		else						\
			(q).q_tail = (e)->el.q_back;		\
		if ((e)->el.q_back)				\
			((t *)(e)->el.q_back)->el.q_forw = (e)->el.q_forw;\
		else						\
			(q).q_head = (e)->el.q_forw;		\
	}							\
	(e)->el.q_back = (e)->el.q_forw = NULL;			\
}

/*
 * Macro to return the head of a doubly linked queue
 *	q = pointer to queue controls
 *	t = control block structure type
 */
#define	Q_HEAD(q,t)	((t *)(q).q_head)

/*
 * Macro to return the next control block of a doubly linked queue
 *	e = current control block
 *	t = control block structure type
 *	el = name of control block's q_elem field
 */
#define	Q_NEXT(e,t,el)	((t *)(e)->el.q_forw)


/*
 * Macro to add a control block onto the head of a singly linked chain
 *	u = control block to add
 *	t = structure type
 *	h = head of chain
 *	l = name of link field
 */
#define LINK2HEAD(u,t,h,l)					\
{								\
	(u)->l = (h);						\
	(h) = (u);						\
}

/*
 * Macro to add a control block onto the tail of a singly linked chain
 *	u = control block to add
 *	t = structure type
 *	h = head of chain
 *	l = name of link field
 */
#define LINK2TAIL(u,t,h,l)					\
{								\
	(u)->l = (t *)NULL;					\
	/* Check for empty chain */				\
	if ((h) == (t *)NULL) {					\
		(h) = (u);					\
	} else {						\
		t	*tp;					\
		/* Loop until we find the end of chain */	\
		for (tp = (h); tp->l != (t *)NULL; tp = tp->l)	\
			;					\
		tp->l = (u);					\
	}							\
}

/*
 * Macro to remove a control block from a singly linked chain
 *	u = control block to unlink
 *	t = structure type
 *	h = head of chain
 *	l = name of link field
 */
#define UNLINK(u,t,h,l)						\
{								\
	/* Check for control block at head of chain */		\
	if ((u) == (h)) {					\
		(h) = (u)->l;					\
	} else {						\
		t	*tp;					\
		/* Loop until we find the control block */	\
		for (tp = (h); tp != (t *)NULL; tp = tp->l) {	\
			if (tp->l == (u))			\
				break;				\
		}						\
		if (tp) {					\
			/* Remove it from chain */		\
			tp->l = (u)->l;				\
		}						\
	}							\
	(u)->l = (t *)NULL;					\
}

/*
 * Macro to remove a control block from a singly linked chain and return
 * an indication of whether the block was found
 *	u = control block to unlink
 *	t = structure type
 *	h = head of chain
 *	l = name of link field
 *	f = flag; 1 => control block found on chain; else 0
 */
#define UNLINKF(u,t,h,l,f)					\
{								\
	/* Check for control block at head of chain */		\
	if ((u) == (h)) {					\
		(h) = (u)->l;					\
		(f) = 1;					\
	} else {						\
		t	*tp;					\
		/* Loop until we find the control block */	\
		for (tp = (h); tp != (t *)NULL; tp = tp->l) {	\
			if (tp->l == (u))			\
				break;				\
		}						\
		if (tp) {					\
			/* Remove it from chain */		\
			tp->l = (u)->l;				\
			(f) = 1;				\
		} else						\
			/* It wasn't on the chain */		\
			(f) = 0;				\
	}							\
	(u)->l = (t *)NULL;					\
}

#endif	/* _NETATM_QUEUE_H */
