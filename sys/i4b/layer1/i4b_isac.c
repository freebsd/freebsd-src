/*
 * Copyright (c) 1997, 1999 Hellmuth Michaelis. All rights reserved.
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
 *---------------------------------------------------------------------------
 *
 *	i4b_isac.c - i4b siemens isdn chipset driver ISAC handler
 *	---------------------------------------------------------
 *
 *	$Id: i4b_isac.c,v 1.2 1999/12/13 21:25:26 hm Exp $ 
 *
 * $FreeBSD: src/sys/i4b/layer1/i4b_isac.c,v 1.6 1999/12/14 20:48:20 hm Exp $
 *
 *      last edit-date: [Mon Dec 13 22:01:05 1999]
 *
 *---------------------------------------------------------------------------*/

#include "isic.h"

#if NISIC > 0

#include "opt_i4b.h"

#include <sys/param.h>
#include <sys/ioccom.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/socket.h>

#include <machine/stdarg.h>
#include <machine/clock.h>

#include <net/if.h>

#include <machine/i4b_debug.h>
#include <machine/i4b_ioctl.h>
#include <machine/i4b_trace.h>

#include <i4b/layer1/i4b_l1.h>
#include <i4b/layer1/i4b_isac.h>
#include <i4b/layer1/i4b_hscx.h>

#include <i4b/include/i4b_global.h>
#include <i4b/include/i4b_l1l2.h>
#include <i4b/include/i4b_mbuf.h>

static u_char isic_isac_exir_hdlr(register struct l1_softc *sc, u_char exir);
static void isic_isac_ind_hdlr(register struct l1_softc *sc, int ind);

/*---------------------------------------------------------------------------*
 *	ISAC interrupt service routine
 *---------------------------------------------------------------------------*/
