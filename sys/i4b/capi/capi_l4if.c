/*
 * Copyright (c) 2001 Cubical Solutions Ltd. All rights reserved.
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
 * capi/capi_l4if.c	The CAPI i4b L4/device interface.
 *
 * $FreeBSD$
 */

#include "i4bcapi.h"
#if NI4BCAPI > 0

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <net/if.h>

#include <machine/i4b_debug.h>
#include <machine/i4b_ioctl.h>
#include <machine/i4b_cause.h>

#include <i4b/include/i4b_l3l4.h>
#include <i4b/include/i4b_mbuf.h>
#include <i4b/include/i4b_global.h>

#include <i4b/layer4/i4b_l4.h>

#include <i4b/capi/capi.h>
#include <i4b/capi/capi_msgs.h>

static void n_connect_request(u_int cdid);
static void n_connect_response(u_int cdid, int response, int cause);
static void n_disconnect_request(u_int cdid, int cause);
static void n_alert_request(u_int cdid);
static void n_mgmt_command(int unit, int cmd, void *parm);
static int  n_download(int unit, int, struct isdn_dr_prot *);

capi_softc_t *capi_sc[MAX_CONTROLLERS] = { NULL, };
int ncapi = 0;

/*
//  i4b_capi_{ret,set}_linktab
//      i4b driver glue.
//
//  i4b_capi_bch_config
//      Called by i4b driver to flush + {en,dis}able a channel.
//
//  i4b_capi_bch_start_tx
//      Called by i4b driver to transmit a queued mbuf.
//
//  i4b_capi_bch_stat
//      Called by i4b driver to obtain statistics information.
*/

static isdn_link_t *
i4b_capi_ret_linktab(int unit, int channel)
{
    capi_softc_t *sc = capi_sc[unit];
    return &sc->sc_bchan[channel].capi_isdn_linktab;
}

static void
i4b_capi_set_linktab(int unit, int channel, drvr_link_t *dlt)
{
    capi_softc_t *sc = capi_sc[unit];
    sc->sc_bchan[channel].capi_drvr_linktab = dlt;
}

static void
i4b_capi_bch_config(int unit, int chan, int bprot, int activate)
{
    capi_softc_t *sc = capi_sc[unit];

    i4b_Bcleanifq(&sc->sc_bchan[chan].tx_queue);
    sc->sc_bchan[chan].tx_queue.ifq_maxlen = IFQ_MAXLEN;
    sc->sc_bchan[chan].txcount = 0;

    /* The telephony drivers use rx_queue for receive. */

    i4b_Bcleanifq(&sc->sc_bchan[chan].rx_queue);
    sc->sc_bchan[chan].rx_queue.ifq_maxlen = IFQ_MAXLEN;
    sc->sc_bchan[chan].rxcount = 0;

    /* HDLC frames are put to in_mbuf */

    i4b_Bfreembuf(sc->sc_bchan[chan].in_mbuf);
    sc->sc_bchan[chan].in_mbuf = NULL;

    /* Because of the difference, we need to remember the protocol. */

    sc->sc_bchan[chan].bprot = bprot;
    sc->sc_bchan[chan].busy = 0;
}

static void
i4b_capi_bch_start_tx(int unit, int chan)
{
    capi_softc_t *sc = capi_sc[unit];
    int s;

    s = SPLI4B();

    if (sc->sc_bchan[chan].state != B_CONNECTED) {
	splx(s);
	printf("capi%d: start_tx on unconnected channel\n", sc->sc_unit);
	return;
    }

    if (sc->sc_bchan[chan].busy) {
	splx(s);
	return;
    }

    capi_start_tx(sc, chan);

    splx(s);
}

static void
i4b_capi_bch_stat(int unit, int chan, bchan_statistics_t *bsp)
{
    capi_softc_t *sc = capi_sc[unit];
    int s = SPLI4B();

    bsp->outbytes = sc->sc_bchan[chan].txcount;
    bsp->inbytes = sc->sc_bchan[chan].rxcount;

    sc->sc_bchan[chan].txcount = 0;
    sc->sc_bchan[chan].rxcount = 0;

    splx(s);
}

