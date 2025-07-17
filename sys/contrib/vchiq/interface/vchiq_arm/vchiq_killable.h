/**
 * Copyright (c) 2010-2012 Broadcom. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions, and the following disclaimer,
 *    without modification.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The names of the above-listed copyright holders may not be used
 *    to endorse or promote products derived from this software without
 *    specific prior written permission.
 *
 * ALTERNATIVELY, this software may be distributed under the terms of the
 * GNU General Public License ("GPL") version 2, as published by the Free
 * Software Foundation.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
 * IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef VCHIQ_KILLABLE_H
#define VCHIQ_KILLABLE_H

#ifdef notyet
#include <linux/mutex.h>
#include <linux/semaphore.h>

#define SHUTDOWN_SIGS   (sigmask(SIGKILL) | sigmask(SIGINT) | sigmask(SIGQUIT) | sigmask(SIGTRAP) | sigmask(SIGSTOP) | sigmask(SIGCONT))

static inline int __must_check down_interruptible_killable(struct semaphore *sem)
{
	/* Allow interception of killable signals only. We don't want to be interrupted by harmless signals like SIGALRM */
	int ret;
	sigset_t blocked, oldset;
	siginitsetinv(&blocked, SHUTDOWN_SIGS);
	sigprocmask(SIG_SETMASK, &blocked, &oldset);
	ret = down_interruptible(sem);
	sigprocmask(SIG_SETMASK, &oldset, NULL);
	return ret;
}
#define down_interruptible down_interruptible_killable


static inline int __must_check mutex_lock_interruptible_killable(struct mutex *lock)
{
	/* Allow interception of killable signals only. We don't want to be interrupted by harmless signals like SIGALRM */
	int ret;
	sigset_t blocked, oldset;
	siginitsetinv(&blocked, SHUTDOWN_SIGS);
	sigprocmask(SIG_SETMASK, &blocked, &oldset);
	ret = mutex_lock_interruptible(lock);
	sigprocmask(SIG_SETMASK, &oldset, NULL);
	return ret;
}
#define mutex_lock_interruptible mutex_lock_interruptible_killable

#endif

#endif
