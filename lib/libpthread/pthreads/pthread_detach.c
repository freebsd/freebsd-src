/* ==== pthread_detach.c =======================================================
 * Copyright (c) 1994 by Chris Provenzano, proven@mit.edu
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *  This product includes software developed by Chris Provenzano.
 * 4. The name of Chris Provenzano may not be used to endorse or promote 
 *	  products derived from this software without specific prior written
 *	  permission.
 *
 * THIS SOFTWARE IS PROVIDED BY CHRIS PROVENZANO ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL CHRIS PROVENZANO BE LIABLE FOR ANY 
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR 
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT 
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF 
 * SUCH DAMAGE.
 *
 * Description : pthread_join function.
 *
 *  1.00 94/01/15 proven
 *      -Started coding this file.
 */

#include <pthread.h>

/* ==========================================================================
 * pthread_detach()
 */
int pthread_detach(pthread_t pthread)
{
	semaphore *plock;
	int ret;

	plock = &(pthread->lock);
	while (SEMAPHORE_TEST_AND_SET(plock)) {
		pthread_yield();
	}

	/* Check that thread isn't detached already */
	if (!(pthread->flags & PF_DETACHED)) {

		pthread->flags |= PF_DETACHED;

		/* Wakeup first threads waiting on a join */
		{
			struct pthread * next_thread;
			semaphore * next_lock;

			if (next_thread = pthread_queue_get(&(pthread->join_queue))) {
				next_lock = &(next_thread->lock);
				while (SEMAPHORE_TEST_AND_SET(next_lock)) {
					pthread_yield();
				}
				pthread_queue_deq(&(pthread->join_queue)); 
				next_thread->state = PS_RUNNING;
				/*
				 * Thread will wake up in pthread_join(), see the thread
				 * it was joined to already detached and unlock itself
				 * and pthread
				 */
			} else {
				SEMAPHORE_RESET(plock);
			}
		}
		ret = OK;

	} else {
		SEMAPHORE_RESET(plock);
		ret = ESRCH;
	}
	return(ret);
}
