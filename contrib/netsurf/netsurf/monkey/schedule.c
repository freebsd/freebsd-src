/*
 * Copyright 2006-2007 Daniel Silverstone <dsilvers@digital-scurf.org>
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

#include <glib.h>
#include <stdlib.h>
#include <stdbool.h>

#include "utils/errors.h"

#include "monkey/schedule.h"

#undef DEBUG_MONKEY_SCHEDULE

#ifdef DEBUG_MONKEY_SCHEDULE
#include "utils/log.h"
#else
#define LOG(X)
#endif

/** Killable callback closure embodiment. */
typedef struct {
        void (*callback)(void *);	/**< The callback function. */
        void *context;			/**< The context for the callback. */
        bool callback_killed;		/**< Whether or not this was killed. */
} _nsgtk_callback_t;

/** List of callbacks which have occurred and are pending running. */
static GList *pending_callbacks = NULL;
/** List of callbacks which are queued to occur in the future. */
static GList *queued_callbacks = NULL;
/** List of callbacks which are about to be run in this ::schedule_run. */
static GList *this_run = NULL;

static gboolean
nsgtk_schedule_generic_callback(gpointer data)
{
        _nsgtk_callback_t *cb = (_nsgtk_callback_t *)(data);
        if (cb->callback_killed) {
                /* This callback instance has been killed. */
                LOG(("CB at %p already dead.", cb));
        }
        queued_callbacks = g_list_remove(queued_callbacks, cb);
        LOG(("CB %p(%p) now pending run", cb->callback, cb->context));
        pending_callbacks = g_list_append(pending_callbacks, cb);
        return FALSE;
}

static void
nsgtk_schedule_kill_callback(void *_target, void *_match)
{
        _nsgtk_callback_t *target = (_nsgtk_callback_t *)_target;
        _nsgtk_callback_t *match = (_nsgtk_callback_t *)_match;
        if ((target->callback == match->callback) &&
            (target->context == match->context)) {
                LOG(("Found match for %p(%p), killing.",
                     target->callback, target->context));
                target->callback = NULL;
                target->context = NULL;
                target->callback_killed = true;
        }
}

static void
schedule_remove(void (*callback)(void *p), void *p)
{
        _nsgtk_callback_t cb_match = {
                .callback = callback,
                .context = p,
        };

        g_list_foreach(queued_callbacks,
                       nsgtk_schedule_kill_callback, &cb_match);
        g_list_foreach(pending_callbacks,
                       nsgtk_schedule_kill_callback, &cb_match);
        g_list_foreach(this_run,
                       nsgtk_schedule_kill_callback, &cb_match);
}

nserror monkey_schedule(int t, void (*callback)(void *p), void *p)
{
        _nsgtk_callback_t *cb;

        /* Kill any pending schedule of this kind. */
        schedule_remove(callback, p);
	if (t < 0) {
		return NSERROR_OK;
	}

	cb = malloc(sizeof(_nsgtk_callback_t));
        cb->callback = callback;
        cb->context = p;
        cb->callback_killed = false;
        /* Prepend is faster right now. */
        LOG(("queued a callback to %p(%p) for %d msecs time", callback, p, t));
        queued_callbacks = g_list_prepend(queued_callbacks, cb);
        g_timeout_add(t, nsgtk_schedule_generic_callback, cb);

	return NSERROR_OK;

}

bool
schedule_run(void)
{
        /* Capture this run of pending callbacks into the list. */
        this_run = pending_callbacks;
        
        if (this_run == NULL)
                return false; /* Nothing to do */

        /* Clear the pending list. */
        pending_callbacks = NULL;

        LOG(("Captured a run of %d callbacks to fire.", g_list_length(this_run)));

        /* Run all the callbacks which made it this far. */
        while (this_run != NULL) {
                _nsgtk_callback_t *cb = (_nsgtk_callback_t *)(this_run->data);
                this_run = g_list_remove(this_run, this_run->data);
                if (!cb->callback_killed) {
                  LOG(("CB DO %p(%p)", cb->callback, cb->context));
                  cb->callback(cb->context);
                } else {
                  LOG(("CB %p(%p) already dead, dropping", cb->callback, cb->context));
                }
                free(cb);
        }
        return true;
}
