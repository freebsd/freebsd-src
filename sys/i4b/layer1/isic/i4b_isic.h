/*-
 * Copyright (c) 1997, 2001 Hellmuth Michaelis. All rights reserved.
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

/*---------------------------------------------------------------------------*
 *
 *	i4b_l1.h - isdn4bsd layer 1 header file
 *	---------------------------------------
 *
 * $FreeBSD$
 *
 *      last edit-date: [Fri Jan 26 13:55:12 2001]
 *
 *---------------------------------------------------------------------------*/

#ifndef _I4B_ISIC_H_
#define _I4B_ISIC_H_

#include <sys/resource.h>
#include <sys/bus.h>
#include <i386/include/bus.h>
#include <sys/rman.h>
#include <i386/include/resource.h>

#include <i4b/include/i4b_l3l4.h>

#include <i4b/layer1/isic/i4b_isic_ext.h>

/*---------------------------------------------------------------------------
 *      isic driver: max no of units
 *      Teles/Creatix/Neuhaus cards have a hardware limitation
 *      as one is able to set 3 (sometimes 4) different configurations by
 *      jumpers so a maximum of 3 (4) cards per ISA bus is possible.
 *---------------------------------------------------------------------------*/
#define ISIC_MAXUNIT	3	/* max no of supported units 0..3 */

#define INFO_IO_BASES	50	/* 49 needed for USR */

struct i4b_info {
	struct resource * io_base[INFO_IO_BASES];
	int               io_rid [INFO_IO_BASES];
	struct resource * irq;
	int               irq_rid;
	struct resource * mem;
	int               mem_rid;
};

/*---------------------------------------------------------------------------*
 *	l1_bchan_state: the state of one B channel
 *---------------------------------------------------------------------------*/
typedef struct
{
	int		unit;		/* cards unit number	*/
	int		channel;	/* which channel is this*/
	caddr_t		hscx;		/* HSCX address		*/
	u_char		hscx_mask;	/* HSCX interrupt mask	*/
	int		bprot;		/* B channel protocol	*/
	int		state;		/* this channels state	*/
#define HSCX_IDLE	0x00		/* channel idle 	*/
#define HSCX_TX_ACTIVE	0x01		/* tx running		*/

	/* receive data from ISDN */

	struct ifqueue	rx_queue;	/* receiver queue	*/

	int		rxcount;	/* rx statistics counter*/

	struct	mbuf	*in_mbuf;	/* rx input buffer	*/
	u_char 		*in_cbptr;	/* curr buffer pointer	*/
	int		in_len;		/* rx input buffer len	*/
	
	/* transmit data to ISDN */

	struct ifqueue	tx_queue;	/* transmitter queue	*/

	int		txcount;	/* tx statistics counter*/

	struct mbuf	*out_mbuf_head;	/* first mbuf in possible chain	*/
	struct mbuf	*out_mbuf_cur;	/* current mbuf in possbl chain */
	unsigned char	*out_mbuf_cur_ptr; /* data pointer into mbuf	*/
	int		out_mbuf_cur_len; /* remaining bytes in mbuf	*/	
	
	/* link between b channel and driver */
	
	isdn_link_t	isic_isdn_linktab;	/* b channel addresses	*/
	drvr_link_t	*isic_drvr_linktab;	/* ptr to driver linktab*/

	/* statistics */

	/* RSTA */
	
	int		stat_VFR;	/* HSCX RSTA Valid FRame */
	int		stat_RDO;	/* HSCX RSTA Rx Data Overflow */	
	int		stat_CRC;	/* HSCX RSTA CRC */
	int		stat_RAB;	/* HSCX RSTA Rx message ABorted */

	/* EXIR */

	int		stat_XDU;	/* HSCX EXIR tx data underrun */
	int		stat_RFO;	/* HSCX EXIR rx frame overflow */
	
} l1_bchan_state_t;

/*---------------------------------------------------------------------------*
 *	l1_softc: the state of the layer 1 of the D channel
 *---------------------------------------------------------------------------*/
struct l1_softc
{
	int		sc_unit;	/* unit number		*/
	int		sc_irq;		/* interrupt vector	*/
	struct i4b_info	sc_resources;

	int		sc_port;	/* port base address	*/

	int		sc_cardtyp;	/* CARD_TYPEP_xxxx	*/

	int		sc_bustyp;	/* IOM1 or IOM2		*/
#define BUS_TYPE_IOM1  0x01
#define BUS_TYPE_IOM2  0x02

	int		sc_trace;	/* output protocol data for tracing */
	unsigned int	sc_trace_dcount;/* d channel trace frame counter */
	unsigned int	sc_trace_bcount;/* b channel trace frame counter */

