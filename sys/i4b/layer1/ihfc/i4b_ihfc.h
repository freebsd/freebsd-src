/*-
 * Copyright (c) 2000 Hans Petter Selasky. All rights reserved.
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
 *	i4b_ihfc.h - ihfc common header file
 *	------------------------------------
 *
 *	last edit-date: [Wed Jul 19 09:40:45 2000]
 *
 *	$Id: i4b_ihfc.h,v 1.9 2000/09/19 13:50:36 hm Exp $
 *
 * $FreeBSD$
 *
 *---------------------------------------------------------------------------*/

#ifndef _I4B_IHFC_H_
#define _I4B_IHFC_H_

#include <i4b/include/i4b_l3l4.h>

/*---------------------------------------------------------------------------*
 *	global stuff						(HFC-1/S/SP)
 *---------------------------------------------------------------------------*/
#define DCH_MAX_LEN		264	/* max length of a D frame */

#define IHFC_ACTIVATION_TIMEOUT	3*hz	/* S0-bus must activate before this time */

#define IHFC_IO_BASES		1

#define IHFC_DISBUSYTO		500	/* do at least 500 inb's before giving up */
#define IHFC_NONBUSYTO		8000	/* do at least 8000 inb's before giving up */

#define IHFC_NTMODE		0	/* use TE-mode as default 	 */
#define IHFC_DLP		0	/* use (8/9) priority as default */

#define	IHFC_MAXUNIT		4

/* #define IHFC_DEBUG	internal debugging enabled  *
 * #undef  IHFC_DEBUG	internal debugging disabled */

/* chan:           *
 * 0 - D1 (tx)     *
 * 1 - D1 (rx)     *
 * 2 - B1 (tx)     *
 * 3 - B1 (rx)     *
 * 4 - B2 (tx)     *
 * 5 - B2 (rx)     */

#define HFC_1			0x01	/* HFC		2B	*/
#define HFC_S			0x02	/* HFC - S	2BDS0	*/
#define HFC_SP			0x04	/* HFC - SP	2BDS0	*/
#define HFC_SPCI		0x08	/* HFC - SPCI	2BDS0 X */
#define HFC_S2M			0x10	/* HFC - S2M	2BDS0 X */
#define HFC_USB			0x20	/* HFC - USB	2BDS0 X */

/*---------------------------------------------------------------------------*
 *	"Help Fix Corruption" macros				(HFC-1/S/SP)
 *
 *	NOTE: If the code does not run at splhigh, we will sporadically
 *	lose bytes. On fast PC's (200 Mhz), this is very little noticable.
 *---------------------------------------------------------------------------*/
#define HFC_VAR	int _s_			/* declare variable	*/
#define HFC_BEG	_s_ = splhigh()		/* save spl		*/
#define HFC_END	splx(_s_)		/* restore spl		*/

/*---------------------------------------------------------------------------*
 *	macros related to i4b linking				(HFC-1/S/SP)
 *---------------------------------------------------------------------------*/
#define S_BLINK		sc->sc_blinktab[(chan > 3) ? 1 : 0]
#define S_BDRVLINK	sc->sc_bdrvlinktab[(chan > 3) ? 1 : 0]

/*---------------------------------------------------------------------------*
 *	macros related to ihfc_sc				(HFC-1/S/SP)
 *---------------------------------------------------------------------------*/

/* statemachine */

#define S_IOM2		(sc->sc_config.i_adf2 & 0x80)
	/* 0x80: IOM2 mode selected */

#define S_DLP		(sc->sc_config.dlp)
#define S_NTMODE	(sc->sc_config.ntmode)
#define S_STDEL		(sc->sc_config.stdel)

#define	S_PHSTATE	sc->sc_statemachine.state
#define	S_STM_T3	sc->sc_statemachine.T3
#define	S_STM_T3CALLOUT	sc->sc_statemachine.T3callout

/* unitnumbers */

