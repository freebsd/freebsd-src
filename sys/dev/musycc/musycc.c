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

#define NMD	10

#define NIQD	32

struct softc {
	int	unit, bus, slot;
	LIST_ENTRY(softc) list;

	device_t f[2];
	struct resource *irq[2];
	void *intrhand[2];
	vm_offset_t physbase[2];
	u_char *virbase[2];

	int nchan;

	struct serial_if {
		enum {WHOKNOWS, E1, T1}	framing;
		u_int32_t last;
		struct softc *sc;
		u_int32_t *ds8370;
		void	*ds847x;
		struct globalr *reg;
		struct groupr *ram;
		struct mycg *mycg;
		struct mdesc *mdt[NHDLC];
		struct mdesc *mdr[NHDLC];
		node_p node;			/* NG node */
		char nodename[NG_NODELEN + 1];	/* NG nodename */
	} serial[NPORT];
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


static LIST_HEAD(, softc) sc_list = LIST_HEAD_INITIALIZER(&sc_list);

static void
poke_847x(void *dummy)
{
	static int count;
	int i;
	struct softc *sc;

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
init_847x(struct softc *sc)
{
	struct serial_if *sif;

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
init_chan(struct serial_if *sif)
{
	int i, j;
	struct mbuf *m;

	printf("init_chan(%p) [%s] [%08x]\n", sif, sif->nodename, sif->sc->reg->glcd);
	init_8370(sif->ds8370);
	tsleep(sif, PZERO | PCATCH, "ds8370", hz);
	sif->reg->gbp = vtophys(sif->ram);
	sif->ram->grcd =  0x00000001;	/* RXENBL */
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
		sif->ram->ttsm[i] = i + (4 << 5);
		sif->ram->rtsm[i] = i + (4 << 5);
		sif->ram->tcct[i] = 0x2000; /* HDLC-FCS16 */
		sif->ram->rcct[i] = 0x2000; /* HDLC-FCS16 */
		sif->ram->tcct[i] |= 0x00000; /* BUFFLEN */
		sif->ram->rcct[i] |= 0x00000; /* BUFFLEN */
		sif->ram->tcct[i] |= ((i + i) << 24); /* BUFFLOC */
		sif->ram->rcct[i] |= ((i + i) << 24); /* BUFFLOC */
		MALLOC(sif->mdt[i], struct mdesc *, 
		    sizeof(struct mdesc) * NMD, M_MUSYCC, M_WAITOK);
		MALLOC(sif->mdr[i], struct mdesc *, 
		    sizeof(struct mdesc) * NMD, M_MUSYCC, M_WAITOK);
		for (j = 0; j < NMD; j ++) {
			sif->mdt[i][j].next = vtophys(&sif->mdt[i][j+1]);
			sif->mdt[i][j].status = 0;

			sif->mdr[i][j].next = vtophys(&sif->mdr[i][j+1]);
			MGETHDR(m, M_WAIT, MT_DATA);
			MCLGET(m, M_WAIT);
			sif->mdr[i][j].m = m;
			sif->mdr[i][j].data = vtophys(m->m_data);
			sif->mdr[i][j].status = 1600;
		}
		/* Close the loop */
		sif->mdt[i][NMD-1].next = vtophys(&sif->mdt[i][0]);
		sif->mdr[i][NMD-1].next = vtophys(&sif->mdr[i][0]);
		sif->ram->thp[i] = vtophys(&sif->mdt[i][0]);
		sif->ram->tmp[i] = vtophys(&sif->mdt[i][0]);
		sif->ram->rhp[i] = vtophys(&sif->mdr[i][0]);
		sif->ram->rmp[i] = vtophys(&sif->mdr[i][0]);
	}
	sif->last = 0x500;
	sif->reg->srd = sif->last;
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
 * Interrupt
 */

static void
musycc_intr0(void *arg)
{
	int i, j, g, k, ch, ev;
	struct softc *sc;
	u_int32_t u,n,c, s;
	struct serial_if *sif;

	sc = arg;

	u = sc->serial[0].reg->isd;
	c = u & 0x7fff;
	if (c == 0)
		return;
	n = u >> 16;
	for (i = n; i < n + c; i++) {
		j = i % NIQD;
		g = (sc->iqd[j] >> 29) & 0x3;
		g |= (sc->iqd[j] >> (14-2)) & 0x4;
		ch = (sc->iqd[j] >> 24) & 0x1f;
		ev = (sc->iqd[j] >> 20) & 0xf;
		sif = &sc->serial[g];
		if (ev == 1) {
			if (sif->last) {
				printf("%08x %d", sc->iqd[j], g);
				printf("/%s", sc->iqd[j] & 0x80000000 ? "T" : "R");
				printf(" cmd %08x\n", sif->last);
			}
			if (sif->last == 0x500) {
				sif->last = 0x520;
				sif->reg->srd = sif->last;
			} else if (sif->last == 0x520) {
				sif->last = 0x800;
				sif->reg->srd = sif->last;
			} else if ((sif->last & ~0x1f) == 0x800) {
				sif->last++;
				if (sif->last == 0x820)
					sif->last = 0xffffffff;
				sif->reg->srd = sif->last;
			} else {
				sif->last = 0xffffffff;
			}
		} else {
			printf("%08x %d", sc->iqd[j], g);
			printf("/%s", sc->iqd[j] & 0x80000000 ? "T" : "R");
			printf("/%02d", ch);
			printf(" %02d", ev);
			printf(":%02d", (sc->iqd[j] >> 16) & 0xf);
			if (sif->mdr[ch] != NULL)
				for (k = 0; k < NMD; k++) {
					s = sif->mdr[ch][k].status;
					if (s & 0x80000000) {
						printf(" >> %08x ", sif->mdr[ch][k].status);
						sif->mdr[ch][k].m->m_len = 
						    sif->mdr[ch][k].m->m_pkthdr.len = s & 0x001f;
						m_print(sif->mdr[ch][k].m);
					} 
					sif->mdr[ch][k].status = 1600;
				}
			printf("\n");
		}
		sc->iqd[i] = 0xffffffff;
	}
	n += c;
	n %= NIQD;
	sc->serial[0].reg->isd = n << 16;
}

static void
musycc_intr1(void *arg)
{
	int i, j;
	struct softc *sc;
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
set_chan_e1(struct serial_if *sif, char *ret)
{
	if (sif->framing == E1)
		return;
	init_chan(sif);
	sif->framing = E1;
	return;
}

static void
set_chan_t1(struct serial_if *sif, char *ret)
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
	struct serial_if *sif;
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
	struct serial_if *sif;
	char *s, *r;

	sif = node->private;

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
		sprintf(s, "Status for %s\n", sif->nodename);
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

	return(EINVAL);
}

static int
ng_rcvdata(hook_p hook, struct mbuf *m, meta_p meta, struct mbuf **ret_m, meta_p *ret_meta)
{

	NG_FREE_DATA(m, meta);
	return (0);
}

static int
ng_connect(hook_p hook)
{

	return (0);
}

static int
ng_disconnect(hook_p hook)
{

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
	struct softc *sc;
	struct resource *res;
	struct serial_if *sif;
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
	/* For function zero allocate a softc */
	if (f == 0) {
		MALLOC(sc, struct softc *, sizeof(*sc), M_MUSYCC, M_WAITOK);
		bzero(sc, sizeof(*sc));
		sc->bus = pci_get_bus(self);
		sc->slot = pci_get_slot(self);
		LIST_INSERT_HEAD(&sc_list, sc, list);
	} else {
		LIST_FOREACH(sc, &sc_list, list) {
			if (sc->bus != pci_get_bus(self))
				continue;
			if (sc->slot != pci_get_slot(self))
				continue;
			break;
		}
	}
	sc->f[f] = self;
	device_set_softc(self, sc);
	rid = PCIR_MAPS;
	res = bus_alloc_resource(self, SYS_RES_MEMORY, &rid,
	    0, ~0, 1, RF_ACTIVE);
	if (res == NULL) {
		device_printf(self, "Could not map memory\n");
		return ENXIO;
	}
	sc->virbase[f] = (u_char *)rman_get_virtual(res);
	sc->physbase[f] = rman_get_start(res);

	/* Allocate interrupt */
	rid = 0;
	sc->irq[f] = bus_alloc_resource(self, SYS_RES_IRQ, &rid, 0, ~0,
	    1, RF_SHAREABLE | RF_ACTIVE);

	if (sc->irq[f] == NULL) {
		printf("couldn't map interrupt\n");
		return(ENXIO);
	}

	error = bus_setup_intr(self, sc->irq[f], INTR_TYPE_NET,
	    f == 0 ? musycc_intr0 : musycc_intr1, sc, &sc->intrhand[f]);

	if (error) {
		printf("couldn't set up irq\n");
		return(ENXIO);
	}

	if (f == 0)
		return (0);

	for (i = 0; i < 2; i++)
		printf("f%d: device %p virtual %p physical %08x\n",
		    i, sc->f[i], sc->virbase[i], sc->physbase[i]);

	sc->reg = (struct globalr *)sc->virbase[0];
	sc->reg->glcd = 0x3f30;	/* XXX: designer magic */
	u32p = (u_int32_t *)sc->virbase[1];
	if ((u32p[0x1200] & 0xffffff00) != 0x13760400) {
		printf("Not a LMC1504 (ID is 0x%08x).  Bailing out.\n",
		    u32p[0x1200]);
		return(ENXIO);
	}
	printf("Found <LanMedia LMC1504>\n");
	sc->nchan = 4;

	u32p[0x1000] = 0xfe;	/* XXX: control-register */
	for (i = 0; i < sc->nchan; i++) {
		sif = &sc->serial[i];
		sif->sc = sc;
		sif->last = 0xffffffff;
		sif->ds8370 = (u_int32_t *)
		    (sc->virbase[1] + i * 0x800);
		sif->ds847x = sc->virbase[0] + i * 0x800;
		sif->reg = (struct globalr *)
		    (sc->virbase[0] + i * 0x800);
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
			sc->bus,
			sc->slot,
			i);
		error = ng_name_node(sif->node, sif->nodename);
#if 0
		init_chan(&sc->serial[i]);
#endif
	}
	sc->ram = (struct globalr *)&sc->serial[0].mycg->cg;
	init_847x(sc);

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