int capi_start_tx(capi_softc_t *sc, int chan)
{
    struct mbuf *m_b3;
    int sent = 0;

    _IF_DEQUEUE(&sc->sc_bchan[chan].tx_queue, m_b3);
    while (m_b3) {
	struct mbuf *m = m_b3->m_next;

	sc->sc_bchan[chan].txcount += m_b3->m_len;
	capi_data_b3_req(sc, chan, m_b3);
	sent++;

	m_b3 = m;
    }

    if (sc->sc_bchan[chan].capi_drvr_linktab) {
	/* Notify i4b driver of activity, and if the queue is drained. */

	if (sent)
	    (*sc->sc_bchan[chan].capi_drvr_linktab->bch_activity)(
		sc->sc_bchan[chan].capi_drvr_linktab->unit, ACT_TX);

	if (IF_QEMPTY(&sc->sc_bchan[chan].tx_queue))
	    (*sc->sc_bchan[chan].capi_drvr_linktab->bch_tx_queue_empty)(
		sc->sc_bchan[chan].capi_drvr_linktab->unit);
    }

    return sent;
}

/*
//  capi_ll_attach
//      Called by a link layer driver at boot time.
*/

int
capi_ll_attach(capi_softc_t *sc)
{
    int i;

    if (ncapi == (sizeof(capi_sc) / sizeof(capi_sc[0]))) {
	printf("capi%d: too many units, increase MAX_CONTROLLERS\n", ncapi);
	return (ENXIO);
    }

    /* Unit type and subtype; sc is partly filled by ll driver */
    
    ctrl_desc[nctrl].unit = ncapi;
    ctrl_desc[nctrl].ctrl_type = CTRL_CAPI;
    ctrl_desc[nctrl].card_type = sc->card_type;

    /* L4 callbacks */
    
    ctrl_types[CTRL_CAPI].get_linktab = i4b_capi_ret_linktab;
    ctrl_types[CTRL_CAPI].set_linktab = i4b_capi_set_linktab;

    ctrl_desc[nctrl].N_CONNECT_REQUEST = n_connect_request;
    ctrl_desc[nctrl].N_CONNECT_RESPONSE = n_connect_response;
    ctrl_desc[nctrl].N_DISCONNECT_REQUEST = n_disconnect_request;
    ctrl_desc[nctrl].N_ALERT_REQUEST = n_alert_request;
    ctrl_desc[nctrl].N_DOWNLOAD = n_download;
    ctrl_desc[nctrl].N_DIAGNOSTICS = NULL; /* XXX todo */
    ctrl_desc[nctrl].N_MGMT_COMMAND = n_mgmt_command;

    /* Unit state */

    sc->sc_enabled = FALSE;
    sc->sc_state = C_DOWN;
    sc->sc_msgid = 0;

    ctrl_desc[nctrl].dl_est = DL_DOWN;
    ctrl_desc[nctrl].nbch = sc->sc_nbch;

    for (i = 0; i < sc->sc_nbch; i++) {
	ctrl_desc[nctrl].bch_state[i] = BCH_ST_FREE;
	sc->sc_bchan[i].ncci = INVALID;
	sc->sc_bchan[i].msgid = 0;
	sc->sc_bchan[i].busy = 0;
	sc->sc_bchan[i].state = B_FREE;

	memset(&sc->sc_bchan[i].tx_queue, 0, sizeof(struct ifqueue));
	memset(&sc->sc_bchan[i].rx_queue, 0, sizeof(struct ifqueue));
	sc->sc_bchan[i].tx_queue.ifq_maxlen = IFQ_MAXLEN;
	sc->sc_bchan[i].rx_queue.ifq_maxlen = IFQ_MAXLEN;

#if defined (__FreeBSD__) && __FreeBSD__ > 4
	mtx_init(&sc->sc_bchan[i].tx_queue.ifq_mtx, "i4b_capi_tx", MTX_DEF);
	mtx_init(&sc->sc_bchan[i].rx_queue.ifq_mtx, "i4b_capi_rx", MTX_DEF);	
#endif    

	sc->sc_bchan[i].txcount = 0;
	sc->sc_bchan[i].rxcount = 0;

	sc->sc_bchan[i].cdid = CDID_UNUSED;
	sc->sc_bchan[i].bprot = BPROT_NONE;
	sc->sc_bchan[i].in_mbuf = NULL;

	sc->sc_bchan[i].capi_drvr_linktab = NULL;

	sc->sc_bchan[i].capi_isdn_linktab.unit = ncapi;
	sc->sc_bchan[i].capi_isdn_linktab.channel = i;
	sc->sc_bchan[i].capi_isdn_linktab.bch_config = i4b_capi_bch_config;
	sc->sc_bchan[i].capi_isdn_linktab.bch_tx_start = i4b_capi_bch_start_tx;
	sc->sc_bchan[i].capi_isdn_linktab.bch_stat = i4b_capi_bch_stat;
	sc->sc_bchan[i].capi_isdn_linktab.tx_queue = &sc->sc_bchan[i].tx_queue;
	sc->sc_bchan[i].capi_isdn_linktab.rx_queue = &sc->sc_bchan[i].rx_queue;
	sc->sc_bchan[i].capi_isdn_linktab.rx_mbuf = &sc->sc_bchan[i].in_mbuf;
    }

    ctrl_desc[nctrl].tei = -1;

    /* Up the controller index and store the softc */

    sc->sc_unit = ncapi;
    capi_sc[ncapi++] = sc;
    sc->ctrl_unit = nctrl++;

    printf("capi%d: card type %d attached\n", sc->sc_unit, sc->card_type);

    return(0);
}

