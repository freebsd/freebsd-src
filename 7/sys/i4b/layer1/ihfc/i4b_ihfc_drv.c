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
 *	i4b_ihfc_drv.c - ihfc ISA PnP-bus interface
 *	-------------------------------------------
 *	Everything which has got anything to do with the
 *	HFC-1/S/SP chips has been put here.
 *
 *	last edit-date: [Fri Jan 12 17:06:52 2001]
 *
 *---------------------------------------------------------------------------*/

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/socket.h>
#include <net/if.h>

#include <sys/mbuf.h>

#include <i4b/include/i4b_mbuf.h>

#include <i4b/include/i4b_debug.h>
#include <i4b/include/i4b_ioctl.h>
#include <i4b/include/i4b_trace.h>

#include <i4b/layer1/i4b_l1.h>
#include <i4b/layer1/i4b_hdlc.h>
#include <i4b/layer1/ihfc/i4b_ihfc.h>
#include <i4b/layer1/ihfc/i4b_ihfc_ext.h>
#include <i4b/layer1/ihfc/i4b_ihfc_drv.h>

#include <machine/bus.h>
#include <sys/rman.h>


/*---------------------------------------------------------------------------*
 *	Local prototypes
 *---------------------------------------------------------------------------*/
	void 	ihfc_loadconfig   (ihfc_sc_t *sc);

static  void	ihfc_trans_Bread  (ihfc_sc_t *sc, u_char chan);
static  void	ihfc_trans_Bwrite (ihfc_sc_t *sc, u_char chan);
static 	void	ihfc_hdlc_Bread   (ihfc_sc_t *sc, u_char chan);
static	void	ihfc_hdlc_Bwrite  (ihfc_sc_t *sc, u_char chan);
static  void	ihfc_hdlc_Dread   (ihfc_sc_t *sc, u_char chan);
static  void	ihfc_hdlc_Dwrite  (ihfc_sc_t *sc, u_char chan);

static  void	ihfc_isac_Dread   (ihfc_sc_t *sc, u_char chan);
static  void	ihfc_isac_Dwrite  (ihfc_sc_t *sc, u_char chan);

	void	ihfc_cmdr_hdlr 	  (ihfc_sc_t *sc, u_char cmdr);
	void	ihfc_exir_hdlr 	  (ihfc_sc_t *sc, u_char exir);

	void	ihfc_sq		  (ihfc_sc_t *sc);

static	void	ihfc_test_Bread	  (ihfc_sc_t *sc, u_char chan);
static 	void	ihfc_test_Bwrite  (ihfc_sc_t *sc, u_char chan);

u_short		ihfc_Bsel_fifo	  (ihfc_sc_t *sc, u_char chan, u_char flag);
u_int32_t	ihfc_Dsel_fifo	  (ihfc_sc_t *sc, u_char chan, u_char flag);


/*---------------------------------------------------------------------------*
 *	Commonly used ISA bus commands
 *---------------------------------------------------------------------------*/
#define IHFC_DATA_OFFSET	0
#define IHFC_REG_OFFSET		1

#define BUS_VAR			bus_space_handle_t h = rman_get_bushandle(S_IOBASE[0]); \
				bus_space_tag_t    t = rman_get_bustag   (S_IOBASE[0])

#define SET_REG(reg)		bus_space_write_1(t,h, IHFC_REG_OFFSET, reg)
#define GET_STAT		bus_space_read_1 (t,h, IHFC_REG_OFFSET)

#define READ_DATA_1		bus_space_read_1 (t,h, IHFC_DATA_OFFSET)
#define READ_BOTH_2		bus_space_read_2 (t,h, IHFC_DATA_OFFSET)

#define	WRITE_DATA_1(data)	bus_space_write_1(t,h, IHFC_DATA_OFFSET, data)
#define WRITE_BOTH_2(data)	bus_space_write_2(t,h, IHFC_DATA_OFFSET, data)

#define DISBUSY(okcmd, tocmd)						\
{									\
	if (GET_STAT & 1)						\
	{								\
		register u_char a;					\
		register u_int to = IHFC_DISBUSYTO;			\
									\
		while(((a = GET_STAT) & 1) && --to);			\
									\
		if (!to)						\
		{							\
			NDBGL1(L1_ERROR, "DISBUSY-TIMEOUT! (a=%04x, "	\
			"unit=%d)", a, S_UNIT);				\
			tocmd;						\
		}							\
		else							\
		{							\
			okcmd;						\
		}							\
	}								\
	else								\
	{								\
		okcmd;							\
	}								\
}

#define WAITBUSY_2(okcmd, tocmd)					\
	{								\
		register u_short a;					\
		register u_int to = IHFC_NONBUSYTO;			\
									\
		while((~(a = READ_BOTH_2) & 0x100) && --to);		\
									\
		if (!to)						\
		{							\
			NDBGL1(L1_ERROR, "NONBUSY-TIMEOUT! (a=%04x, "	\
			"unit=%d)", a, S_UNIT);				\
			tocmd;						\
		}							\
		else							\
		{							\
			okcmd;						\
		}							\
	}

/*---------------------------------------------------------------------------*
 *	Control function					(HFC-1/S/SP)
 *	
 *	Flag:
 *		1: reset and unlock chip (at boot only)
 *		2: prepare for shutdown  (at shutdown only)
 *		3: reset and resume
 *		4: select TE-mode        (boot default)
 *		5: select NT-mode        (only HFC-S/SP/PCI)
 *
 *	Returns != 0 on errornous chip
 *---------------------------------------------------------------------------*/
