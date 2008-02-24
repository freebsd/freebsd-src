/*-
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
 */

/* capi/capi_msgs.c	The CAPI i4b message handlers.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/sys/i4b/capi/capi_msgs.c,v 1.6 2007/07/06 07:17:17 bz Exp $");

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <net/if.h>

#include <i4b/include/i4b_debug.h>
#include <i4b/include/i4b_ioctl.h>
#include <i4b/include/i4b_cause.h>

#include <i4b/include/i4b_l3l4.h>
#include <i4b/include/i4b_mbuf.h>
#include <i4b/include/i4b_global.h>

#include <i4b/layer4/i4b_l4.h>

#include <i4b/capi/capi.h>
#include <i4b/capi/capi_msgs.h>

/*
//  Administrative messages:
//  ------------------------
*/

void capi_listen_req(capi_softc_t *sc, u_int32_t CIP)
{
    struct mbuf *m = i4b_Dgetmbuf(8 + 18);
    u_int8_t *msg;
    u_int16_t msgid;

    if (!m) {
	printf("capi%d: can't get mbuf for listen_req\n", sc->sc_unit);
	return;
    }

    msgid = sc->sc_msgid++;

    msg = capimsg_setu16(mtod(m, u_int8_t*), m->m_len);
    msg = capimsg_setu16(msg, I4BCAPI_APPLID);
    msg = capimsg_setu16(msg, CAPI_LISTEN_REQ);
    msg = capimsg_setu16(msg, msgid);

    msg = capimsg_setu32(msg, 1); /* Controller */
    msg = capimsg_setu32(msg, 0); /* Info mask */
    msg = capimsg_setu32(msg, CIP);
    msg = capimsg_setu32(msg, 0);
    msg = capimsg_setu8(msg, 0);
    msg = capimsg_setu8(msg, 0);

    sc->send(sc, m);
}

void capi_listen_conf(capi_softc_t *sc, struct mbuf *m_in)
{
    u_int8_t *msg = mtod(m_in, u_int8_t*);
    u_int16_t Info;

    capimsg_getu16(msg + 12, &Info);

    if (Info == 0) {
	/* We are now listening. */

	sc->sc_state = C_UP;
	ctrl_desc[sc->ctrl_unit].dl_est = DL_UP;

	i4b_l4_l12stat(sc->ctrl_unit, 1, 1);
	i4b_l4_l12stat(sc->ctrl_unit, 2, 1);

    } else {
	/* XXX sc->sc_state = C_DOWN ? XXX */
	printf("capi%d: can't listen, info=%04x\n", sc->sc_unit, Info);
    }
}

void capi_info_ind(capi_softc_t *sc, struct mbuf *m_in)
{
    struct mbuf *m = i4b_Dgetmbuf(8 + 4);
    u_int8_t *msg = mtod(m_in, u_int8_t*);
    u_int16_t applid, msgid;
    u_int32_t PLCI;

    if (!m) {
	printf("capi%d: can't get mbuf for info_resp\n", sc->sc_unit);
	return;
    }

    msg = capimsg_getu16(msg + 2, &applid);
    msg = capimsg_getu16(msg + 2, &msgid);
    msg = capimsg_getu32(msg, &PLCI);

    /* i4b_l4_info_ind() */
    
    msg = capimsg_setu16(mtod(m, u_int8_t*), m->m_len);
    msg = capimsg_setu16(msg, applid);
    msg = capimsg_setu16(msg, CAPI_INFO_RESP);
    msg = capimsg_setu16(msg, msgid);

    msg = capimsg_setu32(msg, PLCI);

    sc->send(sc, m);
}

void capi_alert_req(capi_softc_t *sc, call_desc_t *cd)
{
    struct mbuf *m = i4b_Dgetmbuf(8 + 5);
    u_int8_t *msg;
    u_int16_t msgid;
    u_int32_t PLCI;

    if (!m) {
	printf("capi%d: can't get mbuf for alert_req\n", sc->sc_unit);
	return;
    }

    msgid = sc->sc_bchan[cd->channelid].msgid = sc->sc_msgid++;
    PLCI = (sc->sc_bchan[cd->channelid].ncci & CAPI_PLCI_MASK);

    msg = capimsg_setu16(mtod(m, u_int8_t*), m->m_len);
    msg = capimsg_setu16(msg, I4BCAPI_APPLID);
    msg = capimsg_setu16(msg, CAPI_ALERT_REQ);
    msg = capimsg_setu16(msg, msgid);

    msg = capimsg_setu32(msg, PLCI);
    msg = capimsg_setu8(msg, 0);

    sc->send(sc, m);
}