/*
//  n_mgmt_command
//      i4b L4 management command.
*/

static void
n_mgmt_command(int unit, int op, void *arg)
{
    capi_softc_t *sc = capi_sc[unit];

    printf("capi%d: mgmt command %d\n", sc->sc_unit, op);

    switch(op) {
    case CMR_DOPEN:
	sc->sc_enabled = TRUE;
	break;

    case CMR_DCLOSE:
	sc->sc_enabled = FALSE;
	break;

    case CMR_SETTRACE:
	break;

    default:
	break;
    }
}

/*
//  n_connect_request
//      i4b L4 wants to connect. We assign a B channel to the call,
//      send a CAPI_CONNECT_REQ, and set the channel to B_CONNECT_CONF.
*/

static void
n_connect_request(u_int cdid)
{
    call_desc_t *cd = cd_by_cdid(cdid);
    capi_softc_t *sc;
    int bch, s;

    if (!cd) {
	printf("capi?: invalid cdid %d\n", cdid);
	return;
    }

    sc = capi_sc[ctrl_desc[cd->controller].unit];
    bch = cd->channelid;

    s = SPLI4B();

    if ((bch < 0) || (bch >= sc->sc_nbch))
	for (bch = 0; bch < sc->sc_nbch; bch++)
	    if (sc->sc_bchan[bch].state == B_FREE)
		break;

    if (bch == sc->sc_nbch) {
	splx(s);
	printf("capi%d: no free B channel\n", sc->sc_unit);
	return;
    }

    cd->channelid = bch;

    capi_connect_req(sc, cd);
    splx(s);
}

/*
//  n_connect_response
//      i4b L4 answers a call. We send a CONNECT_RESP with the proper
//      Reject code, and set the channel to B_CONNECT_B3_IND or B_FREE,
//      depending whether we answer or not.
*/

static void
n_connect_response(u_int cdid, int response, int cause)
{
    call_desc_t *cd = cd_by_cdid(cdid);
    capi_softc_t *sc;
    int bch, s;

    if (!cd) {
	printf("capi?: invalid cdid %d\n", cdid);
	return;
    }

    sc = capi_sc[ctrl_desc[cd->controller].unit];
    bch = cd->channelid;

    T400_stop(cd);
	
    cd->response = response;
    cd->cause_out = cause;

    s = SPLI4B();
    capi_connect_resp(sc, cd);
    splx(s);
}

/*
//  n_disconnect_request
//      i4b L4 wants to disconnect. We send a DISCONNECT_REQ and
//      set the channel to B_DISCONNECT_CONF.
*/

static void
n_disconnect_request(u_int cdid, int cause)
{
    call_desc_t *cd = cd_by_cdid(cdid);
    capi_softc_t *sc;
    int bch, s;

    if (!cd) {
	printf("capi?: invalid cdid %d\n", cdid);
	return;
    }

    sc = capi_sc[ctrl_desc[cd->controller].unit];
    bch = cd->channelid;

    cd->cause_out = cause;

    s = SPLI4B();
    capi_disconnect_req(sc, cd);
    splx(s);
}

/*
//  n_alert_request
//      i4b L4 wants to alert an incoming call. We send ALERT_REQ.
*/

static void
n_alert_request(u_int cdid)
{
    call_desc_t *cd = cd_by_cdid(cdid);
    capi_softc_t *sc;
    int s;

    if (!cd) {
	printf("capi?: invalid cdid %d\n", cdid);
	return;
    }

    sc = capi_sc[ctrl_desc[cd->controller].unit];

    s = SPLI4B();
    capi_alert_req(sc, cd);
    splx(s);
}

/*
//  n_download
//      L4 -> firmware download
*/

static int
n_download(int unit, int numprotos, struct isdn_dr_prot *protocols)
{
    capi_softc_t *sc = capi_sc[unit];

    if (sc->load) {
	(*capi_sc[unit]->load)(sc, protocols[0].bytecount,
			       protocols[0].microcode);
    }

    return(0);
}

#endif /* NI4BCAPI > 0 */
