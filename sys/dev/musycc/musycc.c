/*
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <phk@FreeBSD.org> wrote this file.  As long as you retain this notice you
 * can do whatever you want with this stuff. If we meet some day, and you think
 * this stuff is worth it, you can buy me a beer in return.   Poul-Henning Kamp
 * ----------------------------------------------------------------------------
 *
 * $FreeBSD$
 *
 *
 *
 * Card state machine:
 * -------------------
 *
 * This is the state engine which drives the card "as such" which in reality
 * means the MUSYCC chip.
 *
 *  State	Description
 *
 *  IDLE	The card is in this state when no channels are configured.
 *		This is the state we leave the card in after _attach()
 *
 *  INIT	The card is being initialized
 *
 *  RUNNING	The card is running
 *
 *  FAULT	The card is hosed and being reset
 *
 *      ------------------
 *     /                  \
 *    v                    |
 *  IDLE ---> INIT ---> RUNNING
 *                       ^   |
 *                       |   |
 *                       |   v
 *                       FAULT
 *
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/kernel.h>
#include <sys/sysctl.h>
#include <sys/malloc.h>
#include <sys/bus.h>
#include <sys/mbuf.h>
#include <sys/queue.h>
#include <machine/bus.h>
#include <machine/clock.h>
#include <sys/rman.h>
#include <machine/resource.h>
#include <pci/pcireg.h>
#include <pci/pcivar.h>
#include "pci_if.h"


#include <netgraph/ng_message.h>
#include <netgraph/netgraph.h>  

#include <vm/vm.h>
#include <vm/pmap.h>

static MALLOC_DEFINE(M_MUSYCC, "musycc", "MUSYCC related");

static int maxlatency = 250;
SYSCTL_INT(_debug, OID_AUTO, musycc_maxlatency, CTLFLAG_RW, &maxlatency, 0,
	"The number of milliseconds a packet is allowed to spend in the output queue.  "
	"If the output queue is longer than this number of milliseconds when the packet "
	"arrives for output, the packet will be dropped."
);

static int debug = 0;
SYSCTL_INT(_debug, OID_AUTO, musycc_debug, CTLFLAG_RW, &debug, 0, "");

struct softc;
static void init_8370(struct softc *sc);
static	u_int32_t parse_ts(const char *s, int *nbit);

/*
 * Device driver initialization stuff
 */

static devclass_t musycc_devclass;

/* XXX: Notice, these babies must be aligned to 2k boundaries [5-7] */
struct groupr {
	u_int32_t	thp[32];   /* Transmit Head Pointer [5-29]	     */
	u_int32_t	tmp[32];   /* Transmit Message Pointer [5-30]	     */
	u_int32_t	rhp[32];   /* Receive Head Pointer [5-29]	     */
	u_int32_t	rmp[32];   /* Receive Message Pointer [5-30]	     */
	u_int8_t	ttsm[128]; /* Time Slot Map [5-22]		     */
	u_int8_t	tscm[256]; /* Subchannel Map [5-24]		     */
	u_int32_t	tcct[32];  /* Channel Configuration [5-26]	     */
	u_int8_t	rtsm[128]; /* Time Slot Map [5-22] 		     */
	u_int8_t	rscm[256]; /* Subchannel Map [5-24]		     */
	u_int32_t	rcct[32];  /* Channel Configuration [5-26]           */
	u_int32_t	__glcd;	   /* Global Configuration Descriptor [5-10] */
	u_int32_t	__iqp;	   /* Interrupt Queue Pointer [5-36]	     */
	u_int32_t	__iql;	   /* Interrupt Queue Length [5-36]	     */
	u_int32_t	grcd;	   /* Group Configuration Descriptor [5-16]  */
	u_int32_t	mpd;	   /* Memory Protection Descriptor [5-18]    */
	u_int32_t	mld;	   /* Message Length Descriptor [5-20]       */
	u_int32_t	pcd;	   /* Port Configuration Descriptor [5-19]   */
	u_int32_t	__rbist;   /* Receive BIST status [5-4]              */
	u_int32_t	__tbist;   /* Receive BIST status [5-4]              */
};

struct globalr {
	u_int32_t	gbp;	   /* Group Base Pointer */
	u_int32_t	dacbp;	   /* Dual Address Cycle Base Pointer */
	u_int32_t	srd;	   /* Service Request Descriptor */
	u_int32_t	isd;	   /* Interrupt Service Descriptor */
	u_int32_t	__thp[28];   /* Transmit Head Pointer [5-29]	     */
	u_int32_t	__tmp[32];   /* Transmit Message Pointer [5-30]	     */
	u_int32_t	__rhp[32];   /* Receive Head Pointer [5-29]	     */
	u_int32_t	__rmp[32];   /* Receive Message Pointer [5-30]	     */
	u_int8_t	ttsm[128]; /* Time Slot Map [5-22]		     */
	u_int8_t	tscm[256]; /* Subchannel Map [5-24]		     */
	u_int32_t	tcct[32];  /* Channel Configuration [5-26]	     */
	u_int8_t	rtsm[128]; /* Time Slot Map [5-22] 		     */
	u_int8_t	rscm[256]; /* Subchannel Map [5-24]		     */
	u_int32_t	rcct[32];  /* Channel Configuration [5-26]           */
	u_int32_t	glcd;	   /* Global Configuration Descriptor [5-10] */
	u_int32_t	iqp;	   /* Interrupt Queue Pointer [5-36]	     */
	u_int32_t	iql;	   /* Interrupt Queue Length [5-36]	     */
	u_int32_t	grcd;	   /* Group Configuration Descriptor [5-16]  */
	u_int32_t	mpd;	   /* Memory Protection Descriptor [5-18]    */
	u_int32_t	mld;	   /* Message Length Descriptor [5-20]       */
	u_int32_t	pcd;	   /* Port Configuration Descriptor [5-19]   */
	u_int32_t	rbist;   /* Receive BIST status [5-4]              */
	u_int32_t	tbist;   /* Receive BIST status [5-4]              */
};

/*
 * Because the chan_group must be 2k aligned we create this super 
 * structure so we can use the remaining 476 bytes for something useful
 */

struct mycg {
	struct groupr	cg;
};

struct mdesc {
	u_int32_t	status;
	u_int32_t	data;
	u_int32_t	next;
	/* Software only */
	struct mbuf	*m;
	struct mdesc	*snext;
};

#define NPORT	8

#define NHDLC	32

#define NIQD	32

struct softc;

struct schan {
	enum {DOWN, UP} state;
	struct softc	*sc;
	int		chan;
	u_int32_t	ts;
	char		hookname[8];

	hook_p		hook;

