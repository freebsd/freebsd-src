/*
 * Low-level subroutines for Cronyx-Sigma adapter.
 *
 * Copyright (C) 1994-95 Cronyx Ltd.
 * Author: Serge Vakulenko, <vak@cronyx.ru>
 *
 * This software is distributed with NO WARRANTIES, not even the implied
 * warranties for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * Authors grant any other persons or organisations permission to use
 * or modify this software as long as this message is kept with the software,
 * all derivative works or modified versions.
 *
 * Version 1.6, Wed May 31 16:03:20 MSD 1995
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");
#if defined (MSDOS) || defined (__MSDOS__)
#   include <string.h>
#   include <dos.h>
#   define inb(port)    inportb(port)
#   define inw(port)    inport(port)
#   define outb(port,b) outportb(port,b)
#   define outw(port,w)	outport(port,w)
#   define vtophys(a)	(((unsigned long)(a)>>12 & 0xffff0) +\
			((unsigned)(a) & 0xffff))
#   include "cronyx.h"
#   include "cxreg.h"
#else
#   include <sys/param.h>
#   include <sys/systm.h>
#   include <sys/socket.h>
#   include <net/if.h>
#   include <vm/vm.h>
#   include <vm/vm_param.h>
#   include <vm/pmap.h>
#   ifndef __FreeBSD__
#      include <machine/inline.h>
#   endif
#   include <machine/cronyx.h>
#   include <i386/isa/cxreg.h>
#endif

#define DMA_MASK	0xd4	/* DMA mask register */
#define DMA_MASK_CLEAR	0x04	/* DMA clear mask */
#define DMA_MODE	0xd6	/* DMA mode register */
#define DMA_MODE_MASTER	0xc0	/* DMA master mode */

#define BYTE *(unsigned char*)&

static unsigned char irqmask [] = {
	BCR0_IRQ_DIS,	BCR0_IRQ_DIS,	BCR0_IRQ_DIS,	BCR0_IRQ_3,
	BCR0_IRQ_DIS,	BCR0_IRQ_5,	BCR0_IRQ_DIS,	BCR0_IRQ_7,
	BCR0_IRQ_DIS,	BCR0_IRQ_DIS,	BCR0_IRQ_10,	BCR0_IRQ_11,
	BCR0_IRQ_12,	BCR0_IRQ_DIS,	BCR0_IRQ_DIS,	BCR0_IRQ_15,
};

static unsigned char dmamask [] = {
	BCR0_DMA_DIS,	BCR0_DMA_DIS,	BCR0_DMA_DIS,	BCR0_DMA_DIS,
	BCR0_DMA_DIS,	BCR0_DMA_5,	BCR0_DMA_6,	BCR0_DMA_7,
};

static long cx_rxbaud = CX_SPEED_DFLT;	/* receiver baud rate */
static long cx_txbaud = CX_SPEED_DFLT;	/* transmitter baud rate */

static int cx_univ_mode = M_ASYNC;	/* univ. chan. mode: async or sync */
static int cx_sync_mode = M_HDLC;	/* sync. chan. mode: HDLC, Bisync or X.21 */
static int cx_iftype = 0;		/* univ. chan. interface: upper/lower */

static cx_chan_opt_t chan_opt_dflt = { /* mode-independent options */
	{			/* cor4 */
		7,		/* FIFO threshold, odd is better */
		0,
		0,              /* don't detect 1 to 0 on CTS */
		1,		/* detect 1 to 0 on CD */
		0,              /* detect 1 to 0 on DSR */
	},
	{			/* cor5 */
		0,		/* receive flow control FIFO threshold */
		0,
		0,              /* don't detect 0 to 1 on CTS */
		1,		/* detect 0 to 1 on CD */
		0,              /* detect 0 to 1 on DSR */
	},
	{			/* rcor */
		0,		/* dummy clock source */
		ENCOD_NRZ,      /* NRZ mode */
		0,              /* disable DPLL */
		0,
		0,		/* transmit line value */
	},
	{			/* tcor */
		0,
		0,		/* local loopback mode */
		0,
		1,		/* external 1x clock mode */
		0,
		0,		/* dummy transmit clock source */
	},
};

