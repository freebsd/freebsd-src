/*-
 * Copyright (c) 1999, 2000 Dave Boyce. All rights reserved.
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

/*---------------------------------------------------------------------------
 *
 *      i4b_iwic - isdn4bsd Winbond W6692 driver
 *      ----------------------------------------
 *
 * $FreeBSD$
 *
 *      last edit-date: [Sun Jan 21 11:08:44 2001]
 *
 *---------------------------------------------------------------------------*/

#ifndef _I4B_IWIC_H_
#define _I4B_IWIC_H_

#include <i4b/layer1/iwic/i4b_iwic_ext.h>

/*---------------------------------------------------------------------------*
 *	PCI resources used
 *---------------------------------------------------------------------------*/

#define INFO_IO_BASES	2

struct i4b_info {
	struct resource * io_base[INFO_IO_BASES];
	int               io_rid [INFO_IO_BASES];
	struct resource * irq;
	int               irq_rid;
	struct resource * mem;
	int               mem_rid;
};

/*---------------------------------------------------------------------------*
 *	state of a B channel 
 *---------------------------------------------------------------------------*/
struct iwic_bchan
{
	int unit;			/* unit number		*/
	int channel;			/* channel number	*/
	int offset;			/* offset from iobase	*/
	int bprot;			/* b channel protocol used */
	int state;			/* transceiver state:	*/
#define ST_IDLE		0x00		/* channel idle		*/
#define ST_TX_ACTIVE	0x01    	/* tx running		*/

	int sc_trace_bcount;
	
        /* receive data from ISDN */

	struct ifqueue	rx_queue;	/* receiver queue	*/
	int		rxcount;	/* rx statistics counter*/
	struct mbuf	*in_mbuf;	/* rx input buffer	*/
	u_char		*in_cbptr;	/* curr buffer pointer	*/
	int		in_len;		/* rx input buffer len	*/

	/* transmit data to ISDN */

	struct ifqueue	tx_queue;	/* transmitter queue		*/
	int		txcount;	/* tx statistics counter	*/
	struct mbuf	*out_mbuf_head;	/* first mbuf in possible chain */
	struct mbuf	*out_mbuf_cur;	/* current mbuf in possbl chain */
	unsigned char	*out_mbuf_cur_ptr; /* data pointer into mbuf    */
	int		out_mbuf_cur_len; /* remaining bytes in mbuf    */
	
	/* linktab */
	
	isdn_link_t	iwic_isdn_linktab;
	drvr_link_t	*iwic_drvr_linktab;
};

/*---------------------------------------------------------------------------*
 *	state of a D channel
 *---------------------------------------------------------------------------*/
struct iwic_dchan
{
	int		enabled;
	int		trace_count;
	struct mbuf	*ibuf;
	u_char		*ibuf_ptr;	/* Input buffer pointer */
	int		ibuf_len;	/* Current length of input buffer */
	int		ibuf_max_len;	/* Max length in input buffer */
	int		rx_count;

	int		tx_ready;	/* Can send next 64 bytes of data. */
	int		tx_count;

	struct mbuf	*obuf;
	int		free_obuf;
	u_char		*obuf_ptr;
	int		obuf_len;

	struct mbuf	*obuf2;
	int		free_obuf2;
};

/*---------------------------------------------------------------------------*
 *	state of one iwic unit
 *---------------------------------------------------------------------------*/
struct iwic_softc
{
	int		sc_unit;
	u_int32_t	sc_iobase;
	int		sc_trace;
	int		sc_cardtyp;

	int		sc_I430state;
	int		sc_I430T3;

	int		enabled;

	struct iwic_dchan sc_dchan;
	struct iwic_bchan sc_bchan[2];

	struct i4b_info	sc_resources;
};

/*---------------------------------------------------------------------------*
 *	rd/wr register/fifo macros
 *---------------------------------------------------------------------------*/