	int		sc_state;	/* ISAC state flag	*/
#define ISAC_IDLE	0x00		/* state = idle */
#define ISAC_TX_ACTIVE	0x01		/* state = transmitter active */

	int		sc_init_tries;	/* no of out tries to access S0 */
	
	caddr_t		sc_vmem_addr;	/* card RAM virtual memory base */
	caddr_t		sc_isac;	/* ISAC port base addr	*/
#define ISAC_BASE	(sc->sc_isac)

	caddr_t		sc_ipacbase;	/* IPAC port base addr	*/
#define IPAC_BASE	(sc->sc_ipacbase)

	u_char		sc_isac_mask;	/* ISAC IRQ mask	*/
#define ISAC_IMASK	(sc->sc_isac_mask)

	l1_bchan_state_t	sc_chan[2];	/* B-channel state	*/
#define HSCX_A_BASE	(sc->sc_chan[0].hscx)
#define HSCX_A_IMASK	(sc->sc_chan[0].hscx_mask)
#define HSCX_B_BASE	(sc->sc_chan[1].hscx)
#define HSCX_B_IMASK	(sc->sc_chan[1].hscx_mask)

	struct mbuf	*sc_ibuf;	/* input buffer mgmt	*/
	u_short		sc_ilen;
	u_char		*sc_ib;
					/* this is for the irq TX routine */
	struct mbuf	*sc_obuf;	/* pointer to an mbuf with TX frame */
	u_char		*sc_op;		/* ptr to next chunk of frame to tx */
	int		sc_ol;		/* length of remaining frame to tx */
	int		sc_freeflag;	/* m_freem mbuf if set */

	struct mbuf	*sc_obuf2;	/* pointer to an mbuf with TX frame */
	int		sc_freeflag2;	/* m_freem mbuf if set */	
	
	int		sc_isac_version;	/* version number of ISAC */
	int		sc_hscx_version;	/* version number of HSCX */
	int		sc_ipac_version;	/* version number of IPAC */
	
	int		sc_I430state;	/* I.430 state F3 .... F8 */

	int		sc_I430T3;	/* I.430 Timer T3 running */	

	struct callout_handle sc_T3_callout;

	int		sc_I430T4;	/* Timer T4 running */	

	struct callout_handle sc_T4_callout;

	/*
	 * byte fields for the AVM Fritz!Card PCI. These are packed into
	 * a u_int in the driver.
	 */
	u_char		avma1pp_cmd;
	u_char		avma1pp_txl;
	u_char		avma1pp_prot;

	int		sc_enabled;	/* daemon is running */

	int		sc_ipac;	/* flag, running on ipac */
	int		sc_bfifolen;	/* length of b channel fifos */

#define	ISIC_WHAT_ISAC	0
#define	ISIC_WHAT_HSCXA	1
#define	ISIC_WHAT_HSCXB	2
#define	ISIC_WHAT_IPAC	3

	u_int8_t (*readreg)   (struct l1_softc *sc, int what, bus_size_t offs);
	void	 (*writereg)  (struct l1_softc *sc, int what, bus_size_t offs, u_int8_t data);
	void	 (*readfifo)  (struct l1_softc *sc, int what, void *buf, size_t size);
	void	 (*writefifo) (struct l1_softc *sc, int what, void *data, size_t size);
	void	 (*clearirq)  (struct l1_softc *sc);

#define	ISAC_READ(r)		(*sc->readreg)(sc, ISIC_WHAT_ISAC, (r))
#define	ISAC_WRITE(r,v)		(*sc->writereg)(sc, ISIC_WHAT_ISAC, (r), (v))
#define	ISAC_RDFIFO(b,s)	(*sc->readfifo)(sc, ISIC_WHAT_ISAC, (b), (s))
#define	ISAC_WRFIFO(b,s)	(*sc->writefifo)(sc, ISIC_WHAT_ISAC, (b), (s))

#define	HSCX_READ(n,r)		(*sc->readreg)(sc, ISIC_WHAT_HSCXA+(n), (r))
#define	HSCX_WRITE(n,r,v)	(*sc->writereg)(sc, ISIC_WHAT_HSCXA+(n), (r), (v))
#define	HSCX_RDFIFO(n,b,s)	(*sc->readfifo)(sc, ISIC_WHAT_HSCXA+(n), (b), (s))
#define	HSCX_WRFIFO(n,b,s)	(*sc->writefifo)(sc, ISIC_WHAT_HSCXA+(n), (b), (s))

#define IPAC_READ(r)		(*sc->readreg)(sc, ISIC_WHAT_IPAC, (r))
#define IPAC_WRITE(r, v)	(*sc->writereg)(sc, ISIC_WHAT_IPAC, (r), (v))
};