static cx_opt_async_t opt_async_dflt = { /* default async options */
	{			/* cor1 */
		8-1,		/* 8-bit char length */
		0,		/* don't ignore parity */
		PARM_NOPAR,	/* no parity */
		PAR_EVEN,	/* even parity */
	},
	{			/* cor2 */
		0,              /* disable automatic DSR */
		1,              /* enable automatic CTS */
		0,              /* disable automatic RTS */
		0,		/* no remote loopback */
		0,
		0,              /* disable embedded cmds */
		0,		/* disable XON/XOFF */
		0,		/* disable XANY */
	},
	{			/* cor3 */
		STOPB_1,	/* 1 stop bit */
		0,
		0,		/* disable special char detection */
		FLOWCC_PASS,	/* pass flow ctl chars to the host */
		0,		/* range detect disable */
		0,		/* disable extended spec. char detect */
	},
	{			/* cor6 */
		PERR_INTR,	/* generate exception on parity errors */
		BRK_INTR,	/* generate exception on break condition */
		0,		/* don't translate NL to CR on input */
		0,		/* don't translate CR to NL on input */
		0,		/* don't discard CR on input */
	},
	{			/* cor7 */
		0,		/* don't translate CR to NL on output */
		0,		/* don't translate NL to CR on output */
		0,
		0,		/* don't process flow ctl err chars */
		0,		/* disable LNext option */
		0,		/* don't strip 8 bit on input */
	},
	0, 0, 0, 0, 0, 0, 0,	/* clear schr1-4, scrl, scrh, lnxt */
};

static cx_opt_hdlc_t opt_hdlc_dflt = { /* default hdlc options */
	{			/* cor1 */
		2,              /* 2 inter-frame flags */
		0,		/* no-address mode */
		CLRDET_DISABLE,	/* disable clear detect */
		AFLO_1OCT,	/* 1-byte address field length */
	},
	{			/* cor2 */
		0,		/* disable automatic DSR */
		0,              /* disable automatic CTS */
		0,              /* disable automatic RTS */
		0,
		CRC_INVERT,	/* use CRC V.41 */
		0,
		FCS_NOTPASS,	/* don't pass received CRC to the host */
		0,
	},
	{			/* cor3 */
		0,              /* 0 pad characters sent */
		IDLE_FLAG,	/* idle in flag */
		0,		/* enable FCS */
		FCSP_ONES,	/* FCS preset to all ones (V.41) */
		SYNC_AA,	/* use AAh as sync char */
		0,              /* disable pad characters */
	},
	0, 0, 0, 0,		/* clear rfar1-4 */
	POLY_V41,		/* use V.41 CRC polynomial */
};

static cx_opt_bisync_t opt_bisync_dflt = { /* default bisync options */
	{			/* cor1 */
		8-1,            /* 8-bit char length */
		0,		/* don't ignore parity */
		PARM_NOPAR,	/* no parity */
		PAR_EVEN,	/* even parity */
	},
	{			/* cor2 */
		3-2,            /* send three SYN chars */
		CRC_DONT_INVERT,/* don't invert CRC (CRC-16) */
		0,		/* use ASCII, not EBCDIC */
		0,		/* disable bcc append */
		BCC_CRC16,	/* user CRC16, not LRC */
	},
	{			/* cor3 */
		0,              /* send 0 pad chars */
		IDLE_FLAG,	/* idle in SYN */
		0,		/* enable FCS */
		FCSP_ZEROS,	/* FCS preset to all zeros (CRC-16) */
		PAD_AA,		/* use AAh as pad char */
		0,		/* disable pad characters */
	},
	{			/* cor6 */
		10,		/* DLE - disable special termination char */
	},
	POLY_16,		/* use CRC-16 polynomial */
};

static cx_opt_x21_t opt_x21_dflt = {   /* default x21 options */
	{			/* cor1 */
		8-1,            /* 8-bit char length */
		0,		/* don't ignore parity */
		PARM_NOPAR,	/* no parity */
		PAR_EVEN,	/* even parity */
	},
	{			/* cor2 */
		0,
		0,		/* disable embedded transmitter cmds */
		0,
	},
	{			/* cor3 */
		0,
		0,		/* disable special character detect */
		0,		/* don't treat SYN as special condition */
		0,		/* disable steady state detect */
		X21SYN_2,	/* 2 SYN chars on receive are required */
	},
	{			/* cor6 */
		16,		/* SYN - standard SYN character */
	},
	0, 0, 0,		/* clear schr1-3 */
};

static int cx_probe_chip (int base);
static void cx_setup_chip (cx_chip_t *c);
static void cx_init_board (cx_board_t *b, int num, int port, int irq, int dma,
	int chain, int rev, int osc, int rev2, int osc2);
static void cx_reinit_board (cx_board_t *b);

/*
 * Wait for CCR to clear.
 */
void cx_cmd (int base, int cmd)
{
	unsigned short port = CCR(base);
	unsigned short count;

	/* Wait 10 msec for the previous command to complete. */
	for (count=0; inb(port) && count<20000; ++count)
		continue;

	/* Issue the command. */
	outb (port, cmd);

	/* Wait 10 msec for the command to complete. */
	for (count=0; inb(port) && count<20000; ++count)
		continue;
}

