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

/* capi/capi.h		The CAPI device interface.
 *
 * $FreeBSD$
 */

#ifndef _I4B_CAPI_H_
#define _I4B_CAPI_H_

/*
//  CAPI driver context: B channels and controller softcs.
*/

#define INVALID -1

enum capi_b_state {
    B_FREE,                  /* channel free, ncci invalid */
    B_CONNECT_CONF,          /* wait for CONNECT_CONF */
    B_CONNECT_IND,           /* IND got, wait for appl RESP */
    B_CONNECT_ACTIVE_IND,    /* wait for CONNECT_ACTIVE_IND */
    B_CONNECT_B3_CONF,       /* wait for CONNECT_B3_CONF */
    B_CONNECT_B3_IND,        /* wait for CONNECT_B3_IND */
    B_CONNECT_B3_ACTIVE_IND, /* wait for CONNECT_B3_ACTIVE_IND */
    B_CONNECTED,             /* channel connected & in use */
    B_DISCONNECT_CONF,       /* wait for DISCONNECT_CONF */
    B_DISCONNECT_B3_CONF,    /* wait for DISCONNECT_B3_CONF */
    B_DISCONNECT_IND,        /* wait for DISCONNECT_IND */
};

typedef struct capi_bchan
{
    /* Channel state */

    int ncci;
#define CAPI_CTRL_MASK 0x000000ff
#define CAPI_PLCI_MASK 0x0000ffff
#define CAPI_NCCI_MASK 0xffff0000
    u_int16_t msgid;
    int busy;
    enum capi_b_state state;

    struct ifqueue tx_queue;
    struct ifqueue rx_queue;
    int rxcount;
    int txcount;

    /* The rest is needed for i4b integration */

    int bprot;
    int cdid;

    struct mbuf *in_mbuf;
    isdn_link_t	capi_isdn_linktab;
    drvr_link_t	*capi_drvr_linktab;
} capi_bchan_t;

enum capi_c_state {
    C_DOWN,             /* controller uninitialized */
    C_READY,            /* controller initialized but not listening */
    C_UP,               /* controller listening */
};

typedef struct capi_softc {
    int sc_unit;        /* index in capi_sc[]                      */
    int ctrl_unit;      /* index in isdn_ctrl_tab[]                */
    int card_type;      /* CARD_TYPEC_xxx, filled by ll driver     */
    int sc_nbch;        /* number of b channels on this controller */
    int sc_enabled;     /* is daemon connected TRUE/FALSE          */
    int sc_msgid;       /* next CAPI message id                    */
    char sc_profile[64];/* CAPI profile data                       */
    enum capi_c_state sc_state;

    capi_bchan_t sc_bchan[MAX_BCHAN];

    /* Link layer driver context holder and methods */

    void *ctx;
    
    int (*load)(struct capi_softc *, int, u_int8_t *);
    int (*reg_appl)(struct capi_softc *, int, int);
    int (*rel_appl)(struct capi_softc *, int);
    int (*send)(struct capi_softc *, struct mbuf *);
} capi_softc_t;

extern capi_softc_t *capi_sc[];
extern int ncapi;

/*
//  CAPI upcalls for the link layer.
*/

#define I4BCAPI_APPLID 1

extern int capi_ll_attach(capi_softc_t *);
extern int capi_ll_control(capi_softc_t *, int op, int arg);

#define CAPI_CTRL_READY     0 /* ctrl ready, value=TRUE/FALSE */
#define CAPI_CTRL_PROFILE   1 /* set CAPI profile             */
#define CAPI_CTRL_NEW_NCCI  2 /* new ncci value, assign bchan */
#define CAPI_CTRL_FREE_NCCI 3 /* free ncci value, clear bchan */

extern int capi_ll_receive(capi_softc_t *, struct mbuf *);

extern int capi_start_tx(capi_softc_t *, int bchan);

#endif /* _I4B_CAPI_H_ */
