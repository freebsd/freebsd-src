/* 
 * Copyright (c) 1991 Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * The Mach Operating System project at Carnegie-Mellon University.
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
 *	@(#)queue.h	7.3 (Berkeley) 4/21/91
 *	$Id: queue.h,v 1.2 1993/10/16 16:20:18 rgrimes Exp $
 */

/*
 * Copyright (c) 1987, 1990 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Author: Avadis Tevanian, Jr.
 * 
 * Permission to use, copy, modify and distribute this software and
 * its documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 * 
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS" 
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND 
 * FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 * 
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 */

/*
 *	Type definitions for generic queues.
 */

#ifndef	_QUEUE_H_
#define	_QUEUE_H_

struct queue_entry {
	struct queue_entry	*next;		/* next element */
	struct queue_entry	*prev;		/* previous element */
};

typedef struct queue_entry	*queue_t;
typedef	struct queue_entry	queue_head_t;
typedef	struct queue_entry	queue_chain_t;
typedef	struct queue_entry	*queue_entry_t;

#define round_queue(size)	(((size)+7) & (~7))

#define enqueue(queue,elt)	enqueue_tail(queue, elt)
#define	dequeue(queue)		dequeue_head(queue)

#define	enqueue_head(queue,elt)	insque(elt,queue)
#define	enqueue_tail(queue,elt)	insque(elt,(queue)->prev)
#define	remqueue(queue,elt)	remque(elt)

#define	queue_init(q)		((q)->next = (q)->prev = q)
#define	queue_first(q)		((q)->next)
#define	queue_next(qc)		((qc)->next)
#define	queue_end(q, qe)	((q) == (qe))
#define	queue_empty(q)		queue_end((q), queue_first(q))

#define queue_enter(head, elt, type, field) {			\
	if (queue_empty((head))) {				\
		(head)->next = (queue_entry_t) elt;		\
		(head)->prev = (queue_entry_t) elt;		\
		(elt)->field.next = head;			\
		(elt)->field.prev = head;			\
	} else {						\
		register queue_entry_t prev = (head)->prev;	\
		(elt)->field.prev = prev;			\
		(elt)->field.next = head;			\
		(head)->prev = (queue_entry_t)(elt);		\
		((type)prev)->field.next = (queue_entry_t)(elt);\
	}							\
}

#define	queue_field(head, thing, type, field)			\
		(((head) == (thing)) ? (head) : &((type)(thing))->field)

#define	queue_remove(head, elt, type, field) {			\
	register queue_entry_t next = (elt)->field.next;	\
	register queue_entry_t prev = (elt)->field.prev;	\
	queue_field((head), next, type, field)->prev = prev;	\
	queue_field((head), prev, type, field)->next = next;	\
}

#define	queue_assign(to, from, type, field) {			\
	((type)((from)->prev))->field.next = (to);		\
	((type)((from)->next))->field.prev = (to);		\
	*to = *from;						\
}

#define	queue_remove_first(h, e, t, f) {			\
	e = (t) queue_first((h));				\
	queue_remove((h), (e), t, f);				\
}

#endif	/* !_QUEUE_H_ */