/*
 * Reset the chip.
 */
static int cx_reset (unsigned short port)
{
	int count;

	/* Wait up to 10 msec for revision code to appear after reset. */
	for (count=0; count<20000; ++count)
		if (inb(GFRCR(port)) != 0)
			break;

	cx_cmd (port, CCR_RSTALL);

	/* Firmware revision code should clear imediately. */
	/* Wait up to 10 msec for revision code to appear again. */
	for (count=0; count<20000; ++count)
		if (inb(GFRCR(port)) != 0)
			return (1);

	/* Reset failed. */
	return (0);
}

/*
 * Check if the CD2400 board is present at the given base port.
 */
static int cx_probe_chained_board (int port, int *c0, int *c1)
{
	int rev, i;

	/* Read and check the board revision code. */
	rev = inb (BSR(port));
	*c0 = *c1 = 0;
	switch (rev & BSR_VAR_MASK) {
	case CRONYX_100:	*c0 = 1;	break;
	case CRONYX_400:	*c1 = 1;	break;
	case CRONYX_500:	*c0 = *c1 = 1;	break;
	case CRONYX_410:	*c0 = 1;	break;
	case CRONYX_810:	*c0 = *c1 = 1;	break;
	case CRONYX_410s:	*c0 = 1;	break;
	case CRONYX_810s:	*c0 = *c1 = 1;	break;
	case CRONYX_440:	*c0 = 1;	break;
	case CRONYX_840:	*c0 = *c1 = 1;	break;
	case CRONYX_401:	*c0 = 1;	break;
	case CRONYX_801:	*c0 = *c1 = 1;	break;
	case CRONYX_401s:	*c0 = 1;	break;
	case CRONYX_801s:	*c0 = *c1 = 1;	break;
	case CRONYX_404:	*c0 = 1;	break;
	case CRONYX_703:	*c0 = *c1 = 1;	break;
	default:		return (0);	/* invalid variant code */
	}

	switch (rev & BSR_OSC_MASK) {
	case BSR_OSC_20:	/* 20 MHz */
	case BSR_OSC_18432:	/* 18.432 MHz */
		break;
	default:
		return (0);	/* oscillator frequency does not match */
	}

	for (i=2; i<0x10; i+=2)
		if ((inb (BSR(port)+i) & BSR_REV_MASK) != (rev & BSR_REV_MASK))
			return (0);	/* status changed? */
	return (1);
}

/*
 * Check if the CD2400 board is present at the given base port.
 */
int
cx_probe_board (int port)
{
	int c0, c1, c2=0, c3=0, result;

	if (! cx_probe_chained_board (port, &c0, &c1))
		return (0);     /* no board detected */

	if (! (inb (BSR(port)) & BSR_NOCHAIN)) { /* chained board attached */
		if (! cx_probe_chained_board (port + 0x10, &c2, &c3))
			return (0);     /* invalid chained board? */

		if (! (inb (BSR(port+0x10)) & BSR_NOCHAIN))
			return (0);     /* invalid chained board flag? */
	}

	/* Turn off the reset bit. */
	outb (BCR0(port), BCR0_NORESET);
	if (c2 || c3)
		outb (BCR0(port + 0x10), BCR0_NORESET);

	result = 1;
	if (c0 && ! cx_probe_chip (CS0(port)))
		result = 0;	/* no CD2400 chip here */
	else if (c1 && ! cx_probe_chip (CS1(port)))
		result = 0;	/* no second CD2400 chip */
	else if (c2 && ! cx_probe_chip (CS0(port + 0x10)))
		result = 0;	/* no CD2400 chip on the slave board */
	else if (c3 && ! cx_probe_chip (CS1(port + 0x10)))
		result = 0;	/* no second CD2400 chip on the slave board */

	/* Reset the controller. */
	outb (BCR0(port), 0);
	if (c2 || c3)
		outb (BCR0(port + 0x10), 0);

	/* Yes, we really have valid CD2400 board. */
	return (result);
}

/*
 * Check if the CD2400 chip is present at the given base port.
 */
static int cx_probe_chip (int base)
{
	int rev, newrev, count;

	/* Wait up to 10 msec for revision code to appear after reset. */
	for (count=0; inb(GFRCR(base))==0; ++count)
		if (count >= 20000)
			return (0); /* reset failed */

	/* Read and check the global firmware revision code. */
	rev = inb (GFRCR(base));
	if (rev<REVCL_MIN || rev>REVCL_MAX)
		return (0);	/* CD2400 revision does not match */

	/* Reset the chip. */
	if (! cx_reset (base))
		return (0);

	/* Read and check the new global firmware revision code. */
	newrev = inb (GFRCR(base));
	if (newrev != rev)
		return (0);	/* revision changed */

	/* Yes, we really have CD2400 chip here. */
	return (1);
}