	u_long		rx_drop;	/* mbuf allocation failures */
	u_long		tx_limit;
	u_long		tx_pending;
	struct mdesc	*tx_next_md;	/* next MD */
	struct mdesc	*tx_last_md;	/* last MD */
	int		rx_last_md;	/* index to next MD */
	int		nmd;		/* count of MD's. */

	time_t		last_recv;
	time_t		last_rdrop;
	time_t		last_rxerr;
	u_long		crc_error;
	u_long		dribble_error;
	u_long		long_error;
	u_long		abort_error;
	u_long		short_error;
	u_long		txn, rxn;

	time_t		last_xmit;
	time_t		last_txerr;

	time_t		last_txdrop;
	u_long		tx_drop;

#if 0


	u_long		rx_error;

	u_long		overflow_error;

	int		last_error;
	int		prev_error;

#endif
};

enum framing {WHOKNOWS, E1, E1U, T1, T1U};
enum clocksource {EXT, INT};

struct softc {
	enum framing framing;
	enum clocksource clocksource;
	int nhooks;
	u_int32_t last;
	struct csoftc *csc;
	u_int32_t *ds8370;
	void	*ds847x;
	struct globalr *reg;
	struct groupr *ram;
	struct mycg *mycg;
	struct mdesc *mdt[NHDLC];
	struct mdesc *mdr[NHDLC];
	node_p node;			/* NG node */
	char nodename[NG_NODELEN + 1];	/* NG nodename */
	struct schan *chan[NHDLC];
	u_long		cnt_ferr;
	u_long		cnt_cerr;
	u_long		cnt_lcv;
	u_long		cnt_febe;
	u_long		cnt_berr;
	u_long		cnt_fred;
	u_long		cnt_cofa;
	u_long		cnt_sef;
};

/*
 * SoftC for the entire card.
 */

struct csoftc {
	enum { C_IDLE, C_INIT, C_RUNNING, C_FAULT } state;

	int	unit, bus, slot;
	LIST_ENTRY(csoftc) list;

	device_t f[2];
	struct resource *irq[2];
	void *intrhand[2];
	vm_offset_t physbase[2];
	u_char *virbase[2];

	u_int creg, *cregp;
	int nchan;
	struct softc serial[NPORT];

	struct globalr *reg;
	struct globalr *ram;
	u_int32_t iqd[NIQD];
};

/*
 *
 */

#define NG_NODETYPE	"lmc1504"

static  ng_constructor_t musycc_constructor;
static  ng_rcvmsg_t musycc_rcvmsg;
static  ng_shutdown_t musycc_shutdown;
static  ng_newhook_t musycc_newhook;
static  ng_connect_t musycc_connect;
static  ng_rcvdata_t musycc_rcvdata;
static  ng_disconnect_t musycc_disconnect;

static struct ng_type ngtypestruct = {
	NG_VERSION,
	NG_NODETYPE,
	NULL, 
	musycc_constructor,
	musycc_rcvmsg,
	musycc_shutdown,
	musycc_newhook,
	NULL,
	musycc_connect,
	musycc_rcvdata,
	musycc_rcvdata,
	musycc_disconnect,
	NULL
};

/*
 *
 */

static u_int32_t
parse_ts(const char *s, int *nbit)
{
	unsigned r;
	int i, j;
	char *p;

	r = 0;
	j = -1;
	*nbit = 0;
	while(*s) {
		i = strtol(s, &p, 0);
		if (i < 0 || i > 31)
			return (0);
		while (j != -1 && j < i) {
			r |= 1 << j++;
			(*nbit)++;
		}
		j = -1;
		r |= 1 << i;
		(*nbit)++;
		if (*p == ',') {
			s = p + 1;
			continue;
		} else if (*p == '-') {
			j = i + 1;
			s = p + 1;
			continue;
		} else if (!*p) {
			break;
		} else {
			return (0);
		}
	}
	return (r);
}

/*
 *
 */


static LIST_HEAD(, csoftc) sc_list = LIST_HEAD_INITIALIZER(&sc_list);

static void
poke_847x(void *dummy)
{
	static int count;
	int i;
	struct csoftc *csc;

	timeout(poke_847x, NULL, 1);
	LIST_FOREACH(csc, &sc_list, list)  {
		count++;
		i = (csc->creg >> 24 & 0xf);
		csc->creg &= ~0xf000000;
		i++;
		csc->creg |= (i & 0xf) << 24;
		*csc->cregp = csc->creg;
#if 0
		for (i = 0; i < sc->nchan; i++) {
			if (sc->serial[i].last == 0xffffffff) {
				sc->serial[i].reg->srd = 0;
				sc->serial[i].last = 0;
				return;
			}
		}
#endif
	}
}

static void
init_card(struct csoftc *csc)
{

	printf("init_card(%p)\n", csc);

	csc->state = C_INIT;
	csc->reg->srd = 0x100;
	tsleep(csc, PZERO | PCATCH, "icard", hz / 10);
	csc->reg->gbp = vtophys(csc->ram);
	csc->ram->glcd = 0x3f30;	/* XXX: designer magic */
	
	csc->ram->iqp = vtophys(csc->iqd);
	csc->ram->iql = NIQD - 1;
	csc->ram->dacbp = 0;		/* 32bit only */

	csc->reg->srd = csc->serial[0].last = 0x400;
	tsleep(&csc->serial[0].last, PZERO + PCATCH, "con1", hz);
	timeout(poke_847x, NULL, 1);
	csc->state = C_RUNNING;
}

static void
init_ctrl(struct softc *sc)
{
	int i;

	printf("init_ctrl(%p) [%s] [%08x]\n", sc, sc->nodename, sc->csc->reg->glcd);
	init_8370(sc);
	tsleep(sc, PZERO | PCATCH, "ds8370", hz);
	printf("%s: glcd: [%08x]\n", sc->nodename, sc->csc->reg->glcd);
	sc->reg->gbp = vtophys(sc->ram);
	sc->ram->grcd =  0x00000001;	/* RXENBL */
	sc->ram->grcd |= 0x00000002;	/* TXENBL */
	sc->ram->grcd |= 0x00000004;	/* SUBDSBL */
	if (sc->framing == E1 || sc->framing == T1)
		sc->ram->grcd |= 0x00000008;	/* OOFABT */
	else
		sc->ram->grcd |= 0x00000000;	/* !OOFABT */

	sc->ram->grcd |= 0x00000020;	/* MSKCOFA */

	sc->ram->grcd |= 0x00000440;	/* POLLTH=1 */

	sc->ram->mpd = 0;		/* Memory Protection NI [5-18] */

	sc->ram->pcd =  0x0000001;	/* PORTMD=1 (E1/32ts) */
	sc->ram->pcd |= 1 << 5;		/* TSYNC_EDGE */
	sc->ram->pcd |= 1 << 9;		/* TRITX */

	/* Message length descriptor */
	/* XXX: MTU */
	sc->ram->mld = 1600;
	sc->ram->mld |= (1600 << 16);

	for (i = 0; i < NHDLC; i++) {
		sc->ram->ttsm[i] = 0;
		sc->ram->rtsm[i] = 0;
	}
	sc->reg->srd = sc->last = 0x500;
	tsleep(&sc->last, PZERO + PCATCH, "con1", hz);
	sc->reg->srd = sc->last = 0x520;
	tsleep(&sc->last, PZERO + PCATCH, "con1", hz);
}