void capi_alert_conf(capi_softc_t *sc, struct mbuf *m_in)
{
    u_int8_t *msg = mtod(m_in, u_int8_t*);
    u_int16_t Info;

    msg = capimsg_getu16(msg + 12, &Info);

    if (Info) {
	printf("capi%d: can't alert, info=%04x\n", sc->sc_unit, Info);
    }
}

/*
//  Outgoing call setup:
//  --------------------
//
//             CAPI_CONNECT_REQ -->
//                              <-- CAPI_CONNECT_CONF
//                       (notify Layer 4)
//                              <-- CAPI_CONNECT_ACTIVE_IND
//     CAPI_CONNECT_ACTIVE_RESP -->
//          CAPI_CONNECT_B3_REQ -->
//                              <-- CAPI_CONNECT_B3_CONF
//                              <-- CAPI_CONNECT_B3_ACTIVE_IND
//  CAPI_CONNECT_B3_ACTIVE_RESP -->
//                       (notify Layer 4)
*/

void capi_connect_req(capi_softc_t *sc, call_desc_t *cd)
{
    struct mbuf *m;
    u_int8_t *msg;
    u_int16_t msgid;
    int slen = strlen(cd->src_telno);
    int dlen = strlen(cd->dst_telno);

    m = i4b_Dgetmbuf(8 + 27 + slen + dlen);
    if (!m) {
	printf("capi%d: can't get mbuf for connect_req\n", sc->sc_unit);
	return;
    }

    cd->crflag = CRF_ORIG;

    sc->sc_bchan[cd->channelid].cdid = cd->cdid;
    sc->sc_bchan[cd->channelid].bprot = cd->bprot;
    sc->sc_bchan[cd->channelid].state = B_CONNECT_CONF;
    msgid = sc->sc_bchan[cd->channelid].msgid = sc->sc_msgid++;
    ctrl_desc[sc->ctrl_unit].bch_state[cd->channelid] = BCH_ST_RSVD;

    msg = capimsg_setu16(mtod(m, u_int8_t*), m->m_len);
    msg = capimsg_setu16(msg, I4BCAPI_APPLID);
    msg = capimsg_setu16(msg, CAPI_CONNECT_REQ);
    msg = capimsg_setu16(msg, msgid);

    msg = capimsg_setu32(msg, 1); /* Controller */

    switch (cd->bprot) {
    case BPROT_NONE:
	msg = capimsg_setu16(msg, 0x0010); /* Telephony */
	break;

    case BPROT_RHDLC:
	msg = capimsg_setu16(msg, 0x0002); /* Unrestricted digital */
	break;

    default:
	msg = capimsg_setu16(msg, 0x0002); /* Unrestricted digital */
    }

    msg = capimsg_setu8(msg, 1 + dlen);
    msg = capimsg_setu8(msg, 0x80);
    strncpy(msg, cd->dst_telno, dlen);

    msg = capimsg_setu8(msg + dlen, 2 + slen);
    msg = capimsg_setu8(msg, 0x00);
    msg = capimsg_setu8(msg, 0x80); /* Presentation and screening indicator */
    strncpy(msg, cd->src_telno, slen);

    msg = capimsg_setu8(msg + slen, 0); /* Called & */
    msg = capimsg_setu8(msg, 0); /* Calling party subaddress */
    
    msg = capimsg_setu8(msg, 15); /* B protocol */
    if (cd->bprot == BPROT_NONE)
	msg = capimsg_setu16(msg, 1); /* B1 protocol = transparent */
    else
	msg = capimsg_setu16(msg, 0); /* B1 protocol = HDLC */
    msg = capimsg_setu16(msg, 1); /* B2 protocol = transparent */
    msg = capimsg_setu16(msg, 0); /* B3 protocol = transparent */
    msg = capimsg_setu8(msg, 0); /* B1 parameters */
    msg = capimsg_setu8(msg, 0); /* B2 parameters */
    msg = capimsg_setu8(msg, 0); /* B3 parameters */

    msg = capimsg_setu8(msg, 0); /* Bearer Capability */
    msg = capimsg_setu8(msg, 0); /* Low Layer Compatibility */
    msg = capimsg_setu8(msg, 0); /* High Layer Compatibility */
    msg = capimsg_setu8(msg, 0); /* Additional Info */

    sc->send(sc, m);
}

