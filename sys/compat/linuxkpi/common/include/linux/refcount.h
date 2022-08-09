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

#ifndef _LINUXKPI_LINUX_REFCOUNT_H
#define _LINUXKPI_LINUX_REFCOUNT_H

#include <linux/atomic.h>

struct refcount_linux {
	atomic_t value;
};
typedef struct refcount_linux refcount_t;

static inline void
refcount_set(refcount_t *ref, unsigned int i)
{
	atomic_set(&ref->value, i);
}

static inline void
refcount_inc(refcount_t *ref)
{
	atomic_inc(&ref->value);
}

static inline bool
refcount_inc_not_zero(refcount_t *ref)
{
	return (atomic_inc_not_zero(&ref->value));
}

static inline void
refcount_dec(refcount_t *ref)
{
	atomic_dec(&ref->value);
}

static inline unsigned int
refcount_read(refcount_t *ref)
{
	return atomic_read(&ref->value);
}

static inline bool
refcount_dec_and_lock_irqsave(refcount_t *ref, spinlock_t *lock,
    unsigned long *flags)
{
	if (atomic_dec_and_test(&ref->value) == true) {
		spin_lock_irqsave(lock, flags);
		return (true);
	}
	return (false);
}

/*
 * struct kref uses atomic_t and not refcount_t so
 * we differ from Linux here.
 */
static inline bool
refcount_dec_and_test(atomic_t *r)
{

	return (atomic_dec_and_test(r));
}

#endif /* __LINUXKPI_LINUX_REFCOUNT_H__ */