/*
 *
 */

static void
status_chans(struct softc *sc, char *s)
{
	int i;
	struct schan *scp;

	s += strlen(s);
	for (i = 0; i < NHDLC; i++) {
		scp = sc->chan[i];
		if (scp == NULL)
			continue;
		sprintf(s + strlen(s), "c%2d:", i);
		sprintf(s + strlen(s), " ts %08x", scp->ts);
		sprintf(s + strlen(s), " RX %lus/%lus",
		    time_second - scp->last_recv, time_second - scp->last_rxerr);
		sprintf(s + strlen(s), " TX %lus/%lus/%lus",
		    time_second - scp->last_xmit, 
		    time_second - scp->last_txerr,
		    time_second - scp->last_txdrop);
		sprintf(s + strlen(s), " TXdrop %lu Pend %lu", 
		    scp->tx_drop,
		    scp->tx_pending);
		sprintf(s + strlen(s), " CRC %lu Dribble %lu Long %lu Short %lu Abort %lu",
		    scp->crc_error,
		    scp->dribble_error,
		    scp->long_error,
		    scp->short_error,
		    scp->abort_error);
		sprintf(s + strlen(s), "\n TX: %lu RX: %lu\n",
		    scp->txn, scp->rxn);
	}
}


/*
 *
 */

static void
status_8370(struct softc *sc, char *s)
{
	u_int32_t *p = sc->ds8370;

	s += strlen(s);
	sprintf(s, "Framer: "); s += strlen(s);
	switch (sc->framing) {
		case WHOKNOWS: sprintf(s, "(unconfigured)\n"); break;
		case E1: sprintf(s, "(e1)\n"); break;
		case E1U: sprintf(s, "(e1u)\n"); break;
		case T1: sprintf(s, "(t1)\n"); break;
		case T1U: sprintf(s, "(t1u)\n"); break;
		default: sprintf(s, "(mode %d XXX?)\n", sc->framing); break;
	}
	s += strlen(s);
	sprintf(s, "    Red alarms:"); s += strlen(s);
	if (p[0x47] & 0x08) { sprintf(s, " ALOS"); s += strlen(s); }
	if (p[0x47] & 0x04) { sprintf(s, " LOS"); s += strlen(s); }
	if (sc->framing == E1 || sc->framing == T1) {
		if (p[0x47] & 0x02) { sprintf(s, " LOF"); s += strlen(s); }
	}
	sprintf(s, "\n    Yellow alarms:"); s += strlen(s);
	if (p[0x47] & 0x80) { sprintf(s, " RMYEL"); s += strlen(s); }
	if (p[0x47] & 0x40) { sprintf(s, " RYEL"); s += strlen(s); }
	sprintf(s, "\n    Blue alarms:"); s += strlen(s);
	if (p[0x47] & 0x10) { sprintf(s, " AIS"); s += strlen(s); }
	sprintf(s, "\n"); s += strlen(s);
	sprintf(s, "\n    Various alarms:"); s += strlen(s);
	if (p[0x48] & 0x10) { sprintf(s, " TSHORT"); s += strlen(s); }
	sprintf(s, "\n    Counters:"); s += strlen(s);
	if (sc->framing == E1) {
		sprintf(s, " FERR=%lu", sc->cnt_ferr); s += strlen(s);
	}
	sprintf(s, " CERR=%lu", sc->cnt_cerr); s += strlen(s);
	sprintf(s, " LCV=%lu",  sc->cnt_lcv); s += strlen(s);
	sprintf(s, " FEBE=%lu", sc->cnt_febe); s += strlen(s);
	sprintf(s, " BERR=%lu", sc->cnt_berr); s += strlen(s);
	sprintf(s, " FRED=%lu", sc->cnt_fred); s += strlen(s);
	sprintf(s, " COFA=%lu", sc->cnt_cofa); s += strlen(s);
	sprintf(s, " SEF=%lu", sc->cnt_sef); s += strlen(s);
	sprintf(s, "\n"); s += strlen(s);
}

static void
dump_8370(struct softc *sc, char *s, int offset)
{
	int i, j;
	u_int32_t *p = sc->ds8370;

	s += strlen(s);
	for (i = 0; i < 0x100; i += 16) {
		sprintf(s, "%03x: ", i + offset);
		s += strlen(s);
		for (j = 0; j < 0x10; j ++) {
			sprintf(s, " %02x", p[i + j + offset] & 0xff);
			s += strlen(s);
		}
		sprintf(s, "\n");
		s += strlen(s);
	}
}