#define S_UNIT		sc->sc_unit
#define S_FLAG		sc->sc_flag
#define S_I4BUNIT	sc->sc_i4bunit
#define S_I4BFLAG	sc->sc_i4bflag

/* ISA bus setup */

#define S_IOBASE	sc->sc_resources.io_base
#define S_IORID		sc->sc_resources.io_rid
#define S_IRQ		sc->sc_resources.irq
#define S_IRQRID	sc->sc_resources.irq_rid

/* hardware setup */

#define S_HFC		sc->sc_config.chiptype
#define S_IIO		sc->sc_config.iio
#define S_IIRQ		sc->sc_config.iirq

/* registers of the HFC-S/SP	(write only) */

#define S_HFC_CONFIG	sc->sc_config.cirm

#define S_CIRM		sc->sc_config.cirm
#define S_CTMT		sc->sc_config.ctmt
#define S_TEST		sc->sc_config.test
#define S_SCTRL		sc->sc_config.sctrl
#define S_CLKDEL	sc->sc_config.clkdel
#define S_INT_M1	sc->sc_config.int_m1
#define S_INT_M2	sc->sc_config.int_m2
#define S_CONNECT	sc->sc_config.connect
#define S_SCTRL_R	sc->sc_config.sctrl_r
#define S_MST_MODE	sc->sc_config.mst_mode

/* registers of the HFC-S/SP 	(read only) */

#define S_INT_S1	sc->sc_config.int_s1

/* registers of the ISAC 	(write only) */

#define S_ISAC_CONFIG	sc->sc_config.i_adf2

#define	S_ADF1		sc->sc_config.i_adf1
#define	S_ADF2		sc->sc_config.i_adf2
#define	S_MASK		sc->sc_config.i_mask
#define	S_MODE		sc->sc_config.i_mode
#define	S_SPCR		sc->sc_config.i_spcr
#define	S_SQXR		sc->sc_config.i_sqxr
#define	S_STCR		sc->sc_config.i_stcr
#define	S_STAR2		sc->sc_config.i_star2

/* registers of the ISAC 	(read only) */

#define	S_ISTA		sc->sc_config.i_ista

/* state of the softc */

#define	S_ENABLED	sc->sc_enabled
#define S_INTR_ACTIVE	sc->sc_intr_active

/* SOFT-HDLC */

#define S_HDLC_IB	sc->sc_fifo.chan[chan].hdlc.ib		/* u_short */
#define S_HDLC_CRC	sc->sc_fifo.chan[chan].hdlc.crc		/* u_short */
#define S_HDLC_TMP	sc->sc_fifo.chan[chan].hdlc.tmp		/* u_int   */
#define S_HDLC_FLAG	sc->sc_fifo.chan[chan].hdlc.flag	/* u_char  */
#define S_HDLC_BLEVEL	sc->sc_fifo.chan[chan].hdlc.blevel	/* u_short */

/* stats */

#define S_BYTES		sc->sc_fifo.chan[chan].bytes

/* "Z"-values */

#define S_HDLC_DZ_TAB	sc->sc_fifo.dztable

/* filters */

#define S_PROT		sc->sc_fifo.chan[chan].prot
#define S_FILTER	sc->sc_fifo.chan[chan].filter
#define S_ACTIVITY	sc->sc_fifo.chan[chan].activity
#define S_LAST_CHAN	sc->sc_fifo.last_chan

/* soft reset */

#define RESET_SOFT_CHAN(sc, chan)	bzero(&sc->sc_fifo.chan[chan], sizeof(sc->sc_fifo.chan[0]))

/* trace */

#define S_TRACE		sc->sc_trace
#define S_DTRACECOUNT	sc->sc_Dtracecount
#define S_BTRACECOUNT	sc->sc_Btracecount

/* mbuf */

