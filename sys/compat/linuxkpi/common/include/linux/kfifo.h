/*-
 * Copyright (c) 2021 Beckhoff Automation GmbH & Co. KG
 * Copyright (c) 2022 Bjoern A. Zeeb
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
 */

#ifndef _LINUXKPI_LINUX_KFIFO_H_
#define	_LINUXKPI_LINUX_KFIFO_H_

#include <sys/types.h>

#include <linux/slab.h>
#include <linux/gfp.h>

#define	INIT_KFIFO(x)	0
#define	DECLARE_KFIFO(x, y, z)

#define	DECLARE_KFIFO_PTR(_name, _type)					\
	struct kfifo_ ## _name {					\
		size_t		total;					\
		size_t		count;					\
		size_t		first;					\
		size_t		last;					\
		_type		*head;					\
	} _name

#define	kfifo_len(_kf)							\
({									\
	(_kf)->count;							\
})

#define	kfifo_is_empty(_kf)						\
({									\
	((_kf)->count == 0) ? true : false;				\
})

#define	kfifo_is_full(_kf)						\
({									\
	((_kf)->count == (_kf)->total) ? true : false;			\
})

#define	kfifo_put(_kf, _e)						\
({									\
	bool _rc;							\
									\
	/* Would overflow. */						\
	if (kfifo_is_full(_kf)) {					\
		_rc = false;						\
	} else {							\
		(_kf)->head[(_kf)->last] = (_e);			\
		(_kf)->count++;						\
		(_kf)->last++;						\
		if ((_kf)->last > (_kf)->total)				\
			(_kf)->last = 0;				\
		_rc = true;						\
	}								\
									\
	_rc;								\
})

#define	kfifo_get(_kf, _e)						\
({									\
	bool _rc;							\
									\
	if (kfifo_is_empty(_kf)) {					\
		_rc = false;						\
	} else {							\
		*(_e) = (_kf)->head[(_kf)->first];			\
		(_kf)->count--;						\
		(_kf)->first++;						\
		if ((_kf)->first > (_kf)->total)			\
			(_kf)->first = 0;				\
		_rc = true;						\
	}								\
									\
	_rc;								\
})

#define	kfifo_alloc(_kf, _s, _gfp)					\
({									\
	int _error;							\
									\
	(_kf)->head = kmalloc(sizeof(__typeof(*(_kf)->head)) * (_s), _gfp); \
	if ((_kf)->head == NULL)					\
		_error = ENOMEM;					\
	else {								\
		(_kf)->total = (_s);					\
		_error = 0;						\
	}								\
									\
	_error;								\
})

#define	kfifo_free(_kf)							\
({									\
	kfree((_kf)->head);						\
	(_kf)->head = NULL;						\
	(_kf)->total = (_kf)->count = (_kf)->first = (_kf)->last = 0;	\
})

#endif	/* _LINUXKPI_LINUX_KFIFO_H_*/
