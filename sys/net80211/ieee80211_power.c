/*-
 * Copyright (c) 2002-2007 Sam Leffler, Errno Consulting
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
__FBSDID("$FreeBSD: src/sys/net80211/ieee80211_power.c,v 1.2.6.1 2008/11/25 02:59:29 kensmith Exp $");

/*
 * IEEE 802.11 power save support.
 */
#include <sys/param.h>
#include <sys/systm.h> 
#include <sys/kernel.h>
 
#include <sys/socket.h>

#include <net/if.h>
#include <net/if_media.h>
#include <net/ethernet.h>

#include <net80211/ieee80211_var.h>

#include <net/bpf.h>

static void ieee80211_set_tim(struct ieee80211_node *ni, int set);

void
ieee80211_power_attach(struct ieee80211com *ic)
{
	if (ic->ic_opmode == IEEE80211_M_HOSTAP ||
	    ic->ic_opmode == IEEE80211_M_IBSS) {
		/* NB: driver should override */
		ic->ic_set_tim = ieee80211_set_tim;
	}
}

void
ieee80211_power_lateattach(struct ieee80211com *ic)
{
	/*
	 * Allocate these only if needed.  Beware that we
	 * know adhoc mode doesn't support ATIM yet...
	 */
	if (ic->ic_opmode == IEEE80211_M_HOSTAP) {
		ic->ic_tim_len = howmany(ic->ic_max_aid,8) * sizeof(uint8_t);
		MALLOC(ic->ic_tim_bitmap, uint8_t *, ic->ic_tim_len,
			M_DEVBUF, M_NOWAIT | M_ZERO);
		if (ic->ic_tim_bitmap == NULL) {
			printf("%s: no memory for TIM bitmap!\n", __func__);
			/* XXX good enough to keep from crashing? */
			ic->ic_tim_len = 0;
		}
	}
}

void
ieee80211_power_detach(struct ieee80211com *ic)
{
	if (ic->ic_tim_bitmap != NULL) {
		FREE(ic->ic_tim_bitmap, M_DEVBUF);
		ic->ic_tim_bitmap = NULL;
	}
}

/*
 * Clear any frames queued on a node's power save queue.
 * The number of frames that were present is returned.
 */
int
ieee80211_node_saveq_drain(struct ieee80211_node *ni)
{
	int qlen;

	IEEE80211_NODE_SAVEQ_LOCK(ni);
	qlen = IEEE80211_NODE_SAVEQ_QLEN(ni);
	_IF_DRAIN(&ni->ni_savedq);
	IEEE80211_NODE_SAVEQ_UNLOCK(ni);

	return qlen;
}

/*
 * Age frames on the power save queue. The aging interval is
 * 4 times the listen interval specified by the station.  This
 * number is factored into the age calculations when the frame
 * is placed on the queue.  We store ages as time differences
 * so we can check and/or adjust only the head of the list.
 * If a frame's age exceeds the threshold then discard it.
 * The number of frames discarded is returned so the caller
 * can check if it needs to adjust the tim.
 */
int
ieee80211_node_saveq_age(struct ieee80211_node *ni)
{
	int discard = 0;

	if (IEEE80211_NODE_SAVEQ_QLEN(ni) != 0) {
		struct mbuf *m;

		IEEE80211_NODE_SAVEQ_LOCK(ni);
		while (IF_POLL(&ni->ni_savedq, m) != NULL &&
		     M_AGE_GET(m) < IEEE80211_INACT_WAIT) {
IEEE80211_DPRINTF(ni->ni_ic, IEEE80211_MSG_POWER, "[%s] discard frame, age %u\n", ether_sprintf(ni->ni_macaddr), M_AGE_GET(m));/*XXX*/
			_IEEE80211_NODE_SAVEQ_DEQUEUE_HEAD(ni, m);
			m_freem(m);
			discard++;
		}
		if (m != NULL)
			M_AGE_SUB(m, IEEE80211_INACT_WAIT);
		IEEE80211_NODE_SAVEQ_UNLOCK(ni);

		IEEE80211_NOTE(ni->ni_ic, IEEE80211_MSG_POWER, ni,
		    "discard %u frames for age", discard);
		IEEE80211_NODE_STAT_ADD(ni, ps_discard, discard);
	}
	return discard;
}

