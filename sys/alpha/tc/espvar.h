/* $FreeBSD$ */
/*	$NetBSD: espvar.h,v 1.3.4.1 1996/09/10 17:28:18 cgd Exp $	*/

/*
 * Copyright (c) 1994 Peter Galbavy.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Peter Galbavy.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*#define ESP_DEBUG 0*/

#define FREQTOCCF(freq)	(((freq + 4) / 5))

/* esp revisions */
#define	ESP100		0x01
#define	ESP100A		0x02
#define	ESP200		0x03
#define	NCR53C94	0x04

/*
 * ECB. Holds additional information for each SCSI command Comments: We
 * need a separate scsi command block because we may need to overwrite it
 * with a request sense command.  Basicly, we refrain from fiddling with
 * the scsi_xfer struct (except do the expected updating of return values).
 * We'll generally update: xs->{flags,resid,error,sense,status} and
 * occasionally xs->retries.
 */
struct ecb {
	TAILQ_ENTRY(ecb) chain;
	struct scsi_xfer *xs;	/* SCSI xfer ctrl block from above */
	int		flags;	/* Status */
#define ECB_QNONE	0
#define ECB_QFREE	1
#define ECB_QREADY	2
#define ECB_QNEXUS	3
#define ECB_QBITS	0x07
#define ECB_CHKSENSE	0x08
#define ECB_ABORTED	0x10
#define ECB_SETQ(e, q)	do (e)->flags = ((e)->flags&~ECB_QBITS)|(q); while(0)
	struct scsi_generic cmd;  /* SCSI command block */
	int	 clen;
	char	*daddr;		/* Saved data pointer */
	int	 dleft;		/* Residue */
	struct	callout_handle timeout_ch;
	u_char 	 stat;		/* SCSI status byte */
#if ESP_DEBUG > 0
	char trace[1000];
#endif
};
#if ESP_DEBUG > 0
#define ECB_TRACE(ecb, msg, a, b) do { \
	const char *f = "[" msg "]"; \
	int n = strlen((ecb)->trace); \
	snprintf((ecb)->trace + n, sizeof((ecb)->trace) - n, f, a, b); \
} while(0)
#else
#define ECB_TRACE(ecb, msg, a, b)
#endif

/*
 * Some info about each (possible) target on the SCSI bus.  This should
 * probably have been a "per target+lunit" structure, but we'll leave it at
 * this for now.  Is there a way to reliably hook it up to sc->fordriver??
 */
struct esp_tinfo {
	int	cmds;		/* #commands processed */
	int	dconns;		/* #disconnects */
	int	touts;		/* #timeouts */
	int	perrs;		/* #parity errors */
	int	senses;		/* #request sense commands sent */
	ushort	lubusy;		/* What local units/subr. are busy? */
	u_char  flags;
#define T_NEED_TO_RESET	0x01	/* Should send a BUS_DEV_RESET */
#define T_NEGOTIATE	0x02	/* (Re)Negotiate synchronous options */
#define T_BUSY		0x04	/* Target is busy, i.e. cmd in progress */
#define T_SYNCMODE	0x08	/* sync mode has been negotiated */
#define T_SYNCHOFF	0x10	/* .. */
#define T_RSELECTOFF	0x20	/* .. */
	u_char  period;		/* Period suggestion */
	u_char  offset;		/* Offset suggestion */
} tinfo_t;

/* Register a linenumber (for debugging) */
#define LOGLINE(p)

#define ESP_SHOWECBS	0x01
#define ESP_SHOWINTS	0x02
#define ESP_SHOWCMDS	0x04
#define ESP_SHOWMISC	0x08
#define ESP_SHOWTRAC	0x10
#define ESP_SHOWSTART	0x20
#define ESP_SHOWPHASE	0x40
#define ESP_SHOWDMA	0x80
#define ESP_SHOWCCMDS	0x100
#define ESP_SHOWMSGS	0x200

