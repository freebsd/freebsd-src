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
 * capi/iavc/iavc_lli.c
 *		The AVM ISDN controllers' Low Level Interface.
 *
 * $FreeBSD$
 */

#include "iavc.h"
#include "i4bcapi.h"
#include "pci.h"

#if (NIAVC > 0) && (NI4BCAPI > 0)

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/malloc.h>
#include <net/if.h>

#include <machine/clock.h>

#include <machine/bus.h>
#include <sys/bus.h>
#include <sys/rman.h>
#include <vm/vm.h>
#include <vm/pmap.h>

#include <machine/i4b_debug.h>
#include <machine/i4b_ioctl.h>
#include <machine/i4b_trace.h>

#include <i4b/include/i4b_global.h>
#include <i4b/include/i4b_l3l4.h>
#include <i4b/include/i4b_mbuf.h>

#include <i4b/capi/capi.h>
#include <i4b/capi/capi_msgs.h>

#include <i4b/capi/iavc/iavc.h>

/* Forward declarations of local subroutines... */

static int iavc_send_init(iavc_softc_t *);

static void iavc_handle_rx(iavc_softc_t *);
static void iavc_start_tx(iavc_softc_t *);

/*
//  Callbacks from the upper (capi) layer:
//  --------------------------------------
//
//  iavc_load
//      Resets the board and loads the firmware, then initiates
//      board startup.
//
//  iavc_register
//      Registers a CAPI application id.
//
//  iavc_release
//      Releases a CAPI application id.
//
//  iavc_send
//      Sends a capi message.
*/

int iavc_load(capi_softc_t *capi_sc, int len, u_int8_t *cp)
{
    iavc_softc_t *sc = (iavc_softc_t*) capi_sc->ctx;
    u_int8_t val;

    if(bootverbose)
	printf("iavc%d: reset card ....\n", sc->sc_unit);

    if (sc->sc_dma)
	b1dma_reset(sc);	/* PCI cards */
    else if (sc->sc_t1)
	t1_reset(sc);		/* ISA attachment T1 */
    else
	b1_reset(sc);		/* ISA attachment B1 */

    DELAY(1000);

    if(bootverbose)
	    printf("iavc%d: start loading %d bytes firmware ....\n", sc->sc_unit, len);
    
    while (len && b1io_save_put_byte(sc, *cp++) == 0)
	len--;

    if (len) {
	printf("iavc%d: loading failed, can't write to card, len = %d\n",
	       sc->sc_unit, len);
	return (EIO);
    }

    if(bootverbose)
    	printf("iavc%d: firmware loaded, wait for ACK ....\n", sc->sc_unit);
    
    if(sc->sc_capi.card_type == CARD_TYPEC_AVM_B1_ISA)
	    iavc_put_byte(sc, SEND_POLL);
    else
	    iavc_put_byte(sc, SEND_POLLACK);    	
    
    for (len = 0; len < 1000 && !iavc_rx_full(sc); len++)
	DELAY(100);
    
    if (!iavc_rx_full(sc)) {
	printf("iavc%d: loading failed, no ack\n", sc->sc_unit);
	return (EIO);
    }
    
    val = iavc_get_byte(sc);

    if ((sc->sc_dma && val != RECEIVE_POLLDWORD) ||
	(!sc->sc_dma && val != RECEIVE_POLL)) {
	printf("iavc%d: loading failed, bad ack = %02x\n", sc->sc_unit, val);
	return (EIO);
    }

    if(bootverbose)
	    printf("iavc%d: got ACK = 0x%02x\n", sc->sc_unit, val);    

    if (sc->sc_dma) {
	/* Start the DMA engine */

	int s = SPLI4B();

	sc->sc_csr = AVM_FLAG;
	AMCC_WRITE(sc, AMCC_INTCSR, sc->sc_csr);
	AMCC_WRITE(sc, AMCC_MCSR, (EN_A2P_TRANSFERS|EN_P2A_TRANSFERS|
				   A2P_HI_PRIORITY|P2A_HI_PRIORITY|
				   RESET_A2P_FLAGS|RESET_P2A_FLAGS));

	iavc_write_port(sc, 0x07, 0x30); /* XXX magic numbers from */
	iavc_write_port(sc, 0x10, 0xf0); /* XXX the linux driver */

	sc->sc_recvlen = 0;
	AMCC_WRITE(sc, AMCC_RXPTR, vtophys(&sc->sc_recvbuf[0]));
	AMCC_WRITE(sc, AMCC_RXLEN, 4);
	sc->sc_csr |= EN_RX_TC_INT|EN_TX_TC_INT;
	AMCC_WRITE(sc, AMCC_INTCSR, sc->sc_csr);

	splx(s);
    }

    if(sc->sc_capi.card_type == CARD_TYPEC_AVM_B1_ISA)
	b1isa_setup_irq(sc);
    
    iavc_send_init(sc);

    return 0;
}

