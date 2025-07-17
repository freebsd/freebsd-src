/*	$OpenBSD: if_urtwn.c,v 1.16 2011/02/10 17:26:40 jakemsr Exp $	*/

/*-
 * Copyright (c) 2010 Damien Bergamini <damien.bergamini@free.fr>
 * Copyright (c) 2014 Kevin Lo <kevlo@FreeBSD.org>
 * Copyright (c) 2015-2016 Andriy Voskoboinyk <avos@FreeBSD.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/cdefs.h>
#include "opt_wlan.h"

#include <sys/param.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/mbuf.h>
#include <sys/kernel.h>
#include <sys/socket.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/queue.h>
#include <sys/taskqueue.h>
#include <sys/bus.h>
#include <sys/endian.h>

#include <net/if.h>
#include <net/if_var.h>
#include <net/ethernet.h>
#include <net/if_media.h>

#include <net80211/ieee80211_var.h>
#include <net80211/ieee80211_radiotap.h>
#include <net80211/ieee80211_ratectl.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>

#include <dev/rtwn/if_rtwnreg.h>
#include <dev/rtwn/if_rtwnvar.h>

#include <dev/rtwn/if_rtwn_beacon.h>
#include <dev/rtwn/if_rtwn_debug.h>
#include <dev/rtwn/if_rtwn_ridx.h>
#include <dev/rtwn/if_rtwn_task.h>
#include <dev/rtwn/if_rtwn_tx.h>

#include <dev/rtwn/usb/rtwn_usb_var.h>

#include <dev/rtwn/usb/rtwn_usb_reg.h>
#include <dev/rtwn/usb/rtwn_usb_tx.h>

static struct rtwn_data * _rtwn_usb_getbuf(struct rtwn_usb_softc *);
static struct rtwn_data * rtwn_usb_getbuf(struct rtwn_usb_softc *);
static void		rtwn_usb_txeof(struct rtwn_usb_softc *,
			    struct rtwn_data *, int);

static struct rtwn_data *
_rtwn_usb_getbuf(struct rtwn_usb_softc *uc)
{
	struct rtwn_softc *sc = &uc->uc_sc;
	struct rtwn_data *bf;

	bf = STAILQ_FIRST(&uc->uc_tx_inactive);
	if (bf != NULL)
		STAILQ_REMOVE_HEAD(&uc->uc_tx_inactive, next);
	else {
		RTWN_DPRINTF(sc, RTWN_DEBUG_XMIT,
		    "%s: out of xmit buffers\n", __func__);
	}
	return (bf);
}

static struct rtwn_data *
rtwn_usb_getbuf(struct rtwn_usb_softc *uc)
{
	struct rtwn_softc *sc = &uc->uc_sc;
	struct rtwn_data *bf;

	RTWN_ASSERT_LOCKED(sc);

	bf = _rtwn_usb_getbuf(uc);
	if (bf == NULL) {
		RTWN_DPRINTF(sc, RTWN_DEBUG_XMIT, "%s: stop queue\n",
		    __func__);
	}
	return (bf);
}

static void
rtwn_usb_txeof(struct rtwn_usb_softc *uc, struct rtwn_data *data, int status)
{
	struct rtwn_softc *sc = &uc->uc_sc;
	bool is_empty = true;

	RTWN_ASSERT_LOCKED(sc);

	if (data->ni != NULL)	/* not a beacon frame */
		ieee80211_tx_complete(data->ni, data->m, status);

	if (sc->sc_ratectl != RTWN_RATECTL_NET80211)
		if (sc->sc_tx_n_active > 0)
			sc->sc_tx_n_active--;

	data->ni = NULL;
	data->m = NULL;

	STAILQ_INSERT_TAIL(&uc->uc_tx_inactive, data, next);
	sc->qfullmsk = 0;

#ifndef D4054
	for (int i = RTWN_BULK_TX_FIRST; i < RTWN_BULK_EP_COUNT; i++) {
		if (!STAILQ_EMPTY(&uc->uc_tx_active[i]) ||
		    !STAILQ_EMPTY(&uc->uc_tx_pending[i]))
			is_empty = false;
	}

	if (is_empty)
		sc->sc_tx_timer = 0;
	else
		sc->sc_tx_timer = 5;