int
ihfc_control(ihfc_sc_t *sc, int flag)
{
	BUS_VAR;

	if (flag == 3) goto reset0;
	if (flag == 4)
	{
		S_NTMODE = 0;
		goto mode0;
	}
	if (flag == 5)
	{
		S_NTMODE = 1;
		goto mode0;
	}
	if (flag == 1)
	{
		WRITE_BOTH_2(0x5400 | S_IIO);	/* enable IO (HFC-1/S) */

		S_LAST_CHAN = -1;

		/* HFC-S/SP configuration */

		S_CIRM     = S_IIRQ|0x10; /* IRQ, 8K fifo mode		*/
		S_CLKDEL   = 0x00;	  /* 12.288mhz			*/
		S_CTMT     = 0x03;	  /* transperant mode		*/
		S_CONNECT  = 0x00;	  /* B channel data flow	*/
		S_INT_M1   = 0x40;	  /* interrupt mask		*/
		S_INT_M2   = 0x08;	  /* enable interrupt output	*/
		S_MST_MODE = 0x01; 	  /* master mode		*/
		S_SCTRL    = 0x50;	  /* S/Q on, non cap. line mode */
		S_SCTRL_R  = 0x00;	  /* B channel receive disable	*/
		S_TEST     = 0x00;	  /* no need for awake enable	*/

		if (S_HFC & (HFC_1 | HFC_S))	/* configure timer (50ms) */
		{
			S_CTMT   |= 0x08;
		}
		else
		{
			S_CTMT   |= 0x14;
		}

		/* HFC-1 ISAC configuration (IOM-2 mode) */

		S_ADF1	   = 0x00;	/* */
		S_ADF2	   = 0x80;	/* select mode IOM-2		*/
		S_SPCR	   = 0x00;	/* B channel send disable (0x10 for test loop) */
		S_MASK	   = 0xfb;	/* enable CISQ			*/
		S_MODE	   = 0xc9;	/* receiver enabled		*/
		S_SQXR	   = 0x0f;	/* master, clock always active	*/
		S_STCR	   = 0x70;	/* TIC bus address = 7		*/
		S_STAR2	   = 0x04;	/* enable S/Q			*/

	  mode0:
		if (S_NTMODE)		/* configure NT- or TE-mode */
		{
			S_SCTRL  |=  0x04; 	/* NT mode	*/
			S_CLKDEL &= ~0x7f;	/* clear delay 	*/
			S_CLKDEL |=  0x6c; 	/* set delay	*/
		}
		else
		{
			S_SCTRL  &= ~0x04;	/* TE mode	*/
			S_STDEL  &=  0x7f;	/* use mask!	*/
			S_CLKDEL &= ~0x7f;	/* clear delay	*/
			S_CLKDEL |= S_STDEL;	/* set delay 	*/
		}
		if (S_DLP)		/* configure D-priority */
		{
			S_SCTRL  |= 0x08;	/* (10/11) */
		}
		else
		{
			S_SCTRL  &= ~0x08;	/* (8/9) */
		}

	  reset0:
		/* chip reset (HFC-1/S/SP) */

		if (S_HFC & HFC_1)
		{
			SET_REG((S_CIRM | 0xc8) & 0xdf);

			DELAY(10);	/* HFC-2B manual recommends a 4	*
					 * clock cycle delay after CIRM	*
					 * write with reset=1. A 1us	*
					 * delay, should do for 7.68mhz,*
					 * but just in case I make that	*
					 * 10us.			*/

			SET_REG((S_CIRM | 0xc0) & 0xdf);

			DELAY(250);	/* ISAC manual says reset pulse	*
					 * length is 125us. Accessing	*
					 * ISAC before those 125us, we	*
					 * may risk chip corruption and *
					 * irq failure. The HFC-2B also *
					 * needs some delay to recover, *
					 * so we add some us.		*/
		}
		else
		{
			SET_REG(0x18);

			WRITE_DATA_1(S_CIRM | 8);

			DELAY(10);	/* HFC-2BDS0 manual recommends 	*
					 * a 4 clock cycle delay after	*
					 * CIRM write with reset=1.	*
					 * A 1us delay, should do for	*
					 * 12.288mhz, but just in case	*
					 * I make that 10us.		*/

			WRITE_DATA_1(S_CIRM);

			DELAY(25);	/* HFC-2BDS0 needs some time to	*
					 * recover after CIRM write 	*
					 * with reset=0. Experiments 	*
					 * show this delay should be	*
					 * 8-9us. Just in case we make	*
					 * that 25us.			*/
		}

		{
			/* HFC-1/S/SP chip test				*
			 * 						*
			 * NOTE: after reset the HFC-1/S/SP should be	*
			 * in a mode where it is always non-busy/non-	*
			 * processing, and bit[0] of STATUS/DISBUSY 	*
			 * register, should always return binary '0'	*
			 * until we configure the chips for normal 	*
			 * operation.					*/
#ifdef IHFC_DEBUG
			printf("ihfc: GET_STAT value is: 0x%x\n", GET_STAT);
#endif
			SET_REG(0x30);

			if ((GET_STAT & 1) || (READ_DATA_1 & 0xf)) goto f0;
		}

		ihfc_loadconfig(sc);
		
		if (S_HFC & HFC_1) ihfc_cmdr_hdlr(sc, 0x41);	/* rres, xres */

		S_PHSTATE = 0;
		HFC_FSM(sc, 0);
	}

	if (flag == 2)
	{
		if (S_HFC & HFC_1) S_CIRM &= ~0x03;	/* disable interrupt */

		S_SQXR	   |=  0x40;	/* power down */

		S_INT_M2   &= ~0x01;
		S_MASK	   |=  0x02;

		S_SPCR	   &= ~0x0f;	/* send 1's only */
		S_SCTRL	   &= ~0x83;	/* send 1's only + enable oscillator */

		ihfc_loadconfig(sc);
	}

	return(0);	/* success */

  f0:
	return(1);	/* failure */
}

/*---------------------------------------------------------------------------*
 *	Softc initializer and hardware setup			(HFC-1/S/SP)
 *
 *	Returns: 0 on success
 *		 1 on failure
 *---------------------------------------------------------------------------*/