int iavc_register(capi_softc_t *capi_sc, int applid, int nchan)
{
    iavc_softc_t *sc = (iavc_softc_t*) capi_sc->ctx;
    struct mbuf *m = i4b_Dgetmbuf(23);
    u_int8_t *p;

    if (!m) {
	printf("iavc%d: can't get memory\n", sc->sc_unit);
	return (ENOMEM);
    }

    /*
     * byte  0x12 = SEND_REGISTER
     * dword ApplId
     * dword NumMessages
     * dword NumB3Connections 0..nbch
     * dword NumB3Blocks
     * dword B3Size
     */

    p = amcc_put_byte(mtod(m, u_int8_t*), 0);
    p = amcc_put_byte(p, 0);
    p = amcc_put_byte(p, SEND_REGISTER);
    p = amcc_put_word(p, applid);
#if 0
    p = amcc_put_word(p, 1024 + (nchan + 1));
#else
    p = amcc_put_word(p, 1024 * (nchan + 1));
#endif    
    p = amcc_put_word(p, nchan);
    p = amcc_put_word(p, 8);
    p = amcc_put_word(p, 2048);

    _IF_ENQUEUE(&sc->sc_txq, m);

    iavc_start_tx(sc);

    return 0;
}

int iavc_release(capi_softc_t *capi_sc, int applid)
{
    iavc_softc_t *sc = (iavc_softc_t*) capi_sc->ctx;
    struct mbuf *m = i4b_Dgetmbuf(7);
    u_int8_t *p;

    if (!m) {
	printf("iavc%d: can't get memory\n", sc->sc_unit);
	return (ENOMEM);
    }

    /*
     * byte  0x14 = SEND_RELEASE
     * dword ApplId
     */

    p = amcc_put_byte(mtod(m, u_int8_t*), 0);
    p = amcc_put_byte(p, 0);
    p = amcc_put_byte(p, SEND_RELEASE);
    p = amcc_put_word(p, applid);

    _IF_ENQUEUE(&sc->sc_txq, m);

    iavc_start_tx(sc);
    return 0;
}

int iavc_send(capi_softc_t *capi_sc, struct mbuf *m)
{
    iavc_softc_t *sc = (iavc_softc_t*) capi_sc->ctx;

    if (sc->sc_state != IAVC_UP) {
	printf("iavc%d: attempt to send before device up\n", sc->sc_unit);

	if (m->m_next) i4b_Bfreembuf(m->m_next);
	i4b_Dfreembuf(m);

	return (ENXIO);
    }

    if (_IF_QFULL(&sc->sc_txq)) {

	_IF_DROP(&sc->sc_txq);

	printf("iavc%d: tx overflow, message dropped\n", sc->sc_unit);

	if (m->m_next) i4b_Bfreembuf(m->m_next);
	i4b_Dfreembuf(m);

    } else {
	_IF_ENQUEUE(&sc->sc_txq, m);

	iavc_start_tx(sc);
    }
    
    return 0;
}

/*
//  Functions called by ourself during the initialization sequence:
//  ---------------------------------------------------------------
//
//  iavc_send_init
//      Sends the system initialization message to a newly loaded
//      board, and sets state to INIT.
*/

static int iavc_send_init(iavc_softc_t *sc)
{
    struct mbuf *m = i4b_Dgetmbuf(15);
    u_int8_t *p;
    int s;

    if (!m) {
	printf("iavc%d: can't get memory\n", sc->sc_unit);
	return (ENOMEM);
    }

    /*
     * byte  0x11 = SEND_INIT
     * dword NumApplications
     * dword NumNCCIs
     * dword BoardNumber
     */

    p = amcc_put_byte(mtod(m, u_int8_t*), 0);
    p = amcc_put_byte(p, 0);
    p = amcc_put_byte(p, SEND_INIT);
    p = amcc_put_word(p, 1); /* XXX MaxAppl XXX */
    p = amcc_put_word(p, sc->sc_capi.sc_nbch);
    p = amcc_put_word(p, sc->sc_unit);

    s = SPLI4B();
    _IF_ENQUEUE(&sc->sc_txq, m);

    iavc_start_tx(sc);

    sc->sc_state = IAVC_INIT;
    splx(s);
    return 0;
}

