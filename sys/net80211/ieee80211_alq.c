/*-
 * Copyright (c) 2011 Adrian Chadd, Xenion Lty Ltd
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
 */

#include <sys/cdefs.h>
#ifdef __FreeBSD__
__FBSDID("$FreeBSD$");
#endif

/*
 * net80211 fast-logging support, primarily for debugging.
 *
 * This implements a single debugging queue which includes
 * per-device enumeration where needed.
 */

#include "opt_wlan.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/malloc.h>
#include <sys/endian.h>
#include <sys/kernel.h>
#include <sys/sysctl.h>
#include <sys/pcpu.h>
#include <sys/proc.h>
#include <sys/ucred.h>
#include <sys/alq.h>

#include <sys/socket.h>

#include <net/if.h>
#include <net/if_var.h>
#include <net/if_dl.h>
#include <net/if_clone.h>
#include <net/if_media.h>
#include <net/if_types.h>

#include <net80211/ieee80211_var.h>
#include <net80211/ieee80211_freebsd.h>
#include <net80211/ieee80211_alq.h>

static struct alq *ieee80211_alq;
static int ieee80211_alq_lost;
static char ieee80211_alq_logfile[MAXPATHLEN] = "/tmp/net80211.log";
static unsigned int ieee80211_alq_qsize = 64*1024;

static int
ieee80211_alq_setlogging(int enable)
{
	int error;

	if (enable) {
		if (ieee80211_alq)
			alq_close(ieee80211_alq);

		error = alq_open(&ieee80211_alq,
		    ieee80211_alq_logfile,
		    curthread->td_ucred,
		    ALQ_DEFAULT_CMODE,
		    sizeof (struct ieee80211_alq_rec),
		    ieee80211_alq_qsize);
		ieee80211_alq_lost = 0;
		printf("net80211: logging to %s enabled\n", ieee80211_alq_logfile);
	} else {
		if (ieee80211_alq)
			alq_close(ieee80211_alq);
		ieee80211_alq = NULL;
		printf("net80211: logging disabled\n");
		error = 0;
	}
	return (error);
}

static int
sysctl_ieee80211_alq_log(SYSCTL_HANDLER_ARGS)
{
        int error, enable;

        enable = (ieee80211_alq != NULL);
        error = sysctl_handle_int(oidp, &enable, 0, req);
        if (error || !req->newptr)
                return (error);
        else   
                return (ieee80211_alq_setlogging(enable));
}

SYSCTL_PROC(_net_wlan, OID_AUTO, alq, CTLTYPE_INT|CTLFLAG_RW,
	0, 0, sysctl_ieee80211_alq_log, "I", "Enable net80211 alq logging");
SYSCTL_INT(_net_wlan, OID_AUTO, alq_size, CTLFLAG_RW,
	&ieee80211_alq_qsize, 0, "In-memory log size (#records)");
SYSCTL_INT(_net_wlan, OID_AUTO, alq_lost, CTLFLAG_RW,
	&ieee80211_alq_lost, 0, "Debugging operations not logged");

static struct ale *
ieee80211_alq_get(void)
{
	struct ale *ale;

	ale = alq_get(ieee80211_alq, ALQ_NOWAIT);
	if (!ale)
		ieee80211_alq_lost++;
	return ale;
}

void
ieee80211_alq_log(struct ieee80211vap *vap, uint8_t op, u_char *p, int l)
{
	struct ale *ale;
	struct ieee80211_alq_rec *r;

	ale = ieee80211_alq_get();
	if (! ale)
		return;

	r = (struct ieee80211_alq_rec *) ale;
	r->r_timestamp = ticks;
	r->r_version = 1;
	r->r_wlan = vap->iv_ifp->if_dunit;
	r->r_op = op;
	memcpy(&r->r_payload, p, MIN(l, sizeof(r->r_payload)));
}
