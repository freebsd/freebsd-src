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
 *---------------------------------------------------------------------------*
 *
 *	i4b_l1.h - isdn4bsd layer 1 header file
 *	---------------------------------------
 *
 * $FreeBSD$ 
 *
 *      last edit-date: [Mon Jul  5 15:32:02 1999]
 *
 *---------------------------------------------------------------------------*/

#ifndef I4B_L1_H_
#define I4B_L1_H_

#ifdef __bsdi__
#include <sys/device.h>		/* XXX */
#ifndef ISA_NPORT_CHECK		/* Double yuck XXX */
#include <i386/isa/isavar.h>	/* XXX */
#endif
#endif

#include <i4b/include/i4b_l3l4.h>

/*---------------------------------------------------------------------------
 *	kernel config file flags definition
 *---------------------------------------------------------------------------*/
#define FLAG_TELES_S0_8		1
#define FLAG_TELES_S0_16	2
#define FLAG_TELES_S0_163	3
#define FLAG_AVM_A1		4
#define FLAG_TELES_S0_163_PnP	5
#define FLAG_CREATIX_S0_PnP	6
#define FLAG_USR_ISDN_TA_INT	7
#define FLAG_DRN_NGO		8
#define FLAG_SWS		9
#define FLAG_AVM_A1_PCMCIA	10
#define FLAG_DYNALINK		11
#define FLAG_BLMASTER		12
#define FLAG_ELSA_QS1P_ISA	13
#define FLAG_ELSA_QS1P_PCI	14
#define FLAG_SIEMENS_ITALK	15
#define	FLAG_ELSA_MLIMC		16
#define	FLAG_ELSA_MLMCALL	17
#define FLAG_ITK_IX1		18
#define FLAG_AVMA1PCI     	19
#define FLAG_ELSA_PCC16		20
#define FLAG_AVM_PNP		21
#define FLAG_SIEMENS_ISURF2	22
#define FLAG_ASUSCOM_IPAC	23

#define SEC_DELAY		1000000	/* one second DELAY for DELAY*/

#define MAX_DFRAME_LEN		264	/* max length of a D frame */

#ifndef __bsdi__
#define min(a,b)		((a)<(b)?(a):(b))
#endif

#if !defined(__FreeBSD__) && !defined(__bsdi__)
/* We try to map as few as possible as small as possible io and/or
   memory regions. Each card defines its own interpretation of this
   mapping array. At probe time we have a fixed size array, later
   (when the card type is known) we allocate a minimal array
   dynamically. */

#define	ISIC_MAX_IO_MAPS	49	/* no cardtype needs more yet */

/* one entry in mapping array */
struct isic_io_map {
	bus_space_tag_t t;	/* which bus-space is this? */
	bus_space_handle_t h;	/* handle of mapped bus space region */
	bus_size_t offset;	/* offset into region */
	bus_size_t size;	/* size of region, zero if not ours 
				   (i.e.: don't ever unmap it!) */
};

/* this is passed around at probe time (no struct isic_softc yet) */
struct isic_attach_args {
	int ia_flags;			/* flags from config file */
	int ia_num_mappings;		/* number of io mappings provided */
	struct isic_io_map ia_maps[ISIC_MAX_IO_MAPS];
};
#endif

#ifdef __FreeBSD__
extern int next_isic_unit;
#endif

/*---------------------------------------------------------------------------*
 *	isic_Bchan: the state of one B channel
 *---------------------------------------------------------------------------*/
typedef struct
{
	int		unit;		/* cards unit number	*/
	int		channel;	/* which channel is this*/
	
#if defined(__FreeBSD__) || defined(__bsdi__)
	caddr_t		hscx;		/* HSCX address		*/
#endif

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
	
	isdn_link_t	isdn_linktab;	/* b channel addresses	*/
	drvr_link_t	*drvr_linktab;	/* ptr to driver linktab*/

	/* statistics */

	/* RSTA */
	
	int		stat_VFR;	/* HSCX RSTA Valid FRame */
	int		stat_RDO;	/* HSCX RSTA Rx Data Overflow */	
	int		stat_CRC;	/* HSCX RSTA CRC */
	int		stat_RAB;	/* HSCX RSTA Rx message ABorted */

	/* EXIR */

	int		stat_XDU;	/* HSCX EXIR tx data underrun */
	int		stat_RFO;	/* HSCX EXIR rx frame overflow */
	
} isic_Bchan_t;

/*---------------------------------------------------------------------------*
 *	isic_softc: the state of the layer 1 of the D channel
 *---------------------------------------------------------------------------*/