/*
 * Probe and initialize the board structure.
 */
void cx_init (cx_board_t *b, int num, int port, int irq, int dma)
{
	int rev, chain, rev2;

	rev = inb (BSR(port));
	chain = !(rev & BSR_NOCHAIN);
	rev2 = chain ? inb (BSR(port+0x10)) : 0;
	cx_init_board (b, num, port, irq, dma, chain,
		(rev & BSR_VAR_MASK), (rev & BSR_OSC_MASK),
		(rev2 & BSR_VAR_MASK), (rev2 & BSR_OSC_MASK));
}

/*
 * Initialize the board structure, given the type of the board.
 */
static void
cx_init_board (cx_board_t *b, int num, int port, int irq, int dma,
	int chain, int rev, int osc, int rev2, int osc2)
{
	cx_chan_t *c;
	int i, c0, c1;

	/* Initialize board structure. */
	b->port = port;
	b->num = num;
	b->irq = irq;
	b->dma = dma;
	b->if0type = b->if8type = cx_iftype;

	/* Set channels 0 and 8 mode, set DMA and IRQ. */
	b->bcr0 = b->bcr0b = BCR0_NORESET | dmamask[b->dma] | irqmask[b->irq];

	/* Clear DTR[0..3] and DTR[8..12]. */
	b->bcr1 = b->bcr1b = 0;

	/* Initialize chip structures. */
	for (i=0; i<NCHIP; ++i) {
		b->chip[i].num = i;
		b->chip[i].board = b;
	}
	b->chip[0].port = CS0(port);
	b->chip[1].port = CS1(port);
	b->chip[2].port = CS0(port+0x10);
	b->chip[3].port = CS1(port+0x10);

	/*------------------ Master board -------------------*/

	/* Read and check the board revision code. */
	c0 = c1 = 0;
	b->name[0] = 0;
	switch (rev) {
	case CRONYX_100:  strcpy (b->name, "100");  c0 = 1;      break;
	case CRONYX_400:  strcpy (b->name, "400");  c1 = 1;      break;
	case CRONYX_500:  strcpy (b->name, "500");  c0 = c1 = 1; break;
	case CRONYX_410:  strcpy (b->name, "410");  c0 = 1;      break;
	case CRONYX_810:  strcpy (b->name, "810");  c0 = c1 = 1; break;
	case CRONYX_410s: strcpy (b->name, "410s"); c0 = 1;      break;
	case CRONYX_810s: strcpy (b->name, "810s"); c0 = c1 = 1; break;
	case CRONYX_440:  strcpy (b->name, "440");  c0 = 1;      break;
	case CRONYX_840:  strcpy (b->name, "840");  c0 = c1 = 1; break;
	case CRONYX_401:  strcpy (b->name, "401");  c0 = 1;      break;
	case CRONYX_801:  strcpy (b->name, "801");  c0 = c1 = 1; break;
	case CRONYX_401s: strcpy (b->name, "401s"); c0 = 1;      break;
	case CRONYX_801s: strcpy (b->name, "801s"); c0 = c1 = 1; break;
	case CRONYX_404:  strcpy (b->name, "404");  c0 = 1;      break;
	case CRONYX_703:  strcpy (b->name, "703");  c0 = c1 = 1; break;
	}

	switch (osc) {
	default:
	case BSR_OSC_20: /* 20 MHz */
		b->chip[0].oscfreq = b->chip[1].oscfreq = 20000000L;
		strcat (b->name, "a");
		break;
	case BSR_OSC_18432: /* 18.432 MHz */
		b->chip[0].oscfreq = b->chip[1].oscfreq = 18432000L;
		strcat (b->name, "b");
		break;
	}

	if (! c0)
		b->chip[0].port = 0;
	if (! c1)
		b->chip[1].port = 0;

	/*------------------ Slave board -------------------*/

	if (! chain) {
		b->chip[2].oscfreq = b->chip[3].oscfreq = 0L;
		b->chip[2].port = b->chip[3].port = 0;
	} else {
		/* Read and check the board revision code. */
		c0 = c1 = 0;
		strcat (b->name, "/");
		switch (rev2) {
		case CRONYX_100:  strcat(b->name,"100");  c0=1;    break;
		case CRONYX_400:  strcat(b->name,"400");  c1=1;    break;
		case CRONYX_500:  strcat(b->name,"500");  c0=c1=1; break;
		case CRONYX_410:  strcat(b->name,"410");  c0=1;    break;
		case CRONYX_810:  strcat(b->name,"810");  c0=c1=1; break;
		case CRONYX_410s: strcat(b->name,"410s"); c0=1;    break;
		case CRONYX_810s: strcat(b->name,"810s"); c0=c1=1; break;
		case CRONYX_440:  strcat(b->name,"440");  c0=1;    break;
		case CRONYX_840:  strcat(b->name,"840");  c0=c1=1; break;
		case CRONYX_401:  strcat(b->name,"401");  c0=1;    break;
		case CRONYX_801:  strcat(b->name,"801");  c0=c1=1; break;
		case CRONYX_401s: strcat(b->name,"401s"); c0=1;    break;
		case CRONYX_801s: strcat(b->name,"801s"); c0=c1=1; break;
		case CRONYX_404:  strcat(b->name,"404");  c0=1;    break;
		case CRONYX_703:  strcat(b->name,"703");  c0=c1=1; break;
		}

		switch (osc2) {
		default:
		case BSR_OSC_20: /* 20 MHz */
			b->chip[2].oscfreq = b->chip[3].oscfreq = 20000000L;
			strcat (b->name, "a");
			break;
		case BSR_OSC_18432: /* 18.432 MHz */
			b->chip[2].oscfreq = b->chip[3].oscfreq = 18432000L;
			strcat (b->name, "b");
			break;
		}

		if (! c0)
			b->chip[2].port = 0;
		if (! c1)
			b->chip[3].port = 0;
	}

	/* Initialize channel structures. */
	for (i=0; i<NCHAN; ++i) {
		cx_chan_t *c = b->chan + i;

		c->num = i;
		c->board = b;
		c->chip = b->chip + i*NCHIP/NCHAN;
		c->stat = b->stat + i;
		c->type = T_NONE;
	}

	/*------------------ Master board -------------------*/

	switch (rev) {
	case CRONYX_400:
		break;
	case CRONYX_100:
	case CRONYX_500:
		b->chan[0].type = T_UNIV_RS232;
		break;
	case CRONYX_410:
	case CRONYX_810:
		b->chan[0].type = T_UNIV_V35;
		for (i=1; i<4; ++i)
			b->chan[i].type = T_UNIV_RS232;
		break;
	case CRONYX_410s:
	case CRONYX_810s:
		b->chan[0].type = T_UNIV_V35;
		for (i=1; i<4; ++i)
			b->chan[i].type = T_SYNC_RS232;
		break;
	case CRONYX_440:
	case CRONYX_840:
		b->chan[0].type = T_UNIV_V35;
		for (i=1; i<4; ++i)
			b->chan[i].type = T_SYNC_V35;
		break;
	case CRONYX_401:
	case CRONYX_801:
		b->chan[0].type = T_UNIV_RS449;
		for (i=1; i<4; ++i)
			b->chan[i].type = T_UNIV_RS232;
		break;
	case CRONYX_401s:
	case CRONYX_801s:
		b->chan[0].type = T_UNIV_RS449;
		for (i=1; i<4; ++i)
			b->chan[i].type = T_SYNC_RS232;
		break;
	case CRONYX_404:
		b->chan[0].type = T_UNIV_RS449;
		for (i=1; i<4; ++i)
			b->chan[i].type = T_SYNC_RS449;
		break;
	case CRONYX_703:
		b->chan[0].type = T_UNIV_RS449;
		for (i=1; i<3; ++i)
			b->chan[i].type = T_SYNC_RS449;
		break;
	}

	/* If the second controller is present,
	 * then we have 4..7 channels in async. mode */
	if (b->chip[1].port)
		for (i=4; i<8; ++i)
			b->chan[i].type = T_UNIV_RS232;

	/*------------------ Slave board -------------------*/

	if (chain) {
		switch (rev2) {
		case CRONYX_400:
			break;
		case CRONYX_100:
		case CRONYX_500:
			b->chan[8].type = T_UNIV_RS232;
			break;
		case CRONYX_410:
		case CRONYX_810:
			b->chan[8].type = T_UNIV_V35;
			for (i=9; i<12; ++i)
				b->chan[i].type = T_UNIV_RS232;
			break;
		case CRONYX_410s:
		case CRONYX_810s:
			b->chan[8].type = T_UNIV_V35;
			for (i=9; i<12; ++i)
				b->chan[i].type = T_SYNC_RS232;
			break;
		case CRONYX_440:
		case CRONYX_840:
			b->chan[8].type = T_UNIV_V35;
			for (i=9; i<12; ++i)
				b->chan[i].type = T_SYNC_V35;
			break;
		case CRONYX_401:
		case CRONYX_801:
			b->chan[8].type = T_UNIV_RS449;
			for (i=9; i<12; ++i)
				b->chan[i].type = T_UNIV_RS232;
			break;
		case CRONYX_401s:
		case CRONYX_801s:
			b->chan[8].type = T_UNIV_RS449;
			for (i=9; i<12; ++i)
				b->chan[i].type = T_UNIV_RS232;
			break;
		case CRONYX_404:
			b->chan[8].type = T_UNIV_RS449;
			for (i=9; i<12; ++i)
				b->chan[i].type = T_SYNC_RS449;
			break;
		case CRONYX_703:
			b->chan[8].type = T_UNIV_RS449;
			for (i=9; i<11; ++i)
				b->chan[i].type = T_SYNC_RS449;
			break;
		}

		/* If the second controller is present,
		 * then we have 4..7 channels in async. mode */
		if (b->chip[3].port)
			for (i=12; i<16; ++i)
				b->chan[i].type = T_UNIV_RS232;
	}

	b->nuniv = b->nsync = b->nasync = 0;
	for (c=b->chan; c<b->chan+NCHAN; ++c)
		switch (c->type) {
		case T_ASYNC:      ++b->nasync; break;
		case T_UNIV_RS232:
		case T_UNIV_RS449:
		case T_UNIV_V35:   ++b->nuniv;  break;
		case T_SYNC_RS232:
		case T_SYNC_V35:
		case T_SYNC_RS449: ++b->nsync;  break;
		}

	cx_reinit_board (b);
}

