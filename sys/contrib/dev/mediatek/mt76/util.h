/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2020,2022-2023 Bjoern A. Zeeb <bz@FreeBSD.org>
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
 *
 * $FreeBSD$
 */

#ifndef	_MT76_UTIL_H
#define	_MT76_UTIL_H

#include <linux/kthread.h>

struct mt76_worker
{
	void(*fn)(struct mt76_worker *);
	struct task_struct *task;
	unsigned long state;
};

enum mt76_worker_state {
	MT76_WORKER_SCHEDULED,
	MT76_WORKER_RUNNING,
};

#if 0
bool __mt76_poll(struct mt76_dev *, u32, u32, u32, int);
bool __mt76_poll_msec(struct mt76_dev *, u32, u32, u32, int);
int mt76_get_min_avg_rssi(struct mt76_dev *, bool);
#endif
int mt76_wcid_alloc(u32 *, int);
int __mt76_worker_fn(void *);

/* wcid_phy_mask is [32] */
static inline void
mt76_wcid_mask_set(u32 *mask, u16 bit)
{

	mask[bit / 32] |= BIT(bit % 32);
}

static inline void
mt76_wcid_mask_clear(u32 *mask, u16 bit)
{

	mask[bit / 32] &= ~BIT(bit % 32);
}

/* See, e.g., __mt76_worker_fn for some details. */
static inline int
mt76_worker_setup(struct ieee80211_hw *hw, struct mt76_worker *w,
    void (*wfunc)(struct mt76_worker *), const char *name)
{
	int rc;

	if (wfunc)
		w->fn = wfunc;

	w->task = kthread_run(__mt76_worker_fn, w,
	    "mt76-worker");

	if (!IS_ERR(w->task))
		return (0);

	rc = PTR_ERR(w->task);
	w->task = NULL;
	return (rc);
}

static inline void
mt76_worker_schedule(struct mt76_worker *w)
{

	if (w->task == NULL)
		return;

	if (!test_and_set_bit(MT76_WORKER_SCHEDULED, &w->state) ||
	    !test_bit(MT76_WORKER_RUNNING, &w->state))
		wake_up_process(w->task);
}

static inline void
mt76_worker_enable(struct mt76_worker *w)
{

	if (w->task == NULL)
		return;

	kthread_unpark(w->task);
	mt76_worker_schedule(w);
}

static inline void
mt76_worker_disable(struct mt76_worker *w)
{

	if (w->task == NULL)
		return;

	kthread_park(w->task);
	WRITE_ONCE(w->state, 0);
}

static inline void
mt76_worker_teardown(struct mt76_worker *w)
{

	if (w->task == NULL)
		return;

	kthread_stop(w->task);
	w->task = NULL;
}

static inline void
mt76_skb_set_moredata(struct sk_buff *skb, bool moredata)
{
	/*
	 * This would be net80211::IEEE80211_FC1_MORE_DATA
	 * Implement it as mostly LinuxKPI 802.11 to avoid
	 * further header pollution and possible conflicts.
	 */
	struct ieee80211_hdr *hdr;
	uint16_t val;

	hdr = (struct ieee80211_hdr *)skb->data;
	val = cpu_to_le16(IEEE80211_FC1_MORE_DATA << 8);
	if (!moredata)
		hdr->frame_control &= ~val;
	else
		hdr->frame_control |= val;
}

#endif	/* _MT76_UTIL_H */
