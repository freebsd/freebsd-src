/*
 * Copyright (C) 2004, 2005, 2007, 2008  Internet Systems Consortium, Inc. ("ISC")
 * Copyright (C) 1998-2003  Internet Software Consortium.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
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

/* $Id: timer.h,v 1.28.12.11 2008/06/25 23:45:37 tbox Exp $ */

#ifndef ISC_TIMER_H
#define ISC_TIMER_H 1

/*****
 ***** Module Info
 *****/

/*
 * Timers
 *
 * Provides timers which are event sources in the task system.
 *
 * Three types of timers are supported:
 *
 *	'ticker' timers generate a periodic tick event.
 *
 *	'once' timers generate an idle timeout event if they are idle for too
 *	long, and generate a life timeout event if their lifetime expires.
 *	They are used to implement both (possibly expiring) idle timers and
 *	'one-shot' timers.
 *
 *	'limited' timers generate a periodic tick event until they reach
 *	their lifetime when they generate a life timeout event.
 *
 *	'inactive' timers generate no events.
 *
 * Timers can change type.  It is typical to create a timer as
 * an 'inactive' timer and then change it into a 'ticker' or
 * 'once' timer.
 *
 * MP:
 *	The module ensures appropriate synchronization of data structures it
 *	creates and manipulates.
 *
 *	Clients of this module must not be holding a timer's task's lock when
 *	making a call that affects that timer.  Failure to follow this rule
 *	can result in deadlock.
 *
 *	The caller must ensure that isc_timermgr_destroy() is called only
 *	once for a given manager.
 *
 * Reliability:
 *	No anticipated impact.
 *
 * Resources:
 *	<TBS>
 *
 * Security:
 *	No anticipated impact.
 *
 * Standards:
 *	None.
 */


/***
 *** Imports
 ***/

#include <isc/types.h>
#include <isc/event.h>
#include <isc/eventclass.h>
#include <isc/lang.h>
#include <isc/time.h>

ISC_LANG_BEGINDECLS

/***
 *** Types
 ***/

typedef enum {
	isc_timertype_ticker = 0,
	isc_timertype_once = 1,
	isc_timertype_limited = 2,
	isc_timertype_inactive = 3
} isc_timertype_t;

typedef struct isc_timerevent {
	struct isc_event	common;
	isc_time_t		due;
} isc_timerevent_t;

#define ISC_TIMEREVENT_FIRSTEVENT	(ISC_EVENTCLASS_TIMER + 0)
#define ISC_TIMEREVENT_TICK		(ISC_EVENTCLASS_TIMER + 1)
#define ISC_TIMEREVENT_IDLE		(ISC_EVENTCLASS_TIMER + 2)
#define ISC_TIMEREVENT_LIFE		(ISC_EVENTCLASS_TIMER + 3)
#define ISC_TIMEREVENT_LASTEVENT	(ISC_EVENTCLASS_TIMER + 65535)

/***
 *** Timer and Timer Manager Functions
 ***
 *** Note: all Ensures conditions apply only if the result is success for
 *** those functions which return an isc_result_t.
 ***/

isc_result_t
isc_timer_create(isc_timermgr_t *manager,
		 isc_timertype_t type,
		 isc_time_t *expires,
		 isc_interval_t *interval,
		 isc_task_t *task,
		 isc_taskaction_t action,
		 const void *arg,
		 isc_timer_t **timerp);
/*
 * Create a new 'type' timer managed by 'manager'.  The timers parameters
 * are specified by 'expires' and 'interval'.  Events will be posted to
 * 'task' and when dispatched 'action' will be called with 'arg' as the
 * arg value.  The new timer is returned in 'timerp'.
 *
 * Notes:
 *
 *	For ticker timers, the timer will generate a 'tick' event every
 *	'interval' seconds.  The value of 'expires' is ignored.
 *
 *	For once timers, 'expires' specifies the time when a life timeout
 *	event should be generated.  If 'expires' is 0 (the epoch), then no life
 *	timeout will be generated.  'interval' specifies how long the timer
 *	can be idle before it generates an idle timeout.  If 0, then no
 *	idle timeout will be generated.
 *
 *	If 'expires' is NULL, the epoch will be used.
 *
 *	If 'interval' is NULL, the zero interval will be used.
 *
 * Requires:
 *
 *	'manager' is a valid manager
 *
 *	'task' is a valid task
 *
 *	'action' is a valid action
 *
 *	'expires' points to a valid time, or is NULL.
 *
 *	'interval' points to a valid interval, or is NULL.
 *
 *	type == isc_timertype_inactive ||
 *	('expires' and 'interval' are not both 0)
 *
 *	'timerp' is a valid pointer, and *timerp == NULL
 *
 * Ensures:
 *
 *	'*timerp' is attached to the newly created timer
 *
 *	The timer is attached to the task
 *
 *	An idle timeout will not be generated until at least Now + the
 *	timer's interval if 'timer' is a once timer with a non-zero
 *	interval.
 *
 * Returns:
 *
 *	Success
 *	No memory
 *	Unexpected error
 */

