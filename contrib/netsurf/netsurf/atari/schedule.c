/*
 * Copyright 2008 Vincent Sanders <vince@simtec.co.uk>
 * Copyright 2011 Ole Loots <ole@monochrom.net>
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

#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>

#include "utils/errors.h"

#include "atari/schedule.h"

#ifdef DEBUG_SCHEDULER
#include "utils/log.h"
#else
#define LOG(X)
#endif

#define MS_NOW() ((clock() * 1000) / CLOCKS_PER_SEC)

/* linked list of scheduled callbacks */
static struct nscallback *schedule_list = NULL;

/**
 * scheduled callback.
 */
struct nscallback
{
	struct nscallback *next;
	unsigned long timeout;
	void (*callback)(void *p);
	void *p;
};

static int max_scheduled;
static int cur_scheduled;

/**
 * Unschedule a callback.
 *
 * \param  callback  callback function
 * \param  p         user parameter, passed to callback function
 *
 * All scheduled callbacks matching both callback and p are removed.
 */

static nserror schedule_remove(void (*callback)(void *p), void *p)
{
	struct nscallback *cur_nscb;
	struct nscallback *prev_nscb;
	struct nscallback *unlnk_nscb;

	/* check there is something on the list to remove */
        if (schedule_list == NULL) {
                return NSERROR_OK;
	}

	LOG(("removing %p, %p", callback, p));

	cur_nscb = schedule_list;
	prev_nscb = NULL;

	while (cur_nscb != NULL) {
		if ((cur_nscb->callback ==  callback) &&
                    (cur_nscb->p ==  p)) {
			/* item to remove */
			LOG(("callback entry %p removing  %p(%p)",
			       cur_nscb, cur_nscb->callback, cur_nscb->p));

			/* remove callback */
			unlnk_nscb = cur_nscb;
			cur_nscb = unlnk_nscb->next;

			if (prev_nscb == NULL) {
				schedule_list = cur_nscb;
			} else {
				prev_nscb->next = cur_nscb;
			}
			free (unlnk_nscb);
			cur_scheduled--;
		} else {
			/* move to next element */
			prev_nscb = cur_nscb;
			cur_nscb = prev_nscb->next;
		}
	}
	return NSERROR_OK;
}

/* exported function documented in atari/schedule.h */
nserror atari_schedule(int ival, void (*callback)(void *p), void *p)
{
	struct nscallback *nscb;
	nserror ret;

	/* remove any existing callback of this kind */
	ret = schedule_remove(callback, p);
	if ((ival < 0) || (ret != NSERROR_OK)) {
		return ret;
	}
	
	nscb = calloc(1, sizeof(struct nscallback));

	nscb->timeout = MS_NOW() + ival;

	LOG(("adding callback %p for  %p(%p) at %d ms",
	       nscb, callback, p, nscb->timeout ));

	nscb->callback = callback;
	nscb->p = p;

	/* add to list front */
	nscb->next = schedule_list;
	schedule_list = nscb;
	cur_scheduled++;
	if( cur_scheduled > max_scheduled ) {
		max_scheduled = cur_scheduled;
	}

	return NSERROR_OK;
}


/* exported function documented in atari/schedule.h */
int schedule_run(void)
{
	unsigned long nexttime;
	struct nscallback *cur_nscb;
	struct nscallback *prev_nscb;
	struct nscallback *unlnk_nscb;
	unsigned long now = MS_NOW();

	if (schedule_list == NULL)
		return -1;

	/* reset enumeration to the start of the list */
	cur_nscb = schedule_list;
	prev_nscb = NULL;
	nexttime = cur_nscb->timeout;

	while (cur_nscb != NULL) {
		if (now > cur_nscb->timeout) {
			/* scheduled time */

			/* remove callback */
			unlnk_nscb = cur_nscb;
			if (prev_nscb == NULL) {
				schedule_list = unlnk_nscb->next;
			} else {
				prev_nscb->next = unlnk_nscb->next;
			}

			LOG(("callback entry %p running %p(%p)",
			       unlnk_nscb, unlnk_nscb->callback, unlnk_nscb->p));

			/* call callback */
			unlnk_nscb->callback(unlnk_nscb->p);
			free(unlnk_nscb);
			cur_scheduled--;

			/* need to deal with callback modifying the list. */
			if (schedule_list == NULL) 	{
				LOG(("schedule_list == NULL"));

				return -1; /* no more callbacks scheduled */
			}

			/* reset enumeration to the start of the list */
			cur_nscb = schedule_list;
			prev_nscb = NULL;
			nexttime = cur_nscb->timeout;
		} else {
			/* if the time to the event is sooner than the
			 * currently recorded soonest event record it
			 */
			if (nexttime > cur_nscb->timeout) {
				nexttime = cur_nscb->timeout;
			}
			/* move to next element */
			prev_nscb = cur_nscb;
			cur_nscb = prev_nscb->next;
		}
	}

	/* make rettime relative to now and convert to ms */
	nexttime = nexttime - now;

	LOG(("returning time to next event as %ldms", nexttime ));

	/*return next event time in milliseconds (24days max wait) */
	return nexttime;
}


/* exported function documented in atari/schedule.h */
void list_schedule(void)
{
	struct timeval tv;
	struct nscallback *cur_nscb;

	LOG(("schedule list at ms clock %ld", MS_NOW() ));

	cur_nscb = schedule_list;
	while (cur_nscb != NULL) {
		LOG(("Schedule %p at %ld", cur_nscb, cur_nscb->timeout ));
		cur_nscb = cur_nscb->next;
	}
	LOG(("Maxmium callbacks scheduled: %d", max_scheduled ));
}


/*
 * Local Variables:
 * c-basic-offset:8
 * End:
 */
