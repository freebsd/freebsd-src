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

/* capi/capi_llif.c	The i4b CAPI link layer interface.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

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

/*
//  capi_ll_control
//      CAPI link layer control routine. Called by a link layer
//      driver when its state changes.
*/

int
capi_ll_control(capi_softc_t *sc, int op, int arg)
{
    switch (op) {
    case CAPI_CTRL_READY:
	if (arg) {
	    sc->sc_state = C_READY;

	    /*
	     *  Register our CAPI ApplId and send CAPI_LISTEN_REQ
	     *  with CIP Mask value 1 (match all).
	     */

	    sc->reg_appl(sc, I4BCAPI_APPLID, sc->sc_nbch);
	    capi_listen_req(sc, 0x10007);

	} else {
	    sc->sc_state = C_DOWN;
	    /* XXX go through cds and notify L4 of pdeact? XXX */
	}
	break;

    case CAPI_CTRL_PROFILE:
	bcopy((char*) arg, &sc->sc_profile, sizeof(sc->sc_profile));
	break;

    case CAPI_CTRL_NEW_NCCI:
    case CAPI_CTRL_FREE_NCCI:
	/* We ignore the controller's NCCI notifications. */
	break;

    default:
	printf("capi%d: unknown control %d\n", sc->sc_unit, op);
    }

    return 0;
}

/*
//  i4b_capi_handlers
//      Array of message-handler pairs used to dispatch CAPI
//      messages sent to I4BCAPI_APPLID.
*/

static struct capi_cmdtab {
    u_int16_t cmd;
    void (*handler)(capi_softc_t *, struct mbuf *);
} i4b_capi_handlers[] = {
    { CAPI_LISTEN_CONF,           capi_listen_conf },
    { CAPI_INFO_IND,              capi_info_ind },
    { CAPI_ALERT_CONF,            capi_alert_conf },
    { CAPI_CONNECT_CONF,          capi_connect_conf },
    { CAPI_CONNECT_IND,           capi_connect_ind },
    { CAPI_CONNECT_ACTIVE_IND,    capi_connect_active_ind },
    { CAPI_CONNECT_B3_CONF,       capi_connect_b3_conf },
    { CAPI_CONNECT_B3_IND,        capi_connect_b3_ind },
    { CAPI_CONNECT_B3_ACTIVE_IND, capi_connect_b3_active_ind },
    { CAPI_DATA_B3_CONF,          capi_data_b3_conf },
    { CAPI_DATA_B3_IND,           capi_data_b3_ind },
    { CAPI_DISCONNECT_B3_IND,     capi_disconnect_b3_ind },
    { CAPI_DISCONNECT_CONF,       capi_disconnect_conf },
    { CAPI_DISCONNECT_IND,        capi_disconnect_ind },
    { 0, 0 }
};

/*
//  capi_ll_receive
//      CAPI link layer receive upcall. Called by a link layer
//      driver to dispatch incoming CAPI messages.
*/

int
capi_ll_receive(capi_softc_t *sc, struct mbuf *m)
{
    u_int8_t *p = mtod(m, u_int8_t*);
    u_int16_t len, applid, msgid, cmd;

    capimsg_getu16(p + 0, &len);
    capimsg_getu16(p + 2, &applid);
    capimsg_getu16(p + 4, &cmd);
    capimsg_getu16(p + 6, &msgid);

#if 0
    printf("capi%d: ll_receive hdr %04x %04x %04x %04x\n", sc->sc_unit,
	   len, applid, cmd, msgid);
#endif

    if (applid == I4BCAPI_APPLID) {
	struct capi_cmdtab *e;
	for (e = i4b_capi_handlers; e->cmd && e->cmd != cmd; e++);
	if (e->cmd) (*e->handler)(sc, m);
	else printf("capi%d: unknown message %04x\n", sc->sc_unit, cmd);

    } else {
	/* XXX we could handle arbitrary ApplIds here XXX */
	printf("capi%d: message %04x for unknown applid %d\n", sc->sc_unit,
	       cmd, applid);
    }

    if (m->m_next) {
	i4b_Bfreembuf(m->m_next);
	m->m_next = NULL;
    }
    i4b_Dfreembuf(m);
    return(0);
}