void
isic_isac_irq(struct l1_softc *sc, int ista)
{
	register u_char c = 0;
	DBGL1(L1_F_MSG, "isic_isac_irq", ("unit %d: ista = 0x%02x\n", sc->sc_unit, ista));

	if(ista & ISAC_ISTA_EXI)	/* extended interrupt */
	{
		c |= isic_isac_exir_hdlr(sc, ISAC_READ(I_EXIR));
	}
	
	if(ista & ISAC_ISTA_RME)	/* receive message end */
	{
		register int rest;
		u_char rsta;

		/* get rx status register */
		
		rsta = ISAC_READ(I_RSTA);

		if((rsta & ISAC_RSTA_MASK) != 0x20)
		{
			int error = 0;
			
			if(!(rsta & ISAC_RSTA_CRC))	/* CRC error */
			{
				error++;
				DBGL1(L1_I_ERR, "isic_isac_irq", ("unit %d: CRC error\n", sc->sc_unit));
			}
	
			if(rsta & ISAC_RSTA_RDO)	/* ReceiveDataOverflow */
			{
				error++;
				DBGL1(L1_I_ERR, "isic_isac_irq", ("unit %d: Data Overrun error\n", sc->sc_unit));
			}
	
			if(rsta & ISAC_RSTA_RAB)	/* ReceiveABorted */
			{
				error++;
				DBGL1(L1_I_ERR, "isic_isac_irq", ("unit %d: Receive Aborted error\n", sc->sc_unit));
			}

			if(error == 0)			
				DBGL1(L1_I_ERR, "isic_isac_irq", ("unit %d: RME unknown error, RSTA = 0x%02x!\n", sc->sc_unit, rsta));

			i4b_Dfreembuf(sc->sc_ibuf);

			c |= ISAC_CMDR_RMC|ISAC_CMDR_RRES;

			sc->sc_ibuf = NULL;
			sc->sc_ib = NULL;
			sc->sc_ilen = 0;

			ISAC_WRITE(I_CMDR, ISAC_CMDR_RMC|ISAC_CMDR_RRES);
			ISACCMDRWRDELAY();

			return;
		}

		rest = (ISAC_READ(I_RBCL) & (ISAC_FIFO_LEN-1));

		if(rest == 0)
			rest = ISAC_FIFO_LEN;

		if(sc->sc_ibuf == NULL)
		{
			if((sc->sc_ibuf = i4b_Dgetmbuf(rest)) != NULL)
				sc->sc_ib = sc->sc_ibuf->m_data;
			else
				panic("isic_isac_irq: RME, i4b_Dgetmbuf returns NULL!\n");
			sc->sc_ilen = 0;
		}

		if(sc->sc_ilen <= (MAX_DFRAME_LEN - rest))
		{
			ISAC_RDFIFO(sc->sc_ib, rest);
			sc->sc_ilen += rest;
			
			sc->sc_ibuf->m_pkthdr.len =
				sc->sc_ibuf->m_len = sc->sc_ilen;

			if(sc->sc_trace & TRACE_D_RX)
			{
				i4b_trace_hdr_t hdr;
				hdr.unit = sc->sc_unit;
				hdr.type = TRC_CH_D;
				hdr.dir = FROM_NT;
				hdr.count = ++sc->sc_trace_dcount;
				MICROTIME(hdr.time);
				MPH_Trace_Ind(&hdr, sc->sc_ibuf->m_len, sc->sc_ibuf->m_data);
			}

			c |= ISAC_CMDR_RMC;

			if(sc->sc_enabled &&
			   (ctrl_desc[sc->sc_unit].protocol != PROTOCOL_D64S))
			{
				PH_Data_Ind(sc->sc_unit, sc->sc_ibuf);
			}
			else
			{
				i4b_Dfreembuf(sc->sc_ibuf);
			}
		}
		else
		{
			DBGL1(L1_I_ERR, "isic_isac_irq", ("RME, input buffer overflow!\n"));
			i4b_Dfreembuf(sc->sc_ibuf);
			c |= ISAC_CMDR_RMC|ISAC_CMDR_RRES;
		}

		sc->sc_ibuf = NULL;
		sc->sc_ib = NULL;
		sc->sc_ilen = 0;
	}

	if(ista & ISAC_ISTA_RPF)	/* receive fifo full */
	{
		if(sc->sc_ibuf == NULL)
		{
			if((sc->sc_ibuf = i4b_Dgetmbuf(MAX_DFRAME_LEN)) != NULL)
				sc->sc_ib= sc->sc_ibuf->m_data;
			else
				panic("isic_isac_irq: RPF, i4b_Dgetmbuf returns NULL!\n");
			sc->sc_ilen = 0;
		}

		if(sc->sc_ilen <= (MAX_DFRAME_LEN - ISAC_FIFO_LEN))
		{
			ISAC_RDFIFO(sc->sc_ib, ISAC_FIFO_LEN);
			sc->sc_ilen += ISAC_FIFO_LEN;			
			sc->sc_ib += ISAC_FIFO_LEN;
			c |= ISAC_CMDR_RMC;
		}
		else
		{
			DBGL1(L1_I_ERR, "isic_isac_irq", ("RPF, input buffer overflow!\n"));
			i4b_Dfreembuf(sc->sc_ibuf);
			sc->sc_ibuf = NULL;
			sc->sc_ib = NULL;
			sc->sc_ilen = 0;
			c |= ISAC_CMDR_RMC|ISAC_CMDR_RRES;			
		}
	}

	if(ista & ISAC_ISTA_XPR)	/* transmit fifo empty (XPR bit set) */
	{
		if((sc->sc_obuf2 != NULL) && (sc->sc_obuf == NULL))
		{
			sc->sc_freeflag = sc->sc_freeflag2;
			sc->sc_obuf = sc->sc_obuf2;
			sc->sc_op = sc->sc_obuf->m_data;
			sc->sc_ol = sc->sc_obuf->m_len;
			sc->sc_obuf2 = NULL;
#ifdef NOTDEF			
			printf("ob2=%x, op=%x, ol=%d, f=%d #",
				sc->sc_obuf,
				sc->sc_op,
				sc->sc_ol,
				sc->sc_state);
#endif				
		}
		else
		{
#ifdef NOTDEF
			printf("ob=%x, op=%x, ol=%d, f=%d #",
				sc->sc_obuf,
				sc->sc_op,
				sc->sc_ol,
				sc->sc_state);
#endif
		}			
		
		if(sc->sc_obuf)
		{			
			ISAC_WRFIFO(sc->sc_op, min(sc->sc_ol, ISAC_FIFO_LEN));
	
			if(sc->sc_ol > ISAC_FIFO_LEN)	/* length > 32 ? */
			{
				sc->sc_op += ISAC_FIFO_LEN; /* bufferptr+32 */
				sc->sc_ol -= ISAC_FIFO_LEN; /* length - 32 */
				c |= ISAC_CMDR_XTF;	    /* set XTF bit */
			}
			else
			{
				if(sc->sc_freeflag)
				{
					i4b_Dfreembuf(sc->sc_obuf);
					sc->sc_freeflag = 0;
				}
				sc->sc_obuf = NULL;
				sc->sc_op = NULL;
				sc->sc_ol = 0;
	
				c |= ISAC_CMDR_XTF | ISAC_CMDR_XME;
			}
		}
		else
		{
			sc->sc_state &= ~ISAC_TX_ACTIVE;
		}
	}
	
	if(ista & ISAC_ISTA_CISQ)	/* channel status change CISQ */
	{
		register u_char ci;
	
		/* get command/indication rx register*/
	
		ci = ISAC_READ(I_CIRR);

		/* if S/Q IRQ, read SQC reg to clr SQC IRQ */
	
		if(ci & ISAC_CIRR_SQC)
			(void) ISAC_READ(I_SQRR);

		/* C/I code change IRQ (flag already cleared by CIRR read) */
	
		if(ci & ISAC_CIRR_CIC0)
			isic_isac_ind_hdlr(sc, (ci >> 2) & 0xf);
	}
	
	if(c)
	{
		ISAC_WRITE(I_CMDR, c);
		ISACCMDRWRDELAY();
	}
}

