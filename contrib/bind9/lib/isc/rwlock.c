/*
 * Copyright (C) 2004  Internet Systems Consortium, Inc. ("ISC")
 * Copyright (C) 1998-2001, 2003  Internet Software Consortium.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND ISC DISCLAIMS ALL WARRANTIES WITH
 * REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
 * OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

/* $Id: rwlock.c,v 1.33.2.4.2.1 2004/03/06 08:14:35 marka Exp $ */

#include <config.h>

#include <stddef.h>

#include <isc/magic.h>
#include <isc/msgs.h>
#include <isc/platform.h>
#include <isc/rwlock.h>
#include <isc/util.h>

#define RWLOCK_MAGIC		ISC_MAGIC('R', 'W', 'L', 'k')
#define VALID_RWLOCK(rwl)	ISC_MAGIC_VALID(rwl, RWLOCK_MAGIC)

#ifdef ISC_PLATFORM_USETHREADS

#ifndef RWLOCK_DEFAULT_READ_QUOTA
#define RWLOCK_DEFAULT_READ_QUOTA 4
#endif

#ifndef RWLOCK_DEFAULT_WRITE_QUOTA
#define RWLOCK_DEFAULT_WRITE_QUOTA 4
#endif

#ifdef ISC_RWLOCK_TRACE
#include <stdio.h>		/* Required for fprintf/stderr. */
#include <isc/thread.h>		/* Requried for isc_thread_self(). */

static void
print_lock(const char *operation, isc_rwlock_t *rwl, isc_rwlocktype_t type) {
	fprintf(stderr,
		isc_msgcat_get(isc_msgcat, ISC_MSGSET_RWLOCK,
			       ISC_MSG_PRINTLOCK,
			       "rwlock %p thread %lu %s(%s): %s, %u active, "
			       "%u granted, %u rwaiting, %u wwaiting\n"),
		rwl, isc_thread_self(), operation,
		(type == isc_rwlocktype_read ? 
		 isc_msgcat_get(isc_msgcat, ISC_MSGSET_RWLOCK,
				ISC_MSG_READ, "read") :
		 isc_msgcat_get(isc_msgcat, ISC_MSGSET_RWLOCK,
				ISC_MSG_WRITE, "write")),
	        (rwl->type == isc_rwlocktype_read ?
		 isc_msgcat_get(isc_msgcat, ISC_MSGSET_RWLOCK,
				ISC_MSG_READING, "reading") : 
		 isc_msgcat_get(isc_msgcat, ISC_MSGSET_RWLOCK,
				ISC_MSG_WRITING, "writing")),
	        rwl->active, rwl->granted, rwl->readers_waiting,
		rwl->writers_waiting);
}
#endif

isc_result_t
isc_rwlock_init(isc_rwlock_t *rwl, unsigned int read_quota,
		unsigned int write_quota)
{
	isc_result_t result;

	REQUIRE(rwl != NULL);

	/*
	 * In case there's trouble initializing, we zero magic now.  If all
	 * goes well, we'll set it to RWLOCK_MAGIC.
	 */
	rwl->magic = 0;

	rwl->type = isc_rwlocktype_read;
	rwl->original = isc_rwlocktype_none;
	rwl->active = 0;
	rwl->granted = 0;
	rwl->readers_waiting = 0;
	rwl->writers_waiting = 0;
	if (read_quota == 0)
		read_quota = RWLOCK_DEFAULT_READ_QUOTA;
	rwl->read_quota = read_quota;
	if (write_quota == 0)
		write_quota = RWLOCK_DEFAULT_WRITE_QUOTA;
	rwl->write_quota = write_quota;
	result = isc_mutex_init(&rwl->lock);
	if (result != ISC_R_SUCCESS) {
		UNEXPECTED_ERROR(__FILE__, __LINE__,
				 "isc_mutex_init() %s: %s",
				 isc_msgcat_get(isc_msgcat, ISC_MSGSET_GENERAL,
						ISC_MSG_FAILED, "failed"),
				 isc_result_totext(result));
		return (ISC_R_UNEXPECTED);
	}
	result = isc_condition_init(&rwl->readable);
	if (result != ISC_R_SUCCESS) {
		UNEXPECTED_ERROR(__FILE__, __LINE__,
				 "isc_condition_init(readable) %s: %s",
				 isc_msgcat_get(isc_msgcat, ISC_MSGSET_GENERAL,
						ISC_MSG_FAILED, "failed"),
				 isc_result_totext(result));
		return (ISC_R_UNEXPECTED);
	}
	result = isc_condition_init(&rwl->writeable);
	if (result != ISC_R_SUCCESS) {
		UNEXPECTED_ERROR(__FILE__, __LINE__,
				 "isc_condition_init(writeable) %s: %s",
				 isc_msgcat_get(isc_msgcat, ISC_MSGSET_GENERAL,
						ISC_MSG_FAILED, "failed"),
				 isc_result_totext(result));
		return (ISC_R_UNEXPECTED);
	}

	rwl->magic = RWLOCK_MAGIC;

	return (ISC_R_SUCCESS);
}