#endif
}

static void
rtwn_bulk_tx_callback_qid(struct usb_xfer *xfer, usb_error_t error, int qid)
{
	struct rtwn_usb_softc *uc = usbd_xfer_softc(xfer);
	struct rtwn_softc *sc = &uc->uc_sc;
	struct rtwn_data *data;
	bool do_is_empty_check = false;
	int i;

	RTWN_DPRINTF(sc, RTWN_DEBUG_XMIT,
	    "%s: called, qid=%d\n", __func__, qid);

	RTWN_ASSERT_LOCKED(sc);

	switch (USB_GET_STATE(xfer)){
	case USB_ST_TRANSFERRED:
		data = STAILQ_FIRST(&uc->uc_tx_active[qid]);
		if (data == NULL)
			goto tr_setup;
		STAILQ_REMOVE_HEAD(&uc->uc_tx_active[qid], next);
		rtwn_usb_txeof(uc, data, 0);
		/* FALLTHROUGH */
	case USB_ST_SETUP:
tr_setup:
		data = STAILQ_FIRST(&uc->uc_tx_pending[qid]);
		if (data == NULL) {
			RTWN_DPRINTF(sc, RTWN_DEBUG_XMIT,
			    "%s: empty pending queue\n", __func__);
			do_is_empty_check = true;
			goto finish;
		}
		STAILQ_REMOVE_HEAD(&uc->uc_tx_pending[qid], next);
		STAILQ_INSERT_TAIL(&uc->uc_tx_active[qid], data, next);

		/*
		 * Note: if this is a beacon frame, ensure that it will go
		 * into appropriate queue.
		 */
		if (data->ni == NULL && RTWN_CHIP_HAS_BCNQ1(sc))
			rtwn_switch_bcnq(sc, data->id);
		usbd_xfer_set_frame_data(xfer, 0, data->buf, data->buflen);
		usbd_transfer_submit(xfer);
		if (sc->sc_ratectl != RTWN_RATECTL_NET80211)
			sc->sc_tx_n_active++;
		break;
	default:
		data = STAILQ_FIRST(&uc->uc_tx_active[qid]);
		if (data == NULL)
			goto tr_setup;
		STAILQ_REMOVE_HEAD(&uc->uc_tx_active[qid], next);
		rtwn_usb_txeof(uc, data, 1);
		if (error != 0)
			device_printf(sc->sc_dev,
			    "%s: called; txeof qid=%d, error=%s\n",
			    __func__,
			    qid,
			    usbd_errstr(error));
		if (error != USB_ERR_CANCELLED) {
			usbd_xfer_set_stall(xfer);
			goto tr_setup;
		}
		break;
	}
finish:

	/*
	 * Clear sc_tx_n_active if all the pending transfers are 0.
	 *
	 * This is currently a crutch because net80211 doesn't provide
	 * a way to defer all the FF checks or one of the FF checks.
	 * Eventually this should just be tracked per-endpoint.
	 */
	for (i = RTWN_BULK_TX_FIRST; i < RTWN_BULK_EP_COUNT; i++)
		if (STAILQ_FIRST(&uc->uc_tx_pending[i]) != NULL)
			do_is_empty_check = false;
	if (do_is_empty_check)
		sc->sc_tx_n_active = 0;
#ifdef	IEEE80211_SUPPORT_SUPERG
	/*
	 * If the TX active queue drops below a certain
	 * threshold, ensure we age fast-frames out so they're
	 * transmitted.
	 */
	if (sc->sc_ratectl != RTWN_RATECTL_NET80211 &&
	    sc->sc_tx_n_active <= 1) {
		/* XXX ew - net80211 should defer this for us! */

		/*
		 * Note: this sc_tx_n_active currently tracks
		 * the number of pending transmit submissions
		 * and not the actual depth of the TX frames
		 * pending to the hardware.  That means that
		 * we're going to end up with some sub-optimal
		 * aggregation behaviour.
		 */
		/*
		 * XXX TODO: just make this a callout timer schedule so we can
		 * flush the FF staging queue if we're approaching idle.
		 */
		rtwn_cmd_sleepable(sc, NULL, 0, rtwn_ff_flush_all);
	}
#endif
	/* Kick-start more transmit */
	rtwn_start(sc);
}