/*---------------------------------------------------------------------------*
 *	possible I.430/ISAC states
 *---------------------------------------------------------------------------*/
enum I430states {
	ST_F3,		/* F3 Deactivated	*/
	ST_F4,		/* F4 Awaiting Signal	*/
	ST_F5,		/* F5 Identifying Input */
	ST_F6,		/* F6 Synchronized	*/
	ST_F7,		/* F7 Activated		*/
	ST_F8,		/* F8 Lost Framing	*/
	ST_ILL,		/* Illegal State	*/	
	N_STATES
};

/*---------------------------------------------------------------------------*
 *	possible I.430/ISAC events
 *---------------------------------------------------------------------------*/
enum I430events {
	EV_PHAR,	/* PH ACTIVATE REQUEST 		*/
	EV_T3,		/* Timer 3 expired 		*/
	EV_INFO0,	/* receiving INFO0 		*/
	EV_RSY,		/* receiving any signal		*/
	EV_INFO2,	/* receiving INFO2		*/
	EV_INFO48,	/* receiving INFO4 pri 8/9 	*/
	EV_INFO410,	/* receiving INFO4 pri 10/11	*/	
	EV_DR,		/* Deactivate Request 		*/	
	EV_PU,		/* Power UP			*/
	EV_DIS,		/* Disconnected (only 2085) 	*/
	EV_EI,		/* Error Indication 		*/
	EV_ILL,		/* Illegal Event 		*/
	N_EVENTS
};

enum I430commands {
	CMD_TIM,	/*	Timing				*/
	CMD_RS,		/*	Reset				*/
	CMD_AR8,	/*	Activation request pri 8	*/
	CMD_AR10,	/*	Activation request pri 10	*/
	CMD_DIU,	/*	Deactivate Indication Upstream	*/
	CMD_ILL		/*	Illegal command			*/
};

#define N_COMMANDS CMD_ILL

extern struct l1_softc l1_sc[];

extern void isicintr(struct l1_softc *sc);
extern int  isic_attach_common(device_t dev);
extern void isic_detach_common(device_t dev);
extern void isic_recover(struct l1_softc *sc);

extern void isic_bchannel_setup (int unit, int hscx_channel, int bprot, int activate );

extern void isic_init_linktab ( struct l1_softc *sc );
extern int  isic_isac_init ( struct l1_softc *sc );
extern void isic_isac_irq ( struct l1_softc *sc, int r );
extern void isic_isac_l1_cmd ( struct l1_softc *sc, int command );
extern void isic_next_state ( struct l1_softc *sc, int event );
extern char *isic_printstate ( struct l1_softc *sc );

extern int  isic_hscx_fifo(l1_bchan_state_t *, struct l1_softc *);
extern void isic_hscx_init ( struct l1_softc *sc, int hscx_channel, int activate );
extern void isic_hscx_irq ( struct l1_softc *sc, u_char ista, int hscx_channel, u_char ex_irq );
extern int  isic_hscx_silence ( unsigned char *data, int len );
extern void isic_hscx_cmd( struct l1_softc *sc, int h_chan, unsigned char cmd );
extern void isic_hscx_waitxfw( struct l1_softc *sc, int h_chan );

extern int  isic_probe_s016 (device_t dev);
extern int  isic_attach_s016 (device_t dev);

extern int  isic_probe_s08 (device_t dev);
extern int  isic_attach_s08 (device_t dev);

extern int  isic_probe_Epcc16 (device_t dev);
extern int  isic_attach_Epcc16 (device_t dev);

extern int  isic_probe_s0163 (device_t dev);
extern int  isic_attach_s0163 (device_t dev);

extern int  isic_probe_avma1 (device_t dev);
extern int  isic_attach_avma1 (device_t dev);

extern int  isic_probe_usrtai (device_t dev);
extern int  isic_attach_usrtai (device_t dev);

extern int  isic_probe_itkix1 (device_t dev);
extern int  isic_attach_itkix1 (device_t dev);

extern int  isic_attach_drnngo (device_t dev);
extern int  isic_attach_Cs0P (device_t dev);
extern int  isic_attach_Eqs1pi(device_t dev);
extern int  isic_attach_sws(device_t dev);
extern int  isic_attach_siemens_isurf(device_t dev);
extern int  isic_attach_asi(device_t dev);
extern int  isic_attach_Dyn(device_t dev);
extern int  isic_attach_diva(device_t dev);
extern int  isic_attach_diva_ipac(device_t dev);

#endif /* _I4B_ISIC_H_ */
