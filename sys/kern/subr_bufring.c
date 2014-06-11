/*-
 * Copyright (c) 2007, 2008 Kip Macy <kmacy@freebsd.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");


#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/ktr.h>
#include <sys/buf_ring.h>
#include <sys/mbuf.h>


struct buf_ring *
buf_ring_alloc(int count, struct malloc_type *type, int flags, struct mtx *lock)
{
	struct buf_ring *br;

	KASSERT(powerof2(count), ("buf ring must be size power of 2"));
	
	br = malloc(sizeof(struct buf_ring) + count*sizeof(caddr_t),
	    type, flags|M_ZERO);
	if (br == NULL)
		return (NULL);
#ifdef DEBUG_BUFRING
	br->br_lock = lock;
#endif	
	br->br_prod_size = br->br_cons_size = count;
	br->br_prod_mask = br->br_cons_mask = count-1;
	br->br_prod_head = br->br_cons_head = 0;
	br->br_prod_tail = br->br_cons_tail = 0;
		
	return (br);
}

void
buf_ring_free(struct buf_ring *br, struct malloc_type *type)
{
	free(br, type);
}

/*
 * multi-producer safe lock-free ring buffer enqueue
 *
 */
extern uint32_t panic_on_dup_buf;

int
buf_ring_mbufon(struct buf_ring *br, void *buf)
{
	int i;
	/* We don't count what the driver is peeking at */
	for (i = br->br_cons_head; i != br->br_prod_head;
	     i = ((i + 1) & br->br_cons_mask)) {
		if(br->br_ring[i] == buf) {
			return(1);
		}
	}
	return(0);
}

__attribute__((noinline))
int
buf_ring_enqueue(struct buf_ring *br, void *buf)
{
	uint32_t prod_head, prod_next;
	uint32_t cons_tail;
#ifdef DEBUG_BUFRING
	int i;
	critical_enter();
	mb();
	for (i = br->br_cons_head; i != br->br_prod_head;
	     i = ((i + 1) & br->br_cons_mask))
		if(br->br_ring[i] == buf) {
			if (panic_on_dup_buf)
				panic("help br:%p buf:%p", br, buf);
			critical_exit();
			return(0);
		}
#else
	critical_enter();
#endif	
	do {
		prod_head = br->br_prod_head;
		cons_tail = br->br_cons_tail;

		prod_next = (prod_head + 1) & br->br_prod_mask;
		
		if (prod_next == cons_tail) {
			br->br_drops++;
			critical_exit();
			return (ENOBUFS);
		}
	} while (!atomic_cmpset_int(&br->br_prod_head, prod_head, prod_next));
#ifdef DEBUG_BUFRING
	if (br->br_ring[prod_head] != NULL) {
		printf("Dangling value in enqueue %d br:%p\n", 
		       prod_head, br);
	}
#endif	
	br->br_ring[prod_head] = buf;

	/*
	 * The full memory barrier also avoids that br_prod_tail store
	 * is reordered before the br_ring[prod_head] is full setup.
	 */
	mb();

	/*
	 * If there are other enqueues in progress
	 * that preceeded us, we need to wait for them
	 * to complete 
	 */   
	while (br->br_prod_tail != prod_head)
		cpu_spinwait();
	br->br_prod_tail = prod_next;
	critical_exit();
	return (0);
}

/*
 * multi-consumer safe dequeue 
 *
 */
void *
buf_ring_dequeue_mc(struct buf_ring *br)
{
	uint32_t cons_head, cons_next;
	uint32_t prod_tail;
	void *buf;
	int success;

	critical_enter();
	do {
		cons_head = br->br_cons_head;
		prod_tail = br->br_prod_tail;

		cons_next = (cons_head + 1) & br->br_cons_mask;
		
		if (cons_head == prod_tail) {
			critical_exit();
			return (NULL);
		}
		
		success = atomic_cmpset_int(&br->br_cons_head, cons_head,
		    cons_next);
	} while (success == 0);		

	buf = br->br_ring[cons_head];
#ifdef DEBUG_BUFRING
	br->br_ring[cons_head] = NULL;
#endif
	/*
	 * The full memory barrier also avoids that br_ring[cons_read]
	 * load is reordered after br_cons_tail is set.
	 */
	mb();
	
	/*
	 * If there are other dequeues in progress
	 * that preceeded us, we need to wait for them
	 * to complete 
	 */   
	while (br->br_cons_tail != cons_head)
		cpu_spinwait();

	br->br_cons_tail = cons_next;
	critical_exit();

	return (buf);
}

/*
 * single-consumer dequeue 
 * use where dequeue is protected by a lock
 * e.g. a network driver's tx queue lock
 */