/*---------------------------------------------------------------------------*
 *	ISAC L1 Extended IRQ handler
 *---------------------------------------------------------------------------*/
static u_char
isic_isac_exir_hdlr(register struct l1_softc *sc, u_char exir)
{
	u_char c = 0;
	
	if(exir & ISAC_EXIR_XMR)
	{
		DBGL1(L1_I_ERR, "isic_isac_exir_hdlr", ("EXIRQ Tx Message Repeat\n"));

		c |= ISAC_CMDR_XRES;
	}
	
	if(exir & ISAC_EXIR_XDU)
	{
		DBGL1(L1_I_ERR, "isic_isac_exir_hdlr", ("EXIRQ Tx Data Underrun\n"));

		c |= ISAC_CMDR_XRES;
	}

	if(exir & ISAC_EXIR_PCE)
	{
		DBGL1(L1_I_ERR, "isic_isac_exir_hdlr", ("EXIRQ Protocol Error\n"));
	}

	if(exir & ISAC_EXIR_RFO)
	{
		DBGL1(L1_I_ERR, "isic_isac_exir_hdlr", ("EXIRQ Rx Frame Overflow\n"));

		c |= ISAC_CMDR_RMC|ISAC_CMDR_RRES;
	}

	if(exir & ISAC_EXIR_SOV)
	{
		DBGL1(L1_I_ERR, "isic_isac_exir_hdlr", ("EXIRQ Sync Xfer Overflow\n"));
	}

	if(exir & ISAC_EXIR_MOS)
	{
		DBGL1(L1_I_ERR, "L1 isic_isac_exir_hdlr", ("EXIRQ Monitor Status\n"));
	}

	if(exir & ISAC_EXIR_SAW)
	{
		/* cannot happen, STCR:TSF is set to 0 */
		
		DBGL1(L1_I_ERR, "isic_isac_exir_hdlr", ("EXIRQ Subscriber Awake\n"));
	}

	if(exir & ISAC_EXIR_WOV)
	{
		/* cannot happen, STCR:TSF is set to 0 */

		DBGL1(L1_I_ERR, "isic_isac_exir_hdlr", ("EXIRQ Watchdog Timer Overflow\n"));
	}

	return(c);
}

/*---------------------------------------------------------------------------*
 *	ISAC L1 Indication handler
 *---------------------------------------------------------------------------*/