void capi_connect_conf(capi_softc_t *sc, struct mbuf *m_in)
{
    u_int8_t *msg = mtod(m_in, u_int8_t*);
    call_desc_t *cd;
    u_int16_t msgid;
    u_int32_t PLCI;
    u_int16_t Info;
    int bch;

    msg = capimsg_getu16(msg + 6, &msgid);
    msg = capimsg_getu32(msg, &PLCI);
    msg = capimsg_getu16(msg, &Info);

    for (bch = 0; bch < sc->sc_nbch; bch++)
	if ((sc->sc_bchan[bch].state == B_CONNECT_CONF) &&
	    (sc->sc_bchan[bch].msgid == msgid))
	    break;

    if ((bch == sc->sc_nbch) ||
	(cd = cd_by_cdid(sc->sc_bchan[bch].cdid)) == NULL) {
	printf("capi%d: can't find channel for connect_conf PLCI %x\n",
	       sc->sc_unit, PLCI);
	return;
    }

    if (Info == 0) {
	sc->sc_bchan[bch].state = B_CONNECT_ACTIVE_IND;
	sc->sc_bchan[bch].ncci = PLCI;

	i4b_l4_proceeding_ind(cd);

    } else {
	SET_CAUSE_TV(cd->cause_out, CAUSET_I4B, CAUSE_I4B_L1ERROR);
	i4b_l4_disconnect_ind(cd);
	freecd_by_cd(cd);

	sc->sc_bchan[bch].state = B_FREE;
	ctrl_desc[sc->ctrl_unit].bch_state[bch] = BCH_ST_FREE;

	printf("capi%d: can't connect out, info=%04x\n", sc->sc_unit, Info);
    }
}

void capi_connect_active_ind(capi_softc_t *sc, struct mbuf *m_in)
{
    struct mbuf *m = i4b_Dgetmbuf(8 + 4);
    u_int8_t *msg = mtod(m_in, u_int8_t*);
    call_desc_t *cd;
    u_int16_t applid, msgid;
    u_int32_t PLCI;
    int bch;

    if (!m) {
	printf("capi%d: can't get mbuf for active_ind\n", sc->sc_unit);
	return;
    }

    msg = capimsg_getu16(msg + 2, &applid);
    msg = capimsg_getu16(msg + 2, &msgid);
    msg = capimsg_getu32(msg, &PLCI);

    for (bch = 0; bch < sc->sc_nbch; bch++)
	if ((sc->sc_bchan[bch].state == B_CONNECT_ACTIVE_IND) &&
	    (sc->sc_bchan[bch].ncci == PLCI))
	    break;

    if ((bch == sc->sc_nbch) ||
	(cd = cd_by_cdid(sc->sc_bchan[bch].cdid)) == NULL) {
	printf("capi%d: can't find channel for active_resp, PLCI %x\n",
	       sc->sc_unit, PLCI);
	return;
    }

    msg = capimsg_setu16(mtod(m, u_int8_t*), m->m_len);
    msg = capimsg_setu16(msg, applid);
    msg = capimsg_setu16(msg, CAPI_CONNECT_ACTIVE_RESP);
    msg = capimsg_setu16(msg, msgid);

    msg = capimsg_setu32(msg, PLCI);

    sc->send(sc, m);

    if (cd->crflag == CRF_ORIG) {
	capi_connect_b3_req(sc, cd);

    } else {
	sc->sc_bchan[bch].state = B_CONNECT_B3_IND;
    }
}

void capi_connect_b3_req(capi_softc_t *sc, call_desc_t *cd)
{
    struct mbuf *m = i4b_Dgetmbuf(8 + 5);
    u_int8_t *msg;
    u_int16_t msgid;
    u_int32_t PLCI;

    if (!m) {
	printf("capi%d: can't get mbuf for connect_b3_req\n", sc->sc_unit);
	return;
    }

    sc->sc_bchan[cd->channelid].state = B_CONNECT_B3_CONF;
    msgid = sc->sc_bchan[cd->channelid].msgid = sc->sc_msgid++;
    PLCI = (sc->sc_bchan[cd->channelid].ncci & CAPI_PLCI_MASK);

    msg = capimsg_setu16(mtod(m, u_int8_t*), m->m_len);
    msg = capimsg_setu16(msg, I4BCAPI_APPLID);
    msg = capimsg_setu16(msg, CAPI_CONNECT_B3_REQ);
    msg = capimsg_setu16(msg, msgid);

    msg = capimsg_setu32(msg, PLCI);
    msg = capimsg_setu8(msg, 0); /* NCPI */

    sc->send(sc, m);
}