void *
buf_ring_dequeue_sc(struct buf_ring *br)
{
	uint32_t cons_head, cons_next;
#ifdef PREFETCH_DEFINED
	uint32_t  cons_next_next;
#endif
	uint32_t prod_tail;
	void *buf;
	
	cons_head = br->br_cons_head;
	prod_tail = br->br_prod_tail;
	
	cons_next = (cons_head + 1) & br->br_cons_mask;
#ifdef PREFETCH_DEFINED
	cons_next_next = (cons_head + 2) & br->br_cons_mask;
#endif
	
	if (cons_head == prod_tail) 
		return (NULL);

#ifdef PREFETCH_DEFINED	
	if (cons_next != prod_tail) {		
		prefetch(br->br_ring[cons_next]);
		if (cons_next_next != prod_tail) 
			prefetch(br->br_ring[cons_next_next]);
	}
#endif
	br->br_cons_head = cons_next;
	buf = br->br_ring[cons_head];

#ifdef DEBUG_BUFRING
	br->br_ring[cons_head] = NULL;
	if (!mtx_owned(br->br_lock))
		panic("lock not held on single consumer dequeue");
	if (br->br_cons_tail != cons_head)
		panic("inconsistent list cons_tail=%d cons_head=%d",
		    br->br_cons_tail, cons_head);
#endif
	br->br_cons_tail = cons_next;
	return (buf);
}

/*
 * single-consumer advance after a peek
 * use where it is protected by a lock
 * e.g. a network driver's tx queue lock
 */
void
buf_ring_advance_sc(struct buf_ring *br)
{
	uint32_t cons_head, cons_next;
	uint32_t prod_tail;
	
	cons_head = br->br_cons_head;
	prod_tail = br->br_prod_tail;
	
	cons_next = (cons_head + 1) & br->br_cons_mask;
	if (cons_head == prod_tail) 
		return;
	br->br_cons_head = cons_next;
	br->br_cons_tail = cons_next;
}

void
buf_ring_advance_mc(struct buf_ring *br)
{
	uint32_t cons_head, cons_next;
	uint32_t prod_tail;
	int success;

	critical_enter();
	do {
		cons_head = br->br_cons_head;
		prod_tail = br->br_prod_tail;

		cons_next = (cons_head + 1) & br->br_cons_mask;
		
		if (cons_head == prod_tail) {
			critical_exit();
			return;
		}
		
		success = atomic_cmpset_int(&br->br_cons_head, cons_head,
		    cons_next);
	} while (success == 0);		
	/*
	 * The full memory barrier also avoids that br_ring[cons_read]
	 * load is reordered after br_cons_tail is set.
	 */
	mb();
	
	/*
	 * If there are other dequeues in progress
	 * that preceeded us, we need to wait for them
	 * to complete 
	 */   
	while (br->br_cons_tail != cons_head)
		cpu_spinwait();

	br->br_cons_tail = cons_next;
	critical_exit();
}


/*
 * Used to return a buffer (most likely already there)
 * to the top od the ring. The caller should *not*
 * have used any dequeue to pull it out of the ring
 * but instead should have used the peek() function.
 * This is normally used where the transmit queue
 * of a driver is full, and an mubf must be returned.
 * Most likely whats in the ring-buffer is what
 * is being put back (since it was not removed), but
 * sometimes the lower transmit function may have
 * done a pullup or other function that will have
 * changed it. As an optimzation we always put it
 * back (since jhb says the store is probably cheaper),
 * if we have to do a multi-queue version we will need
 * the compare and an atomic.
 */
void
buf_ring_putback_mc(struct buf_ring *br, void *new)
{
	KASSERT(br->br_cons_head != br->br_prod_tail, 
		("Buf-Ring has none in putback")) ;
	critical_enter();
	br->br_ring[br->br_cons_head] = new;
	mb();
	critical_exit();
}

void
buf_ring_putback_sc(struct buf_ring *br, void *new)
{
	KASSERT(br->br_cons_head != br->br_prod_tail, 
		("Buf-Ring has none in putback")) ;
	br->br_ring[br->br_cons_head] = new;
}

/*
 * return a pointer to the first entry in the ring
 * without modifying it, or NULL if the ring is empty
 * race-prone if not protected by a lock
 */
void *
buf_ring_peek(struct buf_ring *br)
{
	struct mbuf *m;
#ifdef DEBUG_BUFRING
	if ((br->br_lock != NULL) && !mtx_owned(br->br_lock)) {
		printf("br:%p lock not held on single consumer dequeue\n",
		       br);
	}

#endif	
	if (br->br_cons_head == br->br_prod_tail)
		return (NULL);
	m = br->br_ring[br->br_cons_head];
#ifdef DEBUG_BUFRING
	br->br_ring[br->br_cons_head] = NULL;
	mb();
#endif
	return (m);
}

int
buf_ring_full(struct buf_ring *br)
{

	return (((br->br_prod_head + 1) & br->br_prod_mask) == br->br_cons_tail);
}

int
buf_ring_empty(struct buf_ring *br)
{

	return (br->br_cons_head == br->br_prod_tail);
}

int
buf_ring_count(struct buf_ring *br)
{

	return ((br->br_prod_size + br->br_prod_tail - br->br_cons_tail)
	    & br->br_prod_mask);
}
