/*-
 * Copyright (c) 2006 John Baldwin <jhb@FreeBSD.org>
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
 */

/*
 * This module holds the global variables and functions used to maintain
 * lock_object structures.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_ddb.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/ktr.h>
#include <sys/linker_set.h>
#include <sys/lock.h>

#ifdef DDB
#include <ddb/ddb.h>
#endif

void
lock_init(struct lock_object *lock, struct lock_class *class, const char *name,
    const char *type, int flags)
{

	/* Check for double-init and zero object. */
	KASSERT(!lock_initalized(lock), ("lock \"%s\" %p already initialized",
	    name, lock));

	/* Initialize the lock object. */
	lock->lo_class = class;
	lock->lo_name = name;
	lock->lo_type = type != NULL ? type : name;
	lock->lo_flags |= flags | LO_INITIALIZED;
	LOCK_LOG_INIT(lock, 0);
	WITNESS_INIT(lock);
}

void
lock_destroy(struct lock_object *lock)
{

	KASSERT(lock_initalized(lock), ("lock %p is not initialized", lock));
	WITNESS_DESTROY(lock);
	LOCK_LOG_DESTROY(lock, 0);
	lock->lo_flags &= ~LO_INITIALIZED;
}

#ifdef DDB
DB_SHOW_COMMAND(lock, db_show_lock)
{
	struct lock_object *lock;
	struct lock_class *class;

	if (!have_addr)
		return;
	lock = (struct lock_object *)addr;
	if (lock->lo_class != &lock_class_mtx_sleep &&
	    lock->lo_class != &lock_class_mtx_spin &&
	    lock->lo_class != &lock_class_sx) {
		db_printf("Unknown lock class\n");
		return;
	}
	class = LOCK_CLASS(lock);
	db_printf(" class: %s\n", class->lc_name);
	db_printf(" name: %s\n", lock->lo_name);
	if (lock->lo_type && lock->lo_type != lock->lo_name)
		db_printf(" type: %s\n", lock->lo_type);
	class->lc_ddb_show(lock);
}
#endif