int
ihfc_init (ihfc_sc_t *sc, u_char chan, int prot, int activate)
{
	if (chan > 5) goto f0;

	chan &= ~1;

	do
	{	if (chan < 2)			/* D-Channel */
		{
			i4b_Dfreembuf(S_MBUF);
			if (!IF_QEMPTY(&S_IFQUEUE)) i4b_Dcleanifq(&S_IFQUEUE);

			RESET_SOFT_CHAN(sc, chan);

			S_IFQUEUE.ifq_maxlen = IFQ_MAXLEN;

#if defined (__FreeBSD__) && __FreeBSD__ > 4
			if(!mtx_initialized(&S_IFQUEUE.ifq_mtx))
				mtx_init(&S_IFQUEUE.ifq_mtx, "i4b_ihfc", NULL, MTX_DEF);
#endif
			if (!activate) continue;

			if (S_HFC & HFC_1)
			{
				S_FILTER  = (chan & 1) ? ihfc_isac_Dread :
							 ihfc_isac_Dwrite;
			}
			else
			{
				S_FILTER  = (chan & 1) ? ihfc_hdlc_Dread :
							 ihfc_hdlc_Dwrite;
			}
		}
		else				/* B-Channel */
		{
			i4b_Bfreembuf(S_MBUF);
			if (!IF_QEMPTY(&S_IFQUEUE))  i4b_Bcleanifq(&S_IFQUEUE);

			RESET_SOFT_CHAN(sc, chan);

			S_IFQUEUE.ifq_maxlen = IFQ_MAXLEN;

#if defined (__FreeBSD__) && __FreeBSD__ > 4
			if(!mtx_initialized(&S_IFQUEUE.ifq_mtx))
				mtx_init(&S_IFQUEUE.ifq_mtx, "i4b_ihfc", NULL, MTX_DEF);
#endif
			S_PROT = prot;

			if (!activate) continue;

			switch(prot)
			{	case(BPROT_NONE):
					S_FILTER = (chan & 1) ? 
						ihfc_trans_Bread : 
						ihfc_trans_Bwrite;
					break;
				case(BPROT_RHDLC):
					S_FILTER = (chan & 1) ? 
						ihfc_hdlc_Bread : 
						ihfc_hdlc_Bwrite;
					break;
				case(5):
					S_FILTER = (chan & 1) ? 
						ihfc_test_Bread : 
						ihfc_test_Bwrite;
					break;
			}
		}
	} while (++chan & 1);

	S_MASK	  |=  0xfb;	/* disable all, but CISQ interrupt (ISAC)	*/
	S_INT_M1  &=  0x40;	/* disable all, but TE/NT state machine (HFC)	*/
	S_SCTRL   &= ~0x03;	/* B1/B2 send disable (HFC)			*/
	S_SPCR	  &= ~0x0f;	/* B1/B2 send disable (ISAC)			*/
	S_SCTRL_R &= ~0x03;	/* B1/B2 receive disable (HFC)			*/

	chan = 0;
	if (S_FILTER)			/* D-Channel active		*/
	{
		S_MASK   &= 0x2e;	/* enable RME, RPF, XPR, EXI	*/
		S_INT_M1 |= 0x24;	/* enable D-receive, D-transmit */
	}

	chan = 2;
	if (S_FILTER)			/* B1-Channel active		*/
	{
		S_SCTRL   |= 1;		/* send enable (HFC)		*/
		S_SPCR	  |= 8;		/* send enable (ISAC)		*/
		S_SCTRL_R |= 1;		/* receive enable (HFC)		*/
		S_INT_M1  |=  0x80;	/* timer enable (HFC)		*/
		S_INT_M1  &= ~0x04;	/* let D-channel use timer too	*/
	}

	chan = 4;
	if (S_FILTER)			/* B2-Channel active		*/
	{
		S_SCTRL   |= 2;		/* send enable (HFC)		*/
		S_SPCR	  |= 2;		/* send enable (ISAC)		*/
		S_SCTRL_R |= 2;		/* receive enable (HFC)		*/
		S_INT_M1  |=  0x80;	/* timer enable (HFC)		*/
		S_INT_M1  &= ~0x04;	/* let D-channel use timer too	*/
	}

	ihfc_loadconfig(sc);

	/* XXX  reset timer? */

	return 0;	/* success */
  f0:
	return 1;	/* failure */
}

/*---------------------------------------------------------------------------*
 *	Load configuration data					(HFC-1/S/SP)
 *---------------------------------------------------------------------------*/
void 
ihfc_loadconfig(ihfc_sc_t *sc)
{
	BUS_VAR;

	if (S_HFC & HFC_1)
	{
		/* HFC-1 chips w/ISAC: */

		const u_char *src = (void *)&S_ISAC_CONFIG;
		const u_char *dst = (void *)&isac_configtable;

		SET_REG((S_CIRM | 0xc0) & 0xdf);

		S_CTMT = (S_CTMT & ~0x14) | ((S_INT_M1 >> 5) & 0x4);

		SET_REG((S_CTMT | 0xe0) & 0xff);

		while(*dst)
		{
			SET_REG(*dst++);	/* set register	*/

			/* write configuration */	
			DISBUSY(WRITE_DATA_1(*src++), break);
		}
	}
	else
	{
		/* HFC-S/SP chips: */

		const u_char *src = (void *)&S_HFC_CONFIG;
		const u_char *dst = (void *)&ihfc_configtable;

		while(*dst)
		{
			SET_REG(*dst++);	/* set register		*/
			WRITE_DATA_1(*src++);	/* write configuration	*/
		}
	}
}

/*---------------------------------------------------------------------------*
 *	Function State Machine handler (PH layer)		(HFC-1/S/SP)
 *
 *	Flag: 0 = Refresh softc S_PHSTATE + take hints
 *	      1 = Activate
 *	      2 = Deactivate
 *
 *	NOTE: HFC-1 only supports TE mode.
 *---------------------------------------------------------------------------*/
void
ihfc_fsm(ihfc_sc_t *sc, int flag)
{
	const struct ihfc_FSMtable *fsmtab;
	u_char ihfc_cmd = 0;
	u_char isac_cmd = 0;
	u_char tmp;
	BUS_VAR;

	/* get current state (rx/downstream) */

	if (S_HFC & HFC_1)
	{
		SET_REG(0x31); DISBUSY(tmp = (READ_DATA_1 >> 2) & 0xf, return);

		fsmtab = (S_NTMODE) ? &ihfc_TEtable2[tmp]:
				      &ihfc_TEtable2[tmp];
	}
	else
	{
		SET_REG(0x30); tmp = READ_DATA_1 & 0xf;

		fsmtab = (S_NTMODE) ? &ihfc_NTtable[tmp]:
				      &ihfc_TEtable[tmp];
	}

	if (fsmtab->string)
	{
		NDBGL1(L1_I_CICO, "%s (ind=0x%x, flag=%d, unit=%d).",
			fsmtab->string, tmp, flag, S_UNIT);
	}
	else
	{
		NDBGL1(L1_I_ERR, "Illegal indicatation (ind=0x%x, "
			"flag=%d, unit=%d).", tmp, flag, S_UNIT);
	}

	/* indication machine / state change					*
	 *									*
	 * Whenever the state of the S0-line changes, we check to see in which	*
	 * direction the change went. Generally upwards means activate, and	*
	 * downwards means deactivate.						*
	 * The test signal is used to ensure proper syncronization.		*/

	if (fsmtab->state == 0)	/* deactivated indication */
	{
		if (S_PHSTATE != 0)
		{
			isac_cmd = 0x3c; /* deactivate DUI */

			i4b_l1_ph_deactivate_ind(S_I4BUNIT);
		}
	}
	if (fsmtab->state == 2)	/* syncronized indication */
	{
		if (S_PHSTATE != 2)
		{
			if (S_NTMODE) ihfc_cmd = 0x80;
		}
	}
	if (fsmtab->state == 3)	/* activated indication */
	{
		if (S_PHSTATE != 3)
		{
			isac_cmd = (S_DLP) ? 0x24  /* activate AR10 */
					   : 0x20; /* activate AR8  */

			i4b_l1_ph_activate_ind(S_I4BUNIT);
		}
	}
	if (fsmtab->state == 4)	/* error indication */
	{
		if (S_PHSTATE < 4)
		{
			isac_cmd = 0x3c; /* deactivate DUI */
		}
	}

	S_PHSTATE = fsmtab->state;

	if ((flag == 1) && (fsmtab->state != 3))
	{
		isac_cmd = (S_DLP) ? 0x24 : 0x20;
		ihfc_cmd = 0x60;
	}
	if ((flag == 2) && (fsmtab->state != 0))
	{
		isac_cmd = 0x3c;
		ihfc_cmd = 0x40;
	}

	/* set new state (tx / upstream)				      *
	 *								      *
	 * NOTE: HFC-S/SP and ISAC transmitters are always active when 	      *
	 * activated state is reached. The bytes sent to the S0-bus are all   *
	 * high impedance, so they do not disturb.			      *
	 * The HFC-1 has a separate SIEMENS S0-device.			      */

	if (S_HFC & HFC_1)
	{
		if (isac_cmd)
		{
			if (S_IOM2) isac_cmd |= 3;

			SET_REG(0x31); DISBUSY(WRITE_DATA_1(isac_cmd), );

			NDBGL1(L1_I_CICO, "(isac_cmd=0x%x, unit=%d).",
				isac_cmd, S_UNIT);
		}
	}
	else
	{
		if (ihfc_cmd || (fsmtab->state == 5))
		{
			SET_REG(0x30); WRITE_DATA_1(ihfc_cmd);

			NDBGL1(L1_I_CICO, "(ihfc_cmd=0x%x, unit=%d).",
				ihfc_cmd, S_UNIT);
		}
	}
}