/*
 * Reinitialize all channels, using new options and baud rate.
 */
static void
cx_reinit_board (cx_board_t *b)
{
	cx_chan_t *c;

	b->if0type = b->if8type = cx_iftype;
	for (c=b->chan; c<b->chan+NCHAN; ++c) {
		switch (c->type) {
		default:
		case T_NONE:
			continue;
		case T_UNIV_RS232:
		case T_UNIV_RS449:
		case T_UNIV_V35:
			c->mode = (cx_univ_mode == M_ASYNC) ?
				M_ASYNC : cx_sync_mode;
			break;
		case T_SYNC_RS232:
		case T_SYNC_V35:
		case T_SYNC_RS449:
			c->mode = cx_sync_mode;
			break;
		case T_ASYNC:
			c->mode = M_ASYNC;
			break;
		}
		c->rxbaud = cx_rxbaud;
		c->txbaud = cx_txbaud;
		c->opt = chan_opt_dflt;
		c->aopt = opt_async_dflt;
		c->hopt = opt_hdlc_dflt;
		c->bopt = opt_bisync_dflt;
		c->xopt = opt_x21_dflt;
	}
}

/*
 * Set up the board.
 */
void cx_setup_board (cx_board_t *b)
{
	int i;

	/* Disable DMA channel. */
	outb (DMA_MASK, (b->dma & 3) | DMA_MASK_CLEAR);

	/* Reset the controller. */
	outb (BCR0(b->port), 0);
	if (b->chip[2].port || b->chip[3].port)
		outb (BCR0(b->port+0x10), 0);

	/*
	 * Set channels 0 and 8 to RS232 async. mode.
	 * Enable DMA and IRQ.
	 */
	outb (BCR0(b->port), b->bcr0);
	if (b->chip[2].port || b->chip[3].port)
		outb (BCR0(b->port+0x10), b->bcr0b);

	/* Clear DTR[0..3] and DTR[8..12]. */
	outw (BCR1(b->port), b->bcr1);
	if (b->chip[2].port || b->chip[3].port)
		outw (BCR1(b->port+0x10), b->bcr1b);

	/* Initialize all controllers. */
	for (i=0; i<NCHIP; ++i)
		if (b->chip[i].port)
			cx_setup_chip (b->chip + i);

	/* Set up DMA channel to master mode. */
	outb (DMA_MODE, (b->dma & 3) | DMA_MODE_MASTER);

	/* Enable DMA channel. */
	outb (DMA_MASK, b->dma & 3);

	/* Initialize all channels. */
	for (i=0; i<NCHAN; ++i)
		if (b->chan[i].type != T_NONE)
			cx_setup_chan (b->chan + i);
}