#define IWIC_READ(sc,reg)	(inb   ((sc)->sc_iobase + (u_int32_t)(reg)))
#define IWIC_WRITE(sc,reg,val)	(outb  ((sc)->sc_iobase + (u_int32_t)(reg), (val)))
#define IWIC_WRDFIFO(sc,p,l)    (outsb ((sc)->sc_iobase + D_XFIFO, (p), (l)))
#define IWIC_RDDFIFO(sc,p,l)    (insb  ((sc)->sc_iobase + D_RFIFO, (p), (l)))
#define IWIC_WRBFIFO(sc,b,p,l)  (outsb (((sc)->sc_iobase + (b)->offset + B_XFIFO), (p), (l)))
#define IWIC_RDBFIFO(sc,b,p,l)  (insb  (((sc)->sc_iobase + (b)->offset + B_RFIFO), (p), (l)))

/*---------------------------------------------------------------------------*
 *	possible I.430 states
 *---------------------------------------------------------------------------*/
enum I430states
{
	ST_F3N,			/* F3 Deactivated, no clock	*/
	ST_F3,			/* F3 Deactivated		*/
	ST_F4,			/* F4 Awaiting Signal		*/
	ST_F5,			/* F5 Identifying Input		*/
	ST_F6,			/* F6 Synchronized		*/
	ST_F7,			/* F7 Activated			*/
	ST_F8,			/* F8 Lost Framing		*/
	ST_ILL,			/* Illegal State		*/
	N_STATES
};

/*---------------------------------------------------------------------------*
 *	possible I.430 events
 *---------------------------------------------------------------------------*/
enum I430events
{
	EV_PHAR,		/* PH ACTIVATE REQUEST          */
	EV_CE,			/* Clock enabled                */
	EV_T3,			/* Timer 3 expired              */
	EV_INFO0,		/* receiving INFO0              */
	EV_RSY,			/* receiving any signal         */
	EV_INFO2,		/* receiving INFO2              */
	EV_INFO48,		/* receiving INFO4 pri 8/9      */
	EV_INFO410,		/* receiving INFO4 pri 10/11    */
	EV_DR,			/* Deactivate Request           */
	EV_PU,			/* Power UP                     */
	EV_DIS,			/* Disconnected (only 2085)     */
	EV_EI,			/* Error Indication             */
	EV_ILL,			/* Illegal Event                */
	N_EVENTS
};

/*---------------------------------------------------------------------------*
 *	available commands
 *---------------------------------------------------------------------------*/
enum I430commands
{
	CMD_ECK,		/* Enable clock			*/
	CMD_TIM,		/* Timing			*/
	CMD_RT,			/* Reset			*/
	CMD_AR8,		/* Activation request pri 8	*/
	CMD_AR10,		/* Activation request pri 10	*/
	CMD_DIU,		/* Deactivate Indication Upstream */
	CMD_ILL			/* Illegal command		*/
};


extern struct iwic_softc iwic_sc[];

#define iwic_find_sc(unit)	(&iwic_sc[(unit)])

extern void iwic_init(struct iwic_softc *);
extern void iwic_next_state(struct iwic_softc *, int);

extern void iwic_dchan_init(struct iwic_softc *);
extern void iwic_dchan_xirq(struct iwic_softc *);
extern void iwic_dchan_xfer_irq(struct iwic_softc *, int);
extern void iwic_dchan_disable(struct iwic_softc *sc);
extern int iwic_dchan_data_req(struct iwic_softc *sc, struct mbuf *m, int freeflag);
extern void iwic_dchan_transmit(struct iwic_softc *sc);

char *iwic_printstate(struct iwic_softc *sc);

void iwic_init_linktab(struct iwic_softc *sc);
void iwic_bchan_xirq(struct iwic_softc *, int);
void iwic_bchannel_setup(int unit, int h_chan, int bprot, int activate);

#endif /* _I4B_IWIC_H_ */