/*---------------------------------------------------------------------------*
 *	S/Q - channel handler (read)				(HFC-S/SP)
 *---------------------------------------------------------------------------*/
void
ihfc_sq (ihfc_sc_t *sc)
{
	const struct ihfc_SQtable *SQtab;
	register u_char a = 0;
	BUS_VAR;

	if (S_HFC & HFC_1)
	{
		SET_REG(0x31);
		DISBUSY(a = READ_DATA_1, a = 0);

		if (a & 0x80)
		{
			SET_REG(0x3b);
			DISBUSY(a = READ_DATA_1, a = 0);
		}
	}
	else
	{
		SET_REG(0x34);
		a = READ_DATA_1;
	}

	SQtab = (S_NTMODE) ? &ihfc_Qtable[a & 7]:
			     &ihfc_Stable[a & 7];

	if (a & 0x10)
	{
		if (SQtab->string)
		{
			NDBGL1(L1_I_CICO, "%s (unit=%d, int=%x)",
				SQtab->string, S_UNIT, S_INT_S1);
		}
		else
		{
			NDBGL1(L1_ERROR, "Unknown indication = %x (unit=%d)",
				 a & 7, S_UNIT);
		}
	}
}

/*---------------------------------------------------------------------------*
 *	Interrupt handler					(HFC-1)
 *---------------------------------------------------------------------------*/
void
ihfc_intr1 (ihfc_sc_t *sc)
{
	u_char chan;
	u_char tmp;
	BUS_VAR;
	HFC_VAR;

	HFC_BEG;

	SET_REG(0x20); tmp = GET_STAT; DISBUSY(S_ISTA |= READ_DATA_1, );

	if (S_ISTA & 0x04)	/* CIRQ */
	{
		HFC_FSM(sc, 0);

		ihfc_sq(sc);
	}

	S_INTR_ACTIVE = 1;

	if (S_ISTA & 0xc0)	/* RPF or RME */
	{
		chan = 1;
		if (S_FILTER) S_FILTER(sc, chan);
	}
	if (S_ISTA & 0x10)	/* XPR */
	{
		chan = 0;
		if (S_FILTER) S_FILTER(sc, chan);
	}
	if (tmp & 0x04)		/* Timer elapsed (50ms) */
	{
		SET_REG((S_CTMT | 0xf0) & 0xff);

		chan = 6;
		while(chan--)
		{
			if (chan == 1) break;
			if (S_FILTER) S_FILTER(sc, chan);

			HFC_END;
			DELAY(10);
			HFC_BEG;
		}
	}

	S_INTR_ACTIVE = 0;
		
	if (S_ISTA & 0x01)	/* EXIR */
	{
		SET_REG(0x24); DISBUSY(ihfc_exir_hdlr(sc, READ_DATA_1), );
	}

	S_ISTA &= ~(0x1 | 0x4);

	HFC_END;
}

/*---------------------------------------------------------------------------*
 *	Interrupt handler					(HFC-S/SP)
 *---------------------------------------------------------------------------*/
void
ihfc_intr2 (ihfc_sc_t *sc)
{
	u_char chan;
	BUS_VAR;
	HFC_VAR;

	HFC_BEG;

	SET_REG(0x1e); S_INT_S1 = READ_DATA_1;	/* this will enable new interrupts! */

	if (S_INT_S1 & 0x40)
	{
		HFC_FSM(sc, 0); 	/* statemachine changed */

		ihfc_sq(sc);
	}

	S_INTR_ACTIVE = 1;

	if (S_INT_S1 & 0x20)		/* D-Channel frame (rx) */
	{
		chan = 1;
		if (S_FILTER) S_FILTER(sc, chan);
	}
	if (S_INT_S1 & 0x04)		/* D-Channel frame (tx) */
	{
		chan = 0;
		if (S_FILTER && (~S_INT_S1 & 0x80)) S_FILTER(sc, chan);
	}
	if (S_INT_S1 & 0x80)		/* Timer elapsed (50ms) */
	{
		chan = 6;
		while(chan--)
		{
			if (chan == 1) continue;
			if (S_FILTER) S_FILTER(sc, chan);

			HFC_END;
			DELAY(10);
			HFC_BEG;
		}
	}

	S_INTR_ACTIVE = 0;

	HFC_END;
}

/*---------------------------------------------------------------------------*
 *	Select a Bfifo						(HFC-1/S/SP)
 *	and return bytes in FIFO
 *
 *	(this code is optimized)
 *---------------------------------------------------------------------------*/
u_short
ihfc_Bsel_fifo(ihfc_sc_t *sc, u_char chan, u_char flag)
{
	register u_char	 reg = 0x7e + chan;
	register u_short tmp = 0x100;
	register u_short z1;
	register u_short z2;

	BUS_VAR;

	if (S_HFC & (HFC_1 | HFC_S))
	{
		if (S_LAST_CHAN != chan)
		{
			SET_REG(reg);
			DISBUSY(WAITBUSY_2( , return 0), return 0);

			S_LAST_CHAN = chan;
		}
	}
	else
	{
		SET_REG(0x10);
		WRITE_DATA_1(chan - 2);
		DISBUSY( , return 0);
	}

#define FAST_READ (u_char)(tmp = READ_BOTH_2)
#define	FAST_STAT if (tmp & 0x100) DISBUSY( , return 0);

		SET_REG(reg     ); FAST_STAT;	z1  = FAST_READ;
		SET_REG(reg += 4); FAST_STAT; 	z1 |= FAST_READ << 8;
		SET_REG(reg += 4); FAST_STAT;	z2  = FAST_READ;
		SET_REG(reg += 4); FAST_STAT;	z2 |= READ_DATA_1 << 8;

#undef FAST_READ
#undef FAST_STAT

	z1 &= 0x1fff;
	z2 &= 0x1fff;
	
	z1 = 0x5ff - (z2 = z1 - z2 + ((z2 <= z1) ? 0 : 0x600));

	if (chan & 1)
		return(z2);	/* receive channel */
	else
		return(z1);	/* transmit channel */
}