static void
init_8370(struct softc *sc)
{
	int i;
	u_int32_t *p = sc->ds8370;

        p[0x001] = 0x80; /* CR0 - Reset */
        DELAY(20);
        p[0x001] = 0x00; /* CR0 - E1, RFRAME: FAS only */
        DELAY(20);
	if (sc->clocksource == INT) 
		p[0x002] = 0x40; /* JAT_CR - XXX */
	else
		p[0x002] = 0x20; /* JAT_CR - XXX */
        p[0x00D] = 0x01; /* IER6 - ONESEC */
        p[0x014] = 0x00; /* LOOP - */
        p[0x015] = 0x00; /* DL3_TS - */
        p[0x016] = 0x00; /* DL3_BIT - */
        p[0x017] = 0x00; /* DL3_BIT - */
        p[0x018] = 0xFF; /* PIO - XXX */
        p[0x019] = 0x3c; /* POE - CLADO_OE|RCKO_OE */
	if (sc->clocksource == INT)
		p[0x01A] = 0x37; /* CMUX - RSBCKI(RSBCKI), TSBCKI(CLADO), CLADO(RCKO), TCKI(CLADO) */
	else
		p[0x01A] = 0x37; /* CMUX - RSBCKI(RSBCKI), TSBCKI(RSBCKI), CLADO(RCKO), TCKI(RCKO) */

        /* I.431/G.775 */
        p[0x020] = 0x41; /* LIU_CR - SQUELCH */
        p[0x022] = 0xb1; /* RLIU_CR - */
        p[0x024] = 0x1d; /* VGA_MAX - */
        p[0x027] = 0xba; /* DSLICE - */
        p[0x028] = 0xda; /* EQ_OUT - */
        p[0x02a] = 0xa6; /* PRE_EQ - */

	if (sc->framing == E1U || sc->framing == T1U)
		p[0x040] = 0x49; /* RCRO - XXX */
	else
		p[0x040] = 0x09; /* RCRO - XXX */

        p[0x041] = 0x00; /* RPATT - XXX */
        p[0x045] = 0x00; /* RALM - XXX */
        p[0x046] = 0x05; /* LATCH - LATCH_CNT|LATCH_ALM */

        p[0x068] = 0x4c; /* TLIU_CR - TERM|Pulse=6 */
        p[0x070] = 0x04; /* TCR0 - TFRAME=4 */

	if (sc->framing == E1U || sc->framing == T1U)
		p[0x071] = 0x41; /* TCR1 - TZCS */
	else
		p[0x071] = 0x51; /* TCR1 - TZCS */

	if (sc->framing == E1U || sc->framing == T1U)
		p[0x072] = 0x00;
	else
		p[0x072] = 0x1b; /* TCR1 - INS_YEL|INS_MF|INS_CRC|INS_FBIT */

        p[0x073] = 0x00; /* TERROR */
        p[0x074] = 0x00; /* TMAN */

	if (sc->framing == E1U || sc->framing == T1U)
		p[0x075] = 0x0; /* TALM */
	else
		p[0x075] = 0x10; /* TALM - AUTO_YEL */

        p[0x076] = 0x00; /* TPATT */
        p[0x077] = 0x00; /* TLP */

        p[0x090] = 0x05; /* CLAD_CR - XXX */
        p[0x091] = 0x01; /* CSEL - 2048kHz */

	if (sc->framing == E1U || sc->framing == T1U) {
		p[0x0a0] = 0x00;
		p[0x0a6] = 0x00;
		p[0x0b1] = 0x00;
	}

        p[0x0d0] = 0x46; /* SBI_CR - SBI=6 */
        p[0x0d1] = 0x70; /* RSB_CR - XXX */
        p[0x0d2] = 0x00; /* RSYNC_BIT - 0 */
        p[0x0d3] = 0x00; /* RSYNC_TS - 0 */
        p[0x0d4] = 0x30; /* TSB_CR - XXX */
        p[0x0d5] = 0x00; /* TSYNC_BIT - 0 */
        p[0x0d6] = 0x00; /* TSYNC_TS - 0 */
	if (sc->framing == E1U || sc->framing == T1U) 
		p[0x0d7] = 0x05; /* RSIG_CR - 0  | FRZ_OFF*/
	else 
		p[0x0d7] = 0x01; /* RSIG_CR - 0 */
        p[0x0d8] = 0x00; /* RSIG_FRM - 0 */
        for (i = 0; i < 32; i ++) {
                p[0x0e0 + i] = 0x0d; /* SBC$i - RINDO|TINDO|ASSIGN */
                p[0x100 + i] = 0x00; /* TPC$i - 0 */
                p[0x180 + i] = 0x00; /* RPC$i - 0 */
	}
}

/*
 * Interrupts
 */

static void
musycc_intr0_tx_eom(struct softc *sc, int ch)
{
	struct schan *sch;
	struct mdesc *md;

	sch = sc->chan[ch];
	if (sch == NULL || sch->state != UP) {
		/* XXX: this should not happen once the driver is done */
		printf("Xmit packet on uninitialized channel %d\n", ch);
	}
	if (sc->mdt[ch] == NULL)
		return; 	/* XXX: can this happen ? */
	for (;;) {
		md = sch->tx_last_md;
		if (md->status == 0)
			break;
		if (md->status & 0x80000000)
			break;		/* Not our mdesc, done */
		sch->tx_last_md = md->snext;
		md->data = 0;
		if (md->m != NULL) {
			sch->tx_pending -= md->m->m_pkthdr.len;
			m_freem(md->m);
			md->m = NULL;
		}
		md->status = 0;
	}
}

/*
 * Receive interrupt on controller *sc, channel ch
 *
 * We perambulate the Rx descriptor ring until we hit
 * a mdesc which isn't ours to take.
 */

static void
musycc_intr0_rx_eom(struct softc *sc, int ch)
{
	u_int32_t status, error;
	struct schan *sch;
	struct mbuf *m, *m2;
	struct mdesc *md;

	sch = sc->chan[ch];
	if (sch == NULL || sch->state != UP) {
		/* XXX: this should not happen once the driver is done */
		printf("Received packet on uninitialized channel %d\n", ch);
		return;
	}
	if (sc->mdr[ch] == NULL)
		return; 	/* XXX: can this happen ? */
	for (;;) {
		md = &sc->mdr[ch][sch->rx_last_md];
		status = md->status;
		if (!(status & 0x80000000))
			break;		/* Not our mdesc, done */
		m = md->m;
		m->m_len = m->m_pkthdr.len = status & 0x3fff;
		error = (status >> 16) & 0xf;
		if (error == 0) {
			MGETHDR(m2, M_DONTWAIT, MT_DATA);
			if (m2 != NULL) {
				MCLGET(m2, M_DONTWAIT);
				if((m2->m_flags & M_EXT) != 0) {
					/* Substitute the mbuf+cluster. */
					md->m = m2;
					md->data = vtophys(m2->m_data);
					/* Pass the received mbuf upwards. */
					sch->last_recv = time_second;
					ng_queue_data(sch->hook, m, NULL);
				} else {
					/*
				         * We didn't get a mbuf cluster,
					 * drop received packet, free the
					 * mbuf we cannot use and recycle
				         * the mbuf+cluster we already had.
					 */
					m_freem(m2);
					sch->last_rdrop = time_second;
					sch->rx_drop++;
				}
			} else {
				/*
				 * We didn't get a mbuf, drop received packet
				 * and recycle the "old" mbuf+cluster.
				 */
				sch->last_rdrop = time_second;
				sch->rx_drop++;
			}
		} else if (error == 9) {
			sch->last_rxerr = time_second;
			sch->crc_error++;
		} else if (error == 10) {
			sch->last_rxerr = time_second;
			sch->dribble_error++;
		} else if (error == 11) {
			sch->last_rxerr = time_second;
			sch->abort_error++;
		} else if (error == 12) {
			sch->last_rxerr = time_second;
			sch->long_error++;
		} else {
			sch->last_rxerr = time_second;
			/* Receive error, print some useful info */
			printf("%s %s: RX 0x%08x ", sch->sc->nodename, 
			    sch->hookname, status);
			/* Don't print a lot, just the begining will do */
			if (m->m_len > 16)
				m->m_len = m->m_pkthdr.len = 16;
			m_print(m);
			printf("\n");
		}
		md->status = 1600;	/* XXX: MTU */
		/* Check next mdesc in the ring */
		if (++sch->rx_last_md >= sch->nmd)
			sch->rx_last_md = 0;
	}
}