/*
 * Initialize the board.
 */
static void cx_setup_chip (cx_chip_t *c)
{
	/* Reset the chip. */
	cx_reset (c->port);

	/*
	 * Set all interrupt level registers to the same value.
	 * This enables the internal CD2400 priority scheme.
	 */
	outb (RPILR(c->port), BRD_INTR_LEVEL);
	outb (TPILR(c->port), BRD_INTR_LEVEL);
	outb (MPILR(c->port), BRD_INTR_LEVEL);

	/* Set bus error count to zero. */
	outb (BERCNT(c->port), 0);

	/* Set 16-bit DMA mode. */
	outb (DMR(c->port), 0);

	/* Set timer period register to 1 msec (approximately). */
	outb (TPR(c->port), 10);
}

/*
 * Initialize the CD2400 channel.
 */
void cx_setup_chan (cx_chan_t *c)
{
	unsigned short port = c->chip->port;
	int clock, period;

	if (c->num == 0) {
		c->board->bcr0 &= ~BCR0_UMASK;
		if (c->mode != M_ASYNC)
			c->board->bcr0 |= BCR0_UM_SYNC;
		if (c->board->if0type &&
		    (c->type==T_UNIV_RS449 || c->type==T_UNIV_V35))
			c->board->bcr0 |= BCR0_UI_RS449;
		outb (BCR0(c->board->port), c->board->bcr0);
	} else if (c->num == 8) {
		c->board->bcr0b &= ~BCR0_UMASK;
		if (c->mode != M_ASYNC)
			c->board->bcr0b |= BCR0_UM_SYNC;
		if (c->board->if8type &&
		    (c->type==T_UNIV_RS449 || c->type==T_UNIV_V35))
			c->board->bcr0b |= BCR0_UI_RS449;
		outb (BCR0(c->board->port+0x10), c->board->bcr0b);
	}

	/* set current channel number */
	outb (CAR(port), c->num & 3);

	/* reset the channel */
	cx_cmd (port, CCR_CLRCH);

	/* set LIVR to contain the board and channel numbers */
	outb (LIVR(port), c->board->num << 6 | c->num << 2);

	/* clear DTR, RTS, set TXCout/DTR pin */
	outb (MSVR_RTS(port), 0);
	outb (MSVR_DTR(port), c->mode==M_ASYNC ? 0 : MSV_TXCOUT);

	switch (c->mode) {	/* initialize the channel mode */
	case M_ASYNC:
		/* set receiver timeout register */
		outw (RTPR(port), 10);          /* 10 msec, see TPR */

		outb (CMR(port), CMR_RXDMA | CMR_TXDMA | CMR_ASYNC);
		outb (COR1(port), BYTE c->aopt.cor1);
		outb (COR2(port), BYTE c->aopt.cor2);
		outb (COR3(port), BYTE c->aopt.cor3);
		outb (COR6(port), BYTE c->aopt.cor6);
		outb (COR7(port), BYTE c->aopt.cor7);
		outb (SCHR1(port), c->aopt.schr1);
		outb (SCHR2(port), c->aopt.schr2);
		outb (SCHR3(port), c->aopt.schr3);
		outb (SCHR4(port), c->aopt.schr4);
		outb (SCRL(port), c->aopt.scrl);
		outb (SCRH(port), c->aopt.scrh);
		outb (LNXT(port), c->aopt.lnxt);
		break;
	case M_HDLC:
		outb (CMR(port), CMR_RXDMA | CMR_TXDMA | CMR_HDLC);
		outb (COR1(port), BYTE c->hopt.cor1);
		outb (COR2(port), BYTE c->hopt.cor2);
		outb (COR3(port), BYTE c->hopt.cor3);
		outb (RFAR1(port), c->hopt.rfar1);
		outb (RFAR2(port), c->hopt.rfar2);
		outb (RFAR3(port), c->hopt.rfar3);
		outb (RFAR4(port), c->hopt.rfar4);
		outb (CPSR(port), c->hopt.cpsr);
		break;
	case M_BISYNC:
		outb (CMR(port), CMR_RXDMA | CMR_TXDMA | CMR_BISYNC);
		outb (COR1(port), BYTE c->bopt.cor1);
		outb (COR2(port), BYTE c->bopt.cor2);
		outb (COR3(port), BYTE c->bopt.cor3);
		outb (COR6(port), BYTE c->bopt.cor6);
		outb (CPSR(port), c->bopt.cpsr);
		break;
	case M_X21:
		outb (CMR(port), CMR_RXDMA | CMR_TXDMA | CMR_X21);
		outb (COR1(port), BYTE c->xopt.cor1);
		outb (COR2(port), BYTE c->xopt.cor2);
		outb (COR3(port), BYTE c->xopt.cor3);
		outb (COR6(port), BYTE c->xopt.cor6);
		outb (SCHR1(port), c->xopt.schr1);
		outb (SCHR2(port), c->xopt.schr2);
		outb (SCHR3(port), c->xopt.schr3);
		break;
	}

	/* set mode-independent options */
	outb (COR4(port), BYTE c->opt.cor4);
	outb (COR5(port), BYTE c->opt.cor5);

	/* set up receiver clock values */
	if (c->mode == M_ASYNC || c->opt.rcor.dpll) {
		cx_clock (c->chip->oscfreq, c->rxbaud, &clock, &period);
		c->opt.rcor.clk = clock;
	} else {
		c->opt.rcor.clk = CLK_EXT;
		period = 1;
	}
	outb (RCOR(port), BYTE c->opt.rcor);
	outb (RBPR(port), period);

	/* set up transmitter clock values */
	if (c->mode == M_ASYNC || !c->opt.tcor.ext1x) {
		unsigned ext1x = c->opt.tcor.ext1x;
		c->opt.tcor.ext1x = 0;
		cx_clock (c->chip->oscfreq, c->txbaud, &clock, &period);
		c->opt.tcor.clk = clock;
		c->opt.tcor.ext1x = ext1x;
	} else {
		c->opt.tcor.clk = CLK_EXT;
		period = 1;
	}
	outb (TCOR(port), BYTE c->opt.tcor);
	outb (TBPR(port), period);

	/* set receiver A buffer physical address */
	c->arphys = vtophys (c->arbuf);
	outw (ARBADRU(port), (unsigned short) (c->arphys>>16));
	outw (ARBADRL(port), (unsigned short) c->arphys);

	/* set receiver B buffer physical address */
	c->brphys = vtophys (c->brbuf);
	outw (BRBADRU(port), (unsigned short) (c->brphys>>16));
	outw (BRBADRL(port), (unsigned short) c->brphys);

	/* set transmitter A buffer physical address */
	c->atphys = vtophys (c->atbuf);
	outw (ATBADRU(port), (unsigned short) (c->atphys>>16));
	outw (ATBADRL(port), (unsigned short) c->atphys);

	/* set transmitter B buffer physical address */
	c->btphys = vtophys (c->btbuf);
	outw (BTBADRU(port), (unsigned short) (c->btphys>>16));
	outw (BTBADRL(port), (unsigned short) c->btphys);

	c->dtr = 0;
	c->rts = 0;
}