isc_result_t
isc_timer_reset(isc_timer_t *timer,
		isc_timertype_t type,
		isc_time_t *expires,
		isc_interval_t *interval,
		isc_boolean_t purge);
/*
 * Change the timer's type, expires, and interval values to the given
 * values.  If 'purge' is TRUE, any pending events from this timer
 * are purged from its task's event queue.
 *
 * Notes:
 *
 *	If 'expires' is NULL, the epoch will be used.
 *
 *	If 'interval' is NULL, the zero interval will be used.
 *
 * Requires:
 *
 *	'timer' is a valid timer
 *
 *	The same requirements that isc_timer_create() imposes on 'type',
 *	'expires' and 'interval' apply.
 *
 * Ensures:
 *
 *	An idle timeout will not be generated until at least Now + the
 *	timer's interval if 'timer' is a once timer with a non-zero
 *	interval.
 *
 * Returns:
 *
 *	Success
 *	No memory
 *	Unexpected error
 */

isc_result_t
isc_timer_touch(isc_timer_t *timer);
/*
 * Set the last-touched time of 'timer' to the current time.
 *
 * Requires:
 *
 *	'timer' is a valid once timer.
 *
 * Ensures:
 *
 *	An idle timeout will not be generated until at least Now + the
 *	timer's interval if 'timer' is a once timer with a non-zero
 *	interval.
 *
 * Returns:
 *
 *	Success
 *	Unexpected error
 */

void
isc_timer_attach(isc_timer_t *timer, isc_timer_t **timerp);
/*
 * Attach *timerp to timer.
 *
 * Requires:
 *
 *	'timer' is a valid timer.
 *
 *	'timerp' points to a NULL timer.
 *
 * Ensures:
 *
 *	*timerp is attached to timer.
 */

void
isc_timer_detach(isc_timer_t **timerp);
/*
 * Detach *timerp from its timer.
 *
 * Requires:
 *
 *	'timerp' points to a valid timer.
 *
 * Ensures:
 *
 *	*timerp is NULL.
 *
 *	If '*timerp' is the last reference to the timer,
 *	then:
 *
 *		The timer will be shutdown
 *
 *		The timer will detach from its task
 *
 *		All resources used by the timer have been freed
 *
 *		Any events already posted by the timer will be purged.
 *		Therefore, if isc_timer_detach() is called in the context
 *		of the timer's task, it is guaranteed that no more
 *		timer event callbacks will run after the call.
 */

isc_timertype_t
isc_timer_gettype(isc_timer_t *timer);
/*%<
 * Return the timer type.
 *
 * Requires:
 *
 *\li	'timer' to be a valid timer.
 */

isc_result_t
isc_timermgr_create(isc_mem_t *mctx, isc_timermgr_t **managerp);
/*
 * Create a timer manager.
 *
 * Notes:
 *
 *	All memory will be allocated in memory context 'mctx'.
 *
 * Requires:
 *
 *	'mctx' is a valid memory context.
 *
 *	'managerp' points to a NULL isc_timermgr_t.
 *
 * Ensures:
 *
 *	'*managerp' is a valid isc_timermgr_t.
 *
 * Returns:
 *
 *	Success
 *	No memory
 *	Unexpected error
 */

void
isc_timermgr_destroy(isc_timermgr_t **managerp);
/*
 * Destroy a timer manager.
 *
 * Notes:
 *
 *	This routine blocks until there are no timers left in the manager,
 *	so if the caller holds any timer references using the manager, it
 *	must detach them before calling isc_timermgr_destroy() or it will
 *	block forever.
 *
 * Requires:
 *
 *	'*managerp' is a valid isc_timermgr_t.
 *
 * Ensures:
 *
 *	*managerp == NULL
 *
 *	All resources used by the manager have been freed.
 */

void isc_timermgr_poke(isc_timermgr_t *m);

ISC_LANG_ENDDECLS

#endif /* ISC_TIMER_H */
