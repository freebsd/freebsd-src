/*-
 * Copyright (c) 2009 Sam Leffler, Errno Consulting
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
#ifndef _NET80211_IEEE80211_SUPERG_H_
#define _NET80211_IEEE80211_SUPERG_H_

/*
 * Atheros' 802.11 SuperG protocol support.
 */

void	ieee80211_superg_attach(struct ieee80211com *);
void	ieee80211_superg_detach(struct ieee80211com *);
void	ieee80211_superg_vattach(struct ieee80211vap *);
void	ieee80211_superg_vdetach(struct ieee80211vap *);

uint8_t *ieee80211_add_ath(uint8_t *, uint8_t, ieee80211_keyix);
uint8_t *ieee80211_add_athcaps(uint8_t *, const struct ieee80211_node *);
void	ieee80211_parse_ath(struct ieee80211_node *, uint8_t *);
int	ieee80211_parse_athparams(struct ieee80211_node *, uint8_t *,
	    const struct ieee80211_frame *);

struct mbuf *ieee80211_ff_encap(struct ieee80211vap *, struct mbuf *,
	    int, struct ieee80211_key *);

struct mbuf *ieee80211_ff_decap(struct ieee80211_node *, struct mbuf *);

static __inline struct mbuf *
ieee80211_decap_fastframe(struct ieee80211vap *vap, struct ieee80211_node *ni,
    struct mbuf *m)
{
	return IEEE80211_ATH_CAP(vap, ni, IEEE80211_NODE_FF) ?
	    ieee80211_ff_decap(ni, m) : m;
}
#endif /* _NET80211_IEEE80211_SUPERG_H_ */