void capi_connect_b3_conf(capi_softc_t *sc, struct mbuf *m_in)
{
    u_int8_t *msg = mtod(m_in, u_int8_t*);
    call_desc_t *cd;
    u_int16_t msgid;
    u_int32_t NCCI;
    u_int16_t Info;
    int bch;

    msg = capimsg_getu16(msg + 6, &msgid);
    msg = capimsg_getu32(msg, &NCCI);
    msg = capimsg_getu16(msg, &Info);

    for (bch = 0; bch < sc->sc_nbch; bch++)
	if ((sc->sc_bchan[bch].state == B_CONNECT_B3_CONF) &&
	    (sc->sc_bchan[bch].ncci == (NCCI & CAPI_PLCI_MASK)))
	    break;

    if ((bch == sc->sc_nbch) ||
	(cd = cd_by_cdid(sc->sc_bchan[bch].cdid)) == NULL) {
	printf("capi%d: can't find channel for connect_b3_conf NCCI %x\n",
	       sc->sc_unit, NCCI);
	return;
    }

    if (Info == 0) {
	sc->sc_bchan[bch].ncci = NCCI;
	sc->sc_bchan[bch].state = B_CONNECT_B3_ACTIVE_IND;

    } else {
	SET_CAUSE_TV(cd->cause_in, CAUSET_I4B, CAUSE_I4B_OOO); /* XXX */
	i4b_l4_disconnect_ind(cd);
	freecd_by_cd(cd);

	ctrl_desc[sc->ctrl_unit].bch_state[bch] = BCH_ST_RSVD;

	printf("capi%d: can't connect_b3 out, info=%04x\n", sc->sc_unit, Info);

	capi_disconnect_req(sc, cd);
    }
}

void capi_connect_b3_active_ind(capi_softc_t *sc, struct mbuf *m_in)
{
    struct mbuf *m = i4b_Dgetmbuf(8 + 4);
    u_int8_t *msg = mtod(m_in, u_int8_t*);
    call_desc_t *cd;
    u_int16_t applid, msgid;
    u_int32_t NCCI;
    int bch;

    if (!m) {
	printf("capi%d: can't get mbuf for b3_active_ind\n", sc->sc_unit);
	return;
    }

    msg = capimsg_getu16(msg + 2, &applid);
    msg = capimsg_getu16(msg + 2, &msgid);
    msg = capimsg_getu32(msg, &NCCI);

    for (bch = 0; bch < sc->sc_nbch; bch++)
	if ((sc->sc_bchan[bch].state == B_CONNECT_B3_ACTIVE_IND) &&
	    (sc->sc_bchan[bch].ncci == NCCI))
	    break;

    if ((bch == sc->sc_nbch) ||
	(cd = cd_by_cdid(sc->sc_bchan[bch].cdid)) == NULL) {
	printf("capi%d: can't find channel for b3_active_resp NCCI %x\n",
	       sc->sc_unit, NCCI);
	return;
    }

    msg = capimsg_setu16(mtod(m, u_int8_t*), m->m_len);
    msg = capimsg_setu16(msg, I4BCAPI_APPLID);
    msg = capimsg_setu16(msg, CAPI_CONNECT_B3_ACTIVE_RESP);
    msg = capimsg_setu16(msg, msgid);

    msg = capimsg_setu32(msg, NCCI);

    sc->send(sc, m);

    sc->sc_bchan[bch].state = B_CONNECTED;
    i4b_l4_connect_active_ind(cd);
}

/*
//  Incoming call setup:
//  --------------------
//
//                              <-- CAPI_CONNECT_IND
//                       (consult Layer 4)
//            CAPI_CONNECT_RESP -->
//                              <-- CAPI_CONNECT_ACTIVE_IND
//     CAPI_CONNECT_ACTIVE_RESP -->
//                              <-- CAPI_CONNECT_B3_IND
//         CAPI_CONNECT_B3_RESP -->
//                              <-- CAPI_CONNECT_B3_ACTIVE_IND
//  CAPI_CONNECT_B3_ACTIVE_RESP -->
//                       (notify Layer 4)
*/