/*---------------------------------------------------------------------------*
 *	Select a Dfifo						(HFC-S/SP)
 *	and return bytes, and frames in FIFO
 *
 *	Flag values:
 *		0x00: select new fifo + update counters
 *		0x10: increment f1 + update counters
 *		0x20: increment f2 + update counters
 *
 *	NOTE: The upper 16bits holds the number of frames in the FIFO.
 *	NOTE: FIFO has to be selected before you can use flags 0x10/0x20.
 *---------------------------------------------------------------------------*/
u_int32_t
ihfc_Dsel_fifo(ihfc_sc_t *sc, u_char chan, u_char flag)
{
	register u_char	 reg = 0x90 + chan;
	register u_short tmp = 0x100;
	register u_char  f1;
	register u_char  f2;
	register u_short z1;
	register u_short z2;

	BUS_VAR;

	if (S_HFC & (HFC_1 | HFC_S))
	{
		switch(flag)
		{
			case(0x10):
			case(0x20):
				SET_REG(reg);
				if (~GET_STAT & 1)
					WAITBUSY_2( , return 0);

				SET_REG(0xa2 - (flag & 0x10) + chan);
				DISBUSY(READ_DATA_1, return 0);

				SET_REG(reg);
				if (~GET_STAT & 1)
					WAITBUSY_2( , return 0);
				break;

			default:
				if (S_LAST_CHAN != chan)
				{
					SET_REG(reg);
					DISBUSY(WAITBUSY_2( , return 0), return 0);

					S_LAST_CHAN = chan;
				}
				break;
		}
	}
	else
	{
		switch(flag)
		{
			case(0x10):
			case(0x20):
				SET_REG(0xb8 - (flag & 0x10) + chan);
				READ_DATA_1;

				DISBUSY( , return 0);

				if (chan & 1)
				{
					/* Before reading a FIFO a change *
					 * FIFO operation must be done.	  *
					 * (see HFC-SP manual p.38)	  */

					SET_REG(0x10);
					WRITE_DATA_1(chan | 4);

					DISBUSY( , return 0);
				}
				break;

			default:
				SET_REG(0x10);
				WRITE_DATA_1(chan | 4);

				DISBUSY( , return 0);
				break;
		}
	}

#define FAST_READ (u_char)(tmp = READ_BOTH_2)
#define	FAST_STAT if (tmp & 0x100) DISBUSY( , return 0);

		if (S_HFC & HFC_SP) reg = 0x80 + chan;

		SET_REG(reg     ); FAST_STAT;	z1  = FAST_READ;
		SET_REG(reg += 4); FAST_STAT; 	z1 |= FAST_READ << 8;
		SET_REG(reg += 4); FAST_STAT;	z2  = FAST_READ;
		SET_REG(reg += 4); FAST_STAT;	z2 |= FAST_READ << 8;

		if (S_HFC & HFC_SP) reg += 0x26;

		SET_REG(reg -= 2); FAST_STAT; f1  = FAST_READ;
		SET_REG(reg += 4); FAST_STAT; f2  = READ_DATA_1;

#undef FAST_READ
#undef FAST_STAT

	if (~chan & 1)
	{					/* XXX was Z1                */
		S_HDLC_DZ_TAB[f1 & 0xf] = z2;	/* We keep track of the 'Z'  *
						 * values for D-channel (tx),*
						 * so we may calculate the # *
						 * of FIFO bytes free when   *
						 * f1 != f2.		     */
		z2 = S_HDLC_DZ_TAB[f2 & 0xf];
	}

	z1 = 0x1ff - (z2 = (z1 - z2) & 0x1ff);
	f1 = 0xf   - (f2 = (f1 - f2) & 0xf);

	if (chan & 1)
		return(z2 | (f2 << 16));	/* receive channel */
	else
		return(z1 | (f1 << 16));	/* transmit channel */
}


/*---------------------------------------------------------------------------*
 *	Data handler for D channel(write) - chan 0		(HFC-S/SP)
 *---------------------------------------------------------------------------*/
static void
ihfc_hdlc_Dwrite (ihfc_sc_t *sc, u_char chan)
{
	register u_int32_t  sendlen;
	register u_short    len;
	register u_char	  * src;

	BUS_VAR;

	if (!S_MBUF && IF_QEMPTY(&S_IFQUEUE)) return;

	sendlen = ihfc_Dsel_fifo(sc, chan, 0);	/* select new fifo	    *
						 * NOTE: the 16 higher bits *
						 * contain the # of frame-  *
						 * etries free in the FIFO  */
	while (sendlen & ~0xffff)
	{
		if (!S_MBUF)
		{
			if (!(S_MBUF = ihfc_getmbuf(sc, chan))) goto j1;
		}

		src = S_MBUFDATA;
		len = S_MBUFLEN;

		if (len >= 0x1ff) goto j0;	/* frame is too big: skip! */

		sendlen &= 0xffff;		/* only keep # of *
						 * bytes free	  */

		SET_REG((S_HFC & HFC_SP) ? 0xac : 0x96);

		while (sendlen--)
		{
			if (!len--) break;

			DISBUSY(WRITE_DATA_1(*src++), sendlen = -1; len++; break);
		}

		if (!++sendlen)			/* out of fifo: suspend */
		{
			S_MBUFDATA = src;
			S_MBUFLEN  = len;
			break;
		}

		sendlen = ihfc_Dsel_fifo(sc, chan, 0x10);	/* inc F1 */
	   j0:
		i4b_Dfreembuf(S_MBUF);
		S_MBUF = NULL;
	}
  j1:
	return;
}

/*---------------------------------------------------------------------------*
 *	Data handler for D channel(read) - chan 1		(HFC-S/SP)
 *
 *	NOTE: Max framelength is (511 - 3) = 508 bytes, when only one frame
 *	      is received at a time.
 *---------------------------------------------------------------------------*/