/*
 * Control DTR signal for the channel.
 * Turn it on/off.
 */
void cx_chan_dtr (cx_chan_t *c, int on)
{
	c->dtr = on ? 1 : 0;

	if (c->mode == M_ASYNC) {
		outb (CAR(c->chip->port), c->num & 3);
		outb (MSVR_DTR(c->chip->port), on ? MSV_DTR : 0);
		return;
	}

	switch (c->num) {
	default:
		/* Channels 4..7 and 12..15 in syncronous mode
		 * have no DTR signal. */
		break;

	case 1: case 2:  case 3:
		if (c->type == T_UNIV_RS232)
			break;
	case 0:
		if (on)
			c->board->bcr1 |= 0x100 << c->num;
		else
			c->board->bcr1 &= ~(0x100 << c->num);
		outw (BCR1(c->board->port), c->board->bcr1);
		break;

	case 9: case 10: case 11:
		if (c->type == T_UNIV_RS232)
			break;
	case 8:
		if (on)
			c->board->bcr1b |= 0x100 << (c->num & 3);
		else
			c->board->bcr1b &= ~(0x100 << (c->num & 3));
		outw (BCR1(c->board->port+0x10), c->board->bcr1b);
		break;
	}
}

/*
 * Control RTS signal for the channel.
 * Turn it on/off.
 */