void
rtwn_bulk_tx_callback_be(struct usb_xfer *xfer, usb_error_t error)
{

	rtwn_bulk_tx_callback_qid(xfer, error, RTWN_BULK_TX_BE);
}

void
rtwn_bulk_tx_callback_bk(struct usb_xfer *xfer, usb_error_t error)
{

	rtwn_bulk_tx_callback_qid(xfer, error, RTWN_BULK_TX_BK);
}

void
rtwn_bulk_tx_callback_vi(struct usb_xfer *xfer, usb_error_t error)
{

	rtwn_bulk_tx_callback_qid(xfer, error, RTWN_BULK_TX_VI);
}

void
rtwn_bulk_tx_callback_vo(struct usb_xfer *xfer, usb_error_t error)
{

	rtwn_bulk_tx_callback_qid(xfer, error, RTWN_BULK_TX_VO);
}

static void
rtwn_usb_tx_checksum(struct rtwn_tx_desc_common *txd)
{
	txd->txdw7.usb_checksum = 0;
	txd->txdw7.usb_checksum = rtwn_usb_calc_tx_checksum(txd);
}

int
rtwn_usb_tx_start(struct rtwn_softc *sc, struct ieee80211_node *ni,
    struct mbuf *m, uint8_t *tx_desc, uint8_t type, int id)
{
	struct rtwn_usb_softc *uc = RTWN_USB_SOFTC(sc);
	struct rtwn_tx_desc_common *txd;
	struct rtwn_data *data;
	struct usb_xfer *xfer;
	uint16_t ac;
	int qid = 0;

	RTWN_ASSERT_LOCKED(sc);

	if (m->m_pkthdr.len + sc->txdesc_len > RTWN_USB_TXBUFSZ)
		return (EINVAL);

	data = rtwn_usb_getbuf(uc);
	if (data == NULL)
		return (ENOBUFS);

	/* TODO: should really get a consistent AC/TID, ath(4) style */
	ac = M_WME_GETAC(m);

	switch (type) {
	case IEEE80211_FC0_TYPE_CTL:
	case IEEE80211_FC0_TYPE_MGT:
		qid = RTWN_BULK_TX_VO;
		break;
	default:
		qid = uc->wme2qid[ac];
		break;
	}
	xfer = uc->uc_xfer[qid];

	RTWN_DPRINTF(sc, RTWN_DEBUG_XMIT,
	    "%s: called, ac=%d, qid=%d, xfer=%p\n",
	    __func__, ac, qid, xfer);

	txd = (struct rtwn_tx_desc_common *)tx_desc;
	txd->pktlen = htole16(m->m_pkthdr.len);
	txd->offset = sc->txdesc_len;
	txd->flags0 |= RTWN_FLAGS0_OWN;
	rtwn_usb_tx_checksum(txd);

	/* Dump Tx descriptor. */
	rtwn_dump_tx_desc(sc, tx_desc);

	memcpy(data->buf, tx_desc, sc->txdesc_len);
	m_copydata(m, 0, m->m_pkthdr.len,
	    (caddr_t)(data->buf + sc->txdesc_len));

	data->buflen = m->m_pkthdr.len + sc->txdesc_len;
	data->id = id;
	data->ni = ni;
	data->qid = qid;
	if (data->ni != NULL) {
		data->m = m;
#ifndef D4054
		sc->sc_tx_timer = 5;
#endif
	}

	STAILQ_INSERT_TAIL(&uc->uc_tx_pending[qid], data, next);
	if (STAILQ_EMPTY(&uc->uc_tx_inactive))
		sc->qfullmsk = 1;

	usbd_transfer_start(xfer);

	return (0);
}
