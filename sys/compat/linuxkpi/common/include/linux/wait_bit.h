/*-
 * Copyright (c) 2020 The FreeBSD Foundation
 *
 * This software was developed by Emmanuel Vadot under sponsorship
 * from the FreeBSD Foundation.
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
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#ifndef __LINUXKPI_LINUX_WAITBIT_H__
#define	__LINUXKPI_LINUX_WAITBIT_H__

#include <linux/wait.h>
#include <linux/bitops.h>

extern wait_queue_head_t linux_bit_waitq;
extern wait_queue_head_t linux_var_waitq;

#define	wait_var_event_killable(var, cond) \
	wait_event_killable(linux_var_waitq, cond)

#define	wait_var_event_interruptible(var, cond) \
	wait_event_interruptible(linux_var_waitq, cond)

static inline void
clear_and_wake_up_bit(int bit, void *word)
{
	clear_bit_unlock(bit, word);
	wake_up_bit(word, bit);
}

static inline wait_queue_head_t *
bit_waitqueue(void *word, int bit)
{

	return (&linux_bit_waitq);
}

static inline void
wake_up_var(void *var)
{

	wake_up(&linux_var_waitq);
}

static inline wait_queue_head_t *
__var_waitqueue(void *p)
{
	return (&linux_var_waitq);
}

#endif	/* __LINUXKPI_LINUX_WAITBIT_H__ */