static void
musycc_intr0(void *arg)
{
	int i, j, g, ch, ev, er;
	struct csoftc *csc;
	u_int32_t u, u1, n, c;
	struct softc *sc;

	csc = arg;

	for (;;) {
		u = csc->reg->isd;
		c = u & 0x7fff;
		n = u >> 16;
		if (c == 0)
			return;
		if (debug & 1)
			printf("%s: IRQ: %08x n = %d c = %d\n", csc->serial[0].nodename, u, n, c);
		for (i = 0; i < c; i++) {
			j = (n + i) % NIQD;
			u1 = csc->iqd[j];
			g = (u1 >> 29) & 0x3;
			g |= (u1 >> (14-2)) & 0x4;
			ch = (u1 >> 24) & 0x1f;
			ev = (u1 >> 20) & 0xf;
			er = (u1 >> 16) & 0xf;
			sc = &csc->serial[g];
			if ((debug & 2) || er) {
				printf("%08x %d", u1, g);
				printf("/%s", u1 & 0x80000000 ? "T" : "R");
				printf("/%02d", ch);
				printf(" %02d", ev);
				printf(":%02d", er);
				printf("\n");
			}
			switch (ev) {
			case 1: /* SACK		Service Request Acknowledge	    */
#if 0
				printf("%s: SACK: %08x group=%d", sc->nodename, csc->iqd[j], g);
				printf("/%s", csc->iqd[j] & 0x80000000 ? "T" : "R");
				printf(" cmd %08x (%08x) \n", sc->last, sc->reg->srd);
#endif
				sc->last = 0xffffffff;
				wakeup(&sc->last);
				break;
			case 5: /* CHABT	Change To Abort Code (0x7e -> 0xff) */
			case 6: /* CHIC		Change To Idle Code (0xff -> 0x7e)  */
				break;
			case 3: /* EOM		End Of Message			    */
				if (csc->iqd[j] & 0x80000000)
					musycc_intr0_tx_eom(sc, ch);
				else
					musycc_intr0_rx_eom(sc, ch);
				break;
			case 0:
				if (er == 13) {	/* SHT */
					sc->chan[ch]->last_rxerr = time_second;
					sc->chan[ch]->short_error++;
					break;
				}
			default:
				musycc_intr0_tx_eom(sc, ch);
				musycc_intr0_rx_eom(sc, ch);
#if 1
				printf("huh ? %08x %d", u1, g);
				printf("/%s", u1 & 0x80000000 ? "T" : "R");
				printf("/%02d", ch);
				printf(" %02d", ev);
				printf(":%02d", er);
				printf("\n");
#endif
			}
			csc->iqd[j] = 0xffffffff;
			j++;
			j %= NIQD;
			csc->reg->isd = j << 16;
		}
	}
}

static void
musycc_intr1(void *arg)
{
	int i;
	struct csoftc *csc;
	struct softc *sc;
	u_int32_t *u;
	u_int8_t irr;
	
	csc = arg;

	for (i = 0; i < csc->nchan; i++) {
		sc = &csc->serial[i];
                u = sc->ds8370;
		irr = u[3];
		if (irr == 0)
			continue;
		if (u[0x5] & 1) { /* ONESEC */
			sc->cnt_ferr +=  u[0x50] & 0xff;
			sc->cnt_ferr += (u[0x51] & 0xff) << 8;
			sc->cnt_cerr +=  u[0x52] & 0xff;
			sc->cnt_cerr += (u[0x53] & 0xff) << 8;
			sc->cnt_lcv  +=  u[0x54] & 0xff;
			sc->cnt_lcv  += (u[0x55] & 0xff) << 8;
			sc->cnt_febe +=  u[0x56] & 0xff;
			sc->cnt_febe += (u[0x57] & 0xff) << 8;
			sc->cnt_berr +=  u[0x58] & 0xff;
			sc->cnt_berr += (u[0x59] & 0xff) << 8;
			sc->cnt_fred += (u[0x5a] & 0xf0) >> 4;
			sc->cnt_cofa += (u[0x5a] & 0x0c) >> 2;
			sc->cnt_sef  +=  u[0x5a] & 0x03;
		}
		if (debug & 4) {
			int j;

			printf("musycc_intr1:%d %02x", i, irr);
			for (j = 4; j < 0x14; j++)
				printf(" %02x", u[j] & 0xff);
			printf("\n");
		}
	}
}

/*
 * NetGraph Stuff
 */

static int
musycc_constructor(node_p *nodep)
{

	return (EINVAL);
}

static int
musycc_shutdown(node_p nodep)
{

	return (EINVAL);
}

static void
musycc_config(node_p node, char *set, char *ret)
{
	struct softc *sc;
	struct csoftc *csc;
	enum framing wframing;
	int i;

	sc = node->private;
	csc = sc->csc;
	if (csc->state == C_IDLE) 
		init_card(csc);
	while (csc->state != C_RUNNING)
		tsleep(&csc->state, PZERO + PCATCH, "crun", hz/10);
	if (set != NULL) {
		if (!strncmp(set, "line ", 5)) {
			wframing = sc->framing;
			if (!strcmp(set, "line e1")) {
				wframing = E1;
			} else if (!strcmp(set, "line e1u")) {
				wframing = E1U;
			} else {
				strcat(ret, "ENOGROK\n");
				return;
			}
			if (wframing == sc->framing)
				return;
			if (sc->nhooks > 0) {
				sprintf(ret, "Cannot change line when %d hooks open\n", sc->nhooks);
				return;
			}
			sc->framing = wframing;
			init_ctrl(sc);
			return;
		}
		if (!strcmp(set, "clock source internal")) {
			sc->clocksource = INT;
			init_ctrl(sc);
		} else if (!strcmp(set, "clock source line")) {
			sc->clocksource = EXT;
			init_ctrl(sc);
		} else if (!strcmp(set, "show 8370 0")) {
			dump_8370(sc, ret, 0);
		} else if (!strcmp(set, "show 8370 1")) {
			dump_8370(sc, ret, 0x100);
		} else if (!strncmp(set, "creg", 4)) {
			i = strtol(set + 5, 0, 0);
			printf("set creg %d\n", i);
			csc->creg = 0xfe | (i << 24);
			*csc->cregp = csc->creg;
/*
		} else if (!strcmp(set, "reset")) {
			reset_group(sc, ret);
		} else if (!strcmp(set, "reset all")) {
			reset_card(sc, ret);
*/
		} else {
			printf("%s CONFIG SET [%s]\n", sc->nodename, set);
			goto barf;
		}		

		return;
	}
	if (sc->framing == E1)
		strcat(ret, "line e1\n");
	else if (sc->framing == E1U)
		strcat(ret, "line e1u\n");
	if (sc->clocksource == INT)
		strcat(ret, "clock source internal\n");
	else
		strcat(ret, "clock source line\n");
	return;
barf:
	strcpy(ret, "Syntax Error\n");
	strcat(ret, "\tline {e1|e1u}\n");
	strcat(ret, "\tshow 8370 {0|1}\n");
	return;
}