static void
isic_isac_ind_hdlr(register struct l1_softc *sc, int ind)
{
	register int event;
	
	switch(ind)
	{
		case ISAC_CIRR_IAI8:
			DBGL1(L1_I_CICO, "isic_isac_ind_hdlr", ("rx AI8 in state %s\n", isic_printstate(sc)));
			if(sc->sc_bustyp == BUS_TYPE_IOM2)
				isic_isac_l1_cmd(sc, CMD_AR8);
			event = EV_INFO48;
			MPH_Status_Ind(sc->sc_unit, STI_L1STAT, LAYER_ACTIVE);
			break;
			
		case ISAC_CIRR_IAI10:
			DBGL1(L1_I_CICO, "isic_isac_ind_hdlr", ("rx AI10 in state %s\n", isic_printstate(sc)));
			if(sc->sc_bustyp == BUS_TYPE_IOM2)
				isic_isac_l1_cmd(sc, CMD_AR10);
			event = EV_INFO410;
			MPH_Status_Ind(sc->sc_unit, STI_L1STAT, LAYER_ACTIVE);
			break;

		case ISAC_CIRR_IRSY:
			DBGL1(L1_I_CICO, "isic_isac_ind_hdlr", ("rx RSY in state %s\n", isic_printstate(sc)));
			event = EV_RSY;
			break;

		case ISAC_CIRR_IPU:
			DBGL1(L1_I_CICO, "isic_isac_ind_hdlr", ("rx PU in state %s\n", isic_printstate(sc)));
			event = EV_PU;
			break;

		case ISAC_CIRR_IDR:
			DBGL1(L1_I_CICO, "isic_isac_ind_hdlr", ("rx DR in state %s\n", isic_printstate(sc)));
			isic_isac_l1_cmd(sc, CMD_DIU);
			event = EV_DR;			
			break;
			
		case ISAC_CIRR_IDID:
			DBGL1(L1_I_CICO, "isic_isac_ind_hdlr", ("rx DID in state %s\n", isic_printstate(sc)));
			event = EV_INFO0;
			MPH_Status_Ind(sc->sc_unit, STI_L1STAT, LAYER_IDLE);
			break;

		case ISAC_CIRR_IDIS:
			DBGL1(L1_I_CICO, "isic_isac_ind_hdlr", ("rx DIS in state %s\n", isic_printstate(sc)));
			event = EV_DIS;
			break;

		case ISAC_CIRR_IEI:
			DBGL1(L1_I_CICO, "isic_isac_ind_hdlr", ("rx EI in state %s\n", isic_printstate(sc)));
			isic_isac_l1_cmd(sc, CMD_DIU);
			event = EV_EI;
			break;

		case ISAC_CIRR_IARD:
			DBGL1(L1_I_CICO, "isic_isac_ind_hdlr", ("rx ARD in state %s\n", isic_printstate(sc)));
			event = EV_INFO2;
			break;

		case ISAC_CIRR_ITI:
			DBGL1(L1_I_CICO, "isic_isac_ind_hdlr", ("rx TI in state %s\n", isic_printstate(sc)));
			event = EV_INFO0;
			break;

		case ISAC_CIRR_IATI:
			DBGL1(L1_I_CICO, "isic_isac_ind_hdlr", ("rx ATI in state %s\n", isic_printstate(sc)));
			event = EV_INFO0;
			break;

		case ISAC_CIRR_ISD:
			DBGL1(L1_I_CICO, "isic_isac_ind_hdlr", ("rx SD in state %s\n", isic_printstate(sc)));
			event = EV_INFO0;
			break;
		
		default:
			DBGL1(L1_I_ERR, "isic_isac_ind_hdlr", ("UNKNOWN Indication 0x%x in state %s\n", ind, isic_printstate(sc)));
			event = EV_INFO0;
			break;
	}
	isic_next_state(sc, event);
}

/*---------------------------------------------------------------------------*
 *	execute a layer 1 command
 *---------------------------------------------------------------------------*/	