void capi_connect_ind(capi_softc_t *sc, struct mbuf *m_in)
{
    u_int8_t *msg = mtod(m_in, u_int8_t*);
    call_desc_t *cd;
    u_int16_t applid, msgid;
    u_int32_t PLCI;
    u_int16_t CIP;
    u_int8_t x, y, z;
    int bch;

    if ((cd = reserve_cd()) == NULL) {
	printf("capi%d: can't get cd for connect_ind\n", sc->sc_unit);
	return;
    }

    cd->controller = sc->ctrl_unit;
    cd->channelexcl = FALSE;

    for (bch = 0; bch < sc->sc_nbch; bch++)
	if (sc->sc_bchan[bch].state == B_FREE) break;
    sc->sc_bchan[bch].state = B_CONNECT_IND;
    cd->channelid = bch; /* XXX CHAN_ANY XXX */

    cd->crflag = CRF_DEST;
    cd->cr = get_rand_cr(sc->sc_unit);
    cd->scr_ind = SCR_NONE;
    cd->prs_ind = PRS_NONE;
    cd->bprot = BPROT_NONE;
    cd->ilt = NULL;
    cd->dlt = NULL;
    cd->display[0] = '\0';
    cd->datetime[0] = '\0';

    msg = capimsg_getu16(msg + 2, &applid);
    msg = capimsg_getu16(msg + 2, &msgid);
    msg = capimsg_getu32(msg, &PLCI);
    msg = capimsg_getu16(msg, &CIP);

    cd->event = (int) msgid; /* XXX overload */
    cd->Q931state = (int) PLCI; /* XXX overload */

    switch (CIP) {
    case 0x0010:
    case 0x0001: cd->bprot = BPROT_NONE; break;
    case 0x0002: cd->bprot = BPROT_RHDLC; break;
    default:
	NDBGL4(L4_CAPIDBG, "capi%d: unknown CIP = %d", sc->sc_unit, CIP);
	cd->bprot = BPROT_NONE;
    }

    msg = capimsg_getu8(msg, &x); /* Called party struct len */
    if (x) {
	msg = capimsg_getu8(msg, &y); /* Numbering plan */
	z = x - 1;
	if (z >= TELNO_MAX) z = (TELNO_MAX-1);
	strncpy(cd->dst_telno, msg, z);
	msg += x;
	x = z;
    }
    cd->dst_telno[x] = '\0';

    msg = capimsg_getu8(msg, &x); /* Calling party struct len */
    if (x) {
	msg = capimsg_getu8(msg, &y); /* Numbering plan */
	msg = capimsg_getu8(msg, &y); /* Screening/Presentation */
	if ((y & 0x80) == 0) { /* screening used */
	    cd->scr_ind = (y & 3) + SCR_USR_NOSC;
	    cd->prs_ind = ((y >> 5) & 3) + PRS_ALLOWED;
	}
	z = x - 2;
	if (z >= TELNO_MAX) z = (TELNO_MAX-1);
	strncpy(cd->src_telno, msg, z);
	msg += x;
	x = z;
    }
    cd->src_telno[x] = '\0';

    i4b_l4_connect_ind(cd);
}

void capi_connect_resp(capi_softc_t *sc, call_desc_t *cd)
{
    struct mbuf *m;
    u_int8_t *msg;
    u_int16_t msgid;
    u_int32_t PLCI;
    int dlen = strlen(cd->dst_telno);

    m = i4b_Dgetmbuf(8 + 21 + dlen);
    if (!m) {
	printf("capi%d: can't get mbuf for connect_resp\n", sc->sc_unit);
	return;
    }

    msgid = (u_int16_t) cd->event;
    PLCI = (u_int32_t) cd->Q931state;

    msg = capimsg_setu16(mtod(m, u_int8_t*), m->m_len);
    msg = capimsg_setu16(msg, I4BCAPI_APPLID);
    msg = capimsg_setu16(msg, CAPI_CONNECT_RESP);
    msg = capimsg_setu16(msg, msgid);

    msg = capimsg_setu32(msg, PLCI);

    switch (cd->response) {
    case SETUP_RESP_ACCEPT:
	sc->sc_bchan[cd->channelid].cdid = cd->cdid;
	sc->sc_bchan[cd->channelid].ncci = PLCI;
	sc->sc_bchan[cd->channelid].state = B_CONNECT_ACTIVE_IND;
	ctrl_desc[sc->ctrl_unit].bch_state[cd->channelid] = BCH_ST_USED;
	msg = capimsg_setu16(msg, 0); /* Accept the call */
	break;

    case SETUP_RESP_REJECT:
	sc->sc_bchan[cd->channelid].state = B_FREE;
	ctrl_desc[sc->ctrl_unit].bch_state[cd->channelid] = BCH_ST_FREE;
	msg = capimsg_setu16(msg, 2); /* Reject, normal call clearing */
	break;

    case SETUP_RESP_DNTCRE:
	sc->sc_bchan[cd->channelid].state = B_FREE;
	ctrl_desc[sc->ctrl_unit].bch_state[cd->channelid] = BCH_ST_FREE;
	if (sc->sc_nbch == 30) {
	    /* With PRI, we can't really ignore calls  -- normal clearing */
	    msg = capimsg_setu16(msg, (0x3480|CAUSE_Q850_NCCLR));
	} else {
	    msg = capimsg_setu16(msg, 1); /* Ignore */
	}
	break;

    default:
	sc->sc_bchan[cd->channelid].state = B_FREE;
	ctrl_desc[sc->ctrl_unit].bch_state[cd->channelid] = BCH_ST_FREE;
	msg = capimsg_setu16(msg, (0x3480|CAUSE_Q850_CALLREJ));
    }

    msg = capimsg_setu8(msg, 15); /* B protocol */
    if (cd->bprot == BPROT_NONE)
	msg = capimsg_setu16(msg, 1); /* B1 protocol = transparent */
    else
	msg = capimsg_setu16(msg, 0); /* B1 protocol = HDLC */
    msg = capimsg_setu16(msg, 1); /* B2 protocol = transparent */
    msg = capimsg_setu16(msg, 0); /* B3 protocol = transparent */
    msg = capimsg_setu8(msg, 0); /* B1 parameters */
    msg = capimsg_setu8(msg, 0); /* B2 parameters */
    msg = capimsg_setu8(msg, 0); /* B3 parameters */

    msg = capimsg_setu8(msg, 1 + dlen);
    msg = capimsg_setu8(msg, 0x80); /* Numbering plan */
    strncpy(msg, cd->dst_telno, dlen);
    msg = capimsg_setu8(msg + dlen, 0); /* Connected subaddress */
    msg = capimsg_setu8(msg, 0); /* Low Layer Compatibility */
    msg = capimsg_setu8(msg, 0); /* Additional Info */

    sc->send(sc, m);
}