/*
 * Handle status and config enquiries.
 * Respond with a synchronous response.
 */
static int
musycc_rcvmsg(node_p node, struct ng_mesg *msg, const char *retaddr, struct ng_mesg **resp)
{
	struct softc *sc;
	char *s, *r;

	sc = node->private;

	if (msg->header.typecookie != NGM_GENERIC_COOKIE)
		goto out;

	if (msg->header.cmd == NGM_TEXT_STATUS) {
		NG_MKRESPONSE(*resp, msg, 
		    sizeof(struct ng_mesg) + NG_TEXTRESPONSE, M_NOWAIT);
		if (*resp == NULL) {
			FREE(msg, M_NETGRAPH);
			return (ENOMEM);
		}
		s = (char *)(*resp)->data;
		status_8370(sc, s);
		status_chans(sc,s);
		(*resp)->header.arglen = strlen(s) + 1;
		FREE(msg, M_NETGRAPH);
		return (0);
        } else if (msg->header.cmd == NGM_TEXT_CONFIG) {
		if (msg->header.arglen) {
			s = (char *)msg->data;
		} else {
			s = NULL;
		}
		
		NG_MKRESPONSE(*resp, msg, 
		    sizeof(struct ng_mesg) + NG_TEXTRESPONSE, M_NOWAIT);
		if (*resp == NULL) {
			FREE(msg, M_NETGRAPH);
			return (ENOMEM);
		}
		r = (char *)(*resp)->data;
		*r = '\0';
		musycc_config(node, s, r);
		(*resp)->header.arglen = strlen(r) + 1;
		FREE(msg, M_NETGRAPH);
		return (0);
        }

out:
	if (resp)
		*resp = NULL;
	FREE(msg, M_NETGRAPH);
	return (EINVAL);
}

static int
musycc_newhook(node_p node, hook_p hook, const char *name)
{
	struct softc *sc;
	struct csoftc *csc;
	struct schan *sch;
	u_int32_t ts, chan;
	int nbit;

	sc = node->private;
	csc = sc->csc;

	while (csc->state != C_RUNNING)
		tsleep(&csc->state, PZERO + PCATCH, "crun", hz/10);

	if (sc->framing == WHOKNOWS)
		return (EINVAL);

	if (name[0] != 't' || name[1] != 's')
		return (EINVAL);
	ts = parse_ts(name + 2, &nbit);
	if (ts == 0)
		return (EINVAL);
	chan = ffs(ts) - 1;

	if (sc->framing == E1U && nbit == 32)
		;
	else if (sc->framing == T1U && nbit == 24)
		;
	else if (ts & 1)
		return (EINVAL);
		
	if (sc->chan[chan] == NULL) {
		MALLOC(sch, struct schan *, sizeof(*sch), M_MUSYCC, M_WAITOK | M_ZERO);
		sch->sc = sc;
		sch->state = DOWN;
		sch->chan = chan;
		sprintf(sch->hookname, name);	/* XXX overflow ? */
		sc->chan[chan] = sch;
	} else if (sc->chan[chan]->state == UP) {
		return (EBUSY);
	}
	sc->nhooks++;
	sch = sc->chan[chan];
	sch->ts = ts;
	sch->hook = hook;
	sch->tx_limit = nbit * 8;
	hook->private = sch;
	return(0);
}

static int
musycc_rcvdata(hook_p hook, struct mbuf *m, meta_p meta)
{

	struct softc *sc;
	struct csoftc *csc;
	struct schan *sch;
	struct mdesc *md, *md0;
	u_int32_t ch, u, u0, len;
	struct mbuf *m2;

	sch = hook->private;
	sc = sch->sc;
	csc = sc->csc;
	ch = sch->chan;

	if (csc->state != C_RUNNING) {
		printf("csc->state = %d\n", csc->state);
		NG_FREE_DATA(m, meta);
		return (0);
	}

	NG_FREE_META(meta);
	meta = NULL;

	if (sch->state != UP) {
		printf("sch->state = %d\n", sch->state);
		NG_FREE_DATA(m, meta);
		return (0);
	} 
	if (sch->tx_pending + m->m_pkthdr.len > sch->tx_limit * maxlatency) {
		sch->tx_drop++;
		sch->last_txdrop = time_second;
		NG_FREE_DATA(m, meta);
		return (0);
	}

	/* find out if we have enough txmd's */
	m2 = m;
	md = sch->tx_next_md;
	for (len = m2->m_pkthdr.len; len; m2 = m2->m_next) {
		if (m2->m_len == 0)
			continue;
		if (md->status != 0) {
			sch->tx_drop++;
			sch->last_txdrop = time_second;
			NG_FREE_DATA(m, meta);
			return (0);
		}
		len -= m2->m_len;
		md = md->snext;
	}

	m2 = m;
	md = md0 = sch->tx_next_md;
	u0 = 0;
	for (len = m->m_pkthdr.len; len > 0; m = m->m_next) {
		if (m->m_len == 0)
			continue;
		if (md->status != 0) {
			printf("Out of tx md(2)\n");
			sch->last_txerr = time_second;
			sch->tx_drop++;
			sch->last_txdrop = time_second;
			NG_FREE_DATA(m, meta);
			break;
		}

		md->data = vtophys(m->m_data);
		if (md == md0)
			u = 0x00000000;	/* OWNER = CPU */
		else
			u = 0x80000000;	/* OWNER = MUSYCC */
		u |= m->m_len;
		len -= m->m_len;
		if (len > 0) {
			md->m = NULL;
			if (md == md0)
				u0 = u;
			else
				md->status = u;
			md = md->snext;
			continue;
		}
		u |= 0x20000000;	/* EOM */
		md->m = m2;
		sch->tx_pending += m2->m_pkthdr.len;
		if (md == md0) {
			u |= 0x80000000;	/* OWNER = MUSYCC */
			md->status = u;
		} else {
			md->status = u;
			md0->status = u0 | 0x80000000; /* OWNER = MUSYCC */
		}
		sch->last_xmit = time_second;
		sch->tx_next_md = md->snext;
	}
	sch->txn++;
	return (0);
}

