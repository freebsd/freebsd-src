/*-
 * Copyright (c) 2004 John Baldwin <jhb@FreeBSD.org>
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
 * 3. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
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
 *
 * $FreeBSD: src/sys/sys/sleepqueue.h,v 1.3.2.2 2005/01/31 23:26:57 imp Exp $
 */

#ifndef _SYS_SLEEPQUEUE_H_
#define _SYS_SLEEPQUEUE_H_

/*
 * Sleep queue interface.  Sleep/wakeup and condition variables use a sleep
 * queue for the queue of threads blocked on a sleep channel.
 *
 * A thread calls sleepq_lookup() to look up the proper sleep queue in the
 * hash table that is associated with a specified wait channel.  This
 * function returns a pointer to the queue and locks the associated sleep
 * queue chain.  A thread calls sleepq_add() to add themself onto a sleep
 * queue and calls one of the sleepq_wait() functions to actually go to
 * sleep.  If a thread needs to abort a sleep operation it should call
 * sleepq_release() to unlock the associated sleep queue chain lock.  If
 * the thread also needs to remove itself from a queue it just enqueued
 * itself on, it can use sleepq_remove().
 *
 * If the thread only wishes to sleep for a limited amount of time, it can
 * call sleepq_set_timeout() after sleepq_add() to setup a timeout.  It
 * should then use one of the sleepq_timedwait() functions to block.
 *
 * If the thread wants to the sleep to be interruptible by signals, it can
 * call sleepq_catch_signals() after sleepq_add().  It should then use
 * one of the sleepq_wait_sig() functions to block.  After the thread has
 * been resumed, it should call sleepq_calc_signal_retval() to determine
 * if it should return EINTR or ERESTART passing in the value returned from
 * the earlier call to sleepq_catch_signals().
 *
 * A thread is normally resumed from a sleep queue by either the
 * sleepq_signal() or sleepq_broadcast() functions.  Sleepq_signal() wakes
 * the thread with the highest priority that is sleeping on the specified
 * wait channel.  Sleepq_broadcast() wakes all threads that are sleeping
 * on the specified wait channel.  A thread sleeping in an interruptible
 * sleep can be interrupted by calling sleepq_abort().  A thread can also
 * be removed from a specified sleep queue using the sleepq_remove()
 * function.
 *
 * Each thread allocates a sleep queue at thread creation via sleepq_alloc()
 * and releases it at thread destruction via sleepq_free().  Note that
 * a sleep queue is not tied to a specific thread and that the sleep queue
 * released at thread destruction may not be the same sleep queue that the
 * thread allocated when it was created.
 *
 * XXX: Some other parts of the kernel such as ithread sleeping may end up
 * using this interface as well (death to TDI_IWAIT!)
 */

struct mtx;
struct sleepqueue;
struct thread;

#ifdef _KERNEL

#define	SLEEPQ_TYPE		0x0ff		/* Mask of sleep queue types. */
#define	SLEEPQ_MSLEEP		0x00		/* Used by msleep/wakeup. */
#define	SLEEPQ_CONDVAR		0x01		/* Used for a cv. */
#define	SLEEPQ_INTERRUPTIBLE	0x100		/* Sleep is interruptible. */

void	init_sleepqueues(void);
void	sleepq_abort(struct thread *td);
void	sleepq_add(struct sleepqueue *, void *, struct mtx *, const char *,
	    int);
struct sleepqueue *sleepq_alloc(void);
void	sleepq_broadcast(void *, int, int);
int	sleepq_calc_signal_retval(int sig);
int	sleepq_catch_signals(void *wchan);
void	sleepq_free(struct sleepqueue *);
struct sleepqueue *sleepq_lookup(void *);
void	sleepq_release(void *);
void	sleepq_remove(struct thread *, void *);
void	sleepq_signal(void *, int, int);
void	sleepq_set_timeout(void *wchan, int timo);
int	sleepq_timedwait(void *wchan);
int	sleepq_timedwait_sig(void *wchan, int signal_caught);
void	sleepq_wait(void *);
int	sleepq_wait_sig(void *wchan);

#endif	/* _KERNEL */
#endif	/* !_SYS_SLEEPQUEUE_H_ */