void
cx_chan_rts (cx_chan_t *c, int on)
{
	c->rts = on ? 1 : 0;
	outb (CAR(c->chip->port), c->num & 3);
	outb (MSVR_RTS(c->chip->port), on ? MSV_RTS : 0);
}


/*
 * Get the state of CARRIER signal of the channel.
 */
int
cx_chan_cd (cx_chan_t *c)
{
	unsigned char sigval;

	if (c->mode == M_ASYNC) {
		outb (CAR(c->chip->port), c->num & 3);
		return (inb (MSVR(c->chip->port)) & MSV_CD ? 1 : 0);
	}

	/*
	 * Channels 4..7 and 12..15 don't have CD signal available.
	 */
	switch (c->num) {
	default:
		return (1);

	case 1: case 2:  case 3:
		if (c->type == T_UNIV_RS232)
			return (1);
	case 0:
		sigval = inw (BSR(c->board->port)) >> 8;
		break;

	case 9: case 10: case 11:
		if (c->type == T_UNIV_RS232)
			return (1);
	case 8:
		sigval = inw (BSR(c->board->port+0x10)) >> 8;
		break;
	}
	return (~sigval >> 4 >> (c->num & 3) & 1);
}

/*
 * Compute CD2400 clock values.
 */
void cx_clock (long hz, long ba, int *clk, int *div)
{
	static short clocktab[] = { 8, 32, 128, 512, 2048, 0 };

	for (*clk=0; clocktab[*clk]; ++*clk) {
		long c = ba * clocktab[*clk];
		if (hz <= c*256) {
			*div = (2 * hz + c) / (2 * c) - 1;
			return;
		}
	}
	/* Incorrect baud rate.  Return some meaningful values. */
	*clk = 0;
	*div = 255;
}