static void
ihfc_hdlc_Dread (ihfc_sc_t *sc, u_char chan)
{
	register u_char	    tmp = -1;
	register u_char	    to = 15;
	register u_int32_t  reclen;
	register u_short    crc;
	register u_short    len;
	register u_char	  * dst;

	BUS_VAR;

	reclen = ihfc_Dsel_fifo(sc, chan, 0);	/* select new fifo	    *
						 * NOTE: the higher 16 bits *
						 * contain the # of frames  *
						 * to receive.		    */
	while ((reclen & ~0xffff) && to--)
	{
		reclen &= 0xffff;		/* only keep # of	*
						 * bytes to receive	*/

		if (!(S_MBUF = i4b_Dgetmbuf(DCH_MAX_LEN)))
			panic("ihfc_hdlc_Dread: No mbufs(unit=%d)!\n", S_UNIT);

		SET_REG((S_HFC & HFC_SP) ? 0xbd : 0xa7);

		if ((reclen > 2) && (reclen <= (DCH_MAX_LEN+2)))
		{
			dst = S_MBUFDATA;
			len = S_MBUFLEN = (reclen += 1) - 3;
		}
		else
		{
			len = 0;
			dst = NULL;
		}

		crc = -1;	/* NOTE: after a "F1" or "Z1" hardware overflow	*
				 * it appears not to be necessary to reset the 	*
				 * HFC-1/S or SP chips to continue proper	*
				 * operation, only and only, if we always read	*
				 * "Z1-Z2+1" bytes when F1!=F2 followed by a 	*
				 * F2-counter increment. The bi-effect of doing	*
				 * this is the "STAT" field may say frame is ok	*
				 * when the frame is actually bad.		*
				 * The simple solution is to re-CRC the frame	*
				 * including "STAT" field to see if we get	*
				 * CRC == 0x3933. Then we're 99% sure all	*
				 * frames received are good.			*/

		while(reclen--)
		{
			DISBUSY(tmp = READ_DATA_1, break);
			if (len) { len--; *dst++ = tmp; }

			crc = (HDLC_FCS_TAB[(u_char)(tmp ^ crc)] ^ (u_char)(crc >> 8));
		}

		crc ^= 0x3933;

		if (!tmp && !crc)
		{
			ihfc_putmbuf(sc, chan, S_MBUF);
			S_MBUF = NULL;
		}
		else
		{
			NDBGL1(L1_ERROR, "Frame error (len=%d, stat=0x%x, "
				"crc=0x%x, unit=%d)", S_MBUFLEN, (u_char)tmp, crc,
				S_UNIT);

			i4b_Dfreembuf(S_MBUF);
			S_MBUF = NULL;
		}

		reclen = ihfc_Dsel_fifo(sc, chan, 0x20);
	}
}

/*---------------------------------------------------------------------------*
 *	EXIR error handler - ISAC 	(D - channel)		(HFC-1)
 *---------------------------------------------------------------------------*/
void
ihfc_exir_hdlr (ihfc_sc_t *sc, u_char exir)
{
	register u_char a;
	register u_char cmd;

	for (a = 0, cmd = 0; exir; a++, exir >>= 1)
	{
		if (exir & 1)
		{
			NDBGL1(L1_I_ERR, "%s. (unit=%d)",
				ihfc_EXIRtable[a].string, S_UNIT);
			cmd |= ihfc_EXIRtable[a].cmd;
		}
	}

	if (cmd) ihfc_cmdr_hdlr(sc, cmd);
}

/*---------------------------------------------------------------------------*
 *	CMDR handler - ISAC 	(D - channel)			(HFC-1)
 *---------------------------------------------------------------------------*/
void
ihfc_cmdr_hdlr (ihfc_sc_t *sc, u_char cmdr)
{
	BUS_VAR;

	SET_REG(0x21); DISBUSY(WRITE_DATA_1(cmdr); DELAY(30), );
}

/*---------------------------------------------------------------------------*
 *	Data handler for D channel(write) - chan 0 		(HFC-1)
 *---------------------------------------------------------------------------*/
static void
ihfc_isac_Dwrite (ihfc_sc_t *sc, u_char chan)
{
	register u_char   sendlen = 32;
	register u_char   cmd = 0;
	register u_short  len;
	register u_char * src;

	BUS_VAR;

	if (~S_ISTA & 0x10) goto j0;

	if (!S_MBUF)
		if (!(S_MBUF = ihfc_getmbuf(sc, chan))) goto j0;

	len = S_MBUFLEN;
	src = S_MBUFDATA;

	SET_REG(0x00);

	while(sendlen--)	/* write data */
	{
		if (!len--) break;
		DISBUSY(WRITE_DATA_1(*src++), goto a0);
	}

	cmd |= 0x08;
	
	if (!++sendlen)		/* suspend */
	{
		S_MBUFLEN  = len;
		S_MBUFDATA = src;
	}
	else
	{
	   a0:
		i4b_Dfreembuf(S_MBUF);
		S_MBUF = NULL;

		cmd |= 0x02;
	}

	if (cmd) ihfc_cmdr_hdlr(sc, cmd);

	S_ISTA &= ~0x10;
  j0:
	return;
}

/*---------------------------------------------------------------------------*
 *	Data handler for D channel(read) - chan 1 		(HFC-1)
 *---------------------------------------------------------------------------*/
static void
ihfc_isac_Dread (ihfc_sc_t *sc, u_char chan)
{
	register u_char   cmd = 0;
	register u_char   reclen;
	register u_short  tmp;
	register u_short  len;
	register u_char * dst;

	BUS_VAR;

	if (!(S_ISTA & 0xc0)) goto j1;	/* only receive data *
					 * on interrupt	     */

	if (!S_MBUF)
	{
		if (!(S_MBUF = i4b_Dgetmbuf(DCH_MAX_LEN)))
			panic("ihfc%d: (D) Out of mbufs!\n", S_UNIT);
	}

	len = S_MBUFLEN;
	dst = S_MBUFDATA + (DCH_MAX_LEN - len);

	if (S_ISTA & 0x80)	/* RME */
	{
		SET_REG(0x27); DISBUSY(tmp = (READ_DATA_1 ^ 0x20), goto j0);

		if (tmp & 0x70) goto j0;	/* error */

		SET_REG(0x25); DISBUSY(tmp = (READ_DATA_1 & 0x1f), goto j0);

		reclen = (tmp) ? tmp : 32;
	}
	else			/* RPF */
	{
		reclen = 32;
	}

	if ((len -= reclen) <= DCH_MAX_LEN)	/* get data */
	{
		SET_REG(0x00);

		while(reclen--)
		{
			DISBUSY(*dst++ = READ_DATA_1, goto j0);
		}
	}
	else		/* soft rdo or error */
	{
	  j0:	i4b_Dfreembuf(S_MBUF);
		S_MBUF = NULL;
		
		cmd |= 0x40;

		NDBGL1(L1_I_ERR, "Frame error (unit=%d)", S_UNIT);
	}

	if (S_ISTA & 0x80) 	/* frame complete */
	{
		if (S_MBUF)
		{
			S_MBUFLEN = (DCH_MAX_LEN - len);
			ihfc_putmbuf(sc, chan, S_MBUF);
			S_MBUF = NULL;
		}
	}

	if (S_MBUF)	/* suspend */
	{
		S_MBUFLEN = len;
	}

	ihfc_cmdr_hdlr(sc, cmd | 0x80);

	S_ISTA &= ~0xc0;
  j1:
	return;
}

/*---------------------------------------------------------------------------*
 *	Data handler for B channel(write) - chan 2 and 4	(HFC-1/S/SP)
 *
 *	NOTE: No XDU checking!
 *---------------------------------------------------------------------------*/
