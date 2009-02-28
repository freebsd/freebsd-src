/*-
 * Copyright (c) 2007-2009 Sam Leffler, Errno Consulting
 * Copyright (c) 2007-2009 Intel Corporation
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
 *
 * $FreeBSD$
 */
#ifndef _NET80211_IEEE80211_TDMA_H_
#define _NET80211_IEEE80211_TDMA_H_

/*
 * TDMA-mode implementation definitions.
 */
struct ieee80211_tdma_state {
	u_int	tdma_slotlen;		/* bss slot length (us) */
	uint8_t	tdma_slotcnt;		/* bss slot count */
	uint8_t	tdma_bintval;		/* beacon interval (slots) */
	uint8_t	tdma_slot;		/* station slot # */
	uint8_t	tdma_inuse[1];		/* mask of slots in use */
#define	IEEE80211_TDMA_MAXSLOTS	8
	void	*tdma_peer;		/* peer station cookie */
	uint8_t	tdma_active[1];		/* mask of active slots */
	int	tdma_count;		/* active/inuse countdown */

	/* parent method pointers */
	int	(*tdma_newstate)(struct ieee80211vap *, enum ieee80211_state,
		    int arg);
	void	(*tdma_recv_mgmt)(struct ieee80211_node *,
		    struct mbuf *, int, int, int, uint32_t);
	void	(*tdma_opdetach)(struct ieee80211vap *);
};

void	ieee80211_tdma_vattach(struct ieee80211vap *);

int	ieee80211_tdma_getslot(struct ieee80211vap *vap);
void	ieee80211_parse_tdma(struct ieee80211_node *ni, const uint8_t *ie);
uint8_t *ieee80211_add_tdma(uint8_t *frm, struct ieee80211vap *vap);
struct ieee80211_beacon_offsets;
void	ieee80211_tdma_update_beacon(struct ieee80211vap *vap,
	    struct ieee80211_beacon_offsets *bo);
struct ieee80211req;
int	ieee80211_tdma_ioctl_get80211(struct ieee80211vap *vap,
	    struct ieee80211req *ireq);
int	ieee80211_tdma_ioctl_set80211(struct ieee80211vap *vap,
	    struct ieee80211req *ireq);
#endif /* !_NET80211_IEEE80211_TDMA_H_ */