/*
 * Indicate whether there are frames queued for a station in power-save mode.
 */
static void
ieee80211_set_tim(struct ieee80211_node *ni, int set)
{
	struct ieee80211com *ic = ni->ni_ic;
	uint16_t aid;

	KASSERT(ic->ic_opmode == IEEE80211_M_HOSTAP ||
		ic->ic_opmode == IEEE80211_M_IBSS,
		("operating mode %u", ic->ic_opmode));

	aid = IEEE80211_AID(ni->ni_associd);
	KASSERT(aid < ic->ic_max_aid,
		("bogus aid %u, max %u", aid, ic->ic_max_aid));

	IEEE80211_BEACON_LOCK(ic);
	if (set != (isset(ic->ic_tim_bitmap, aid) != 0)) {
		if (set) {
			setbit(ic->ic_tim_bitmap, aid);
			ic->ic_ps_pending++;
		} else {
			clrbit(ic->ic_tim_bitmap, aid);
			ic->ic_ps_pending--;
		}
		/* NB: we know ic is in RUN state so no need to check */
		ic->ic_update_beacon(ic, IEEE80211_BEACON_TIM);
	}
	IEEE80211_BEACON_UNLOCK(ic);
}

/*
 * Save an outbound packet for a node in power-save sleep state.
 * The new packet is placed on the node's saved queue, and the TIM
 * is changed, if necessary.
 */
void
ieee80211_pwrsave(struct ieee80211_node *ni, struct mbuf *m)
{
	struct ieee80211com *ic = ni->ni_ic;
	int qlen, age;

	IEEE80211_NODE_SAVEQ_LOCK(ni);
	if (_IF_QFULL(&ni->ni_savedq)) {
		_IF_DROP(&ni->ni_savedq);
		IEEE80211_NODE_SAVEQ_UNLOCK(ni);
		IEEE80211_DPRINTF(ic, IEEE80211_MSG_ANY,
			"[%s] pwr save q overflow, drops %d (size %d)\n",
			ether_sprintf(ni->ni_macaddr), 
			ni->ni_savedq.ifq_drops, IEEE80211_PS_MAX_QUEUE);
#ifdef IEEE80211_DEBUG
		if (ieee80211_msg_dumppkts(ic))
			ieee80211_dump_pkt(ic, mtod(m, caddr_t), m->m_len, -1, -1);
#endif
		m_freem(m);
		return;
	}
	/*
	 * Tag the frame with it's expiry time and insert
	 * it in the queue.  The aging interval is 4 times
	 * the listen interval specified by the station. 
	 * Frames that sit around too long are reclaimed
	 * using this information.
	 */
	/* TU -> secs.  XXX handle overflow? */
	age = IEEE80211_TU_TO_MS((ni->ni_intval * ic->ic_bintval) << 2) / 1000;
	_IEEE80211_NODE_SAVEQ_ENQUEUE(ni, m, qlen, age);
	IEEE80211_NODE_SAVEQ_UNLOCK(ni);

	IEEE80211_DPRINTF(ic, IEEE80211_MSG_POWER,
		"[%s] save frame with age %d, %u now queued\n",
		ether_sprintf(ni->ni_macaddr), age, qlen);

	if (qlen == 1 && ic->ic_set_tim != NULL)
		ic->ic_set_tim(ni, 1);
}

/*
 * Handle station power-save state change.
 */