void capi_connect_b3_ind(capi_softc_t *sc, struct mbuf *m_in)
{
    struct mbuf *m = i4b_Dgetmbuf(8 + 7);
    u_int8_t *msg = mtod(m_in, u_int8_t*);
    u_int16_t applid, msgid;
    u_int32_t NCCI;
    int bch;

    if (!m) {
	printf("capi%d: can't get mbuf for connect_b3_resp\n", sc->sc_unit);
	return;
    }

    msg = capimsg_getu16(msg + 2, &applid);
    msg = capimsg_getu16(msg + 2, &msgid);
    msg = capimsg_getu32(msg, &NCCI);

    for (bch = 0; bch < sc->sc_nbch; bch++)
	if ((sc->sc_bchan[bch].state == B_CONNECT_B3_IND) &&
	    (sc->sc_bchan[bch].ncci == (NCCI & CAPI_PLCI_MASK)))
	    break;

    msg = capimsg_setu16(mtod(m, u_int8_t*), m->m_len);
    msg = capimsg_setu16(msg, applid);
    msg = capimsg_setu16(msg, CAPI_CONNECT_B3_RESP);
    msg = capimsg_setu16(msg, msgid);

    msg = capimsg_setu32(msg, NCCI);

    if (bch == sc->sc_nbch) {
	printf("capi%d: can't get cd for connect_b3_resp NCCI %x\n",
	       sc->sc_unit, NCCI);
	msg = capimsg_setu16(msg, 8); /* Reject, destination OOO */

    } else {
	sc->sc_bchan[bch].ncci = NCCI;
	sc->sc_bchan[bch].state = B_CONNECT_B3_ACTIVE_IND;
	msg = capimsg_setu16(msg, 0); /* Accept */
    }

    msg = capimsg_setu8(msg, 0); /* NCPI */

    sc->send(sc, m);
}

/*
//  Data transfer:
//  --------------
*/

void capi_data_b3_req(capi_softc_t *sc, int chan, struct mbuf *m_b3)
{
    struct mbuf *m = i4b_Dgetmbuf(8 + 14);
    u_int8_t *msg;
    u_int16_t msgid;

    if (!m) {
	printf("capi%d: can't get mbuf for data_b3_req\n", sc->sc_unit);
	return;
    }

    msgid = sc->sc_bchan[chan].msgid = sc->sc_msgid++;
    sc->sc_bchan[chan].busy = 1;

    msg = capimsg_setu16(mtod(m, u_int8_t*), m->m_len);
    msg = capimsg_setu16(msg, I4BCAPI_APPLID);
    msg = capimsg_setu16(msg, CAPI_DATA_B3_REQ);
    msg = capimsg_setu16(msg, msgid);

    msg = capimsg_setu32(msg, sc->sc_bchan[chan].ncci);
    msg = capimsg_setu32(msg, (u_int32_t) m_b3->m_data); /* Pointer */
    msg = capimsg_setu16(msg, m_b3->m_len);
    msg = capimsg_setu16(msg, chan);
    msg = capimsg_setu16(msg, 0); /* Flags */

    m->m_next = m_b3;

    sc->send(sc, m);
}