static int
musycc_connect(hook_p hook)
{
	struct softc *sc;
	struct csoftc *csc;
	struct schan *sch;
	int nts, nbuf, i, nmd, ch;
	struct mbuf *m;

	sch = hook->private;
	sc = sch->sc;
	csc = sc->csc;
	ch = sch->chan;

	while (csc->state != C_RUNNING)
		tsleep(&csc->state, PZERO + PCATCH, "crun", hz/10);

	if (sch->state == UP)
		return (0);
	sch->state = UP;

	/* Setup the Time Slot Map */
	nts = 0;
	for (i = ch; i < 32; i++) {
		if (sch->ts & (1 << i)) {
			sc->ram->rtsm[i] = ch | (4 << 5);
			sc->ram->ttsm[i] = ch | (4 << 5);
			nts++;		
		}
	}

	/* 
	 * Find the length of the first run of timeslots.
	 * XXX: find the longest instead.
	 */
	nbuf = 0;
	for (i = ch; i < 32; i++) {
		if (sch->ts & (1 << i))
			nbuf++;
		else
			break;
	}
		
	printf("Connect ch= %d ts= %08x nts= %d nbuf = %d\n", 
	    ch, sch->ts, nts, nbuf);

	/* Reread the Time Slot Map */
	sc->reg->srd = sc->last = 0x1800;
	tsleep(&sc->last, PZERO + PCATCH, "con1", hz);
	sc->reg->srd = sc->last = 0x1820;
	tsleep(&sc->last, PZERO + PCATCH, "con2", hz);

	/* Set the channel mode */
	sc->ram->tcct[ch] = 0x2800; /* HDLC-FCS16 | MAXSEL[2] */
	sc->ram->rcct[ch] = 0x2800; /* HDLC-FCS16 | MAXSEL[2] */

	/*
	 * Allocate the FIFO space
	 * We don't do subchanneling so we can use 128 dwords [4-13]
	 */
	sc->ram->tcct[ch] |= (1 + 2 * (nbuf - 1)) << 16; /* BUFFLEN */
	sc->ram->rcct[ch] |= (1 + 2 * (nbuf - 1)) << 16; /* BUFFLEN */
	sc->ram->tcct[ch] |= ((ch * 2) << 24);	 /* BUFFLOC */
	sc->ram->rcct[ch] |= ((ch * 2) << 24);	 /* BUFFLOC */

	/* Reread the Channel Configuration Descriptor for this channel */
	sc->reg->srd = sc->last = 0x0b00 + ch;
	tsleep(&sc->last, PZERO + PCATCH, "con3", hz);
	sc->reg->srd = sc->last = 0x0b20 + ch;
	tsleep(&sc->last, PZERO + PCATCH, "con4", hz);

	/*
	 * Figure out how many receive buffers we want:  10 + nts * 2
	 *  1 timeslot,  50 bytes packets -> 68msec
	 * 31 timeslots, 50 bytes packets -> 14msec
	 */
	sch->nmd = nmd = 200 + nts * 4;
	sch->rx_last_md = 0;
	MALLOC(sc->mdt[ch], struct mdesc *, 
	    sizeof(struct mdesc) * nmd, M_MUSYCC, M_WAITOK);
	MALLOC(sc->mdr[ch], struct mdesc *, 
	    sizeof(struct mdesc) * nmd, M_MUSYCC, M_WAITOK);
	for (i = 0; i < nmd; i++) {
		if (i == nmd - 1) {
			sc->mdt[ch][i].snext = &sc->mdt[ch][0];
			sc->mdt[ch][i].next = vtophys(sc->mdt[ch][i].snext);
			sc->mdr[ch][i].snext = &sc->mdr[ch][0];
			sc->mdr[ch][i].next = vtophys(sc->mdr[ch][i].snext);
		} else {
			sc->mdt[ch][i].snext = &sc->mdt[ch][i + 1];
			sc->mdt[ch][i].next = vtophys(sc->mdt[ch][i].snext);
			sc->mdr[ch][i].snext = &sc->mdr[ch][i + 1];
			sc->mdr[ch][i].next = vtophys(sc->mdr[ch][i].snext);
		}
		sc->mdt[ch][i].status = 0;
		sc->mdt[ch][i].m = NULL;
		sc->mdt[ch][i].data = 0;

		MGETHDR(m, M_WAIT, MT_DATA);
		if (m == NULL)
			goto errfree;
		MCLGET(m, M_WAIT);
		if ((m->m_flags & M_EXT) == 0) {
			/* We've waited mbuf_wait and still got nothing.
			   We're calling with M_TRYWAIT anyway - a little
			   defensive programming costs us very little - if
			   anything at all in the case of error. */
			m_free(m);
			goto errfree;
		}
		sc->mdr[ch][i].m = m;
		sc->mdr[ch][i].data = vtophys(m->m_data);
		sc->mdr[ch][i].status = 1600; /* MTU */
	}
	sch->tx_last_md = sch->tx_next_md = &sc->mdt[ch][0];

	/* Configure it into the chip */
	sc->ram->thp[ch] = vtophys(&sc->mdt[ch][0]);
	sc->ram->tmp[ch] = vtophys(&sc->mdt[ch][0]);
	sc->ram->rhp[ch] = vtophys(&sc->mdr[ch][0]);
	sc->ram->rmp[ch] = vtophys(&sc->mdr[ch][0]);

	/* Activate the Channel */
	sc->reg->srd = sc->last = 0x0800 + ch;
	tsleep(&sc->last, PZERO + PCATCH, "con4", hz);
	sc->reg->srd = sc->last = 0x0820 + ch;
	tsleep(&sc->last, PZERO + PCATCH, "con3", hz);

	return (0);

errfree:
	while (i > 0) {
		/* Don't leak all the previously allocated mbufs in this loop */
		i--;
		m_free(sc->mdr[ch][i].m);
	}
	FREE(sc->mdt[ch], M_MUSYCC);
	FREE(sc->mdr[ch], M_MUSYCC);
	return (ENOBUFS);
}

static int
musycc_disconnect(hook_p hook)
{
	struct softc *sc;
	struct csoftc *csc;
	struct schan *sch;
	int i, ch;

	sch = hook->private;
	sc = sch->sc;
	csc = sc->csc;
	ch = sch->chan;

	while (csc->state != C_RUNNING)
		tsleep(&csc->state, PZERO + PCATCH, "crun", hz/10);

	/* Deactivate the channel */
	sc->reg->srd = sc->last = 0x0900 + sch->chan;
	tsleep(&sc->last, PZERO + PCATCH, "con3", hz);
	sc->reg->srd = sc->last = 0x0920 + sch->chan;
	tsleep(&sc->last, PZERO + PCATCH, "con4", hz);

	if (sch->state == DOWN)
		return (0);
	sch->state = DOWN;

	sc->ram->thp[ch] = 0;
	sc->ram->tmp[ch] = 0;
	sc->ram->rhp[ch] = 0;
	sc->ram->rmp[ch] = 0;
	for (i = 0; i < sch->nmd; i++) {
		if (sc->mdt[ch][i].m != NULL)
			m_freem(sc->mdt[ch][i].m);
		if (sc->mdr[ch][i].m != NULL)
			m_freem(sc->mdr[ch][i].m);
	}
	FREE(sc->mdt[ch], M_MUSYCC);
	sc->mdt[ch] = NULL;
	FREE(sc->mdr[ch], M_MUSYCC);
	sc->mdr[ch] = NULL;

	for (i = 0; i < 32; i++) {
		if (sch->ts & (1 << i)) {
			sc->ram->rtsm[i] = 0;
			sc->ram->ttsm[i] = 0;
		}
	}
	sc->nhooks--;
	sch->tx_pending = 0;

	return (0);
}