static void
ihfc_trans_Bwrite (ihfc_sc_t *sc, u_char chan)
{
	register u_short  sendlen;
	register u_short  len;
	register u_char * src;

	BUS_VAR;

	if (!S_MBUF && IF_QEMPTY(&S_IFQUEUE)) return;

	sendlen = (u_short)ihfc_Bsel_fifo(sc, chan, 0);

	SET_REG(0xaa + chan);

	while (1)
	{
		if (!S_MBUF)
		{
			S_MBUF = ihfc_getmbuf(sc, chan);
			if (!S_MBUF) break;
		}

		src = S_MBUFDATA;
		len = S_MBUFLEN;

		while (sendlen--)
		{
			if (!len--) break;

			DISBUSY(WRITE_DATA_1(*src++), sendlen = -1; len++; break);
		}

		if (!++sendlen)		/* out of fifo: Suspend */
		{
			S_MBUFDATA = src;
			S_MBUFLEN  = len;
			break;
		}

		i4b_Dfreembuf(S_MBUF);
		S_MBUF = NULL;
	}
}

/*---------------------------------------------------------------------------*
 *	Data handler for B channel(read) - chan 3 and 5		(HFC-1/S/SP)
 *	(this code is optimized)
 *---------------------------------------------------------------------------*/
static void
ihfc_trans_Bread (ihfc_sc_t *sc, u_char chan)
{
	register u_short  reclen;
	register u_short  tmp;
	register u_short  len;
	register u_char	* dst;

	BUS_VAR;

	reclen = (u_short)ihfc_Bsel_fifo(sc, chan, 0);

	while (1)
	{
		SET_REG(0xba + chan);

		tmp = 0x100;

		if (!S_MBUF)
			if (!(S_MBUF = i4b_Bgetmbuf(BCH_MAX_DATALEN)))
				panic("ihfc%d: (B) Out of mbufs!\n", S_UNIT);

		len   = S_MBUFLEN;
		dst   = S_MBUFDATA + (BCH_MAX_DATALEN - len);

		while (reclen--)
		{
			if (!len--) break;

			if (tmp & 0x100) DISBUSY( , reclen = -1; len++; break);
			*dst++ = (u_char)(tmp = READ_BOTH_2);
		}

		if (~tmp & 0x100)
		{
			SET_REG(0x30);
			READ_DATA_1;	/* a read to the data port	*
					 * will disable the internal	*
					 * disbusy signal for HFC-1/S	*
					 * chips. This is neccessary	*
					 * to avvoid data loss.		*/
		}

		if (!++reclen)		/* out of fifo: suspend */
		{
			S_MBUFLEN = len;
			break;
		}

		S_MBUFLEN = (BCH_MAX_DATALEN - ++len);

		ihfc_putmbuf(sc, chan, S_MBUF);

		S_MBUF = NULL;
	}
}

/*---------------------------------------------------------------------------*
 *	Data handler for B channel(write) - chan 2 and 4	(HFC-1/S/SP)
 *	
 *	NOTE: Software HDLC encoding!
 *---------------------------------------------------------------------------*/
static void
ihfc_hdlc_Bwrite (ihfc_sc_t *sc, u_char chan)
{
	register u_short  blevel  = S_HDLC_BLEVEL;
	register u_char   flag    = S_HDLC_FLAG;
	register u_int    tmp     = S_HDLC_TMP;
	register u_short  crc     = S_HDLC_CRC;
	register u_short  ib      = S_HDLC_IB;
	register u_char * src     = NULL;
	register u_short  len     = 0;
	register u_short  sendlen;
	register u_short  tmp2;

	BUS_VAR;

	if (!S_MBUF && IF_QEMPTY(&S_IFQUEUE) && (flag == 2)) return;

	sendlen = (u_short)ihfc_Bsel_fifo(sc, chan, 0);

	SET_REG(0xaa + chan);

	if (S_MBUF)
	{
		/* resume */

		src = S_MBUFDATA; 
		len = S_MBUFLEN;

		if (sendlen == 0x5ff)
		{ 
			/* XDU */

			flag = -2;
			len  = 0;

			NDBGL1(L1_S_ERR, "XDU (unit=%d)", S_UNIT);
		}
	}

	while (sendlen--)
	{
		HDLC_ENCODE(*src++, len, tmp, tmp2, blevel, ib, crc, flag,
		{/* gfr */
			i4b_Bfreembuf(S_MBUF);
			S_MBUF = ihfc_getmbuf(sc, chan);

			if (S_MBUF)
			{
				src = S_MBUFDATA;
				len = S_MBUFLEN;
			}
			else
			{
				sendlen = 0;	/* Exit after final FS, *
						 * else the buffer will *
						 * only be filled with  *
						 * "0x7e"-bytes!        */
			}
		},
		{/* wrd */
			
			DISBUSY(WRITE_DATA_1((u_char)tmp), sendlen = 0);
		},
		d );
	}

	if (S_MBUF)		/* suspend */
	{
		S_MBUFDATA = src;
		S_MBUFLEN  = len;
	}

	S_HDLC_IB	= ib;
	S_HDLC_BLEVEL	= blevel;
	S_HDLC_TMP  	= tmp;
	S_HDLC_FLAG 	= flag;
	S_HDLC_CRC  	= crc;
}

/*---------------------------------------------------------------------------*
 *	Data handler for B channel(read) - chan 3 and 5		(HFC-1/S/SP)
 *	
 *	NOTE: Software HDLC decoding!
 *---------------------------------------------------------------------------*/