void capi_data_b3_conf(capi_softc_t *sc, struct mbuf *m_in)
{
    u_int8_t *msg = mtod(m_in, u_int8_t*);
    u_int32_t NCCI;
    u_int16_t handle;
    u_int16_t Info;

    msg = capimsg_getu32(msg + 8, &NCCI);
    msg = capimsg_getu16(msg, &handle);
    msg = capimsg_getu16(msg, &Info);

    if (Info == 0) {
	sc->sc_bchan[handle].busy = 0;
	capi_start_tx(sc, handle);

    } else {
	printf("capi%d: data_b3_conf NCCI %x handle %x info=%04x\n",
	       sc->sc_unit, NCCI, handle, Info);
    }
}

void capi_data_b3_ind(capi_softc_t *sc, struct mbuf *m_in)
{
    struct mbuf *m = i4b_Dgetmbuf(8 + 6);
    u_int8_t *msg = mtod(m_in, u_int8_t*);
    u_int16_t applid, msgid;
    u_int32_t NCCI;
    u_int16_t handle;
    int bch;

    if (!m) {
	printf("capi%d: can't get mbuf for data_b3_resp\n", sc->sc_unit);
	return;
    }

    msg = capimsg_getu16(msg + 2, &applid);
    msg = capimsg_getu16(msg + 2, &msgid);
    msg = capimsg_getu32(msg, &NCCI);
    msg = capimsg_getu16(msg + 6, &handle);

    for (bch = 0; bch < sc->sc_nbch; bch++)
	if ((sc->sc_bchan[bch].state == B_CONNECTED) &&
	    (sc->sc_bchan[bch].ncci == NCCI))
	    break;

    if (bch == sc->sc_nbch) {
	printf("capi%d: can't find channel for data_b3_ind NCCI %x\n",
	       sc->sc_unit, NCCI);

    } else {
	if (sc->sc_bchan[bch].bprot == BPROT_RHDLC) {
	    /* HDLC drivers use rx_mbuf */

	    sc->sc_bchan[bch].in_mbuf = m_in->m_next;
	    sc->sc_bchan[bch].rxcount += m_in->m_next->m_len;
	    m_in->m_next = NULL; /* driver frees */

	    (*sc->sc_bchan[bch].capi_drvr_linktab->bch_rx_data_ready)(
		sc->sc_bchan[bch].capi_drvr_linktab->unit);

	} else {
	    /* Telephony drivers use rx_queue */

	    if (!_IF_QFULL(&sc->sc_bchan[bch].rx_queue)) {
		_IF_ENQUEUE(&sc->sc_bchan[bch].rx_queue, m_in->m_next);
		sc->sc_bchan[bch].rxcount += m_in->m_next->m_len;
		m_in->m_next = NULL; /* driver frees */
	    }

	    (*sc->sc_bchan[bch].capi_drvr_linktab->bch_rx_data_ready)(
		sc->sc_bchan[bch].capi_drvr_linktab->unit);
	}
    }

    msg = capimsg_setu16(mtod(m, u_int8_t*), m->m_len);
    msg = capimsg_setu16(msg, I4BCAPI_APPLID);
    msg = capimsg_setu16(msg, CAPI_DATA_B3_RESP);
    msg = capimsg_setu16(msg, msgid);

    msg = capimsg_setu32(msg, NCCI);
    msg = capimsg_setu16(msg, handle);

    sc->send(sc, m);
}

/*
//  Connection teardown:
//  --------------------
*/

void capi_disconnect_req(capi_softc_t *sc, call_desc_t *cd)
{
    struct mbuf *m = i4b_Dgetmbuf(8 + 5);
    u_int8_t *msg;
    u_int16_t msgid;
    u_int32_t PLCI;

    if (!m) {
	printf("capi%d: can't get mbuf for disconnect_req\n", sc->sc_unit);
	return;
    }

    sc->sc_bchan[cd->channelid].state = B_DISCONNECT_CONF;
    ctrl_desc[sc->ctrl_unit].bch_state[cd->channelid] = BCH_ST_RSVD;
    msgid = sc->sc_bchan[cd->channelid].msgid = sc->sc_msgid++;
    PLCI = (sc->sc_bchan[cd->channelid].ncci & CAPI_PLCI_MASK);

    msg = capimsg_setu16(mtod(m, u_int8_t*), m->m_len);
    msg = capimsg_setu16(msg, I4BCAPI_APPLID);
    msg = capimsg_setu16(msg, CAPI_DISCONNECT_REQ);
    msg = capimsg_setu16(msg, msgid);

    msg = capimsg_setu32(msg, PLCI);
    msg = capimsg_setu8(msg, 0); /* Additional Info */

    sc->send(sc, m);
}