static isc_result_t
doit(isc_rwlock_t *rwl, isc_rwlocktype_t type, isc_boolean_t nonblock) {
	isc_boolean_t skip = ISC_FALSE;
	isc_boolean_t done = ISC_FALSE;
	isc_result_t result = ISC_R_SUCCESS;

	REQUIRE(VALID_RWLOCK(rwl));

	LOCK(&rwl->lock);

#ifdef ISC_RWLOCK_TRACE
	print_lock(isc_msgcat_get(isc_msgcat, ISC_MSGSET_RWLOCK,
				  ISC_MSG_PRELOCK, "prelock"), rwl, type);
#endif

	if (type == isc_rwlocktype_read) {
		if (rwl->readers_waiting != 0)
			skip = ISC_TRUE;
		while (!done) {
			if (!skip &&
			    ((rwl->active == 0 ||
			      (rwl->type == isc_rwlocktype_read &&
			       (rwl->writers_waiting == 0 ||
			        rwl->granted < rwl->read_quota)))))
			{
				rwl->type = isc_rwlocktype_read;
				rwl->active++;
				rwl->granted++;
				done = ISC_TRUE;
			} else if (nonblock) {
				result = ISC_R_LOCKBUSY;
				done = ISC_TRUE;
			} else {
				skip = ISC_FALSE;
				rwl->readers_waiting++;
				WAIT(&rwl->readable, &rwl->lock);
				rwl->readers_waiting--;
			}
		}
	} else {
		if (rwl->writers_waiting != 0)
			skip = ISC_TRUE;
		while (!done) {
			if (!skip && rwl->active == 0) {
				rwl->type = isc_rwlocktype_write;
				rwl->active = 1;
				rwl->granted++;
				done = ISC_TRUE;
			} else if (nonblock) {
				result = ISC_R_LOCKBUSY;
				done = ISC_TRUE;
			} else {
				skip = ISC_FALSE;
				rwl->writers_waiting++;
				WAIT(&rwl->writeable, &rwl->lock);
				rwl->writers_waiting--;
			}
		}
	}

#ifdef ISC_RWLOCK_TRACE
	print_lock(isc_msgcat_get(isc_msgcat, ISC_MSGSET_RWLOCK,
				  ISC_MSG_POSTLOCK, "postlock"), rwl, type);
#endif

	UNLOCK(&rwl->lock);

	return (result);
}

isc_result_t
isc_rwlock_lock(isc_rwlock_t *rwl, isc_rwlocktype_t type) {
	return (doit(rwl, type, ISC_FALSE));
}

isc_result_t
isc_rwlock_trylock(isc_rwlock_t *rwl, isc_rwlocktype_t type) {
	return (doit(rwl, type, ISC_TRUE));
}

isc_result_t
isc_rwlock_tryupgrade(isc_rwlock_t *rwl) {
	isc_result_t result = ISC_R_SUCCESS;

	REQUIRE(VALID_RWLOCK(rwl));
	LOCK(&rwl->lock);
	REQUIRE(rwl->type == isc_rwlocktype_read);
	REQUIRE(rwl->active != 0);

	/* If we are the only reader then succeed. */
	if (rwl->active == 1) {
		rwl->original = (rwl->original == isc_rwlocktype_none) ?
				isc_rwlocktype_read : isc_rwlocktype_none;
		rwl->type = isc_rwlocktype_write;
	} else
		result = ISC_R_LOCKBUSY;

	UNLOCK(&rwl->lock);
	return (result);
}

void
isc_rwlock_downgrade(isc_rwlock_t *rwl) {

	REQUIRE(VALID_RWLOCK(rwl));
	LOCK(&rwl->lock);
	REQUIRE(rwl->type == isc_rwlocktype_write);
	REQUIRE(rwl->active == 1);

	rwl->type = isc_rwlocktype_read;
	rwl->original = (rwl->original == isc_rwlocktype_none) ?
			isc_rwlocktype_write : isc_rwlocktype_none;
	/*
	 * Resume processing any read request that were blocked when
	 * we upgraded.
	 */
	if (rwl->original == isc_rwlocktype_none &&
	    (rwl->writers_waiting == 0 || rwl->granted < rwl->read_quota) &&
	    rwl->readers_waiting > 0)
		BROADCAST(&rwl->readable);

	UNLOCK(&rwl->lock);
}

