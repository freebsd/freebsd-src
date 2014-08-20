/*
 * Copyright 2008 François Revol <mmu_man@users.sourceforge.net>
 *
 * This file is part of NetSurf, http://www.netsurf-browser.org/
 *
 * NetSurf is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * NetSurf is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#define __STDBOOL_H__	1
#include <stdlib.h>
#include <stdbool.h>
#include <OS.h>
#include <List.h>

extern "C" {
#include "utils/errors.h"
#include "beos/schedule.h"
#include "desktop/browser.h"

#ifdef DEBUG_BEOS_SCHEDULE
#include "utils/log.h"
#else
#define LOG(X)
#endif
}

/** Killable callback closure embodiment. */
typedef struct {
	void (*callback)(void *);	/**< The callback function. */
	void *context;			/**< The context for the callback. */
	bool callback_killed;		/**< Whether or not this was killed. */
	bool callback_fired;		/**< Whether or not this has fired yet. */
	bigtime_t timeout;
} _nsbeos_callback_t;

/** List of all callbacks. */
static BList *callbacks = NULL;

/** earliest deadline. It's used for select() in gui_poll() */
bigtime_t earliest_callback_timeout = B_INFINITE_TIMEOUT;


static bool
nsbeos_schedule_kill_callback(void *_target, void *_match)
{
	_nsbeos_callback_t *target = (_nsbeos_callback_t *)_target;
	_nsbeos_callback_t *match = (_nsbeos_callback_t *)_match;
	if ((target->callback == match->callback) &&
	    (target->context == match->context)) {
		LOG(("Found match for %p(%p), killing.",
		     target->callback, target->context));
		target->callback = NULL;
		target->context = NULL;
		target->callback_killed = true;
	}
	return false;
}

static void
schedule_remove(void (*callback)(void *p), void *p)
{
	LOG(("schedule_remove() for %p(%p)", cb->callback, cb->context));
	if (callbacks == NULL)
		return;
	_nsbeos_callback_t cb_match;
	cb_match.callback = callback;
	cb_match.context = p;

	callbacks->DoForEach(nsbeos_schedule_kill_callback, &cb_match);
}

nserror beos_schedule(int t, void (*callback)(void *p), void *p)
{
	LOG(("t:%d cb:%p p:%p", t, cb->callback, cb->context));

	if (callbacks == NULL) {
		callbacks = new BList;
        }

	/* Kill any pending schedule of this kind. */
	schedule_remove(callback, p);

        if (t < 0) {
          return NSERROR_OK;
        }

	bigtime_t timeout = system_time() + t * 1000LL;
	_nsbeos_callback_t *cb = (_nsbeos_callback_t *)malloc(sizeof(_nsbeos_callback_t));
	cb->callback = callback;
	cb->context = p;
	cb->callback_killed = cb->callback_fired = false;
	cb->timeout = timeout;
	if (earliest_callback_timeout > timeout) {
		earliest_callback_timeout = timeout;
	}
	callbacks->AddItem(cb);

        return NSERROR_OK;
}

bool
schedule_run(void)
{
	LOG(("schedule_run()"));

	earliest_callback_timeout = B_INFINITE_TIMEOUT;
	if (callbacks == NULL)
		return false; /* Nothing to do */

	bigtime_t now = system_time();
	int32 i;

	LOG(("Checking %ld callbacks to for deadline.", this_run->CountItems()));

	/* Run all the callbacks which made it this far. */
	for (i = 0; i < callbacks->CountItems(); ) {
		_nsbeos_callback_t *cb = (_nsbeos_callback_t *)(callbacks->ItemAt(i));
		if (cb->timeout > now) {
			// update next deadline
			if (earliest_callback_timeout > cb->timeout)
				earliest_callback_timeout = cb->timeout;
			i++;
			continue;
		}
		LOG(("Running callbacks %p(%p).", cb->callback, cb->context));
		if (!cb->callback_killed)
			cb->callback(cb->context);
		callbacks->RemoveItem(cb);
		free(cb);
	}
	return true;
}