/*
//  Functions called during normal operation:
//  -----------------------------------------
//
//  iavc_receive_init
//      Reads the initialization reply and calls capi_ll_control().
//
//  iavc_receive_new_ncci
//      Reads a new NCCI notification and calls capi_ll_control().
//
//  iavc_receive_free_ncci
//      Reads a freed NCCI notification and calls capi_ll_control().
//
//  iavc_receive_task_ready
//      Reads a task ready message -- which should not occur XXX.
//
//  iavc_receive_debugmsg
//      Reads a debug message -- which should not occur XXX.
//
//  iavc_receive_start
//      Reads a START TRANSMIT message and unblocks device.
//
//  iavc_receive_stop
//      Reads a STOP TRANSMIT message and blocks device.
//
//  iavc_receive
//      Reads an incoming message and calls capi_ll_receive().
*/

static int iavc_receive_init(iavc_softc_t *sc, u_int8_t *dmabuf)
{
    u_int32_t Length;
    u_int8_t *p;
    u_int8_t *cardtype, *serial, *profile, *version, *caps, *prot;

    if (sc->sc_dma) {
	p = amcc_get_word(dmabuf, &Length);
    } else {
	Length = iavc_get_slice(sc, sc->sc_recvbuf);
	p = sc->sc_recvbuf;
    }

#if 0
    {
	int len = 0;
	printf("iavc%d: rx_init: ", sc->sc_unit);
	    while (len < Length) {
		printf(" %02x", p[len]);
		if (len && (len % 16) == 0) printf("\n");
		len++;
	    }
	    if (len % 16) printf("\n");
    }
#endif

    version = (p + 1);
    p += (*p + 1); /* driver version */
    cardtype = (p + 1);
    p += (*p + 1); /* card type */
    p += (*p + 1); /* hardware ID */
    serial = (p + 1);
    p += (*p + 1); /* serial number */
    caps = (p + 1);
    p += (*p + 1); /* supported options */
    prot = (p + 1);
    p += (*p + 1); /* supported protocols */
    profile = (p + 1);

    if (cardtype && serial && profile) {
	int nbch = ((profile[3]<<8) | profile[2]);

	printf("iavc%d: AVM %s, s/n %s, %d chans, f/w rev %s, prot %s\n",
		sc->sc_unit, cardtype, serial, nbch, version, prot);

	if(bootverbose)
		printf("iavc%d: %s\n", sc->sc_unit, caps);

        capi_ll_control(&sc->sc_capi, CAPI_CTRL_PROFILE, (int) profile);

    } else {
	printf("iavc%d: no profile data in info response?\n", sc->sc_unit);
    }

    sc->sc_blocked = TRUE; /* controller will send START when ready */
    return 0;
}

static int iavc_receive_start(iavc_softc_t *sc, u_int8_t *dmabuf)
{
    struct mbuf *m = i4b_Dgetmbuf(3);
    u_int8_t *p;

    if (sc->sc_blocked && sc->sc_state == IAVC_UP)
	printf("iavc%d: receive_start\n", sc->sc_unit);

    if (!m) {
	printf("iavc%d: can't get memory\n", sc->sc_unit);
	return (ENOMEM);
    }

    /*
     * byte  0x73 = SEND_POLLACK
     */

    p = amcc_put_byte(mtod(m, u_int8_t*), 0);
    p = amcc_put_byte(p, 0);
    p = amcc_put_byte(p, SEND_POLLACK);
    
    _IF_PREPEND(&sc->sc_txq, m);

    NDBGL4(L4_IAVCDBG, "iavc%d: blocked = %d, state = %d",
		sc->sc_unit, sc->sc_blocked, sc->sc_state);

    sc->sc_blocked = FALSE;
    iavc_start_tx(sc);
    
    /* If this was our first START, register our readiness */

    if (sc->sc_state != IAVC_UP) {
	sc->sc_state = IAVC_UP;
	capi_ll_control(&sc->sc_capi, CAPI_CTRL_READY, TRUE);
    }

    return 0;
}