/*
 * PCI initialization stuff
 */

static int
musycc_probe(device_t self)
{
	char desc[40];

	if (sizeof(struct groupr) != 1572) {
		printf("sizeof(struct groupr) = %d, should be 1572\n",
		    sizeof(struct groupr));
		return(ENXIO);
	}

	if (sizeof(struct globalr) != 1572) {
		printf("sizeof(struct globalr) = %d, should be 1572\n",
		    sizeof(struct globalr));
		return(ENXIO);
	}

	if (sizeof(struct mycg) > 2048) {
		printf("sizeof(struct mycg) = %d, should be <= 2048\n",
		    sizeof(struct mycg));
		return(ENXIO);
	}

	switch (pci_get_devid(self)) {
	case 0x8471109e: strcpy(desc, "CN8471 MUSYCC"); break;
	case 0x8472109e: strcpy(desc, "CN8472 MUSYCC"); break;
	case 0x8474109e: strcpy(desc, "CN8474 MUSYCC"); break;
	case 0x8478109e: strcpy(desc, "CN8478 MUSYCC"); break;
	default:
		return (ENXIO);
	}

	switch (pci_get_function(self)) {
	case 0: strcat(desc, " Network controller"); break;
	case 1: strcat(desc, " Ebus bridge"); break;
	default:
		return (ENXIO);
	}

	device_set_desc_copy(self, desc);
	return 0;
}

static int
musycc_attach(device_t self)
{
	struct csoftc *csc;
	struct resource *res;
	struct softc *sc;
	int rid, i, error;
	int f;
	u_int32_t	*u32p, u;
	static int once;

	if (!once) {
		once++;
		error = ng_newtype(&ngtypestruct);
		if (error != 0) 
			printf("ng_newtype() failed %d\n", error);
	}
	printf("We have %d pad bytes in mycg\n", 2048 - sizeof(struct mycg));

	f = pci_get_function(self);
	/* For function zero allocate a csoftc */
	if (f == 0) {
		MALLOC(csc, struct csoftc *, sizeof(*csc), M_MUSYCC, M_WAITOK | M_ZERO);
		csc->bus = pci_get_bus(self);
		csc->slot = pci_get_slot(self);
		LIST_INSERT_HEAD(&sc_list, csc, list);
	} else {
		LIST_FOREACH(csc, &sc_list, list) {
			if (csc->bus != pci_get_bus(self))
				continue;
			if (csc->slot != pci_get_slot(self))
				continue;
			break;
		}
	}
	csc->f[f] = self;
	device_set_softc(self, csc);
	rid = PCIR_MAPS;
	res = bus_alloc_resource(self, SYS_RES_MEMORY, &rid,
	    0, ~0, 1, RF_ACTIVE);
	if (res == NULL) {
		device_printf(self, "Could not map memory\n");
		return ENXIO;
	}
	csc->virbase[f] = (u_char *)rman_get_virtual(res);
	csc->physbase[f] = rman_get_start(res);

	/* Allocate interrupt */
	rid = 0;
	csc->irq[f] = bus_alloc_resource(self, SYS_RES_IRQ, &rid, 0, ~0,
	    1, RF_SHAREABLE | RF_ACTIVE);

	if (csc->irq[f] == NULL) {
		printf("couldn't map interrupt\n");
		return(ENXIO);
	}

	error = bus_setup_intr(self, csc->irq[f], INTR_TYPE_NET,
	    f == 0 ? musycc_intr0 : musycc_intr1, csc, &csc->intrhand[f]);

	if (error) {
		printf("couldn't set up irq\n");
		return(ENXIO);
	}

	if (f == 0)
		return (0);

	for (i = 0; i < 2; i++)
		printf("f%d: device %p virtual %p physical %08x\n",
		    i, csc->f[i], csc->virbase[i], csc->physbase[i]);

	csc->reg = (struct globalr *)csc->virbase[0];
	csc->reg->glcd = 0x3f30;	/* XXX: designer magic */
	u32p = (u_int32_t *)csc->virbase[1];
	u = u32p[0x1200];
	if ((u & 0xffff0000) != 0x13760000) {
		printf("Not a LMC1504 (ID is 0x%08x).  Bailing out.\n", u);
		return(ENXIO);
	}
	csc->nchan = (u >> 8) & 0xf;
	printf("Found <LanMedia LMC1504 Rev %d Chan %d>\n", (u >> 12) & 0xf, csc->nchan);

	csc->creg = 0xfe;
	csc->cregp = &u32p[0x1000];
	*csc->cregp = csc->creg;	
	for (i = 0; i < csc->nchan; i++) {
		sc = &csc->serial[i];
		sc->csc = csc;
		sc->last = 0xffffffff;
		sc->ds8370 = (u_int32_t *)
		    (csc->virbase[1] + i * 0x800);
		sc->ds847x = csc->virbase[0] + i * 0x800;
		sc->reg = (struct globalr *)
		    (csc->virbase[0] + i * 0x800);
		MALLOC(sc->mycg, struct mycg *, 
		    sizeof(struct mycg), M_MUSYCC, M_WAITOK | M_ZERO);
		sc->ram = &sc->mycg->cg;

		error = ng_make_node_common(&ngtypestruct, &sc->node);
		if (error) {
			printf("ng_make_node_common() failed %d\n", error);
			continue;
		}	
		sc->node->private = sc;
		sprintf(sc->nodename, "sync-%d-%d-%d",
			csc->bus,
			csc->slot,
			i);
		error = ng_name_node(sc->node, sc->nodename);
		/* XXX Apparently failure isn't a problem */
	}
	csc->ram = (struct globalr *)&csc->serial[0].mycg->cg;
	sc = &csc->serial[0];
	sc->reg->srd = sc->last = 0x100;
	csc->state = C_IDLE;

	return 0;
}

static device_method_t musycc_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		musycc_probe),
	DEVMETHOD(device_attach,	musycc_attach),
	DEVMETHOD(device_suspend,	bus_generic_suspend),
	DEVMETHOD(device_resume,	bus_generic_resume),
	DEVMETHOD(device_shutdown,	bus_generic_shutdown),

	{0, 0}
};
 
static driver_t musycc_driver = {
	"musycc",
	musycc_methods,
	0
};

DRIVER_MODULE(musycc, pci, musycc_driver, musycc_devclass, 0, 0);

