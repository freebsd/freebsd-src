/*
 * Copyright 2008 Vincent Sanders <vince@simtec.co.uk>
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

#include <sys/time.h>
#include <time.h>

#include "utils/log.h"
#include "utils/utils.h"
#include "utils/errors.h"

#include "windows/schedule.h"

#ifdef DEBUG_SCHEDULER
#define SRLOG(x) LOG(x)
#else
#define SRLOG(x)
#endif

/* linked list of scheduled callbacks */
static struct nscallback *schedule_list = NULL;

/**
 * scheduled callback.
 */
struct nscallback
{
        struct nscallback *next;
	struct timeval tv;
	void (*callback)(void *p);
	void *p;
};


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

	SRLOG(("removing %p, %p", callback, p));

        cur_nscb = schedule_list;
        prev_nscb = NULL;

        while (cur_nscb != NULL) {
                if ((cur_nscb->callback ==  callback) &&
                    (cur_nscb->p ==  p)) {
                        /* item to remove */

                        SRLOG(("callback entry %p removing  %p(%p)",
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
                } else {
                        /* move to next element */
                        prev_nscb = cur_nscb;
                        cur_nscb = prev_nscb->next;
                }
        }
	return NSERROR_OK;
}

/* exported interface documented in windows/schedule.h */
nserror win32_schedule(int ival, void (*callback)(void *p), void *p)
{
	struct nscallback *nscb;
	struct timeval tv;
	nserror ret;

	ret = schedule_remove(callback, p);
	if ((ival < 0) || (ret != NSERROR_OK)) {
		return ret;
	}

        tv.tv_sec = ival / 1000; /* miliseconds to seconds */
        tv.tv_usec = (ival % 1000) * 1000; /* remainder to microseconds */

	nscb = calloc(1, sizeof(struct nscallback));
	if (nscb == NULL) {
		return NSERROR_NOMEM;
	}

	SRLOG(("adding callback %p for %p(%p) at %d cs",
	       nscb, callback, p, ival));

	gettimeofday(&nscb->tv, NULL);
	timeradd(&nscb->tv, &tv, &nscb->tv);

	nscb->callback = callback;
	nscb->p = p;

        /* add to list front */
        nscb->next = schedule_list;
        schedule_list = nscb;

	return NSERROR_OK;
}

/* exported interface documented in schedule.h */
int 
schedule_run(void)
{
	struct timeval tv;
	struct timeval nexttime;
	struct timeval rettime;
        struct nscallback *cur_nscb;
        struct nscallback *prev_nscb;
        struct nscallback *unlnk_nscb;

        if (schedule_list == NULL)
                return -1;

	/* reset enumeration to the start of the list */
        cur_nscb = schedule_list;
        prev_nscb = NULL;
	nexttime = cur_nscb->tv;

	gettimeofday(&tv, NULL);

        while (cur_nscb != NULL) {
                if (timercmp(&tv, &cur_nscb->tv, >)) {
                        /* scheduled time */

                        /* remove callback */
                        unlnk_nscb = cur_nscb;

                        if (prev_nscb == NULL) {
                                schedule_list = unlnk_nscb->next;
                        } else {
                                prev_nscb->next = unlnk_nscb->next;
                        }

                        SRLOG(("callback entry %p running %p(%p)",
                             unlnk_nscb, unlnk_nscb->callback, unlnk_nscb->p));
                        /* call callback */
                        unlnk_nscb->callback(unlnk_nscb->p);

                        free(unlnk_nscb);

			/* dispatched events can modify the list,
			 * instead of locking we simply reset list
			 * enumeration to the start.
			 */ 
			if (schedule_list == NULL)
				return -1; /* no more callbacks scheduled */
			
                        cur_nscb = schedule_list;
                        prev_nscb = NULL;
			nexttime = cur_nscb->tv;
                } else {
			/* if the time to the event is sooner than the
			 * currently recorded soonest event record it 
			 */
			if (timercmp(&nexttime, &cur_nscb->tv, >)) {
				nexttime = cur_nscb->tv;
			}
                        /* move to next element */
                        prev_nscb = cur_nscb;
                        cur_nscb = prev_nscb->next;
                }
        }

	/* make returned time relative to now */
	timersub(&nexttime, &tv, &rettime);

	SRLOG(("returning time to next event as %ldms",
	     (rettime.tv_sec * 1000) + (rettime.tv_usec / 1000)));

	/* return next event time in milliseconds (24days max wait) */
        return (rettime.tv_sec * 1000) + (rettime.tv_usec / 1000);
}

/* exported interface documented in schedule.h */
void list_schedule(void)
{
	struct timeval tv;
        struct nscallback *cur_nscb;

	gettimeofday(&tv, NULL);

        LOG(("schedule list at %ld:%ld", tv.tv_sec, tv.tv_usec));

        cur_nscb = schedule_list;

        while (cur_nscb != NULL) {
                LOG(("Schedule %p at %ld:%ld",
                     cur_nscb, cur_nscb->tv.tv_sec, cur_nscb->tv.tv_usec));
                cur_nscb = cur_nscb->next;
        }
}


/*
 * Local Variables:
 * c-basic-offset:8
 * End:
 */