struct isic_softc
{
#if !defined(__FreeBSD__)
	/* We are inherited from this class. All drivers must have this
	   as their first entry in struct softc. */
	struct device	sc_dev;
#endif

	int		sc_unit;	/* unit number		*/
	int		sc_irq;		/* interrupt vector	*/

#ifdef __FreeBSD__
	int		sc_port;	/* port base address	*/
#elif defined(__bsdi__)
	struct isadev	sc_id;		/* ISA/PCI device */
	struct intrhand	sc_ih;		/* interrupt vectoring */
	int		sc_flags;
	int		sc_port;
	caddr_t		sc_maddr;
	int		sc_abustype;	/* PCI, ISA etcetera */
#else
	u_int		sc_maddr;	/* "memory address" for card config register */
	int sc_num_mappings;		/* number of io mappings provided */
	struct isic_io_map *sc_maps;

#define	MALLOC_MAPS(sc)	\
	(sc)->sc_maps = (struct isic_io_map*)malloc(sizeof((sc)->sc_maps[0])*(sc)->sc_num_mappings, M_DEVBUF, 0)
#endif

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
	
#if defined(__FreeBSD__) || defined(__bsdi__)
	caddr_t		sc_vmem_addr;	/* card RAM virtual memory base */
	caddr_t		sc_isac;	/* ISAC port base addr	*/
#define ISAC_BASE	(sc->sc_isac)

	caddr_t		sc_ipacbase;	/* IPAC port base addr	*/
#define IPAC_BASE	(sc->sc_ipacbase)
#endif

	u_char		sc_isac_mask;	/* ISAC IRQ mask	*/
#define ISAC_IMASK	(sc->sc_isac_mask)

	isic_Bchan_t	sc_chan[2];	/* B-channel state	*/
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

	int		sc_I430state;	/* I.430 state F3 .... F8 */

	int		sc_I430T3;	/* I.430 Timer T3 running */	
#if defined(__FreeBSD_version) && __FreeBSD_version >= 300001
	struct callout_handle sc_T3_callout;
#endif
	
	int		sc_I430T4;	/* Timer T4 running */	

#if defined(__FreeBSD__) && __FreeBSD__ >=3
	struct callout_handle sc_T4_callout;
#endif

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
	
#if defined(__FreeBSD__) || defined(__bsdi__)

	u_char		(*readreg)(u_char *, u_int);
	void		(*writereg)(u_char *, u_int, u_int);
	void		(*readfifo)(void *, const void *, size_t);
	void		(*writefifo)(void *, const void *, size_t);
	void		(*clearirq)(void *);

#define	ISAC_READ(r)		(*sc->readreg)(ISAC_BASE, (r))
#define	ISAC_WRITE(r,v)		(*sc->writereg)(ISAC_BASE, (r), (v));
#define	ISAC_RDFIFO(b,s)	(*sc->readfifo)((b), ISAC_BASE, (s))
#define	ISAC_WRFIFO(b,s)	(*sc->writefifo)(ISAC_BASE, (b), (s))

#define	HSCX_READ(n,r)		(*sc->readreg)(sc->sc_chan[(n)].hscx, (r))
#define	HSCX_WRITE(n,r,v)	(*sc->writereg)(sc->sc_chan[(n)].hscx, (r), (v))
#define	HSCX_RDFIFO(n,b,s)	(*sc->readfifo)((b), sc->sc_chan[(n)].hscx, (s))
#define	HSCX_WRFIFO(n,b,s)	(*sc->writefifo)(sc->sc_chan[(n)].hscx, (b), (s))

#define	IPAC_READ(r)		(*sc->readreg)(IPAC_BASE, (r))
#define	IPAC_WRITE(r,v)		(*sc->writereg)(IPAC_BASE, (r), (v));

#else	/* ! __FreeBSD__ */

#define	ISIC_WHAT_ISAC	0
#define	ISIC_WHAT_HSCXA	1
#define	ISIC_WHAT_HSCXB	2
#define	ISIC_WHAT_IPAC	3