static void
ihfc_hdlc_Bread (ihfc_sc_t *sc, u_char chan)
{
	register u_char   blevel = S_HDLC_BLEVEL;
		 u_char   flag   = S_HDLC_FLAG;
	register u_short  crc    = S_HDLC_CRC;
	register u_int    tmp    = S_HDLC_TMP;
	register u_short  ib     = S_HDLC_IB;
	register u_char * dst    = NULL;
	register u_short  tmp2   = 0x100;
	register u_short  len    = 0;
	register u_short  reclen;

	BUS_VAR;

	if (S_MBUF)
	{
		/* resume */

		len = S_MBUFLEN;
		dst = S_MBUFDATA + (BCH_MAX_DATALEN - len);
	}

	reclen = (u_short)ihfc_Bsel_fifo(sc, chan, 0);

	SET_REG(0xba + chan);

	while (reclen--)
	{
		HDLC_DECODE(*dst++, len, tmp, tmp2, blevel, ib, crc, flag,
		{/* rdd */
			/* if (tmp2 & 0x100) while (GET_STAT & 1);
			 * tmp2 = READ_BOTH_2;
			 */

			DISBUSY(tmp2 = READ_DATA_1, reclen = 0; tmp2 = 0);
		},
		{/* nfr */
			if (!(S_MBUF = i4b_Bgetmbuf(BCH_MAX_DATALEN)))
				panic("ihfc:(B) Out of mbufs!\n");
				
			dst = S_MBUFDATA;
			len = BCH_MAX_DATALEN;
		},
		{/* cfr */
			len = (BCH_MAX_DATALEN - len);

			if ((!len) || (len > BCH_MAX_DATALEN))
			{
				/* NOTE: frames without any data,               *
				 * only crc field, should be silently discared. */

				i4b_Bfreembuf(S_MBUF);
				NDBGL1(L1_S_MSG, "Bad frame (len=%d, unit=%d)", len, S_UNIT);
				goto s0;
			}

			if (crc)
			{	i4b_Bfreembuf(S_MBUF);
				NDBGL1(L1_S_ERR, "CRC (crc=0x%04x, len=%d, unit=%d)", crc, len, S_UNIT);
				goto s0;
			}

			S_MBUFLEN = len;

			ihfc_putmbuf(sc, chan, S_MBUF);
		 s0:
			S_MBUF = NULL;
		},
		{/* rab */
			i4b_Bfreembuf(S_MBUF);
			S_MBUF = NULL;

			NDBGL1(L1_S_MSG, "Read Abort (unit=%d)", S_UNIT);
		},
		{/* rdo */
			i4b_Bfreembuf(S_MBUF);
			S_MBUF = NULL;

			NDBGL1(L1_S_ERR, "RDO (unit=%d)", S_UNIT);
		},
		continue,
		d);
	}

	/* SET_REG(0x30);
	 * if (~tmp2 & 0x100) READ_DATA_1;	kill disbusy signal
	 */

	if (S_MBUF) S_MBUFLEN = len;	/* suspend */

	S_HDLC_IB 	= ib;
	S_HDLC_CRC	= crc;
	S_HDLC_TMP	= tmp;
	S_HDLC_FLAG 	= flag;
	S_HDLC_BLEVEL	= blevel;
}

/*---------------------------------------------------------------------------*
 *	Data handler for B channel(write) - chan 2 and 4	(HFC-1/S/SP)
 *
 *	This filter generates a pattern which is recognized
 *	and examinated and verified by ihfc_test_Bread.
 *
 *	NOTE: This filter is only for testing purpose.
 *---------------------------------------------------------------------------*/
static void
ihfc_test_Bwrite (ihfc_sc_t *sc, u_char chan)
{
	struct mbuf *m;

	register u_char	 fb;
	register u_short sendlen, tlen;
	register u_short xlen = S_HDLC_IB;
	BUS_VAR;

	goto j0;

	while((m = ihfc_getmbuf(sc, chan)))	/* internal loop */
	{
		if (chan == 2)
			ihfc_putmbuf(sc, 5, m);
		else
			ihfc_putmbuf(sc, 3, m);
	}

	j0:

	sendlen = /* (u_short)ihfc_Bsel_fifo(sc, chan, 0); */ 0;

	if (sendlen == 0x5ff) printf("(send empty)");

	SET_REG(0xaa + chan);

	S_BYTES += sendlen;

	tlen    = S_HDLC_CRC;

	if (sendlen > 0x400) printf("(slow: %d)", sendlen);

	fb = 0x80;

	while (sendlen--)
	{
		if (!tlen--) fb |= 0x20;

		if (!xlen--)
		{
			while(GET_STAT & 1);
			WRITE_DATA_1(0x3e);
			xlen = 200;
		}
		else
		{
			while(GET_STAT & 1);
			WRITE_DATA_1((xlen + 1) & 0xef);
		}

		fb = 0;
	}

	S_HDLC_IB = xlen;
}

/*---------------------------------------------------------------------------*
 *	Data handler for B channel(read) - chan 3 and 5		(HFC-1/S/SP)
 *
 *	This filter examins and verifies the pattern
 *	generated by ihfc_test_Bwrite.
 *
 *	NOTE: This filter is only for testing purpose.
 *---------------------------------------------------------------------------*/
static void
ihfc_test_Bread (ihfc_sc_t *sc, u_char chan)
{
	static u_short toterrors = 0;

	register u_short reclen, len, tlen;
	register u_char fb, tmp;

	register u_short xlen = S_HDLC_IB;
	register u_char *dst = NULL;
	register u_char error = S_HDLC_TMP;
	register u_char ecount = S_HDLC_FLAG;

	BUS_VAR;

	if (S_UNIT != 0) return;

	reclen = /* (u_short)ihfc_Bsel_fifo(sc, chan, 0); */ 0;

	S_BYTES += reclen;

	tlen   = S_HDLC_CRC;

	fb = 0x40;

	if (S_MBUF)
	{
		len = S_MBUFLEN;
		dst = S_MBUFDATA + (BCH_MAX_DATALEN - len);
	}
	else
	{
		len = 0;
	}

	SET_REG(0xba + chan);

	while (reclen--)
	{
/*		if (tmp2 & 0x100) while(GET_STAT & 1);
 *		tmp = (u_char)(tmp2 = READ_BOTH_2);
 */
		if (GET_STAT & 1)
		{	
			/* if (!(++busy % 4)) reclen++; */
			while(GET_STAT & 1);
		}

		tmp = READ_DATA_1;

		if ((tmp & 0x3f) == 0x3e)
		{
			if ((BCH_MAX_DATALEN - len) != 201) error |= 4;

			if ((S_MBUF) && (error))
			{
				if (len) { len--; *dst++ = error; }
				if (len) { len--; *dst++ = xlen+1; }
				if (len) { len--; *dst++ = ecount; }

				S_MBUFLEN = BCH_MAX_DATALEN - len;

				if (S_TRACE & TRACE_B_RX)
					ihfc_putmbuf(sc, chan, S_MBUF);
				else
					i4b_Bfreembuf(S_MBUF);

				S_MBUF = NULL;
				printf("(error%d, %d, %d)", S_UNIT, ecount, toterrors++);
			}

			i4b_Bfreembuf(S_MBUF);
			S_MBUF = i4b_Bgetmbuf(BCH_MAX_DATALEN);

			dst = S_MBUFDATA;
			len = BCH_MAX_DATALEN;

			xlen = 200;
			error = 0;
			ecount = 0;

		/*	SET_REG(0xba + chan); */
		}
		else
		{
			if (!xlen) error |= 2;
			if ((tmp ^ xlen--) & 0xef) { error |= 1; ecount++; }
		}
		if (!tlen--) fb |= 0x20;

		if (len--)
		{
			*dst++ = (tmp | fb);
		}
		else
		{
			len++;
		}

		fb = 0;
	}

	if (S_MBUF)
	{
		S_MBUFLEN = len;
	}

	S_HDLC_IB = xlen;
	S_HDLC_TMP = error;
	S_HDLC_FLAG = ecount;
}