static int iavc_receive_stop(iavc_softc_t *sc, u_int8_t *dmabuf)
{
    printf("iavc%d: receive_stop\n", sc->sc_unit);
    sc->sc_blocked = TRUE;
    return 0;
}

static int iavc_receive_new_ncci(iavc_softc_t *sc, u_int8_t *dmabuf)
{
    u_int32_t ApplId, NCCI, WindowSize;

    if (sc->sc_dma) {
	dmabuf = amcc_get_word(dmabuf, &ApplId);
	dmabuf = amcc_get_word(dmabuf, &NCCI);
	dmabuf = amcc_get_word(dmabuf, &WindowSize);
    } else {
	ApplId = iavc_get_word(sc);
	NCCI   = iavc_get_word(sc);
	WindowSize = iavc_get_word(sc);
    }

    capi_ll_control(&sc->sc_capi, CAPI_CTRL_NEW_NCCI, NCCI);
    return 0;
}

static int iavc_receive_free_ncci(iavc_softc_t *sc, u_int8_t *dmabuf)
{
    u_int32_t ApplId, NCCI;

    if (sc->sc_dma) {
	dmabuf = amcc_get_word(dmabuf, &ApplId);
	dmabuf = amcc_get_word(dmabuf, &NCCI);
    } else {
	ApplId = iavc_get_word(sc);
	NCCI   = iavc_get_word(sc);
    }

    capi_ll_control(&sc->sc_capi, CAPI_CTRL_FREE_NCCI, NCCI);
    return 0;
}

static int iavc_receive_task_ready(iavc_softc_t *sc, u_int8_t *dmabuf)
{
    u_int32_t TaskId, Length;
    u_int8_t *p;
    printf("iavc%d: receive_task_ready\n", sc->sc_unit);
    
    if (sc->sc_dma) {
	p = amcc_get_word(dmabuf, &TaskId);
	p = amcc_get_word(p, &Length);
    } else {
	TaskId = iavc_get_word(sc);
	Length = iavc_get_slice(sc, sc->sc_recvbuf);
	p = sc->sc_recvbuf;
    }

    /* XXX could show the message if trace enabled? XXX */
    return 0;
}

static int iavc_receive_debugmsg(iavc_softc_t *sc, u_int8_t *dmabuf)
{
    u_int32_t Length;
    u_int8_t *p;
    printf("iavc%d: receive_debugmsg\n", sc->sc_unit);
    
    if (sc->sc_dma) {
	p = amcc_get_word(dmabuf, &Length);
    } else {
	Length = iavc_get_slice(sc, sc->sc_recvbuf);
	p = sc->sc_recvbuf;
    }

    /* XXX could show the message if trace enabled? XXX */
    return 0;
}

static int iavc_receive(iavc_softc_t *sc, u_int8_t *dmabuf, int b3data)
{
    struct mbuf *m;
    u_int32_t ApplId, Length;

    /*
     * byte  0x21 = RECEIVE_MESSAGE
     * dword ApplId
     * dword length
     * ...   CAPI msg
     *
     * --or--
     *
     * byte  0x22 = RECEIVE_DATA_B3_IND
     * dword ApplId
     * dword length
     * ...   CAPI msg
     * dword datalen
     * ...   B3 data
     */

    if (sc->sc_dma) {
	dmabuf = amcc_get_word(dmabuf, &ApplId);
	dmabuf = amcc_get_word(dmabuf, &Length);
    } else {
	ApplId = iavc_get_word(sc);
	Length = iavc_get_slice(sc, sc->sc_recvbuf);
	dmabuf = sc->sc_recvbuf;
    }

    m = i4b_Dgetmbuf(Length);
    if (!m) {
	printf("iavc%d: can't get memory for receive\n", sc->sc_unit);
	return (ENOMEM);
    }

    bcopy(dmabuf, mtod(m, u_int8_t*), Length);

#if 0
	{
	    u_int8_t *p = mtod(m, u_int8_t*);
	    int len = 0;
	    printf("iavc%d: applid=%d, len=%d\n", sc->sc_unit, ApplId, Length);
	    while (len < m->m_len) {
		printf(" %02x", p[len]);
		if (len && (len % 16) == 0) printf("\n");
		len++;
	    }
	    if (len % 16) printf("\n");
	}
#endif

    if (b3data) {
	if (sc->sc_dma) {
	    dmabuf = amcc_get_word(dmabuf + Length, &Length);
	} else {
	    Length = iavc_get_slice(sc, sc->sc_recvbuf);
	    dmabuf = sc->sc_recvbuf;
	}

	m->m_next = i4b_Bgetmbuf(Length);
	if (!m->m_next) {
	    printf("iavc%d: can't get memory for receive\n", sc->sc_unit);
	    i4b_Dfreembuf(m);
	    return (ENOMEM);
	}

	bcopy(dmabuf, mtod(m->m_next, u_int8_t*), Length);
    }

    capi_ll_receive(&sc->sc_capi, m);
    return 0;
}