isc_result_t
isc_rwlock_unlock(isc_rwlock_t *rwl, isc_rwlocktype_t type) {

	REQUIRE(VALID_RWLOCK(rwl));
	LOCK(&rwl->lock);
	REQUIRE(rwl->type == type);

	UNUSED(type);

#ifdef ISC_RWLOCK_TRACE
	print_lock(isc_msgcat_get(isc_msgcat, ISC_MSGSET_RWLOCK,
				  ISC_MSG_PREUNLOCK, "preunlock"), rwl, type);
#endif

	INSIST(rwl->active > 0);
	rwl->active--;
	if (rwl->active == 0) {
		if (rwl->original != isc_rwlocktype_none) {
			rwl->type = rwl->original;
			rwl->original = isc_rwlocktype_none;
		}
		if (rwl->type == isc_rwlocktype_read) {
			rwl->granted = 0;
			if (rwl->writers_waiting > 0) {
				rwl->type = isc_rwlocktype_write;
				SIGNAL(&rwl->writeable);
			} else if (rwl->readers_waiting > 0) {
				/* Does this case ever happen? */
				BROADCAST(&rwl->readable);
			}
		} else {
			if (rwl->readers_waiting > 0) {
				if (rwl->writers_waiting > 0 &&
				    rwl->granted < rwl->write_quota) {
					SIGNAL(&rwl->writeable);
				} else {
					rwl->granted = 0;
					rwl->type = isc_rwlocktype_read;
					BROADCAST(&rwl->readable);
				}
			} else if (rwl->writers_waiting > 0) {
				rwl->granted = 0;
				SIGNAL(&rwl->writeable);
			} else {
				rwl->granted = 0;
			}
		}
	}
	INSIST(rwl->original == isc_rwlocktype_none);

#ifdef ISC_RWLOCK_TRACE
	print_lock(isc_msgcat_get(isc_msgcat, ISC_MSGSET_RWLOCK,
				  ISC_MSG_POSTUNLOCK, "postunlock"),
		   rwl, type);
#endif

	UNLOCK(&rwl->lock);

	return (ISC_R_SUCCESS);
}

void
isc_rwlock_destroy(isc_rwlock_t *rwl) {
	REQUIRE(VALID_RWLOCK(rwl));

	LOCK(&rwl->lock);
	REQUIRE(rwl->active == 0 &&
		rwl->readers_waiting == 0 &&
		rwl->writers_waiting == 0);
	UNLOCK(&rwl->lock);

	rwl->magic = 0;
	(void)isc_condition_destroy(&rwl->readable);
	(void)isc_condition_destroy(&rwl->writeable);
	DESTROYLOCK(&rwl->lock);
}

#else /* ISC_PLATFORM_USETHREADS */

isc_result_t
isc_rwlock_init(isc_rwlock_t *rwl, unsigned int read_quota,
		unsigned int write_quota)
{
	REQUIRE(rwl != NULL);

	UNUSED(read_quota);
	UNUSED(write_quota);

	rwl->type = isc_rwlocktype_read;
	rwl->active = 0;
	rwl->magic = RWLOCK_MAGIC;

	return (ISC_R_SUCCESS);
}

isc_result_t
isc_rwlock_lock(isc_rwlock_t *rwl, isc_rwlocktype_t type) {
	REQUIRE(VALID_RWLOCK(rwl));

	if (type == isc_rwlocktype_read) {
		if (rwl->type != isc_rwlocktype_read && rwl->active != 0)
			return (ISC_R_LOCKBUSY);
		rwl->type = isc_rwlocktype_read;
		rwl->active++;
	} else {
		if (rwl->active != 0)
			return (ISC_R_LOCKBUSY);
		rwl->type = isc_rwlocktype_write;
		rwl->active = 1;
	}
        return (ISC_R_SUCCESS);
}

isc_result_t
isc_rwlock_trylock(isc_rwlock_t *rwl, isc_rwlocktype_t type) {
	return (isc_rwlock_lock(rwl, type));
}

isc_result_t
isc_rwlock_tryupgrade(isc_rwlock_t *rwl) {
	isc_result_t result = ISC_R_SUCCESS;

	REQUIRE(VALID_RWLOCK(rwl));
	REQUIRE(rwl->type == isc_rwlocktype_read);
	REQUIRE(rwl->active != 0);
	
	/* If we are the only reader then succeed. */
	if (rwl->active == 1)
		rwl->type = isc_rwlocktype_write;
	else
		result = ISC_R_LOCKBUSY;
	return (result);
}

void
isc_rwlock_downgrade(isc_rwlock_t *rwl) {

	REQUIRE(VALID_RWLOCK(rwl));
	REQUIRE(rwl->type == isc_rwlocktype_write);
	REQUIRE(rwl->active == 1);

	rwl->type = isc_rwlocktype_read;
}

isc_result_t
isc_rwlock_unlock(isc_rwlock_t *rwl, isc_rwlocktype_t type) {
	REQUIRE(VALID_RWLOCK(rwl));
	REQUIRE(rwl->type == type);

	UNUSED(type);

	INSIST(rwl->active > 0);
	rwl->active--;

	return (ISC_R_SUCCESS);
}

void
isc_rwlock_destroy(isc_rwlock_t *rwl) {
	REQUIRE(rwl != NULL);
	REQUIRE(rwl->active == 0);
	rwl->magic = 0;
}

#endif /* ISC_PLATFORM_USETHREADS */