void capi_disconnect_conf(capi_softc_t *sc, struct mbuf *m_in)
{
    u_int8_t *msg = mtod(m_in, u_int8_t*);
    call_desc_t *cd;
    u_int32_t PLCI;
    int bch;

    msg = capimsg_getu32(msg + 8, &PLCI);

    for (bch = 0; bch < sc->sc_nbch; bch++)
	if ((sc->sc_bchan[bch].state == B_DISCONNECT_CONF) &&
	    ((sc->sc_bchan[bch].ncci & CAPI_PLCI_MASK) == PLCI))
	    break;

    if (bch == sc->sc_nbch) {
	printf("capi%d: can't find channel for disconnect_conf PLCI %x\n",
	       sc->sc_unit, PLCI);
	return;
    }

    cd = cd_by_cdid(sc->sc_bchan[bch].cdid);
    if (!cd) {
	printf("capi%d: can't find cd for disconnect_conf PLCI %x\n",
	       sc->sc_unit, PLCI);
    } else {
	i4b_l4_disconnect_ind(cd);
	freecd_by_cd(cd);
    }

    sc->sc_bchan[bch].state = B_FREE;
    ctrl_desc[sc->ctrl_unit].bch_state[bch] = BCH_ST_FREE;
}

void capi_disconnect_b3_ind(capi_softc_t *sc, struct mbuf *m_in)
{
    struct mbuf *m = i4b_Dgetmbuf(8 + 4);
    u_int8_t *msg = mtod(m_in, u_int8_t*);
    u_int16_t applid, msgid;
    u_int32_t NCCI;

    if (!m) {
	printf("capi%d: can't get mbuf for disconnect_b3_resp\n", sc->sc_unit);
	return;
    }

    msg = capimsg_getu16(msg + 2, &applid);
    msg = capimsg_getu16(msg + 2, &msgid);
    msg = capimsg_getu32(msg, &NCCI);

    /* XXX update bchan state? XXX */

    msg = capimsg_setu16(mtod(m, u_int8_t*), m->m_len);
    msg = capimsg_setu16(msg, applid);
    msg = capimsg_setu16(msg, CAPI_DISCONNECT_B3_RESP);
    msg = capimsg_setu16(msg, msgid);

    msg = capimsg_setu32(msg, NCCI);

    sc->send(sc, m);
}

void capi_disconnect_ind(capi_softc_t *sc, struct mbuf *m_in)
{
    struct mbuf *m = i4b_Dgetmbuf(8 + 4);
    u_int8_t *msg = mtod(m_in, u_int8_t*);
    call_desc_t *cd;
    u_int16_t applid, msgid;
    u_int32_t PLCI;
    u_int16_t Reason;
    int bch;

    if (!m) {
	printf("capi%d: can't get mbuf for disconnect_resp\n", sc->sc_unit);
	return;
    }

    msg = capimsg_getu16(msg + 2, &applid);
    msg = capimsg_getu16(msg + 2, &msgid);
    msg = capimsg_getu32(msg, &PLCI);
    msg = capimsg_getu16(msg, &Reason);

    for (bch = 0; bch < sc->sc_nbch; bch++)
	if ((sc->sc_bchan[bch].state != B_FREE) &&
	    ((sc->sc_bchan[bch].ncci & CAPI_PLCI_MASK) == PLCI))
	    break;

    if (bch < sc->sc_nbch) {
	/* We may not have a bchan assigned if call was ignored. */

	cd = cd_by_cdid(sc->sc_bchan[bch].cdid);
	sc->sc_bchan[bch].state = B_DISCONNECT_IND;
    } else cd = NULL;

    if (cd) {
	if ((Reason & 0xff00) == 0x3400) {
	    SET_CAUSE_TV(cd->cause_in, CAUSET_Q850, (Reason & 0x7f));
	} else {
	    SET_CAUSE_TV(cd->cause_in, CAUSET_I4B, CAUSE_I4B_NORMAL);
	}

	i4b_l4_disconnect_ind(cd);
	freecd_by_cd(cd);

	sc->sc_bchan[bch].state = B_FREE;
	ctrl_desc[sc->ctrl_unit].bch_state[bch] = BCH_ST_FREE;
    }

    msg = capimsg_setu16(mtod(m, u_int8_t*), m->m_len);
    msg = capimsg_setu16(msg, applid);
    msg = capimsg_setu16(msg, CAPI_DISCONNECT_RESP);
    msg = capimsg_setu16(msg, msgid);

    msg = capimsg_setu32(msg, PLCI);

    sc->send(sc, m);
}