/*
//  iavc_handle_intr
//      Checks device interrupt status and calls iavc_handle_{rx,tx}()
//      as necessary.
//
//  iavc_handle_rx
//      Reads in the command byte and calls the subroutines above.
//
//  iavc_start_tx
//      Initiates DMA on the next queued message if possible.
*/

void iavc_handle_intr(iavc_softc_t *sc)
{
    u_int32_t status;
    u_int32_t newcsr;

    if (!sc->sc_dma) {
	while (iavc_rx_full(sc))
	    iavc_handle_rx(sc);
	return;
    }

    status = AMCC_READ(sc, AMCC_INTCSR);
    if ((status & ANY_S5933_INT) == 0)
	return;

    newcsr = sc->sc_csr | (status & ALL_INT);
    if (status & TX_TC_INT) newcsr &= ~EN_TX_TC_INT;
    if (status & RX_TC_INT) newcsr &= ~EN_RX_TC_INT;
    AMCC_WRITE(sc, AMCC_INTCSR, newcsr);
    sc->sc_intr = TRUE;

    if (status & RX_TC_INT) {
	u_int32_t rxlen;

	if (sc->sc_recvlen == 0) {
	    sc->sc_recvlen = *((u_int32_t*)(&sc->sc_recvbuf[0]));
	    rxlen = (sc->sc_recvlen + 3) & ~3;
	    AMCC_WRITE(sc, AMCC_RXPTR, vtophys(&sc->sc_recvbuf[4]));
	    AMCC_WRITE(sc, AMCC_RXLEN, rxlen);
	} else {
	    iavc_handle_rx(sc);
	    sc->sc_recvlen = 0;
	    AMCC_WRITE(sc, AMCC_RXPTR, vtophys(&sc->sc_recvbuf[0]));
	    AMCC_WRITE(sc, AMCC_RXLEN, 4);
	}
    }

    if (status & TX_TC_INT) {
	sc->sc_csr &= ~EN_TX_TC_INT;
	iavc_start_tx(sc);
    }

    AMCC_WRITE(sc, AMCC_INTCSR, sc->sc_csr);
    sc->sc_intr = FALSE;
}

static void iavc_handle_rx(iavc_softc_t *sc)
{
    u_int8_t *dmabuf = 0, cmd;

    if (sc->sc_dma) {
	dmabuf = amcc_get_byte(&sc->sc_recvbuf[4], &cmd);
    } else {
	cmd = iavc_get_byte(sc);
    }

    NDBGL4(L4_IAVCDBG, "iavc%d: command = 0x%02x", sc->sc_unit, cmd);
    
    switch (cmd) {
    case RECEIVE_DATA_B3_IND:
	iavc_receive(sc, dmabuf, TRUE);
	break;

    case RECEIVE_MESSAGE:
	iavc_receive(sc, dmabuf, FALSE);
	break;

    case RECEIVE_NEW_NCCI:
	iavc_receive_new_ncci(sc, dmabuf);
	break;

    case RECEIVE_FREE_NCCI:
	iavc_receive_free_ncci(sc, dmabuf);
	break;

    case RECEIVE_START:
	iavc_receive_start(sc, dmabuf);
	break;

    case RECEIVE_STOP:
	iavc_receive_stop(sc, dmabuf);
	break;

    case RECEIVE_INIT:
	iavc_receive_init(sc, dmabuf);
	break;

    case RECEIVE_TASK_READY:
	iavc_receive_task_ready(sc, dmabuf);
	break;

    case RECEIVE_DEBUGMSG:
	iavc_receive_debugmsg(sc, dmabuf);
	break;

    default:
	printf("iavc%d: unknown msg %02x\n", sc->sc_unit, cmd);
    }
}