	u_int8_t	(*readreg) __P((struct isic_softc *sc, int what, bus_size_t offs));
	void		(*writereg) __P((struct isic_softc *sc, int what, bus_size_t offs, u_int8_t data));
	void		(*readfifo) __P((struct isic_softc *sc, int what, void *buf, size_t size));
	void		(*writefifo) __P((struct isic_softc *sc, int what, const void *data, size_t size));
	void		(*clearirq) __P((struct isic_softc *sc));

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

#endif	/* __FreeBSD__ */
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

#ifdef __FreeBSD__

extern struct isic_softc isic_sc[];

extern void isic_recover(struct isic_softc *sc);
extern int isic_realattach(struct isa_device *dev, unsigned int iobase2);
extern int isic_attach_avma1 ( struct isa_device *dev );
extern int isic_attach_fritzpcmcia ( struct isa_device *dev );
extern int isic_attach_Cs0P ( struct isa_device *dev, unsigned int iobase2);
extern int isic_attach_Dyn ( struct isa_device *dev, unsigned int iobase2);
extern int isic_attach_s016 ( struct isa_device *dev );
extern int isic_attach_s0163 ( struct isa_device *dev );
extern int isic_attach_s0163P ( struct isa_device *dev, unsigned int iobase2);
extern int isic_attach_s08 ( struct isa_device *dev );
extern int isic_attach_usrtai ( struct isa_device *dev );
extern int isic_attach_itkix1 ( struct isa_device *dev );
extern int isic_attach_drnngo ( struct isa_device *dev, unsigned int iobase2);
extern int isic_attach_sws ( struct isa_device *dev );
extern int isic_attach_Eqs1pi(struct isa_device *dev, unsigned int iobase2);
extern int isic_attach_avm_pnp(struct isa_device *dev, unsigned int iobase2);
extern int isic_attach_siemens_isurf(struct isa_device *dev, unsigned int iobase2);
extern int isic_attach_Eqs1pp(int unit, unsigned int iobase1, unsigned int iobase2);
extern int isic_attach_asi(struct isa_device *dev, unsigned int iobase2);
extern void isic_bchannel_setup (int unit, int hscx_channel, int bprot, int activate );
extern int isic_hscx_fifo(isic_Bchan_t *, struct isic_softc *);
extern void isic_hscx_init ( struct isic_softc *sc, int hscx_channel, int activate );
extern void isic_hscx_irq ( struct isic_softc *sc, u_char ista, int hscx_channel, u_char ex_irq );
extern int isic_hscx_silence ( unsigned char *data, int len );
extern void isic_hscx_cmd( struct isic_softc *sc, int h_chan, unsigned char cmd );
extern void isic_hscx_waitxfw( struct isic_softc *sc, int h_chan );
extern void isic_init_linktab ( struct isic_softc *sc );
extern int isic_isac_init ( struct isic_softc *sc );
extern void isic_isac_irq ( struct isic_softc *sc, int r );
extern void isic_isac_l1_cmd ( struct isic_softc *sc, int command );
extern void isic_next_state ( struct isic_softc *sc, int event );
extern char *isic_printstate ( struct isic_softc *sc );
extern int isic_probe_avma1 ( struct isa_device *dev );
extern int isic_probe_avma1_pcmcia ( struct isa_device *dev );
extern int isic_probe_avm_pnp ( struct isa_device *dev, unsigned int iobase2);
extern int isic_probe_siemens_isurf ( struct isa_device *dev, unsigned int iobase2);
extern int isic_probe_Cs0P ( struct isa_device *dev, unsigned int iobase2);
extern int isic_probe_Dyn ( struct isa_device *dev, unsigned int iobase2);
extern int isic_probe_s016 ( struct isa_device *dev );
extern int isic_probe_s0163 ( struct isa_device *dev );
extern int isic_probe_s0163P ( struct isa_device *dev, unsigned int iobase2);
extern int isic_probe_s08 ( struct isa_device *dev );
extern int isic_probe_usrtai ( struct isa_device *dev );
extern int isic_probe_itkix1 ( struct isa_device *dev );
extern int isic_probe_drnngo ( struct isa_device *dev, unsigned int iobase2);
extern int isic_probe_sws ( struct isa_device *dev );
extern int isic_probe_Eqs1pi(struct isa_device *dev, unsigned int iobase2);
extern int isic_probe_asi(struct isa_device *dev, unsigned int iobase2);

#elif defined(__bsdi__)

extern struct isic_softc *isic_sc[];
#define isic_find_sc(unit)	(isic_sc[(unit)])

#define ATTACHARGS struct device *, struct device *, struct isa_attach_args *
#define MATCHARGS struct device *, struct cfdata *, struct isa_attach_args *
extern int isa_isicmatch(MATCHARGS);
extern int isa_isicattach(ATTACHARGS);
extern int isicintr(void *);
extern void isic_recover(struct isic_softc *sc);
extern int isic_realattach(ATTACHARGS);
extern int isic_attach_avma1(ATTACHARGS);
extern int isic_attach_fritzpcmcia(ATTACHARGS);
extern int isic_attach_Cs0P(ATTACHARGS);
extern int isic_attach_Dyn(ATTACHARGS);
extern int isic_attach_s016(ATTACHARGS);
extern int isic_attach_s0163(ATTACHARGS);
extern int isic_attach_s0163P(ATTACHARGS);
extern int isic_attach_s08(ATTACHARGS);
extern int isic_attach_usrtai(ATTACHARGS);
extern int isic_attach_itkix1(ATTACHARGS);
extern int isic_attach_drnngo(ATTACHARGS);
extern int isic_attach_sws(ATTACHARGS);
extern int isic_attach_Eqs1pi(ATTACHARGS);
extern int isic_attach_Eqs1pp(ATTACHARGS);
extern void isic_bchannel_setup(int unit, int hscx_channel, int bprot, int activate );
extern void isic_hscx_init(struct isic_softc *sc, int hscx_channel, int activate );
extern void isic_hscx_irq(struct isic_softc *sc, u_char ista, int hscx_channel, u_char ex_irq );
extern int isic_hscx_silence(unsigned char *data, int len );
extern void isic_hscx_cmd(struct isic_softc *sc, int h_chan, unsigned char cmd );
extern void isic_hscx_waitxfw(struct isic_softc *sc, int h_chan );
extern void isic_init_linktab(struct isic_softc *sc );
extern int isic_isac_init(struct isic_softc *sc );
extern void isic_isac_irq(struct isic_softc *sc, int r );
extern void isic_isac_l1_cmd(struct isic_softc *sc, int command );
extern void isic_next_state(struct isic_softc *sc, int event );
extern char *isic_printstate(struct isic_softc *sc );
extern int isic_probe_avma1(MATCHARGS);
extern int isic_probe_avma1_pcmcia(MATCHARGS);
extern int isic_probe_Cs0P(MATCHARGS);
extern int isic_probe_Dyn(MATCHARGS);
extern int isic_probe_s016(MATCHARGS);
extern int isic_probe_s0163(MATCHARGS);
extern int isic_probe_s0163P(MATCHARGS);
extern int isic_probe_s08(MATCHARGS);
extern int isic_probe_usrtai(MATCHARGS);
extern int isic_probe_itkix1(MATCHARGS);
extern int isic_probe_drnngo(MATCHARGS);
extern int isic_probe_sws(MATCHARGS);
extern int isic_probe_Eqs1pi(MATCHARGS);

#undef MATCHARGS
#undef ATTACHARGS
#else /* not FreeBSD/__bsdi__ */

extern void isic_recover __P((struct isic_softc *sc));
extern int isicattach __P((int flags, struct isic_softc *sc));
extern int isicintr __P((void *));
extern int isicprobe __P((struct isic_attach_args *ia));
extern int isic_attach_avma1 __P((struct isic_softc *sc));
extern int isic_attach_s016 __P((struct isic_softc *sc));
extern int isic_attach_s0163 __P((struct isic_softc *sc));
extern int isic_attach_s08 __P((struct isic_softc *sc));
extern int isic_attach_usrtai __P((struct isic_softc *sc));
extern int isic_attach_itkix1 __P((struct isic_softc *sc));
extern void isic_bchannel_setup __P((int unit, int hscx_channel, int bprot, int activate));
extern void isic_hscx_init __P((struct isic_softc *sc, int hscx_channel, int activate));
extern void isic_hscx_irq __P((struct isic_softc *sc, u_char ista, int hscx_channel, u_char ex_irq));
extern int isic_hscx_silence __P(( unsigned char *data, int len ));
extern void isic_hscx_cmd __P(( struct isic_softc *sc, int h_chan, unsigned char cmd ));
extern void isic_hscx_waitxfw __P(( struct isic_softc *sc, int h_chan ));
extern void isic_init_linktab __P((struct isic_softc *sc));
extern int isic_isac_init __P((struct isic_softc *sc));
extern void isic_isac_irq __P((struct isic_softc *sc, int r));
extern void isic_isac_l1_cmd __P((struct isic_softc *sc, int command));
extern void isic_next_state __P((struct isic_softc *sc, int event));
extern char * isic_printstate __P((struct isic_softc *sc));
extern int isic_probe_avma1 __P((struct isic_attach_args *ia));
extern int isic_probe_s016 __P((struct isic_attach_args *ia));
extern int isic_probe_s0163 __P((struct isic_attach_args *ia));
extern int isic_probe_s08 __P((struct isic_attach_args *ia));
extern int isic_probe_usrtai __P((struct isic_attach_args *ia));
extern int isic_probe_itkix1 __P((struct isic_attach_args *ia));

extern struct isic_softc *isic_sc[];

#define isic_find_sc(unit)	(isic_sc[(unit)])

#endif /*  __FreeBSD__ */

#endif /* I4B_L1_H_ */