#ifdef ESP_DEBUG
extern int esp_debug;
#define ESP_ECBS(str)  do {if (esp_debug & ESP_SHOWECBS) printf str;} while (0)
#define ESP_MISC(str)  do {if (esp_debug & ESP_SHOWMISC) printf str;} while (0)
#define ESP_INTS(str)  do {if (esp_debug & ESP_SHOWINTS) printf str;} while (0)
#define ESP_TRACE(str) do {if (esp_debug & ESP_SHOWTRAC) printf str;} while (0)
#define ESP_CMDS(str)  do {if (esp_debug & ESP_SHOWCMDS) printf str;} while (0)
#define ESP_START(str) do {if (esp_debug & ESP_SHOWSTART) printf str;}while (0)
#define ESP_PHASE(str) do {if (esp_debug & ESP_SHOWPHASE) printf str;}while (0)
#define ESP_DMA(str)   do {if (esp_debug & ESP_SHOWDMA) printf str;}while (0)
#define ESP_MSGS(str)  do {if (esp_debug & ESP_SHOWMSGS) printf str;}while (0)
#else
#define ESP_ECBS(str)
#define ESP_MISC(str)
#define ESP_INTS(str)
#define ESP_TRACE(str)
#define ESP_CMDS(str)
#define ESP_START(str)
#define ESP_PHASE(str)
#define ESP_DMA(str)
#define ESP_MSGS(str)
#endif

#define ESP_MAX_MSG_LEN 8

struct esp_softc {
	device_t sc_dev;			/* us as a device */
#ifdef SPARC_DRIVER
	struct sbusdev sc_sd;			/* sbus device */
	struct intrhand sc_ih;			/* intr handler */
#endif
	struct evcnt sc_intrcnt;		/* intr count */
	struct scsi_link sc_link;		/* scsi lint struct */
#ifdef SPARC_DRIVER
	volatile u_char *sc_reg;		/* the registers */
	struct dma_softc *sc_dma;		/* pointer to my dma */
#else
	volatile u_int32_t *sc_reg;		/* the registers */
	struct tcds_slotconfig *sc_dma;		/* DMA/slot info lives here. */
	void	*sc_cookie;			/* intr. handling cookie */
#endif

	/* register defaults */
	u_char	sc_cfg1;			/* Config 1 */
	u_char	sc_cfg2;			/* Config 2, not ESP100 */
	u_char	sc_cfg3;			/* Config 3, only ESP200 */
	u_char	sc_ccf;				/* Clock Conversion */
	u_char	sc_timeout;

	/* register copies, see espreadregs() */
	u_char	sc_espintr;
	u_char	sc_espstat;
	u_char	sc_espstep;
	u_char	sc_espfflags;

	/* Lists of command blocks */
	TAILQ_HEAD(ecb_list, ecb) free_list,
				  ready_list,
				  nexus_list;

	struct ecb *sc_nexus;			/* current command */
	struct ecb sc_ecb[8];			/* one per target */
	struct esp_tinfo sc_tinfo[8];

	/* Data about the current nexus (updated for every cmd switch) */
	caddr_t	sc_dp;				/* Current data pointer */
	ssize_t	sc_dleft;			/* Data left to transfer */

	/* Adapter state */
	int	sc_phase;		/* Copy of what bus phase we are in */
	int	sc_prevphase;		/* Copy of what bus phase we were in */
	u_char	sc_state;		/* State applicable to the adapter */
	u_char	sc_flags;
	u_char	sc_selid;
	u_char	sc_lastcmd;

	/* Message stuff */
	u_char	sc_msgpriq;	/* One or more messages to send (encoded) */
	u_char	sc_msgout;	/* What message is on its way out? */
	u_char	sc_omess[ESP_MAX_MSG_LEN];
	caddr_t	sc_omp;	/* Message pointer (for multibyte messages) */
	size_t	sc_omlen;
	u_char	sc_imess[ESP_MAX_MSG_LEN + 1];
	caddr_t	sc_imp;	/* Message pointer (for multibyte messages) */
	size_t	sc_imlen;

	/* hardware/openprom stuff */
	int sc_node;				/* PROM node ID */
	int sc_freq;				/* Freq in HZ */
#ifdef SPARC_DRIVER
	int sc_pri;				/* SBUS priority */
#endif
	int sc_id;				/* our scsi id */
	int sc_rev;				/* esp revision */
	int sc_minsync;				/* minimum sync period / 4 */
	int sc_maxxfer;				/* maximum transfer size */
};

/* values for sc_state */
#define ESP_IDLE	0x01	/* waiting for something to do */
#define ESP_TMP_UNAVAIL	0x02	/* Don't accept SCSI commands */
#define ESP_SELECTING	0x03	/* SCSI command is arbiting  */
#define ESP_RESELECTED	0x04	/* Has been reselected */
#define ESP_HASNEXUS	0x05	/* Actively using the SCSI bus */
#define ESP_CLEANING	0x06
#define ESP_SBR		0x07	/* Expect a SCSI RST because we commanded it */