static void iavc_start_tx(iavc_softc_t *sc)
{
    struct mbuf *m;
    u_int8_t *dmabuf;
    u_int32_t txlen = 0;
    
    /* If device has put us on hold, punt. */

    if (sc->sc_blocked) {
	return;
    }

    /* If using DMA and transmitter busy, punt. */
    
    if (sc->sc_dma && (sc->sc_csr & EN_TX_TC_INT)) {
	return;
    }

    /* Else, see if we have messages to send. */

    _IF_DEQUEUE(&sc->sc_txq, m);
    if (!m) {
	return;
    }

    /* Have message, will send. */

    if (CAPIMSG_LEN(m->m_data)) {
	/* A proper CAPI message, possibly with B3 data */

	if (sc->sc_dma) {
	    /* Copy message to DMA buffer. */

	    if (m->m_next) {
		dmabuf = amcc_put_byte(&sc->sc_sendbuf[0], SEND_DATA_B3_REQ);
	    } else {
		dmabuf = amcc_put_byte(&sc->sc_sendbuf[0], SEND_MESSAGE);
	    }

	    dmabuf = amcc_put_word(dmabuf, m->m_len);
	    bcopy(m->m_data, dmabuf, m->m_len);
	    dmabuf += m->m_len;
	    txlen = 5 + m->m_len;

	    if (m->m_next) {
		dmabuf = amcc_put_word(dmabuf, m->m_next->m_len);
		bcopy(m->m_next->m_data, dmabuf, m->m_next->m_len);
		txlen += 4 + m->m_next->m_len;
	    }

	} else {
	    /* Use PIO. */

	    if (m->m_next) {
		iavc_put_byte(sc, SEND_DATA_B3_REQ);
		NDBGL4(L4_IAVCDBG, "iavc%d: tx SDB3R msg, len = %d", sc->sc_unit, m->m_len);
	    } else {
		iavc_put_byte(sc, SEND_MESSAGE);
		NDBGL4(L4_IAVCDBG, "iavc%d: tx SM msg, len = %d", sc->sc_unit, m->m_len);
	    }
#if 0
    {
	u_int8_t *p = mtod(m, u_int8_t*);
	int len;
	for (len = 0; len < m->m_len; len++) {
	    printf(" %02x", *p++);
	    if (len && (len % 16) == 0) printf("\n");
	}
	if (len % 16) printf("\n");
    }
#endif

	    iavc_put_slice(sc, m->m_data, m->m_len);

	    if (m->m_next) {
		iavc_put_slice(sc, m->m_next->m_data, m->m_next->m_len);
	    }
	}

    } else {
	/* A board control message to be sent as is */

	if (sc->sc_dma) {
	    bcopy(m->m_data + 2, &sc->sc_sendbuf[0], m->m_len - 2);
	    txlen = m->m_len - 2;

	} else {
#if 0
    {
	u_int8_t *p = mtod(m, u_int8_t*) + 2;
	int len;
	printf("iavc%d: tx BDC msg, len = %d, msg =", sc->sc_unit, m->m_len-2);
	for (len = 0; len < m->m_len-2; len++) {
	    printf(" %02x", *p++);
	    if (len && (len % 16) == 0) printf("\n");
	}
	if (len % 16) printf("\n");
    }
#endif

	    txlen = m->m_len - 2;
	    dmabuf = mtod(m, char*) + 2;
	    while(txlen--)
	    	b1io_put_byte(sc, *dmabuf++);
	}
    }

    if (m->m_next) {
	i4b_Bfreembuf(m->m_next);
	m->m_next = NULL;
    }
    i4b_Dfreembuf(m);

    if (sc->sc_dma) {
	/* Start transmitter */

	txlen = (txlen + 3) & ~3;
	AMCC_WRITE(sc, AMCC_TXPTR, vtophys(&sc->sc_sendbuf[0]));
	AMCC_WRITE(sc, AMCC_TXLEN, txlen);
	sc->sc_csr |= EN_TX_TC_INT;

	if (!sc->sc_intr)
	    AMCC_WRITE(sc, AMCC_INTCSR, sc->sc_csr);
    }
}

#endif
