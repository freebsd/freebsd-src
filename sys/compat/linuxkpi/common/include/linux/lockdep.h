/*-
 * Copyright (c) 2010 Isilon Systems, Inc.
 * Copyright (c) 2010 iX Systems, Inc.
 * Copyright (c) 2010 Panasas, Inc.
 * Copyright (c) 2013, 2014 Mellanox Technologies, Ltd.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#ifndef _LINUXKPI_LINUX_LOCKDEP_H_
#define	_LINUXKPI_LINUX_LOCKDEP_H_

#include <sys/types.h>
#include <sys/lock.h>

struct lock_class_key {
};
struct lockdep_map {
};
struct pin_cookie {
};

#define	lockdep_set_class(lock, key)
#define	lockdep_set_subclass(lock, sub)
#define	lockdep_set_class_and_name(lock, key, name)
#define	lockdep_set_current_reclaim_state(g) do { } while (0)
#define	lockdep_clear_current_reclaim_state() do { } while (0)
#define	lockdep_init_map(_map, _name, _key, _x) do { } while(0)
#define	lockdep_register_key(key) do { } while(0)
#define	lockdep_unregister_key(key) do { } while(0)

#ifdef INVARIANTS
#define	lockdep_assert(cond) do { WARN_ON(!cond); } while (0)
#define	lockdep_assert_once(cond) do { WARN_ON_ONCE(!cond); } while (0)

#define	lockdep_assert_not_held(m) do {					\
	struct lock_object *__lock = (struct lock_object *)(m);		\
	LOCK_CLASS(__lock)->lc_assert(__lock, LA_UNLOCKED);		\
} while (0)

#define	lockdep_assert_held(m) do {					\
	struct lock_object *__lock = (struct lock_object *)(m);		\
	LOCK_CLASS(__lock)->lc_assert(__lock, LA_LOCKED);		\
} while (0)

#define	lockdep_assert_held_once(m) do {				\
	struct lock_object *__lock = (struct lock_object *)(m);		\
	LOCK_CLASS(__lock)->lc_assert(__lock, LA_LOCKED | LA_NOTRECURSED); \
} while (0)

#define	lockdep_assert_none_held_once() do { } while (0)

static __inline bool
lockdep_is_held(void *__m)
{
	struct lock_object *__lock;
	struct thread *__td;

	__lock = __m;
	return (LOCK_CLASS(__lock)->lc_owner(__lock, &__td) != 0);
}
#define	lockdep_is_held_type(_m, _t) lockdep_is_held(_m)

#else
#define	lockdep_assert(cond) do { } while (0)
#define	lockdep_assert_once(cond) do { } while (0)

#define	lockdep_assert_not_held(m) do { (void)(m); } while (0)
#define	lockdep_assert_held(m) do { (void)(m); } while (0)
#define	lockdep_assert_none_held_once() do { } while (0)

#define	lockdep_assert_held_once(m) do { (void)(m); } while (0)

#define	lockdep_is_held(m)	1
#define	lockdep_is_held_type(_m, _t)	1
#endif

#define	might_lock(m)	do { } while (0)
#define	might_lock_read(m) do { } while (0)
#define	might_lock_nested(m, n) do { } while (0)

#define	lock_acquire(...) do { } while (0)
#define	lock_release(...) do { } while (0)
#define	lock_acquire_shared_recursive(...) do { } while (0)

#define	mutex_acquire(...) do { } while (0)
#define	mutex_release(...) do { } while (0)

#define	lockdep_pin_lock(l) ({ struct pin_cookie __pc = { }; __pc; })
#define	lockdep_repin_lock(l,c) do { (void)(l); (void)(c); } while (0)
#define	lockdep_unpin_lock(l,c) do { (void)(l); (void)(c); } while (0)

#define	lock_map_acquire(_map) do { } while (0)
#define	lock_map_acquire_read(_map) do { } while (0)
#define	lock_map_release(_map) do { } while (0)

#endif /* _LINUXKPI_LINUX_LOCKDEP_H_ */