/* values for sc_flags */
#define ESP_DROP_MSGI	0x01	/* Discard all msgs (parity err detected) */
#define ESP_DOINGDMA	0x02	/* The FIFO data path is active! */
#define ESP_BUSFREE_OK	0x04	/* Bus free phase is OK. */
#define ESP_SYNCHNEGO	0x08	/* Synch negotiation in progress. */
/*#define ESP_BLOCKED	0x10	* Don't schedule new scsi bus operations */
#define ESP_DISCON	0x10	/* Target sent DISCONNECT msg */
#define ESP_ABORTING	0x20	/* Bailing out */
#define ESP_ICCS	0x40	/* Expect status phase results */
#define ESP_WAITI	0x80	/* Waiting for non-DMA data to arrive */

/* values for sc_msgout */
#define SEND_DEV_RESET		0x01
#define SEND_PARITY_ERROR	0x02
#define SEND_ABORT		0x04
#define SEND_REJECT		0x08
#define SEND_INIT_DET_ERR	0x10
#define SEND_IDENTIFY  		0x20
#define SEND_SDTR		0x40

/* SCSI Status codes */
#define ST_GOOD			0x00
#define ST_CHKCOND		0x02
#define ST_CONDMET		0x04
#define ST_BUSY			0x08
#define ST_INTERMED		0x10
#define ST_INTERMED_CONDMET	0x14
#define ST_RESERVATION_CONFLICT	0x18
#define ST_CMD_TERM		0x22
#define ST_QUEUE_FULL		0x28

#define ST_MASK			0x3e /* bit 0,6,7 is reserved */

/* phase bits */
#define IOI			0x01
#define CDI			0x02
#define MSGI			0x04

/* Information transfer phases */
#define DATA_OUT_PHASE		(0)
#define DATA_IN_PHASE		(IOI)
#define COMMAND_PHASE		(CDI)
#define STATUS_PHASE		(CDI|IOI)
#define MESSAGE_OUT_PHASE	(MSGI|CDI)
#define MESSAGE_IN_PHASE	(MSGI|CDI|IOI)

#define PHASE_MASK		(MSGI|CDI|IOI)

/* Some pseudo phases for getphase()*/
#define BUSFREE_PHASE		0x100	/* Re/Selection no longer valid */
#define INVALID_PHASE		0x101	/* Re/Selection valid, but no REQ yet */
#define PSEUDO_PHASE		0x100	/* "pseudo" bit */

static u_char ESP_READ_REG(struct esp_softc *, int);

#if 1
static __inline u_char
ESP_READ_REG(sc, reg)
	struct esp_softc *sc;
	int reg;
{
	u_char v;

	v = sc->sc_reg[reg * 2] & 0xff;
	alpha_mb();
	return v;
}
#else
#define	ESP_READ_REG(sc, reg)			\
	((u_char)((sc)->sc_reg[(reg) * 2] & 0xff))
#endif
#define	ESP_WRITE_REG(sc, reg, val)		\
	do {					\
		u_char v = (val);		\
		(sc)->sc_reg[(reg) * 2] = v;	\
		alpha_mb();			\
	} while (0)

#ifdef ESP_DEBUG
#define	ESPCMD(sc, cmd) do {				\
	if (esp_debug & ESP_SHOWCCMDS)			\
		printf("<cmd:0x%x>", (unsigned)cmd);	\
	sc->sc_lastcmd = cmd;				\
	ESP_WRITE_REG(sc, ESP_CMD, cmd);		\
} while (0)
#else
#define	ESPCMD(sc, cmd)		ESP_WRITE_REG(sc, ESP_CMD, cmd)
#endif

#define SAME_ESP(unit, bp, ca) \
	((bp->val[0] == ca->ca_slot && bp->val[1] == ca->ca_offset) || \
	 (bp->val[0] == -1 && bp->val[1] == unit))

#ifndef SPARC_DRIVER
/* DMA macros for ESP */
#define	DMA_ISINTR(sc)		tcds_dma_isintr(sc)
#define	DMA_RESET(sc)		tcds_dma_reset(sc)
#define	DMA_INTR(sc)		tcds_dma_intr(sc)
#define	DMA_SETUP(sc, addr, len, datain, dmasize) \
				tcds_dma_setup(sc, addr, len, datain, dmasize)
#define	DMA_GO(sc)		tcds_dma_go(sc)
#define	DMA_ISACTIVE(sc)	tcds_dma_isactive(sc)
#endif
