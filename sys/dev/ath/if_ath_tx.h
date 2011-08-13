/*-
 * Copyright (c) 2002-2009 Sam Leffler, Errno Consulting
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer,
 *    without modification.
 * 2. Redistributions in binary form must reproduce at minimum a disclaimer
 *    similar to the "NO WARRANTY" disclaimer below ("Disclaimer") and any
 *    redistribution must be conditioned upon including a substantially
 *    similar Disclaimer requirement for further binary redistribution.
 *
 * NO WARRANTY
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF NONINFRINGEMENT, MERCHANTIBILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
 * THE COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
 * IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGES.
 *
 * $FreeBSD$
 */
#ifndef	__IF_ATH_TX_H__
#define	__IF_ATH_TX_H__

extern void ath_freetx(struct mbuf *m);
extern void ath_tx_node_flush(struct ath_softc *sc, struct ath_node *an);
extern void ath_txfrag_cleanup(struct ath_softc *sc, ath_bufhead *frags,
    struct ieee80211_node *ni);
extern int ath_txfrag_setup(struct ath_softc *sc, ath_bufhead *frags,
    struct mbuf *m0, struct ieee80211_node *ni);
extern int ath_tx_start(struct ath_softc *sc, struct ieee80211_node *ni,
    struct ath_buf *bf, struct mbuf *m0);
extern int ath_raw_xmit(struct ieee80211_node *ni, struct mbuf *m,
    const struct ieee80211_bpf_params *params);

/* software queue stuff */
extern void ath_tx_swq(struct ath_softc *sc, struct ieee80211_node *ni,
    struct ath_txq *txq, struct ath_buf *bf);
extern void ath_tx_tid_init(struct ath_softc *sc, struct ath_node *an);
extern void ath_tx_tid_cleanup(struct ath_softc *sc, struct ath_node *an);
extern void ath_tx_tid_hw_queue_aggr(struct ath_softc *sc, struct ath_node *an,
    int tid);
extern void ath_tx_tid_hw_queue_norm(struct ath_softc *sc, struct ath_node *an,
    int tid);
extern void ath_txq_sched(struct ath_softc *sc, struct ath_txq *txq);

/* TX addba handling */
extern	int ath_addba_request(struct ieee80211_node *ni,
    struct ieee80211_tx_ampdu *tap, int dialogtoken,
    int baparamset, int batimeout);
extern	int ath_addba_response(struct ieee80211_node *ni,
    struct ieee80211_tx_ampdu *tap, int dialogtoken,
    int code, int batimeout);
extern	void ath_addba_stop(struct ieee80211_node *ni,
    struct ieee80211_tx_ampdu *tap);

#endif