void
ieee80211_node_pwrsave(struct ieee80211_node *ni, int enable)
{
	struct ieee80211com *ic = ni->ni_ic;
	struct mbuf *m, *mhead, *mtail;
	int mcount;

	if (enable) {
		if ((ni->ni_flags & IEEE80211_NODE_PWR_MGT) == 0)
			ic->ic_ps_sta++;
		ni->ni_flags |= IEEE80211_NODE_PWR_MGT;
		IEEE80211_NOTE(ic, IEEE80211_MSG_POWER, ni,
		    "power save mode on, %u sta's in ps mode", ic->ic_ps_sta);
		return;
	}

	if (ni->ni_flags & IEEE80211_NODE_PWR_MGT)
		ic->ic_ps_sta--;
	ni->ni_flags &= ~IEEE80211_NODE_PWR_MGT;
	IEEE80211_NOTE(ic, IEEE80211_MSG_POWER, ni,
	    "power save mode off, %u sta's in ps mode", ic->ic_ps_sta);
	/* XXX if no stations in ps mode, flush mc frames */

	/*
	 * Flush queued unicast frames.
	 */
	if (IEEE80211_NODE_SAVEQ_QLEN(ni) == 0) {
		if (ic->ic_set_tim != NULL)
			ic->ic_set_tim(ni, 0);		/* just in case */
		return;
	}
	IEEE80211_NOTE(ic, IEEE80211_MSG_POWER, ni,
	    "flush ps queue, %u packets queue", IEEE80211_NODE_SAVEQ_QLEN(ni));
	/*
	 * Unload the frames from the ps q but don't send them
	 * to the driver yet.  We do this in two stages to minimize
	 * locking but also because there's no easy way to preserve
	 * ordering given the existing ifnet access mechanisms.
	 * XXX could be optimized
	 */
	IEEE80211_NODE_SAVEQ_LOCK(ni);
	mcount = IEEE80211_NODE_SAVEQ_QLEN(ni);
	mhead = mtail = NULL;
	for (;;) {
		_IEEE80211_NODE_SAVEQ_DEQUEUE_HEAD(ni, m);
		if (m == NULL)
			break;
		if (mhead == NULL) {
			mhead = m;
			m->m_nextpkt = NULL;
		} else
			mtail->m_nextpkt = m;
		mtail = m;
	}
	IEEE80211_NODE_SAVEQ_UNLOCK(ni);
	if (mhead != NULL) {
		/* XXX need different driver interface */
		/* XXX bypasses q max */
		IF_PREPEND_LIST(&ic->ic_ifp->if_snd, mhead, mtail, mcount);
	}
	if (ic->ic_set_tim != NULL)
		ic->ic_set_tim(ni, 0);
}

/*
 * Handle power-save state change in station mode.
 */
void
ieee80211_sta_pwrsave(struct ieee80211com *ic, int enable)
{
	struct ieee80211_node *ni = ic->ic_bss;
	int qlen;

	if (!((enable != 0) ^ ((ni->ni_flags & IEEE80211_NODE_PWR_MGT) != 0)))
		return;

	IEEE80211_NOTE(ic, IEEE80211_MSG_POWER, ni,
	    "sta power save mode %s", enable ? "on" : "off");
	if (!enable) {
		ni->ni_flags &= ~IEEE80211_NODE_PWR_MGT;
		ieee80211_send_nulldata(ieee80211_ref_node(ni));
		/*
		 * Flush any queued frames; we can do this immediately
		 * because we know they'll be queued behind the null
		 * data frame we send the ap.
		 * XXX can we use a data frame to take us out of ps?
		 */
		qlen = IEEE80211_NODE_SAVEQ_QLEN(ni);
		if (qlen != 0) {
			IEEE80211_NOTE(ic, IEEE80211_MSG_POWER, ni,
			    "flush ps queue, %u packets queued", qlen);
			for (;;) {
				struct mbuf *m;

				IEEE80211_NODE_SAVEQ_LOCK(ni);
				_IEEE80211_NODE_SAVEQ_DEQUEUE_HEAD(ni, m);
				IEEE80211_NODE_SAVEQ_UNLOCK(ni);
				if (m == NULL)
					break;
				/* XXX need different driver interface */
				/* XXX bypasses q max */
				IF_ENQUEUE(&ic->ic_ifp->if_snd, m);
			}
		}
	} else {
		ni->ni_flags |= IEEE80211_NODE_PWR_MGT;
		ieee80211_send_nulldata(ieee80211_ref_node(ni));
	}
}