void
isic_isac_l1_cmd(struct l1_softc *sc, int command)
{
	u_char cmd;

#ifdef I4B_SMP_WORKAROUND

	/* XXXXXXXXXXXXXXXXXXX */
	
	/*
	 * patch from Wolfgang Helbig:
	 *
	 * Here is a patch that makes i4b work on an SMP:
	 * The card (TELES 16.3) didn't interrupt on an SMP machine.
	 * This is a gross workaround, but anyway it works *and* provides
	 * some information as how to finally fix this problem.
	 */
	
	HSCX_WRITE(0, H_MASK, 0xff);
	HSCX_WRITE(1, H_MASK, 0xff);
	ISAC_WRITE(I_MASK, 0xff);
	DELAY(100);
	HSCX_WRITE(0, H_MASK, HSCX_A_IMASK);
	HSCX_WRITE(1, H_MASK, HSCX_B_IMASK);
	ISAC_WRITE(I_MASK, ISAC_IMASK);

	/* XXXXXXXXXXXXXXXXXXX */
	
#endif /* I4B_SMP_WORKAROUND */

	if(command < 0 || command > CMD_ILL)
	{
		DBGL1(L1_I_ERR, "isic_isac_l1_cmd", ("illegal cmd 0x%x in state %s\n", command, isic_printstate(sc)));
		return;
	}
                                           
	if(sc->sc_bustyp == BUS_TYPE_IOM2)
		cmd = ISAC_CIX0_LOW;
	else
		cmd = 0;

	switch(command)
	{
		case CMD_TIM:
			DBGL1(L1_I_CICO, "isic_isac_l1_cmd", ("tx TIM in state %s\n", isic_printstate(sc)));
			cmd |= (ISAC_CIXR_CTIM << 2);
			break;

		case CMD_RS:
			DBGL1(L1_I_CICO, "isic_isac_l1_cmd", ("tx RS in state %s\n", isic_printstate(sc)));
			cmd |= (ISAC_CIXR_CRS << 2);
			break;

		case CMD_AR8:
			DBGL1(L1_I_CICO, "isic_isac_l1_cmd", ("tx AR8 in state %s\n", isic_printstate(sc)));
			cmd |= (ISAC_CIXR_CAR8 << 2);
			break;

		case CMD_AR10:
			DBGL1(L1_I_CICO, "isic_isac_l1_cmd", ("tx AR10 in state %s\n", isic_printstate(sc)));
			cmd |= (ISAC_CIXR_CAR10 << 2);
			break;

		case CMD_DIU:
			DBGL1(L1_I_CICO, "isic_isac_l1_cmd", ("tx DIU in state %s\n", isic_printstate(sc)));
			cmd |= (ISAC_CIXR_CDIU << 2);
			break;
	}
	ISAC_WRITE(I_CIXR, cmd);
}

/*---------------------------------------------------------------------------*
 *	L1 ISAC initialization
 *---------------------------------------------------------------------------*/