#define S_MBUF		sc->sc_fifo.chan[chan].buffer.mbuf
#define S_MBUFDUMMY	sc->sc_fifo.chan[chan].buffer.mbufdummy
#define S_MBUFLEN	sc->sc_fifo.chan[chan].buffer.mbuf->m_len
#define S_MBUFPKTHDR	sc->sc_fifo.chan[chan].buffer.mbuf->m_pkthdr
#define S_MBUFDATA	sc->sc_fifo.chan[chan].buffer.mbuf->m_data
#define S_MBUFDAT	sc->sc_fifo.chan[chan].buffer.mbuf->m_dat

#define S_IFQUEUE	sc->sc_fifo.chan[chan].buffer.ifqueue

/* hfc control */

#define HFC_INIT	ihfc_init
#define HFC_INTR	((S_HFC & HFC_1) ? ihfc_intr1 : ihfc_intr2)
#define HFC_FSM		ihfc_fsm
#define HFC_CONTROL	ihfc_control

/* softc parts */

struct ihfc_sc;

struct sc_resources {
	struct resource * io_base[IHFC_IO_BASES];
	int	      	  io_rid [IHFC_IO_BASES];
	struct resource * irq;
	int	     	  irq_rid;
};

struct hdlc {
	u_char  flag;
	u_short blevel;
	u_short crc;
	u_short ib;
	u_int   tmp;
};

struct buffer {
	struct ifqueue	ifqueue;	/* data queue	*/
	struct mbuf    	*mbuf;		/* current mbuf	*/
	struct mbuf	*mbufdummy;	/* temporary	*/
};

struct chan {
	struct hdlc	hdlc;
	u_int		bytes;
	u_int		prot;
	struct buffer	buffer;
	void (*filter)(struct ihfc_sc *sc, u_char chan);
};

struct sc_fifo {
	struct	chan chan[6];
	u_short	dztable[16];
	u_char	last_chan;
};

struct sc_config {
	/* software only: */

	u_short	chiptype;	/* chiptype (eg. HFC_1)	*/
	u_char  dlp;		/* D-priority		*/
	u_short	iio;		/* internal IO		*/
	u_char	iirq;		/* internal IRQ		*/
	u_char  ntmode;		/* mode 		*/
	u_char	stdel;		/* S/T delay		*/

	/* write only: */
	u_char cirm;
	u_char ctmt;
	u_char int_m1;
	u_char int_m2;
	u_char mst_mode;
	u_char clkdel;
	u_char sctrl;
	u_char connect;
	u_char test;
	u_char sctrl_r;

	/* isac write only - hfc-1: */
	u_char i_adf2;
	u_char i_spcr;
	u_char i_sqxr;
	u_char i_adf1;
	u_char i_stcr;
	u_char i_mode;
	u_char i_mask;
	u_char i_star2;

	/* read only: */
	u_char int_s1;

	/* isac read only - hfc-1: */
	u_char i_ista;
};

struct sc_statemachine {
	u_char			state;		/* see i4b_ihfc_drv.h */
	u_char			usync;
	u_char			T3;		/* T3 running 	      */
	struct callout_handle	T3callout;
};

/*---------------------------------------------------------------------------*
 *	HFC softc
 *---------------------------------------------------------------------------*/
typedef	struct	ihfc_sc
{	int			sc_unit;
	int			sc_flag;

	int			sc_i4bunit;	/* L0IHFCUNIT(sc_unit)	 */
	int			sc_i4bflag;	/* FLAG_TEL_S0_16_3C ..	 */

	u_char			sc_enabled;	/* daemon running if set */
	u_char			sc_intr_active;	/* interrupt is active	 */

	int			sc_trace;
	u_int			sc_Btracecount;
	u_int			sc_Dtracecount;

	struct sc_config	sc_config;
	struct sc_resources	sc_resources;
	struct sc_statemachine	sc_statemachine;

	isdn_link_t		sc_blinktab[2];
	drvr_link_t		*sc_bdrvlinktab[2];

	struct sc_fifo 		sc_fifo;
} ihfc_sc_t;

extern ihfc_sc_t	ihfc_softc[];

#endif /* _I4B_IHFC_H_ */
