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
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/kernel.h>
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

static void init_8370(u_int32_t *p);
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
	int		tx_next_md;	/* index to the next MD */
	int		tx_last_md;	/* index to the last MD */
	int		rx_last_md;	/* index to next MD */
	int		nmd;		/* count of MD's. */
#if 0

	time_t		last_recv;
	time_t		last_rxerr;
	time_t		last_xmit;

	u_long		rx_error;

	u_long		short_error;
	u_long		crc_error;
	u_long		dribble_error;
	u_long		long_error;
	u_long		abort_error;
	u_long		overflow_error;

	int		last_error;
	int		prev_error;

	u_long		tx_pending;
#endif
};


struct softc {
	enum {WHOKNOWS, E1, T1}	framing;
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
};

struct csoftc {
	int	unit, bus, slot;
	LIST_ENTRY(csoftc) list;

	device_t f[2];
	struct resource *irq[2];
	void *intrhand[2];
	vm_offset_t physbase[2];
	u_char *virbase[2];

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

static  ng_constructor_t ng_constructor;
static  ng_rcvmsg_t ng_rcvmsg;
static  ng_shutdown_t ng_shutdown;
static  ng_newhook_t ng_newhook;
static  ng_connect_t ng_connect;
static  ng_rcvdata_t ng_rcvdata;
static  ng_disconnect_t ng_disconnect;

static struct ng_type ngtypestruct = {
	NG_VERSION,
	NG_NODETYPE,
	NULL, 
	ng_constructor,
	ng_rcvmsg,
	ng_shutdown,
	ng_newhook,
	NULL,
	ng_connect,
	ng_rcvdata,
	ng_rcvdata,
	ng_disconnect,
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
	j = 0;
	*nbit = 0;
	while(*s) {
		i = strtol(s, &p, 0);
		if (i < 1 || i > 31)
			return (0);
		while (j && j < i) {
			r |= 1 << j++;
			(*nbit)++;
		}
		j = 0;
		r |= 1 << i;
		(*nbit)++;
		if (*p == ',') {
			s = p + 1;
			continue;
		} else if (*p == '-') {
			j = i;
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
	struct csoftc *sc;

	timeout(poke_847x, NULL, 5*hz);
	LIST_FOREACH(sc, &sc_list, list)  {
		count++;
		for (i = 0; i < sc->nchan; i++) {
			if (sc->serial[i].last == 0xffffffff) {
				sc->serial[i].reg->srd = 0;
				sc->serial[i].last = 0;
				return;
			}
		}
	}
}

static void
init_847x(struct csoftc *sc)
{
	struct softc *sif;

	printf("init_847x(%p)\n", sc);
	sif = &sc->serial[0];
	sif->reg->gbp = vtophys(sc->ram);
	sc->ram->glcd = 0x3f30;	/* XXX: designer magic */
	sc->ram->iqp = vtophys(sc->iqd);
	sc->ram->iql = NIQD - 1;
	sif->reg->srd = 0x400;
	sif->last = 0x400;
	sc->ram->dacbp = 0;		/* 32bit only */

	timeout(poke_847x, NULL, 30*hz);
}

static void
init_ctrl(struct softc *sif)
{
	int i;

	printf("init_ctrl(%p) [%s] [%08x]\n", sif, sif->nodename, sif->csc->reg->glcd);
	init_8370(sif->ds8370);
	tsleep(sif, PZERO | PCATCH, "ds8370", hz);
	sif->reg->gbp = vtophys(sif->ram);
	sif->ram->grcd =  0x00000001;	/* RXENBL */
	sif->ram->grcd |= 0x00000002;	/* TXENBL */
	sif->ram->grcd |= 0x00000004;	/* SUBDSBL */
	sif->ram->grcd |= 0x00000008;	/* OOFABT */
#if 0
	sif->ram->grcd |= 0x00000010;	/* MSKOOF */
	sif->ram->grcd |= 0x00000020;	/* MSKCOFA */
#endif
	sif->ram->grcd |= 0x00000c00;	/* POLLTH=3 */
#if 0
	sif->ram->grcd |= 0x00008000;	/* SFALIGN */
#endif

	sif->ram->mpd = 0;		/* Memory Protection NI [5-18] */

	sif->ram->pcd =  0x0000001;	/* PORTMD=1 (E1/32ts) */
	sif->ram->pcd |= 0x0000000;	/* XXX */

	/* Message length descriptor */
	/* XXX: MTU */
	sif->ram->mld = 1600;
	sif->ram->mld |= (1600 << 16);

	for (i = 0; i < NHDLC; i++) {
		sif->ram->ttsm[i] = 0;
		sif->ram->rtsm[i] = 0;
	}
	sif->reg->srd = sif->last = 0x500;
}

/*
 *
 */

static void
init_8370(u_int32_t *p)
{
	int i;

        p[0x001] = 0x80; /* CR0 - Reset */
        DELAY(20);
        p[0x001] = 0x00; /* CR0 - E1, RFRAME: FAS only */
        DELAY(20);
        p[0x002] = 0x00; /* JAT_CR - XXX */
        p[0x014] = 0x00; /* LOOP - */
        p[0x015] = 0x00; /* DL3_TS - */
        p[0x016] = 0x00; /* DL3_BIT - */
        p[0x017] = 0x00; /* DL3_BIT - */
        p[0x018] = 0xFF; /* PIO - XXX */
        p[0x019] = 0x3c; /* POE - CLADO_OE|RCKO_OE */
        p[0x01A] = 0x15; /* CMUX - RSBCKI(RSBCKI), TSBCKI(RSBCKI), CLADO(RCKO), TCKI(RCKO) */

        /* I.431/G.775 */
        p[0x020] = 0x41; /* LIU_CR - SQUELCH */
        p[0x022] = 0xb1; /* RLIU_CR - */
        p[0x024] = 0x1d; /* VGA_MAX - */
        p[0x027] = 0xba; /* DSLICE - */
        p[0x028] = 0xda; /* EQ_OUT - */
        p[0x02a] = 0xa6; /* PRE_EQ - */

        p[0x040] = 0x09; /* RCRO - XXX */
        p[0x041] = 0x00; /* RPATT - XXX */
        p[0x045] = 0x00; /* RALM - XXX */
        p[0x046] = 0x05; /* LATCH - LATCH_CNT|LATCH_ALM */

        p[0x068] = 0x4c; /* TLIU_CR - TERM|Pulse=6 */
        p[0x070] = 0x04; /* TCR0 - TFRAME=4 */
        p[0x071] = 0x51; /* TCR1 - TZCS */
        p[0x072] = 0x1b; /* TCR1 - INS_YEL|INS_MF|INS_CRC|INS_FBIT */
        p[0x073] = 0x00; /* TERROR */
        p[0x074] = 0x00; /* TMAN */
        p[0x075] = 0x10; /* TALM - AUTO_YEL */
        p[0x076] = 0x00; /* TPATT */
        p[0x077] = 0x00; /* TLP */

        p[0x090] = 0x05; /* CLAD_CR - XXX */
        p[0x091] = 0x01; /* CSEL - 2048kHz */
        p[0x0d0] = 0x46; /* SBI_CR - SBI=6 */
        p[0x0d1] = 0x70; /* RSB_CR - XXX */
        p[0x0d2] = 0x00; /* RSYNC_BIT - 0 */
        p[0x0d3] = 0x00; /* RSYNC_TS - 0 */
        p[0x0d4] = 0x30; /* TSB_CR - XXX */
        p[0x0d5] = 0x00; /* TSYNC_BIT - 0 */
        p[0x0d6] = 0x00; /* TSYNC_TS - 0 */
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
	u_int32_t status;
	struct schan *sch;
	struct mbuf *m;
	struct mdesc *md;

	sch = sc->chan[ch];
	if (sch == NULL || sch->state != UP) {
		/* XXX: this should not happen once the driver is done */
		printf("Xmit packet on uninitialized channel %d\n", ch);
		return;
	}
	if (sc->mdt[ch] == NULL)
		return; 	/* XXX: can this happen ? */
	for (;;) {
		if (sch->tx_last_md == sch->tx_next_md)
			break;
		md = &sc->mdt[ch][sch->tx_last_md];
		status = md->status;
		if (status & 0x80000000)
			break;		/* Not our mdesc, done */
		m = md->m;
		if (m)
			m_freem(m);
		md->m = NULL;
		md->data = 0;
		md->status = 0;
		if (++sch->tx_last_md >= sch->nmd)
			sch->tx_last_md = 0;
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
					ng_queue_data(sch->hook, m, NULL);
				} else {
					/*
				         * We didn't get a mbuf cluster,
					 * drop received packet, free the
					 * mbuf we cannot use and recycle
				         * the mbuf+cluster we already had.
					 */
					m_freem(m2);
					sch->rx_drop++;
				}
			} else {
				/*
				 * We didn't get a mbuf, drop received packet
				 * and recycle the "old" mbuf+cluster.
				 */
				sch->rx_drop++;
			}
		} else {
			/* Receive error, print some useful info */
			printf("%s %s: RX 0x%08x ", sch->sc->nodename, 
			    sch->hookname, status);
			/* Don't print a lot, just the begining will do */
			if (m->m_len > 16)
				m->m_len = m->m_pkthdr.len = 16;
			m_print(m);
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
	int i, j, g, ch, ev;
	struct csoftc *csc;
	u_int32_t u, n, c;
	struct softc *sc;

	csc = arg;

	u = csc->serial[0].reg->isd;
	c = u & 0x7fff;
	if (c == 0)
		return;
	n = u >> 16;
	for (i = n; i < n + c; i++) {
		j = i % NIQD;
		g = (csc->iqd[j] >> 29) & 0x3;
		g |= (csc->iqd[j] >> (14-2)) & 0x4;
		ch = (csc->iqd[j] >> 24) & 0x1f;
		ev = (csc->iqd[j] >> 20) & 0xf;
		sc = &csc->serial[g];
		switch (ev) {
		case 1: /* SACK		Service Request Acknowledge	    */
			if (sc->last) {
				printf("%08x %d", csc->iqd[j], g);
				printf("/%s", csc->iqd[j] & 0x80000000 ? "T" : "R");
				printf(" cmd %08x\n", sc->last);
			}
			if (sc->last == 0x500) {
				sc->last = 0x520;
				sc->reg->srd = sc->last;
			} else {
				sc->last = 0xffffffff;
				wakeup(&sc->last);
			}
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
		default:
			printf("%08x %d", csc->iqd[j], g);
			printf("/%s", csc->iqd[j] & 0x80000000 ? "T" : "R");
			printf("/%02d", ch);
			printf(" %02d", ev);
			printf(":%02d", (csc->iqd[j] >> 16) & 0xf);
			printf("\n");
		}
		csc->iqd[i] = 0xffffffff;
	}
	n += c;
	n %= NIQD;
	csc->serial[0].reg->isd = n << 16;
}

static void
musycc_intr1(void *arg)
{
	int i, j;
	struct csoftc *sc;
	u_int32_t *u;
	u_int8_t irr;
	
	sc = arg;

	for (i = 0; i < sc->nchan; i++) {
                u = sc->serial[i].ds8370;
		irr = u[3];
		if (irr == 0)
			continue;
		printf("musycc_intr1:%d %02x", i, irr);
		for (j = 4; j < 0x14; j++)
			printf(" %02x", u[j] & 0xff);
		printf("\n");
	}
}

/*
 * High-Level stuff
 */

static void
set_chan_e1(struct softc *sif, char *ret)
{
	if (sif->framing == E1)
		return;
	init_ctrl(sif);
	sif->framing = E1;
	return;
}

static void
set_chan_t1(struct softc *sif, char *ret)
{
	if (sif->framing == T1)
		return;
	strcpy(ret, "Error: T1 framing not implemented\n");
	return;
}

/*
 * NetGraph Stuff
 */

static int
ng_constructor(node_p *nodep)
{

	return (EINVAL);
}

static int
ng_shutdown(node_p nodep)
{

	return (EINVAL);
}

static void
ng_config(node_p node, char *set, char *ret)
{
	struct softc *sif;
	sif = node->private;

	printf("%s: CONFIG %p %p\n", sif->nodename, set, ret);
	if (set != NULL) {
		printf("%s CONFIG SET [%s]\n", sif->nodename, set);
		if (!strcmp(set, "line e1"))
			set_chan_e1(sif, ret);
		else if (!strcmp(set, "line t1"))
			set_chan_t1(sif, ret);
		else
			goto barf;
		return;
	}
	if (sif->framing == E1)
		strcat(ret, "line e1\n");
	if (sif->framing == T1)
		strcat(ret, "line t1\n");
	return;
barf:
	strcpy(ret, "Syntax Error\n");
	strcat(ret, "\tline [e1|t1]\n");
	strcat(ret, "\tcrc4\n");
	strcat(ret, "\tesf\n");
	return;
}

static int
ng_rcvmsg(node_p node, struct ng_mesg *msg, const char *retaddr, struct ng_mesg **resp, hook_p lasthook)
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
		sprintf(s, "Status for %s\n", sc->nodename);
		(*resp)->header.arglen = strlen(s) + 1;
		FREE(msg, M_NETGRAPH);
		return (0);
        }
	if (msg->header.cmd == NGM_TEXT_CONFIG) {
		if (msg->header.arglen) {
			s = (char *)msg->data;
			printf("text_config %d \"%s\"\n", msg->header.arglen, s);
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
		ng_config(node, s, r);
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
ng_newhook(node_p node, hook_p hook, const char *name)
{
	struct softc *sc;
	struct csoftc *csc;
	struct schan *sch;
	u_int32_t ts, chan;
	int nbit;

	sc = node->private;
	csc = sc->csc;

	printf("Newhook(\"%s\",\"%s\")\n", sc->nodename, name);
	if (name[0] != 't' || name[1] != 's')
		return (EINVAL);
	ts = parse_ts(name + 2, &nbit);
	if (ts == 0)
		return (EINVAL);
	chan = ffs(ts) - 1;
	if (sc->chan[chan] == NULL) {
		MALLOC(sch, struct schan *, sizeof(*sch), M_MUSYCC, M_WAITOK);
		bzero(sch, sizeof(*sch));
		sch->sc = sc;
		sch->state = DOWN;
		sch->chan = chan;
		sprintf(sch->hookname, name);	/* XXX overflow ? */
		sc->chan[chan] = sch;
	} else if (sc->chan[chan]->state == UP) {
		return (EBUSY);
	}
	sch = sc->chan[chan];
	sch->ts = ts;
	sch->hook = hook;
	sch->tx_limit = nbit * 8;
	hook->private = sch;
	return(0);
}

static int
ng_rcvdata(hook_p hook, struct mbuf *m, meta_p meta, struct mbuf **ret_m, meta_p *ret_meta)
{

	struct softc *sc;
	struct csoftc *csc;
	struct schan *sch;
	struct mdesc *md;
	u_int32_t ch, u, len;
	struct mbuf *m2;

	sch = hook->private;
	sc = sch->sc;
	csc = sc->csc;
	ch = sch->chan;

	NG_FREE_META(meta);
	meta = NULL;

	/* XXX: check channel state */

	m2 = m;
	len = m->m_pkthdr.len;
	while (len) {
		md = &sc->mdt[ch][sch->tx_next_md];

		if (++sch->tx_next_md >= sch->nmd)
			sch->tx_next_md = 0;

		md->data = vtophys(m->m_data);
		len -= m->m_len;
		u = 0x80000000;	/* OWNER = MUSYCC */
		if (len > 0) {
			md->m = 0;
		} else {
			u |= 1 << 29;	/* EOM */
			md->m = m2;
		}	

		u |= m->m_len;
		md->status = u;
		m = m->m_next;
	}
	return (0);
}

static int
ng_connect(hook_p hook)
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
	sc->ram->tcct[ch] = 0x2000; /* HDLC-FCS16 */
	sc->ram->rcct[ch] = 0x2000; /* HDLC-FCS16 */

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
	sch->nmd = nmd = 10 + nts * 2;
	sch->rx_last_md = 0;
	sch->tx_next_md = 0;
	sch->tx_last_md = 0;
	MALLOC(sc->mdt[ch], struct mdesc *, 
	    sizeof(struct mdesc) * nmd, M_MUSYCC, M_WAITOK);
	MALLOC(sc->mdr[ch], struct mdesc *, 
	    sizeof(struct mdesc) * nmd, M_MUSYCC, M_WAITOK);
	for (i = 0; i < nmd; i++) {
		if (i == nmd - 1) {
			sc->mdt[ch][i].next = vtophys(&sc->mdt[ch][0]);
			sc->mdr[ch][i].next = vtophys(&sc->mdr[ch][0]);
		} else {
			sc->mdt[ch][i].next = vtophys(&sc->mdt[ch][i + 1]);
			sc->mdr[ch][i].next = vtophys(&sc->mdr[ch][i + 1]);
		}
		sc->mdt[ch][i].status = 0;
		sc->mdt[ch][i].m = NULL;
		sc->mdt[ch][i].data = 0;

		MGETHDR(m, M_WAIT, MT_DATA);
		MCLGET(m, M_WAIT);
		sc->mdr[ch][i].m = m;
		sc->mdr[ch][i].data = vtophys(m->m_data);
		sc->mdr[ch][i].status = 1600; /* MTU */
	}

	/* Configure it into the chip */
	sc->ram->thp[ch] = vtophys(&sc->mdt[ch][0]);
	sc->ram->tmp[ch] = vtophys(&sc->mdt[ch][0]);
	sc->ram->rhp[ch] = vtophys(&sc->mdr[ch][0]);
	sc->ram->rmp[ch] = vtophys(&sc->mdr[ch][0]);

	/* Activate the Channel */
	sc->reg->srd = sc->last = 0x0800 + ch;
	tsleep(&sc->last, PZERO + PCATCH, "con3", hz);
	sc->reg->srd = sc->last = 0x0820 + ch;
	tsleep(&sc->last, PZERO + PCATCH, "con4", hz);

	return (0);
}

static int
ng_disconnect(hook_p hook)
{
	struct softc *sc;
	struct csoftc *csc;
	struct schan *sch;
	int i, ch;

	sch = hook->private;
	sc = sch->sc;
	csc = sc->csc;
	ch = sch->chan;

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

	/* Deactivate the channel */
	sc->reg->srd = sc->last = 0x0900 + sch->chan;
	tsleep(&sc->last, PZERO + PCATCH, "con3", hz);
	sc->reg->srd = sc->last = 0x0920 + sch->chan;
	tsleep(&sc->last, PZERO + PCATCH, "con4", hz);

	for (i = 0; i < 32; i++) {
		if (sch->ts & (1 << i)) {
			sc->ram->rtsm[i] = 0;
			sc->ram->ttsm[i] = 0;
		}
	}
#if 0
	/* We don't really need to reread the Time Slot Map */
	sc->reg->srd = sc->last = 0x1800;
	tsleep(&sc->last, PZERO + PCATCH, "con1", hz);
	sc->reg->srd = sc->last = 0x1820;
	tsleep(&sc->last, PZERO + PCATCH, "con2", hz);
#endif

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
	struct softc *sif;
	int rid, i, error;
	int f;
	u_int32_t	*u32p;
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
		MALLOC(csc, struct csoftc *, sizeof(*csc), M_MUSYCC, M_WAITOK);
		bzero(csc, sizeof(*csc));
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
	if ((u32p[0x1200] & 0xffffff00) != 0x13760400) {
		printf("Not a LMC1504 (ID is 0x%08x).  Bailing out.\n",
		    u32p[0x1200]);
		return(ENXIO);
	}
	printf("Found <LanMedia LMC1504>\n");
	csc->nchan = 4;

	u32p[0x1000] = 0xfe;	/* XXX: control-register */
	for (i = 0; i < csc->nchan; i++) {
		sif = &csc->serial[i];
		sif->csc = csc;
		sif->last = 0xffffffff;
		sif->ds8370 = (u_int32_t *)
		    (csc->virbase[1] + i * 0x800);
		sif->ds847x = csc->virbase[0] + i * 0x800;
		sif->reg = (struct globalr *)
		    (csc->virbase[0] + i * 0x800);
		MALLOC(sif->mycg, struct mycg *, 
		    sizeof(struct mycg), M_MUSYCC, M_WAITOK);
		bzero(sif->mycg, sizeof(struct mycg));
		sif->ram = &sif->mycg->cg;

		error = ng_make_node_common(&ngtypestruct, &sif->node);
		if (error) {
			printf("ng_make_node_common() failed %d\n", error);
			continue;
		}	
		sif->node->private = sif;
		sprintf(sif->nodename, "pci%d-%d-%d-%d",
			device_get_unit(device_get_parent(self)),
			csc->bus,
			csc->slot,
			i);
		error = ng_name_node(sif->node, sif->nodename);
	}
	csc->ram = (struct globalr *)&csc->serial[0].mycg->cg;
	init_847x(csc);

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

