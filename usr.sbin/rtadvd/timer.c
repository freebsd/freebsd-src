/*	$FreeBSD$	*/
/*	$KAME: timer.c,v 1.9 2002/06/10 19:59:47 itojun Exp $	*/

/*
 * Copyright (C) 1998 WIDE Project.
 * Copyright (C) 2011 Hiroki Sato <hrs@FreeBSD.org>
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
 * 3. Neither the name of the project nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE PROJECT AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE PROJECT OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/time.h>
#include <sys/queue.h>
#include <sys/socket.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <netinet/in.h>

#include <unistd.h>
#include <syslog.h>
#include <stdlib.h>
#include <string.h>
#include <search.h>
#include <netdb.h>

#include "rtadvd.h"
#include "timer_subr.h"
#include "timer.h"

struct rtadvd_timer_head_t ra_timer =
    TAILQ_HEAD_INITIALIZER(ra_timer);
static struct timeval tm_limit = {0x7fffffff, 0x7fffffff};
static struct timeval tm_max;

void
rtadvd_timer_init(void)
{

	tm_max = tm_limit;
	TAILQ_INIT(&ra_timer);
}

void
rtadvd_update_timeout_handler(void)
{
	struct ifinfo *ifi;

	TAILQ_FOREACH(ifi, &ifilist, ifi_next) {
		switch (ifi->ifi_state) {
		case IFI_STATE_CONFIGURED:
		case IFI_STATE_TRANSITIVE:
			if (ifi->ifi_ra_timer != NULL)
				continue;

			syslog(LOG_DEBUG, "<%s> add timer for %s (idx=%d)",
			    __func__, ifi->ifi_ifname, ifi->ifi_ifindex);
			ifi->ifi_ra_timer = rtadvd_add_timer(ra_timeout,
			    ra_timer_update, ifi, ifi);
			ra_timer_update((void *)ifi,
			    &ifi->ifi_ra_timer->rat_tm);
			rtadvd_set_timer(&ifi->ifi_ra_timer->rat_tm,
			    ifi->ifi_ra_timer);
			break;
		case IFI_STATE_UNCONFIGURED:
			if (ifi->ifi_ra_timer == NULL)
				continue;

			syslog(LOG_DEBUG,
			    "<%s> remove timer for %s (idx=%d)", __func__,
			    ifi->ifi_ifname, ifi->ifi_ifindex);
			rtadvd_remove_timer(ifi->ifi_ra_timer);
			ifi->ifi_ra_timer = NULL;
			break;
		}
	}

	return;
}

struct rtadvd_timer *
rtadvd_add_timer(struct rtadvd_timer *(*timeout)(void *),
    void (*update)(void *, struct timeval *),
    void *timeodata, void *updatedata)
{
	struct rtadvd_timer *rat;

	if (timeout == NULL) {
		syslog(LOG_ERR,
		    "<%s> timeout function unspecified", __func__);
		exit(1);
	}

	rat = malloc(sizeof(*rat));
	if (rat == NULL) {
		syslog(LOG_ERR,
		    "<%s> can't allocate memory", __func__);
		exit(1);
	}
	memset(rat, 0, sizeof(*rat));

	rat->rat_expire = timeout;
	rat->rat_update = update;
	rat->rat_expire_data = timeodata;
	rat->rat_update_data = updatedata;
	rat->rat_tm = tm_max;

	/* link into chain */
	TAILQ_INSERT_TAIL(&ra_timer, rat, rat_next);

	return (rat);
}

void
rtadvd_remove_timer(struct rtadvd_timer *rat)
{

	if (rat == NULL)
		return;

	TAILQ_REMOVE(&ra_timer, rat, rat_next);
	free(rat);
}

/*
 * Check expiration for each timer. If a timer expires,
 * call the expire function for the timer and update the timer.
 * Return the next interval for select() call.
 */
struct timeval *
rtadvd_check_timer(void)
{
	static struct timeval returnval;
	struct timeval now;
	struct rtadvd_timer *rat;

	gettimeofday(&now, NULL);
	tm_max = tm_limit;
	TAILQ_FOREACH(rat, &ra_timer, rat_next) {
		if (TIMEVAL_LEQ(&rat->rat_tm, &now)) {
			if (((*rat->rat_expire)(rat->rat_expire_data) == NULL))
				continue; /* the timer was removed */
			if (rat->rat_update)
				(*rat->rat_update)(rat->rat_update_data, &rat->rat_tm);
			TIMEVAL_ADD(&rat->rat_tm, &now, &rat->rat_tm);
		}
		if (TIMEVAL_LT(&rat->rat_tm, &tm_max))
			tm_max = rat->rat_tm;
	}
	if (TIMEVAL_EQUAL(&tm_max, &tm_limit)) {
		/* no need to timeout */
		return (NULL);
	} else if (TIMEVAL_LT(&tm_max, &now)) {
		/* this may occur when the interval is too small */
		returnval.tv_sec = returnval.tv_usec = 0;
	} else
		TIMEVAL_SUB(&tm_max, &now, &returnval);
	return (&returnval);
}

void
rtadvd_set_timer(struct timeval *tm, struct rtadvd_timer *rat)
{
	struct timeval now;

	/* reset the timer */
	gettimeofday(&now, NULL);
	TIMEVAL_ADD(&now, tm, &rat->rat_tm);

	/* update the next expiration time */
	if (TIMEVAL_LT(&rat->rat_tm, &tm_max))
		tm_max = rat->rat_tm;

	return;
}