int
isic_isac_init(struct l1_softc *sc)
{
	ISAC_IMASK = 0xff;		/* disable all irqs */

	ISAC_WRITE(I_MASK, ISAC_IMASK);

	if(sc->sc_bustyp != BUS_TYPE_IOM2)
	{
		DBGL1(L1_I_SETUP, "isic_isac_setup", ("configuring for IOM-1 mode\n"));

		/* ADF2: Select mode IOM-1 */		
		ISAC_WRITE(I_ADF2, 0x00);

		/* SPCR: serial port control register:
		 *	SPU - software power up = 0
		 *	SAC - SIP port high Z
		 *	SPM - timing mode 0
		 *	TLP - test loop = 0
		 *	C1C, C2C - B1 and B2 switched to/from SPa
		 */
		ISAC_WRITE(I_SPCR, ISAC_SPCR_C1C1|ISAC_SPCR_C2C1);

		/* SQXR: S/Q channel xmit register:
                 *	SQIE - S/Q IRQ enable = 0
		 *	SQX1-4 - Fa bits = 1
		 */
		ISAC_WRITE(I_SQXR, ISAC_SQXR_SQX1|ISAC_SQXR_SQX2|ISAC_SQXR_SQX3|ISAC_SQXR_SQX4);

		/* ADF1: additional feature reg 1:
		 *	WTC - watchdog = 0
		 *	TEM - test mode = 0
		 *	PFS - pre-filter = 0
		 *	CFS - IOM clock/frame always active
		 *	FSC1/2 - polarity of 8kHz strobe
		 *	ITF - interframe fill = idle
		 */	
		ISAC_WRITE(I_ADF1, ISAC_ADF1_FC2);	/* ADF1 */

		/* STCR: sync transfer control reg:
		 *	TSF - terminal secific functions = 0
		 *	TBA - TIC bus address = 7
		 *	STx/SCx = 0
		 */
		ISAC_WRITE(I_STCR, ISAC_STCR_TBA2|ISAC_STCR_TBA1|ISAC_STCR_TBA0);

		/* MODE: Mode Register:
		 *	MDSx - transparent mode 2
		 *	TMD  - timer mode = external
		 *	RAC  - Receiver enabled
		 *	DIMx - digital i/f mode
		 */
		ISAC_WRITE(I_MODE, ISAC_MODE_MDS2|ISAC_MODE_MDS1|ISAC_MODE_RAC|ISAC_MODE_DIM0);
	}
	else
	{
		DBGL1(L1_I_SETUP, "isic_isac_setup", ("configuring for IOM-2 mode\n"));

		/* ADF2: Select mode IOM-2 */		
		ISAC_WRITE(I_ADF2, ISAC_ADF2_IMS);

		/* SPCR: serial port control register:
		 *	SPU - software power up = 0
		 *	SPM - timing mode 0
		 *	TLP - test loop = 0
		 *	C1C, C2C - B1 + C1 and B2 + IC2 monitoring
		 */
		ISAC_WRITE(I_SPCR, 0x00);

		/* SQXR: S/Q channel xmit register:
		 *	IDC  - IOM direction = 0 (master)
		 *	CFS  - Config Select = 0 (clock always active)
		 *	CI1E - C/I channel 1 IRQ enable = 0
                 *	SQIE - S/Q IRQ enable = 0
		 *	SQX1-4 - Fa bits = 1
		 */
		ISAC_WRITE(I_SQXR, ISAC_SQXR_SQX1|ISAC_SQXR_SQX2|ISAC_SQXR_SQX3|ISAC_SQXR_SQX4);

		/* ADF1: additional feature reg 1:
		 *	WTC - watchdog = 0
		 *	TEM - test mode = 0
		 *	PFS - pre-filter = 0
		 *	IOF - IOM i/f off = 0
		 *	ITF - interframe fill = idle
		 */	
		ISAC_WRITE(I_ADF1, 0x00);

		/* STCR: sync transfer control reg:
		 *	TSF - terminal secific functions = 0
		 *	TBA - TIC bus address = 7
		 *	STx/SCx = 0
		 */
		ISAC_WRITE(I_STCR, ISAC_STCR_TBA2|ISAC_STCR_TBA1|ISAC_STCR_TBA0);

		/* MODE: Mode Register:
		 *	MDSx - transparent mode 2
		 *	TMD  - timer mode = external
		 *	RAC  - Receiver enabled
		 *	DIMx - digital i/f mode
		 */
		ISAC_WRITE(I_MODE, ISAC_MODE_MDS2|ISAC_MODE_MDS1|ISAC_MODE_RAC|ISAC_MODE_DIM0);
	}

#ifdef NOTDEF
	/*
	 * XXX a transmitter reset causes an ISAC tx IRQ which will not
	 * be serviced at attach time under some circumstances leaving
	 * the associated IRQ line on the ISA bus active. This prevents
	 * any further interrupts to be serviced because no low -> high
	 * transition can take place anymore. (-hm)
	 */
	 
	/* command register:
	 *	RRES - HDLC receiver reset
	 *	XRES - transmitter reset
	 */
	ISAC_WRITE(I_CMDR, ISAC_CMDR_RRES|ISAC_CMDR_XRES);
	ISACCMDRWRDELAY();
#endif
	
	/* enabled interrupts:
	 * ===================
	 * RME  - receive message end
	 * RPF  - receive pool full
	 * XPR  - transmit pool ready
	 * CISQ - CI or S/Q channel change
	 * EXI  - extended interrupt
	 */

	ISAC_IMASK = ISAC_MASK_RSC |	/* auto mode only	*/
		     ISAC_MASK_TIN | 	/* timer irq		*/
		     ISAC_MASK_SIN;	/* sync xfer irq	*/

	ISAC_WRITE(I_MASK, ISAC_IMASK);

	return(0);
}

#endif /* NISIC > 0 */
