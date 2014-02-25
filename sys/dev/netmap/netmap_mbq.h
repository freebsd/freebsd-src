/*
 * Copyright (C) 2013 Vincenzo Maffione. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *   1. Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *   2. Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer in the
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

/*
 * $FreeBSD$
 */


#ifndef __NETMAP_MBQ_H__
#define __NETMAP_MBQ_H__

/*
 * These function implement an mbuf tailq with an optional lock.
 * The base functions act ONLY ON THE QUEUE, whereas the "safe"
 * variants (mbq_safe_*) also handle the lock.
 */

/* XXX probably rely on a previous definition of SPINLOCK_T */
#ifdef linux
#define SPINLOCK_T  safe_spinlock_t
#else
#define SPINLOCK_T  struct mtx
#endif

/* A FIFO queue of mbufs with an optional lock. */
struct mbq {
    struct mbuf *head;
    struct mbuf *tail;
    int count;
    SPINLOCK_T lock;
};

/* XXX "destroy" does not match "init" as a name.
 * We should also clarify whether init can be used while
 * holding a lock, and whether mbq_safe_destroy() is a NOP.
 */
void mbq_init(struct mbq *q);
void mbq_destroy(struct mbq *q);
void mbq_enqueue(struct mbq *q, struct mbuf *m);
struct mbuf *mbq_dequeue(struct mbq *q);
void mbq_purge(struct mbq *q);

/* XXX missing mbq_lock() and mbq_unlock */

void mbq_safe_init(struct mbq *q);
void mbq_safe_destroy(struct mbq *q);
void mbq_safe_enqueue(struct mbq *q, struct mbuf *m);
struct mbuf *mbq_safe_dequeue(struct mbq *q);
void mbq_safe_purge(struct mbq *q);

static inline unsigned int mbq_len(struct mbq *q)
{
    return q->count;
}

#endif /* __NETMAP_MBQ_H_ */
