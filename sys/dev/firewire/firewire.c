/*
 * Copyright (c) 2003 Hidetoshi Shimokawa
 * Copyright (c) 1998-2002 Katsushi Kobayashi and Hidetoshi Shimokawa
 * All rights reserved.
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
 *    must display the acknowledgement as bellow:
 *
 *    This product includes software developed by K. Kobayashi and H. Shimokawa
 *
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 * 
 * $FreeBSD$
 *
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/types.h>

#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/conf.h>
#include <sys/sysctl.h>

#if __FreeBSD_version < 500000
#include <machine/clock.h>	/* for DELAY() */
#endif

#include <sys/bus.h>		/* used by smbus and newbus */
#include <machine/bus.h>

#include <dev/firewire/firewire.h>
#include <dev/firewire/firewirereg.h>
#include <dev/firewire/fwmem.h>
#include <dev/firewire/iec13213.h>
#include <dev/firewire/iec68113.h>

struct crom_src_buf {
	struct crom_src	src;
	struct crom_chunk root;
	struct crom_chunk vendor;
	struct crom_chunk hw;
};

int firewire_debug=0, try_bmr=1;
SYSCTL_INT(_debug, OID_AUTO, firewire_debug, CTLFLAG_RW, &firewire_debug, 0,
	"FireWire driver debug flag");
SYSCTL_NODE(_hw, OID_AUTO, firewire, CTLFLAG_RD, 0, "FireWire Subsystem");
SYSCTL_INT(_hw_firewire, OID_AUTO, try_bmr, CTLFLAG_RW, &try_bmr, 0,
	"Try to be a bus manager");

MALLOC_DEFINE(M_FW, "firewire", "FireWire");
MALLOC_DEFINE(M_FWXFER, "fw_xfer", "XFER/FireWire");

#define FW_MAXASYRTY 4
#define FW_MAXDEVRCNT 4

devclass_t firewire_devclass;

static int firewire_match      __P((device_t));
static int firewire_attach      __P((device_t));
static int firewire_detach      __P((device_t));
static int firewire_resume      __P((device_t));
#if 0
static int firewire_shutdown    __P((device_t));
#endif
static device_t firewire_add_child   __P((device_t, int, const char *, int));
static void fw_try_bmr __P((void *));
static void fw_try_bmr_callback __P((struct fw_xfer *));
static void fw_asystart __P((struct fw_xfer *));
static int fw_get_tlabel __P((struct firewire_comm *, struct fw_xfer *));
static void fw_bus_probe __P((struct firewire_comm *));
static void fw_bus_explore __P((struct firewire_comm *));
static void fw_bus_explore_callback __P((struct fw_xfer *));
static void fw_attach_dev __P((struct firewire_comm *));
#ifdef FW_VMACCESS
static void fw_vmaccess __P((struct fw_xfer *));
#endif
struct fw_xfer *asyreqq __P((struct firewire_comm *, u_int8_t, u_int8_t, u_int8_t,
	u_int32_t, u_int32_t, void (*)__P((struct fw_xfer *))));
static int fw_bmr __P((struct firewire_comm *));

static device_method_t firewire_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		firewire_match),
	DEVMETHOD(device_attach,	firewire_attach),
	DEVMETHOD(device_detach,	firewire_detach),
	DEVMETHOD(device_suspend,	bus_generic_suspend),
	DEVMETHOD(device_resume,	firewire_resume),
	DEVMETHOD(device_shutdown,	bus_generic_shutdown),

	/* Bus interface */
	DEVMETHOD(bus_add_child,	firewire_add_child),
	DEVMETHOD(bus_print_child,	bus_generic_print_child),

	{ 0, 0 }
};
char linkspeed[7][0x10]={"S100","S200","S400","S800","S1600","S3200","Unknown"};

/* IEEE-1394a Table C-2 Gap count as a function of hops*/
#define MAX_GAPHOP 15
u_int gap_cnt[] = { 5,  5,  7,  8, 10, 13, 16, 18,
		   21, 24, 26, 29, 32, 35, 37, 40};

static driver_t firewire_driver = {
	"firewire",
	firewire_methods,
	sizeof(struct firewire_softc),
};

/*
 * Lookup fwdev by node id.
 */
struct fw_device *
fw_noderesolve_nodeid(struct firewire_comm *fc, int dst)
{
	struct fw_device *fwdev;
	int s;

	s = splfw();
	STAILQ_FOREACH(fwdev, &fc->devices, link)
		if (fwdev->dst == dst)
			break;
	splx(s);

	if(fwdev == NULL) return NULL;
	if(fwdev->status == FWDEVINVAL) return NULL;
	return fwdev;
}

/*
 * Lookup fwdev by EUI64.
 */
struct fw_device *
fw_noderesolve_eui64(struct firewire_comm *fc, struct fw_eui64 *eui)
{
	struct fw_device *fwdev;
	int s;

	s = splfw();
	STAILQ_FOREACH(fwdev, &fc->devices, link)
		if (FW_EUI64_EQUAL(fwdev->eui, *eui))
			break;
	splx(s);

	if(fwdev == NULL) return NULL;
	if(fwdev->status == FWDEVINVAL) return NULL;
	return fwdev;
}

/*
 * Async. request procedure for userland application.
 */
int
fw_asyreq(struct firewire_comm *fc, int sub, struct fw_xfer *xfer)
{
	int err = 0;
	struct fw_xferq *xferq;
	int tl = 0, len;
	struct fw_pkt *fp;
	int tcode;
	struct tcode_info *info;

	if(xfer == NULL) return EINVAL;
	if(xfer->send.len > MAXREC(fc->maxrec)){
		printf("send.len > maxrec\n");
		return EINVAL;
	}
	if(xfer->act.hand == NULL){
		printf("act.hand == NULL\n");
		return EINVAL;
	}
	fp = (struct fw_pkt *)xfer->send.buf;

	tcode = fp->mode.common.tcode & 0xf;
	info = &fc->tcode[tcode];
	if (info->flag == 0) {
		printf("invalid tcode=%d\n", tcode);
		return EINVAL;
	}
	if (info->flag & FWTI_REQ)
		xferq = fc->atq;
	else
		xferq = fc->ats;
	len = info->hdr_len;
	if (info->flag & FWTI_BLOCK_STR)
		len += fp->mode.stream.len;
	else if (info->flag & FWTI_BLOCK_ASY)
		len += fp->mode.rresb.len;
	if( len >  xfer->send.len ){
		printf("len(%d) > send.len(%d) (tcode=%d)\n",
				len, xfer->send.len, tcode);
		return EINVAL; 
	}
	xfer->send.len = len;

	if(xferq->start == NULL){
		printf("xferq->start == NULL\n");
		return EINVAL;
	}
	if(!(xferq->queued < xferq->maxq)){
		device_printf(fc->bdev, "Discard a packet (queued=%d)\n",
			xferq->queued);
		return EINVAL;
	}


	if (info->flag & FWTI_TLABEL) {
		if((tl = fw_get_tlabel(fc, xfer)) == -1 )
			return EIO;
		fp->mode.hdr.tlrt = tl << 2;
	}

	xfer->tl = tl;
	xfer->resp = 0;
	xfer->fc = fc;
	xfer->q = xferq;
	xfer->retry_req = fw_asybusy;

	fw_asystart(xfer);
	return err;
}
/*
 * Wakeup blocked process.
 */
void
fw_asy_callback(struct fw_xfer *xfer){
	wakeup(xfer);
	return;
}
/*
 * Postpone to later retry.
 */
void fw_asybusy(struct fw_xfer *xfer){
	printf("fw_asybusy\n");
/*
	xfer->ch =  timeout((timeout_t *)fw_asystart, (void *)xfer, 20000);
*/
	DELAY(20000);
	fw_asystart(xfer);
	return;
}

/*
 * Async. request with given xfer structure.
 */
static void
fw_asystart(struct fw_xfer *xfer)
{
	struct firewire_comm *fc = xfer->fc;
	int s;
	if(xfer->retry++ >= fc->max_asyretry){
		device_printf(fc->bdev, "max_asyretry exceeded\n");
		xfer->resp = EBUSY;
		xfer->state = FWXF_BUSY;
		xfer->act.hand(xfer);
		return;
	}
#if 0 /* XXX allow bus explore packets only after bus rest */
	if (fc->status < FWBUSEXPLORE) {
		xfer->resp = EAGAIN;
		xfer->state = FWXF_BUSY;
		if (xfer->act.hand != NULL)
			xfer->act.hand(xfer);
		return;
	}
#endif
	s = splfw();
	xfer->state = FWXF_INQ;
	STAILQ_INSERT_TAIL(&xfer->q->q, xfer, link);
	xfer->q->queued ++;
	splx(s);
	/* XXX just queue for mbuf */
	if (xfer->mbuf == NULL)
		xfer->q->start(fc);
	return;
}

static int
firewire_match( device_t dev )
{
	device_set_desc(dev, "IEEE1394(FireWire) bus");
	return -140;
}

static void
firewire_xfer_timeout(struct firewire_comm *fc)
{
	struct fw_xfer *xfer;
	struct tlabel *tl;
	struct timeval tv;
	struct timeval split_timeout;
	int i, s;

	split_timeout.tv_sec = 6;
	split_timeout.tv_usec = 0;

	microtime(&tv);
	timevalsub(&tv, &split_timeout);

	s = splfw();
	for (i = 0; i < 0x40; i ++) {
		while ((tl = STAILQ_FIRST(&fc->tlabels[i])) != NULL) {
			xfer = tl->xfer;
			if (timevalcmp(&xfer->tv, &tv, >))
				/* the rests are newer than this */
				break;
			device_printf(fc->bdev,
				"split transaction timeout dst=0x%x tl=0x%x state=%d\n",
				xfer->dst, i, xfer->state);
			xfer->resp = ETIMEDOUT;
			STAILQ_REMOVE_HEAD(&fc->tlabels[i], link);
			fw_xfer_done(xfer);
		}
	}
	splx(s);
}

static void
firewire_watchdog(void *arg)
{
	struct firewire_comm *fc;

	fc = (struct firewire_comm *)arg;
	firewire_xfer_timeout(fc);
	fc->timeout(fc);
	callout_reset(&fc->timeout_callout, hz,
			(void *)firewire_watchdog, (void *)fc);
}

/*
 * The attach routine.
 */
static int
firewire_attach(device_t dev)
{
	int unit;
	struct firewire_softc *sc = device_get_softc(dev);
	device_t pa = device_get_parent(dev);
	struct firewire_comm *fc;

	fc = (struct firewire_comm *)device_get_softc(pa);
	sc->fc = fc;
	fc->status = FWBUSNOTREADY;

	unit = device_get_unit(dev);
	if( fc->nisodma > FWMAXNDMA) fc->nisodma = FWMAXNDMA;

	fwdev_makedev(sc);

	CALLOUT_INIT(&sc->fc->timeout_callout);
	CALLOUT_INIT(&sc->fc->bmr_callout);
	CALLOUT_INIT(&sc->fc->retry_probe_callout);
	CALLOUT_INIT(&sc->fc->busprobe_callout);

	callout_reset(&sc->fc->timeout_callout, hz,
			(void *)firewire_watchdog, (void *)sc->fc);

	/* Locate our children */
	bus_generic_probe(dev);

	/* launch attachement of the added children */
	bus_generic_attach(dev);

	/* bus_reset */
	fc->ibr(fc);

	return 0;
}

/*
 * Attach it as child.
 */
static device_t
firewire_add_child(device_t dev, int order, const char *name, int unit)
{
        device_t child;
	struct firewire_softc *sc;

	sc = (struct firewire_softc *)device_get_softc(dev);
	child = device_add_child(dev, name, unit);
	if (child) {
		device_set_ivars(child, sc->fc);
		device_probe_and_attach(child);
	}

	return child;
}

static int
firewire_resume(device_t dev)
{
	struct firewire_softc *sc;

	sc = (struct firewire_softc *)device_get_softc(dev);
	sc->fc->status = FWBUSNOTREADY;
	
	bus_generic_resume(dev);

	return(0);
}

/*
 * Dettach it.
 */
static int
firewire_detach(device_t dev)
{
	struct firewire_softc *sc;
	struct csrdir *csrd, *next;
	struct fw_device *fwdev, *fwdev_next;
	int err;

	sc = (struct firewire_softc *)device_get_softc(dev);
	if ((err = fwdev_destroydev(sc)) != 0)
		return err;

	if ((err = bus_generic_detach(dev)) != 0)
		return err;

	callout_stop(&sc->fc->timeout_callout);
	callout_stop(&sc->fc->bmr_callout);
	callout_stop(&sc->fc->retry_probe_callout);
	callout_stop(&sc->fc->busprobe_callout);

	/* XXX xfree_free and untimeout on all xfers */
	for (fwdev = STAILQ_FIRST(&sc->fc->devices); fwdev != NULL;
							fwdev = fwdev_next) {
		fwdev_next = STAILQ_NEXT(fwdev, link);
		free(fwdev, M_FW);
	}
	for (csrd = SLIST_FIRST(&sc->fc->csrfree); csrd != NULL; csrd = next) {
		next = SLIST_NEXT(csrd, link);
		free(csrd, M_FW);
	}
	free(sc->fc->topology_map, M_FW);
	free(sc->fc->speed_map, M_FW);
	free(sc->fc->crom_src_buf, M_FW);
	return(0);
}
#if 0
static int
firewire_shutdown( device_t dev )
{
	return 0;
}
#endif


static void
fw_xferq_drain(struct fw_xferq *xferq)
{
	struct fw_xfer *xfer;

	while ((xfer = STAILQ_FIRST(&xferq->q)) != NULL) {
		STAILQ_REMOVE_HEAD(&xferq->q, link);
		xferq->queued --;
		xfer->resp = EAGAIN;
		fw_xfer_done(xfer);
	}
}

void
fw_drain_txq(struct firewire_comm *fc)
{
	int i;

	fw_xferq_drain(fc->atq);
	fw_xferq_drain(fc->ats);
	for(i = 0; i < fc->nisodma; i++)
		fw_xferq_drain(fc->it[i]);
}

static void
fw_reset_csr(struct firewire_comm *fc)
{
	int i;

	CSRARC(fc, STATE_CLEAR)
			= 1 << 23 | 0 << 17 | 1 << 16 | 1 << 15 | 1 << 14 ;
	CSRARC(fc, STATE_SET) = CSRARC(fc, STATE_CLEAR);
	CSRARC(fc, NODE_IDS) = 0x3f;

	CSRARC(fc, TOPO_MAP + 8) = 0;
	fc->irm = -1;

	fc->max_node = -1;

	for(i = 2; i < 0x100/4 - 2 ; i++){
		CSRARC(fc, SPED_MAP + i * 4) = 0;
	}
	CSRARC(fc, STATE_CLEAR) = 1 << 23 | 0 << 17 | 1 << 16 | 1 << 15 | 1 << 14 ;
	CSRARC(fc, STATE_SET) = CSRARC(fc, STATE_CLEAR);
	CSRARC(fc, RESET_START) = 0;
	CSRARC(fc, SPLIT_TIMEOUT_HI) = 0;
	CSRARC(fc, SPLIT_TIMEOUT_LO) = 800 << 19;
	CSRARC(fc, CYCLE_TIME) = 0x0;
	CSRARC(fc, BUS_TIME) = 0x0;
	CSRARC(fc, BUS_MGR_ID) = 0x3f;
	CSRARC(fc, BANDWIDTH_AV) = 4915;
	CSRARC(fc, CHANNELS_AV_HI) = 0xffffffff;
	CSRARC(fc, CHANNELS_AV_LO) = 0xffffffff;
	CSRARC(fc, IP_CHANNELS) = (1 << 31);

	CSRARC(fc, CONF_ROM) = 0x04 << 24;
	CSRARC(fc, CONF_ROM + 4) = 0x31333934; /* means strings 1394 */
	CSRARC(fc, CONF_ROM + 8) = 1 << 31 | 1 << 30 | 1 << 29 |
				1 << 28 | 0xff << 16 | 0x09 << 8;
	CSRARC(fc, CONF_ROM + 0xc) = 0;

/* DV depend CSRs see blue book */
	CSRARC(fc, oPCR) &= ~DV_BROADCAST_ON; 
	CSRARC(fc, iPCR) &= ~DV_BROADCAST_ON; 

	CSRARC(fc, STATE_CLEAR) &= ~(1 << 23 | 1 << 15 | 1 << 14 );
	CSRARC(fc, STATE_SET) = CSRARC(fc, STATE_CLEAR);
}

static void
fw_init_crom(struct firewire_comm *fc)
{
	struct crom_src *src;

	fc->crom_src_buf = (struct crom_src_buf *)
		malloc(sizeof(struct crom_src_buf), M_FW, M_WAITOK | M_ZERO);
	if (fc->crom_src_buf == NULL)
		return;

	src = &fc->crom_src_buf->src;
	bzero(src, sizeof(struct crom_src));

	/* BUS info sample */
	src->hdr.info_len = 4;

	src->businfo.bus_name = CSR_BUS_NAME_IEEE1394;

	src->businfo.irmc = 1;
	src->businfo.cmc = 1;
	src->businfo.isc = 1;
	src->businfo.bmc = 1;
	src->businfo.pmc = 0;
	src->businfo.cyc_clk_acc = 100;
	src->businfo.max_rec = fc->maxrec;
	src->businfo.max_rom = MAXROM_4;
	src->businfo.generation = 1;
	src->businfo.link_spd = fc->speed;

	src->businfo.eui64.hi = fc->eui.hi;
	src->businfo.eui64.lo = fc->eui.lo;

	STAILQ_INIT(&src->chunk_list);

	fc->crom_src = src;
	fc->crom_root = &fc->crom_src_buf->root;
}

static void
fw_reset_crom(struct firewire_comm *fc)
{
	struct crom_src_buf *buf;
	struct crom_src *src;
	struct crom_chunk *root;

	if (fc->crom_src_buf == NULL)
		fw_init_crom(fc);

	buf =  fc->crom_src_buf;
	src = fc->crom_src;
	root = fc->crom_root;

	STAILQ_INIT(&src->chunk_list);

	bzero(root, sizeof(struct crom_chunk));
	crom_add_chunk(src, NULL, root, 0);
	crom_add_entry(root, CSRKEY_NCAP, 0x0083c0); /* XXX */
	/* private company_id */
	crom_add_entry(root, CSRKEY_VENDOR, CSRVAL_VENDOR_PRIVATE);
	crom_add_simple_text(src, root, &buf->vendor, "FreeBSD Project");
	crom_add_entry(root, CSRKEY_HW, __FreeBSD_version);
	crom_add_simple_text(src, root, &buf->hw, hostname);
}

/*
 * Called after bus reset.
 */
void
fw_busreset(struct firewire_comm *fc)
{
	struct firewire_dev_comm *fdc;
	struct crom_src *src;
	device_t *devlistp;
	void *newrom;
	int i, devcnt;

	switch(fc->status){
	case FWBUSMGRELECT:
		callout_stop(&fc->bmr_callout);
		break;
	default:
		break;
	}
	fc->status = FWBUSRESET;
	fw_reset_csr(fc);
	fw_reset_crom(fc);

	if (device_get_children(fc->bdev, &devlistp, &devcnt) == 0) {
		for( i = 0 ; i < devcnt ; i++)
			if (device_get_state(devlistp[i]) >= DS_ATTACHED)  {
				fdc = device_get_softc(devlistp[i]);
				if (fdc->post_busreset != NULL)
					fdc->post_busreset(fdc);
			}
		free(devlistp, M_TEMP);
	}

	newrom = malloc(CROMSIZE, M_FW, M_NOWAIT | M_ZERO);
	src = &fc->crom_src_buf->src;
	crom_load(src, (u_int32_t *)newrom, CROMSIZE);
	if (bcmp(newrom, fc->config_rom, CROMSIZE) != 0) {
		/* bump generation and reload */
		src->businfo.generation ++;
		/* generation must be between 0x2 and 0xF */
		if (src->businfo.generation < 2)
			src->businfo.generation ++;
		crom_load(src, (u_int32_t *)newrom, CROMSIZE);
		bcopy(newrom, (void *)fc->config_rom, CROMSIZE);
	}
	free(newrom, M_FW);
}

/* Call once after reboot */
void fw_init(struct firewire_comm *fc)
{
	int i;
	struct csrdir *csrd;
#ifdef FW_VMACCESS
	struct fw_xfer *xfer;
	struct fw_bind *fwb;
#endif

	fc->max_asyretry = FW_MAXASYRTY;

	fc->arq->queued = 0;
	fc->ars->queued = 0;
	fc->atq->queued = 0;
	fc->ats->queued = 0;

	fc->arq->buf = NULL;
	fc->ars->buf = NULL;
	fc->atq->buf = NULL;
	fc->ats->buf = NULL;

	fc->arq->flag = 0;
	fc->ars->flag = 0;
	fc->atq->flag = 0;
	fc->ats->flag = 0;

	STAILQ_INIT(&fc->atq->q);
	STAILQ_INIT(&fc->ats->q);

	for( i = 0 ; i < fc->nisodma ; i ++ ){
		fc->it[i]->queued = 0;
		fc->ir[i]->queued = 0;

		fc->it[i]->start = NULL;
		fc->ir[i]->start = NULL;

		fc->it[i]->buf = NULL;
		fc->ir[i]->buf = NULL;

		fc->it[i]->flag = FWXFERQ_STREAM;
		fc->ir[i]->flag = FWXFERQ_STREAM;

		STAILQ_INIT(&fc->it[i]->q);
		STAILQ_INIT(&fc->ir[i]->q);

		STAILQ_INIT(&fc->it[i]->binds);
		STAILQ_INIT(&fc->ir[i]->binds);
	}

	fc->arq->maxq = FWMAXQUEUE;
	fc->ars->maxq = FWMAXQUEUE;
	fc->atq->maxq = FWMAXQUEUE;
	fc->ats->maxq = FWMAXQUEUE;

	for( i = 0 ; i < fc->nisodma ; i++){
		fc->ir[i]->maxq = FWMAXQUEUE;
		fc->it[i]->maxq = FWMAXQUEUE;
	}
/* Initialize csr registers */
	fc->topology_map = (struct fw_topology_map *)malloc(
				sizeof(struct fw_topology_map),
				M_FW, M_NOWAIT | M_ZERO);
	fc->speed_map = (struct fw_speed_map *)malloc(
				sizeof(struct fw_speed_map),
				M_FW, M_NOWAIT | M_ZERO);
	CSRARC(fc, TOPO_MAP) = 0x3f1 << 16;
	CSRARC(fc, TOPO_MAP + 4) = 1;
	CSRARC(fc, SPED_MAP) = 0x3f1 << 16;
	CSRARC(fc, SPED_MAP + 4) = 1;

	STAILQ_INIT(&fc->devices);
	STAILQ_INIT(&fc->pending);

/* Initialize csr ROM work space */
	SLIST_INIT(&fc->ongocsr);
	SLIST_INIT(&fc->csrfree);
	for( i = 0 ; i < FWMAXCSRDIR ; i++){
		csrd = (struct csrdir *) malloc(sizeof(struct csrdir), M_FW,M_NOWAIT);
		if(csrd == NULL) break;
		SLIST_INSERT_HEAD(&fc->csrfree, csrd, link);
	}

/* Initialize Async handlers */
	STAILQ_INIT(&fc->binds);
	for( i = 0 ; i < 0x40 ; i++){
		STAILQ_INIT(&fc->tlabels[i]);
	}

/* DV depend CSRs see blue book */
#if 0
	CSRARC(fc, oMPR) = 0x3fff0001; /* # output channel = 1 */
	CSRARC(fc, oPCR) = 0x8000007a;
	for(i = 4 ; i < 0x7c/4 ; i+=4){
		CSRARC(fc, i + oPCR) = 0x8000007a; 
	}
 
	CSRARC(fc, iMPR) = 0x00ff0001; /* # input channel = 1 */
	CSRARC(fc, iPCR) = 0x803f0000;
	for(i = 4 ; i < 0x7c/4 ; i+=4){
		CSRARC(fc, i + iPCR) = 0x0; 
	}
#endif

	fc->crom_src_buf = NULL;

#ifdef FW_VMACCESS
	xfer = fw_xfer_alloc();
	if(xfer == NULL) return;

	fwb = (struct fw_bind *)malloc(sizeof (struct fw_bind), M_FW, M_NOWAIT);
	if(fwb == NULL){
		fw_xfer_free(xfer);
	}
	xfer->act.hand = fw_vmaccess;
	xfer->fc = fc;
	xfer->sc = NULL;

	fwb->start_hi = 0x2;
	fwb->start_lo = 0;
	fwb->addrlen = 0xffffffff;
	fwb->xfer = xfer;
	fw_bindadd(fc, fwb);
#endif
}

/*
 * To lookup binded process from IEEE1394 address.
 */
struct fw_bind *
fw_bindlookup(struct firewire_comm *fc, u_int32_t dest_hi, u_int32_t dest_lo)
{
	struct fw_bind *tfw;
	for(tfw = STAILQ_FIRST(&fc->binds) ; tfw != NULL ;
		tfw = STAILQ_NEXT(tfw, fclist)){
		if (tfw->act_type != FWACT_NULL &&
			tfw->start_hi == dest_hi &&
			tfw->start_lo <= dest_lo &&
			(tfw->start_lo + tfw->addrlen) > dest_lo){
			return(tfw);
		}
	}
	return(NULL);
}

/*
 * To bind IEEE1394 address block to process.
 */
int
fw_bindadd(struct firewire_comm *fc, struct fw_bind *fwb)
{
	struct fw_bind *tfw, *tfw2 = NULL;
	int err = 0;
	tfw = STAILQ_FIRST(&fc->binds);
	if(tfw == NULL){
		STAILQ_INSERT_HEAD(&fc->binds, fwb, fclist);
		goto out;
	}
	if((tfw->start_hi > fwb->start_hi) ||
		(tfw->start_hi == fwb->start_hi &&
		(tfw->start_lo > (fwb->start_lo + fwb->addrlen)))){
		STAILQ_INSERT_HEAD(&fc->binds, fwb, fclist);
		goto out;
	}
	for(; tfw != NULL; tfw = STAILQ_NEXT(tfw, fclist)){
		if((tfw->start_hi < fwb->start_hi) ||
		   (tfw->start_hi == fwb->start_hi &&
		    (tfw->start_lo + tfw->addrlen) < fwb->start_lo)){
		   tfw2 = STAILQ_NEXT(tfw, fclist);
			if(tfw2 == NULL)
				break;
			if((tfw2->start_hi > fwb->start_hi) ||
			   (tfw2->start_hi == fwb->start_hi &&
			    tfw2->start_lo > (fwb->start_lo + fwb->addrlen))){
				break;
			}else{
				err = EBUSY;
				goto out;
			}
		}
	}
	if(tfw != NULL){
		STAILQ_INSERT_AFTER(&fc->binds, tfw, fwb, fclist);
	}else{
		STAILQ_INSERT_TAIL(&fc->binds, fwb, fclist);
	}
out:
	if (!err && fwb->act_type == FWACT_CH)
		STAILQ_INSERT_HEAD(&fc->ir[fwb->sub]->binds, fwb, chlist);
	return err;
}

/*
 * To free IEEE1394 address block.
 */
int
fw_bindremove(struct firewire_comm *fc, struct fw_bind *fwb)
{
	int s;
	struct fw_xfer *xfer, *next;

	s = splfw();
	/* shall we check the existance? */
	STAILQ_REMOVE(&fc->binds, fwb, fw_bind, fclist);
	/* shall we do this? */
	for (xfer = STAILQ_FIRST(&fwb->xferlist); xfer != NULL; xfer = next) {
		next = STAILQ_NEXT(xfer, link);
		fw_xfer_free(xfer);
	}
	STAILQ_INIT(&fwb->xferlist);

	splx(s);
	return 0;
}

/*
 * To free transaction label.
 */
static void
fw_tl_free(struct firewire_comm *fc, struct fw_xfer *xfer)
{
	struct tlabel *tl;
	int s = splfw();

	for( tl = STAILQ_FIRST(&fc->tlabels[xfer->tl]); tl != NULL;
		tl = STAILQ_NEXT(tl, link)){
		if(tl->xfer == xfer){
			STAILQ_REMOVE(&fc->tlabels[xfer->tl], tl, tlabel, link);
			free(tl, M_FW);
			splx(s);
			return;
		}
	}
	splx(s);
	return;
}

/*
 * To obtain XFER structure by transaction label.
 */
static struct fw_xfer *
fw_tl2xfer(struct firewire_comm *fc, int node, int tlabel)
{
	struct fw_xfer *xfer;
	struct tlabel *tl;
	int s = splfw();

	for( tl = STAILQ_FIRST(&fc->tlabels[tlabel]); tl != NULL;
		tl = STAILQ_NEXT(tl, link)){
		if(tl->xfer->dst == node){
			xfer = tl->xfer;
			splx(s);
			if (firewire_debug > 2)
				printf("fw_tl2xfer: found tl=%d\n", tlabel);
			return(xfer);
		}
	}
	if (firewire_debug > 1)
		printf("fw_tl2xfer: not found tl=%d\n", tlabel);
	splx(s);
	return(NULL);
}

/*
 * To allocate IEEE1394 XFER structure.
 */
struct fw_xfer *
fw_xfer_alloc(struct malloc_type *type)
{
	struct fw_xfer *xfer;

	xfer = malloc(sizeof(struct fw_xfer), type, M_NOWAIT | M_ZERO);
	if (xfer == NULL)
		return xfer;

	microtime(&xfer->tv);
	xfer->malloc = type;

	return xfer;
}

struct fw_xfer *
fw_xfer_alloc_buf(struct malloc_type *type, int send_len, int recv_len)
{
	struct fw_xfer *xfer;

	xfer = fw_xfer_alloc(type);
	xfer->send.len = send_len;
	xfer->recv.len = recv_len;
	if (xfer == NULL)
		return(NULL);
	if (send_len) {
		xfer->send.buf = malloc(send_len, type, M_NOWAIT | M_ZERO);
		if (xfer->send.buf == NULL) {
			fw_xfer_free(xfer);
			return(NULL);
		}
	}
	if (recv_len) {
		xfer->recv.buf = malloc(recv_len, type, M_NOWAIT);
		if (xfer->recv.buf == NULL) {
			if (xfer->send.buf != NULL)
				free(xfer->send.buf, type);
			fw_xfer_free(xfer);
			return(NULL);
		}
	}
	return(xfer);
}

/*
 * IEEE1394 XFER post process.
 */
void
fw_xfer_done(struct fw_xfer *xfer)
{
	if (xfer->act.hand == NULL) {
		printf("act.hand == NULL\n");
		return;
	}

	if (xfer->fc->status != FWBUSRESET)
		xfer->act.hand(xfer);
	else {
		printf("fw_xfer_done: pending\n");
		if (xfer->fc != NULL)
			STAILQ_INSERT_TAIL(&xfer->fc->pending, xfer, link);
		else
			panic("fw_xfer_done: why xfer->fc is NULL?");
	}
}

void
fw_xfer_unload(struct fw_xfer* xfer)
{
	int s;

	if(xfer == NULL ) return;
	if(xfer->state == FWXF_INQ){
		printf("fw_xfer_free FWXF_INQ\n");
		s = splfw();
		STAILQ_REMOVE(&xfer->q->q, xfer, fw_xfer, link);
		xfer->q->queued --;
		splx(s);
	}
	if (xfer->fc != NULL) {
#if 1
		if(xfer->state == FWXF_START)
			/*
			 * This could happen if:
			 *  1. We call fwohci_arcv() before fwohci_txd().
			 *  2. firewire_watch() is called.
			 */
			printf("fw_xfer_free FWXF_START\n");
#endif
		fw_tl_free(xfer->fc, xfer);
	}
	xfer->state = FWXF_INIT;
	xfer->resp = 0;
	xfer->retry = 0;
}
/*
 * To free IEEE1394 XFER structure. 
 */
void
fw_xfer_free( struct fw_xfer* xfer)
{
	if(xfer == NULL ) return;
	fw_xfer_unload(xfer);
	if(xfer->send.buf != NULL){
		free(xfer->send.buf, xfer->malloc);
	}
	if(xfer->recv.buf != NULL){
		free(xfer->recv.buf, xfer->malloc);
	}
	free(xfer, xfer->malloc);
}

static void
fw_asy_callback_free(struct fw_xfer *xfer)
{
#if 0
	printf("asyreq done state=%d resp=%d\n",
				xfer->state, xfer->resp);
#endif
	fw_xfer_free(xfer);
}

/*
 * To configure PHY. 
 */
static void
fw_phy_config(struct firewire_comm *fc, int root_node, int gap_count)
{
	struct fw_xfer *xfer;
	struct fw_pkt *fp;

	fc->status = FWBUSPHYCONF;

	xfer = fw_xfer_alloc_buf(M_FWXFER, 12, 0);
	if (xfer == NULL)
		return;
	xfer->fc = fc;
	xfer->retry_req = fw_asybusy;
	xfer->act.hand = fw_asy_callback_free;

	fp = (struct fw_pkt *)xfer->send.buf;
	fp->mode.ld[1] = 0;
	if (root_node >= 0)
		fp->mode.ld[1] |= (root_node & 0x3f) << 24 | 1 << 23;
	if (gap_count >= 0)
		fp->mode.ld[1] |= 1 << 22 | (gap_count & 0x3f) << 16;
	fp->mode.ld[2] = ~fp->mode.ld[1];
/* XXX Dangerous, how to pass PHY packet to device driver */
	fp->mode.common.tcode |= FWTCODE_PHY;

	if (firewire_debug)
		printf("send phy_config root_node=%d gap_count=%d\n",
						root_node, gap_count);
	fw_asyreq(fc, -1, xfer);
}

#if 0
/*
 * Dump self ID. 
 */
static void
fw_print_sid(u_int32_t sid)
{
	union fw_self_id *s;
	s = (union fw_self_id *) &sid;
	printf("node:%d link:%d gap:%d spd:%d del:%d con:%d pwr:%d"
		" p0:%d p1:%d p2:%d i:%d m:%d\n",
		s->p0.phy_id, s->p0.link_active, s->p0.gap_count,
		s->p0.phy_speed, s->p0.phy_delay, s->p0.contender,
		s->p0.power_class, s->p0.port0, s->p0.port1,
		s->p0.port2, s->p0.initiated_reset, s->p0.more_packets);
}
#endif

/*
 * To receive self ID. 
 */
void fw_sidrcv(struct firewire_comm* fc, u_int32_t *sid, u_int len)
{
	u_int32_t *p;
	union fw_self_id *self_id;
	u_int i, j, node, c_port = 0, i_branch = 0;

	fc->sid_cnt = len /(sizeof(u_int32_t) * 2);
	fc->status = FWBUSINIT;
	fc->max_node = fc->nodeid & 0x3f;
	CSRARC(fc, NODE_IDS) = ((u_int32_t)fc->nodeid) << 16;
	fc->status = FWBUSCYMELECT;
	fc->topology_map->crc_len = 2;
	fc->topology_map->generation ++;
	fc->topology_map->self_id_count = 0;
	fc->topology_map->node_count = 0;
	fc->speed_map->generation ++;
	fc->speed_map->crc_len = 1 + (64*64 + 3) / 4;
	self_id = &fc->topology_map->self_id[0];
	for(i = 0; i < fc->sid_cnt; i ++){
		if (sid[1] != ~sid[0]) {
			printf("fw_sidrcv: invalid self-id packet\n");
			sid += 2;
			continue;
		}
		*self_id = *((union fw_self_id *)sid);
		fc->topology_map->crc_len++;
		if(self_id->p0.sequel == 0){
			fc->topology_map->node_count ++;
			c_port = 0;
#if 0
			fw_print_sid(sid[0]);
#endif
			node = self_id->p0.phy_id;
			if(fc->max_node < node){
				fc->max_node = self_id->p0.phy_id;
			}
			/* XXX I'm not sure this is the right speed_map */
			fc->speed_map->speed[node][node]
					= self_id->p0.phy_speed;
			for (j = 0; j < node; j ++) {
				fc->speed_map->speed[j][node]
					= fc->speed_map->speed[node][j]
					= min(fc->speed_map->speed[j][j],
							self_id->p0.phy_speed);
			}
			if ((fc->irm == -1 || self_id->p0.phy_id > fc->irm) &&
			  (self_id->p0.link_active && self_id->p0.contender)) {
				fc->irm = self_id->p0.phy_id;
			}
			if(self_id->p0.port0 >= 0x2){
				c_port++;
			}
			if(self_id->p0.port1 >= 0x2){
				c_port++;
			}
			if(self_id->p0.port2 >= 0x2){
				c_port++;
			}
		}
		if(c_port > 2){
			i_branch += (c_port - 2);
		}
		sid += 2;
		self_id++;
		fc->topology_map->self_id_count ++;
	}
	device_printf(fc->bdev, "%d nodes", fc->max_node + 1);
	/* CRC */
	fc->topology_map->crc = fw_crc16(
			(u_int32_t *)&fc->topology_map->generation,
			fc->topology_map->crc_len * 4);
	fc->speed_map->crc = fw_crc16(
			(u_int32_t *)&fc->speed_map->generation,
			fc->speed_map->crc_len * 4);
	/* byteswap and copy to CSR */
	p = (u_int32_t *)fc->topology_map;
	for (i = 0; i <= fc->topology_map->crc_len; i++)
		CSRARC(fc, TOPO_MAP + i * 4) = htonl(*p++);
	p = (u_int32_t *)fc->speed_map;
	CSRARC(fc, SPED_MAP) = htonl(*p++);
	CSRARC(fc, SPED_MAP + 4) = htonl(*p++);
	/* don't byte-swap u_int8_t array */
	bcopy(p, &CSRARC(fc, SPED_MAP + 8), (fc->speed_map->crc_len - 1)*4);

	fc->max_hop = fc->max_node - i_branch;
	printf(", maxhop <= %d", fc->max_hop);
		
	if(fc->irm == -1 ){
		printf(", Not found IRM capable node");
	}else{
		printf(", cable IRM = %d", fc->irm);
		if (fc->irm == fc->nodeid)
			printf(" (me)");
	}
	printf("\n");

	if (try_bmr && (fc->irm != -1) && (CSRARC(fc, BUS_MGR_ID) == 0x3f)) {
		if (fc->irm == fc->nodeid) {
			fc->status = FWBUSMGRDONE;
			CSRARC(fc, BUS_MGR_ID) = fc->set_bmr(fc, fc->irm);
			fw_bmr(fc);
		} else {
			fc->status = FWBUSMGRELECT;
			callout_reset(&fc->bmr_callout, hz/8,
				(void *)fw_try_bmr, (void *)fc);
		}
	} else
		fc->status = FWBUSMGRDONE;

	callout_reset(&fc->busprobe_callout, hz/4,
			(void *)fw_bus_probe, (void *)fc);
}

/*
 * To probe devices on the IEEE1394 bus. 
 */
static void
fw_bus_probe(struct firewire_comm *fc)
{
	int s;
	struct fw_device *fwdev, *next;

	s = splfw();
	fc->status = FWBUSEXPLORE;
	fc->retry_count = 0;

/*
 * Invalidate all devices, just after bus reset. Devices 
 * to be removed has not been seen longer time.
 */
	for (fwdev = STAILQ_FIRST(&fc->devices); fwdev != NULL; fwdev = next) {
		next = STAILQ_NEXT(fwdev, link);
		if (fwdev->status != FWDEVINVAL) {
			fwdev->status = FWDEVINVAL;
			fwdev->rcnt = 0;
		} else if(fwdev->rcnt < FW_MAXDEVRCNT) {
			fwdev->rcnt ++;
		} else {
			STAILQ_REMOVE(&fc->devices, fwdev, fw_device, link);
			free(fwdev, M_FW);
		}
	}
	fc->ongonode = 0;
	fc->ongoaddr = CSRROMOFF;
	fc->ongodev = NULL;
	fc->ongoeui.hi = 0xffffffff; fc->ongoeui.lo = 0xffffffff;
	fw_bus_explore(fc);
	splx(s);
}

/*
 * To collect device informations on the IEEE1394 bus. 
 */
static void
fw_bus_explore(struct firewire_comm *fc )
{
	int err = 0;
	struct fw_device *fwdev, *pfwdev, *tfwdev;
	u_int32_t addr;
	struct fw_xfer *xfer;
	struct fw_pkt *fp;

	if(fc->status != FWBUSEXPLORE)
		return;

loop:
	if(fc->ongonode == fc->nodeid) fc->ongonode++;

	if(fc->ongonode > fc->max_node) goto done;
	if(fc->ongonode >= 0x3f) goto done;

	/* check link */
	/* XXX we need to check phy_id first */
	if (!fc->topology_map->self_id[fc->ongonode].p0.link_active) {
		if (firewire_debug)
			printf("node%d: link down\n", fc->ongonode);
		fc->ongonode++;
		goto loop;
	}

	if(fc->ongoaddr <= CSRROMOFF &&
		fc->ongoeui.hi == 0xffffffff &&
		fc->ongoeui.lo == 0xffffffff ){
		fc->ongoaddr = CSRROMOFF;
		addr = 0xf0000000 | fc->ongoaddr;
	}else if(fc->ongoeui.hi == 0xffffffff ){
		fc->ongoaddr = CSRROMOFF + 0xc;
		addr = 0xf0000000 | fc->ongoaddr;
	}else if(fc->ongoeui.lo == 0xffffffff ){
		fc->ongoaddr = CSRROMOFF + 0x10;
		addr = 0xf0000000 | fc->ongoaddr;
	}else if(fc->ongodev == NULL){
		STAILQ_FOREACH(fwdev, &fc->devices, link)
			if (FW_EUI64_EQUAL(fwdev->eui, fc->ongoeui))
				break;
		if(fwdev != NULL){
			fwdev->dst = fc->ongonode;
			fwdev->status = FWDEVINIT;
			fc->ongodev = fwdev;
			fc->ongoaddr = CSRROMOFF;
			addr = 0xf0000000 | fc->ongoaddr;
			goto dorequest;
		}
		fwdev = malloc(sizeof(struct fw_device), M_FW,
							M_NOWAIT | M_ZERO);
		if(fwdev == NULL)
			return;
		fwdev->fc = fc;
		fwdev->rommax = 0;
		fwdev->dst = fc->ongonode;
		fwdev->eui.hi = fc->ongoeui.hi; fwdev->eui.lo = fc->ongoeui.lo;
		fwdev->status = FWDEVINIT;
		fwdev->speed = fc->speed_map->speed[fc->nodeid][fc->ongonode];

		pfwdev = NULL;
		STAILQ_FOREACH(tfwdev, &fc->devices, link) {
			if (tfwdev->eui.hi > fwdev->eui.hi ||
					(tfwdev->eui.hi == fwdev->eui.hi &&
					tfwdev->eui.lo > fwdev->eui.lo))
				break;
			pfwdev = tfwdev;
		}
		if (pfwdev == NULL)
			STAILQ_INSERT_HEAD(&fc->devices, fwdev, link);
		else
			STAILQ_INSERT_AFTER(&fc->devices, pfwdev, fwdev, link);

		device_printf(fc->bdev, "New %s device ID:%08x%08x\n",
			linkspeed[fwdev->speed],
			fc->ongoeui.hi, fc->ongoeui.lo);

		fc->ongodev = fwdev;
		fc->ongoaddr = CSRROMOFF;
		addr = 0xf0000000 | fc->ongoaddr;
	}else{
		addr = 0xf0000000 | fc->ongoaddr;
	}
dorequest:
#if 0
	xfer = asyreqq(fc, FWSPD_S100, 0, 0,
		((FWLOCALBUS | fc->ongonode) << 16) | 0xffff , addr,
		fw_bus_explore_callback);
	if(xfer == NULL) goto done;
#else
	xfer = fw_xfer_alloc_buf(M_FWXFER, 16, 16);
	if(xfer == NULL){
		goto done;
	}
	xfer->spd = 0;
	fp = (struct fw_pkt *)xfer->send.buf;
	fp->mode.rreqq.dest_hi = 0xffff;
	fp->mode.rreqq.tlrt = 0;
	fp->mode.rreqq.tcode = FWTCODE_RREQQ;
	fp->mode.rreqq.pri = 0;
	fp->mode.rreqq.src = 0;
	xfer->dst = FWLOCALBUS | fc->ongonode;
	fp->mode.rreqq.dst = xfer->dst;
	fp->mode.rreqq.dest_lo = addr;
	xfer->act.hand = fw_bus_explore_callback;

	if (firewire_debug)
		printf("node%d: explore addr=0x%x\n",
				fc->ongonode, fc->ongoaddr);
	err = fw_asyreq(fc, -1, xfer);
	if(err){
		fw_xfer_free( xfer);
		return;
	}
#endif
	return;
done:
	/* fw_attach_devs */
	fc->status = FWBUSEXPDONE;
	if (firewire_debug)
		printf("bus_explore done\n");
	fw_attach_dev(fc);
	return;

}

/* Portable Async. request read quad */
struct fw_xfer *
asyreqq(struct firewire_comm *fc, u_int8_t spd, u_int8_t tl, u_int8_t rt,
	u_int32_t addr_hi, u_int32_t addr_lo,
	void (*hand) __P((struct fw_xfer*)))
{
	struct fw_xfer *xfer;
	struct fw_pkt *fp;
	int err;

	xfer = fw_xfer_alloc_buf(M_FWXFER, 16, 16);
	if (xfer == NULL)
		return NULL;

	xfer->spd = spd; /* XXX:min(spd, fc->spd) */
	fp = (struct fw_pkt *)xfer->send.buf;
	fp->mode.rreqq.dest_hi = addr_hi & 0xffff;
	if(tl & FWP_TL_VALID){
		fp->mode.rreqq.tlrt = (tl & 0x3f) << 2;
	}else{
		fp->mode.rreqq.tlrt = 0;
	}
	fp->mode.rreqq.tlrt |= rt & 0x3;
	fp->mode.rreqq.tcode = FWTCODE_RREQQ;
	fp->mode.rreqq.pri = 0;
	fp->mode.rreqq.src = 0;
	xfer->dst = addr_hi >> 16;
	fp->mode.rreqq.dst = xfer->dst;
	fp->mode.rreqq.dest_lo = addr_lo;
	xfer->act.hand = hand;

	err = fw_asyreq(fc, -1, xfer);
	if(err){
		fw_xfer_free( xfer);
		return NULL;
	}
	return xfer;
}

/*
 * Callback for the IEEE1394 bus information collection. 
 */
static void
fw_bus_explore_callback(struct fw_xfer *xfer)
{
	struct firewire_comm *fc;
	struct fw_pkt *sfp,*rfp;
	struct csrhdr *chdr;
	struct csrdir *csrd;
	struct csrreg *csrreg;
	u_int32_t offset;

	
	if(xfer == NULL) {
		printf("xfer == NULL\n");
		return;
	}
	fc = xfer->fc;

	if (firewire_debug)
		printf("node%d: callback addr=0x%x\n",
			fc->ongonode, fc->ongoaddr);

	if(xfer->resp != 0){
		printf("node%d: resp=%d addr=0x%x\n",
			fc->ongonode, xfer->resp, fc->ongoaddr);
		goto errnode;
	}

	if(xfer->send.buf == NULL){
		printf("node%d: send.buf=NULL addr=0x%x\n",
			fc->ongonode, fc->ongoaddr);
		goto errnode;
	}
	sfp = (struct fw_pkt *)xfer->send.buf;

	if(xfer->recv.buf == NULL){
		printf("node%d: recv.buf=NULL addr=0x%x\n",
			fc->ongonode, fc->ongoaddr);
		goto errnode;
	}
	rfp = (struct fw_pkt *)xfer->recv.buf;
#if 0
	{
		u_int32_t *qld;
		int i;
		qld = (u_int32_t *)xfer->recv.buf;
		printf("len:%d\n", xfer->recv.len);
		for( i = 0 ; i <= xfer->recv.len && i < 32; i+= 4){
			printf("0x%08x ", rfp->mode.ld[i/4]);
			if((i % 16) == 15) printf("\n");
		}
		if((i % 16) != 15) printf("\n");
	}
#endif
	if(fc->ongodev == NULL){
		if(sfp->mode.rreqq.dest_lo == (0xf0000000 | CSRROMOFF)){
			rfp->mode.rresq.data = ntohl(rfp->mode.rresq.data);
			chdr = (struct csrhdr *)(&rfp->mode.rresq.data);
/* If CSR is minimal confinguration, more investgation is not needed. */
			if(chdr->info_len == 1){
				if (firewire_debug)
					printf("node%d: minimal config\n",
								fc->ongonode);
				goto nextnode;
			}else{
				fc->ongoaddr = CSRROMOFF + 0xc;
			}
		}else if(sfp->mode.rreqq.dest_lo == (0xf0000000 |(CSRROMOFF + 0xc))){
			fc->ongoeui.hi = ntohl(rfp->mode.rresq.data);
			fc->ongoaddr = CSRROMOFF + 0x10;
		}else if(sfp->mode.rreqq.dest_lo == (0xf0000000 |(CSRROMOFF + 0x10))){
			fc->ongoeui.lo = ntohl(rfp->mode.rresq.data);
			if (fc->ongoeui.hi == 0 && fc->ongoeui.lo == 0) {
				if (firewire_debug)
					printf("node%d: eui64 is zero.\n",
							fc->ongonode);
				goto nextnode;
			}
			fc->ongoaddr = CSRROMOFF;
		}
	}else{
		if (fc->ongoaddr == CSRROMOFF &&
		    fc->ongodev->csrrom[0] == ntohl(rfp->mode.rresq.data)) {
			fc->ongodev->status = FWDEVATTACHED;
			goto nextnode;
		}
		fc->ongodev->csrrom[(fc->ongoaddr - CSRROMOFF)/4] = ntohl(rfp->mode.rresq.data);
		if(fc->ongoaddr > fc->ongodev->rommax){
			fc->ongodev->rommax = fc->ongoaddr;
		}
		csrd = SLIST_FIRST(&fc->ongocsr);
		if((csrd = SLIST_FIRST(&fc->ongocsr)) == NULL){
			chdr = (struct csrhdr *)(fc->ongodev->csrrom);
			offset = CSRROMOFF;
		}else{
			chdr = (struct csrhdr *)&fc->ongodev->csrrom[(csrd->off - CSRROMOFF)/4];
			offset = csrd->off;
		}
		if(fc->ongoaddr > (CSRROMOFF + 0x14) && fc->ongoaddr != offset){
			csrreg = (struct csrreg *)&fc->ongodev->csrrom[(fc->ongoaddr - CSRROMOFF)/4];
			if( csrreg->key == 0x81 || csrreg->key == 0xd1){
				csrd = SLIST_FIRST(&fc->csrfree);
				if(csrd == NULL){
					goto nextnode;
				}else{
					csrd->ongoaddr = fc->ongoaddr;
					fc->ongoaddr += csrreg->val * 4;
					csrd->off = fc->ongoaddr;
					SLIST_REMOVE_HEAD(&fc->csrfree, link);
					SLIST_INSERT_HEAD(&fc->ongocsr, csrd, link);
					goto nextaddr;
				}
			}
		}
		fc->ongoaddr += 4;
		if(((fc->ongoaddr - offset)/4 > chdr->crc_len) &&
				(fc->ongodev->rommax < 0x414)){
			if(fc->ongodev->rommax <= 0x414){
				csrd = SLIST_FIRST(&fc->csrfree);
				if(csrd == NULL) goto nextnode;
				csrd->off = fc->ongoaddr;
				csrd->ongoaddr = fc->ongoaddr;
				SLIST_REMOVE_HEAD(&fc->csrfree, link);
				SLIST_INSERT_HEAD(&fc->ongocsr, csrd, link);
			}
			goto nextaddr;
		}

		while(((fc->ongoaddr - offset)/4 > chdr->crc_len)){
			if(csrd == NULL){
				goto nextnode;
			};
			fc->ongoaddr = csrd->ongoaddr + 4;
			SLIST_REMOVE_HEAD(&fc->ongocsr, link);
			SLIST_INSERT_HEAD(&fc->csrfree, csrd, link);
			csrd = SLIST_FIRST(&fc->ongocsr);
			if((csrd = SLIST_FIRST(&fc->ongocsr)) == NULL){
				chdr = (struct csrhdr *)(fc->ongodev->csrrom);
				offset = CSRROMOFF;
			}else{
				chdr = (struct csrhdr *)&(fc->ongodev->csrrom[(csrd->off - CSRROMOFF)/4]);
				offset = csrd->off;
			}
		}
		if((fc->ongoaddr - CSRROMOFF) > CSRROMSIZE){
			goto nextnode;
		}
	}
nextaddr:
	fw_xfer_free( xfer);
	fw_bus_explore(fc);
	return;
errnode:
	fc->retry_count++;
	if (fc->ongodev != NULL)
		fc->ongodev->status = FWDEVINVAL;
nextnode:
	fw_xfer_free( xfer);
	fc->ongonode++;
/* housekeeping work space */
	fc->ongoaddr = CSRROMOFF;
	fc->ongodev = NULL;
	fc->ongoeui.hi = 0xffffffff; fc->ongoeui.lo = 0xffffffff;
	while((csrd = SLIST_FIRST(&fc->ongocsr)) != NULL){
		SLIST_REMOVE_HEAD(&fc->ongocsr, link);
		SLIST_INSERT_HEAD(&fc->csrfree, csrd, link);
	}
	fw_bus_explore(fc);
	return;
}

/*
 * To attach sub-devices layer onto IEEE1394 bus.
 */
static void
fw_attach_dev(struct firewire_comm *fc)
{
	struct fw_device *fwdev;
	struct fw_xfer *xfer;
	int i, err;
	device_t *devlistp;
	int devcnt;
	struct firewire_dev_comm *fdc;

	STAILQ_FOREACH(fwdev, &fc->devices, link)
		if (fwdev->status == FWDEVINIT)
			fwdev->status = FWDEVATTACHED;

	err = device_get_children(fc->bdev, &devlistp, &devcnt);
	if( err != 0 )
		return;
	for( i = 0 ; i < devcnt ; i++){
		if (device_get_state(devlistp[i]) >= DS_ATTACHED)  {
			fdc = device_get_softc(devlistp[i]);
			if (fdc->post_explore != NULL)
				fdc->post_explore(fdc);
		}
	}
	free(devlistp, M_TEMP);

	/* call pending handlers */
	i = 0;
	while ((xfer = STAILQ_FIRST(&fc->pending))) {
		STAILQ_REMOVE_HEAD(&fc->pending, link);
		i++;
		if (xfer->act.hand)
			xfer->act.hand(xfer);
	}
	if (i > 0)
		printf("fw_attach_dev: %d pending handlers called\n", i);
	if (fc->retry_count > 0) {
		printf("probe failed for %d node\n", fc->retry_count);
#if 0
		callout_reset(&fc->retry_probe_callout, hz*2,
					(void *)fc->ibr, (void *)fc);
#endif
	}
	return;
}

/*
 * To allocate uniq transaction label.
 */
static int
fw_get_tlabel(struct firewire_comm *fc, struct fw_xfer *xfer)
{
	u_int i;
	struct tlabel *tl, *tmptl;
	int s;
	static u_int32_t label = 0;

	s = splfw();
	for( i = 0 ; i < 0x40 ; i ++){
		label = (label + 1) & 0x3f;
		for(tmptl = STAILQ_FIRST(&fc->tlabels[label]);
			tmptl != NULL; tmptl = STAILQ_NEXT(tmptl, link)){
			if(tmptl->xfer->dst == xfer->dst) break;
		}
		if(tmptl == NULL) {
			tl = malloc(sizeof(struct tlabel),M_FW,M_NOWAIT);
			if (tl == NULL) {
				splx(s);
				return (-1);
			}
			tl->xfer = xfer;
			STAILQ_INSERT_TAIL(&fc->tlabels[label], tl, link);
			splx(s);
			if (firewire_debug > 1)
				printf("fw_get_tlabel: dst=%d tl=%d\n",
						xfer->dst, label);
			return(label);
		}
	}
	splx(s);

	printf("fw_get_tlabel: no free tlabel\n");
	return(-1);
}

static void
fw_rcv_copy(struct fw_xfer *xfer, struct iovec *vec, int nvec)
{
	char *p;
	int res, i, len;

	p = xfer->recv.buf;
	res = xfer->recv.len;
	for (i = 0; i < nvec; i++, vec++) {
		len = vec->iov_len;
		if (res < len) {
			printf("rcv buffer(%d) is %d bytes short.\n",
						xfer->recv.len, len - res);
			len = res;
		}
		bcopy(vec->iov_base, p, len);
		p += len;
		res -= len;
		if (res <= 0)
			break;
	}
	xfer->recv.len -= res;
}

/*
 * Generic packet receving process.
 */
void
fw_rcv(struct firewire_comm *fc, struct iovec *vec, int nvec, u_int sub, u_int spd)
{
	struct fw_pkt *fp, *resfp;
	struct fw_xfer *xfer;
	struct fw_bind *bind;
	struct firewire_softc *sc;
	int tcode, s;
	int i, len, oldstate;
#if 0
	{
		u_int32_t *qld;
		int i;
		qld = (u_int32_t *)buf;
		printf("spd %d len:%d\n", spd, len);
		for( i = 0 ; i <= len && i < 32; i+= 4){
			printf("0x%08x ", ntohl(qld[i/4]));
			if((i % 16) == 15) printf("\n");
		}
		if((i % 16) != 15) printf("\n");
	}
#endif
	fp = (struct fw_pkt *)vec[0].iov_base;
	tcode = fp->mode.common.tcode;
#if 0 /* XXX this check is not valid for RRESQ and WREQQ */
	if (vec[0].iov_len < fc->tcode[tcode].hdr_len) {
#if __FreeBSD_version >= 500000
		printf("fw_rcv: iov_len(%zu) is less than"
#else
		printf("fw_rcv: iov_len(%u) is less than"
#endif
			" hdr_len(%d:tcode=%d)\n", vec[0].iov_len,
			fc->tcode[tcode].hdr_len, tcode);
	}
#endif
	switch (tcode) {
	case FWTCODE_WRES:
	case FWTCODE_RRESQ:
	case FWTCODE_RRESB:
	case FWTCODE_LRES:
		xfer = fw_tl2xfer(fc, fp->mode.hdr.src,
					fp->mode.hdr.tlrt >> 2);
		if(xfer == NULL) {
			printf("fw_rcv: unknown response "
					"tcode=%d src=0x%x tl=0x%x rt=%d data=0x%x\n",
					tcode,
					fp->mode.hdr.src,
					fp->mode.hdr.tlrt >> 2,
					fp->mode.hdr.tlrt & 3,
					fp->mode.rresq.data);
#if 1
			printf("try ad-hoc work around!!\n");
			xfer = fw_tl2xfer(fc, fp->mode.hdr.src,
					(fp->mode.hdr.tlrt >> 2)^3);
			if (xfer == NULL) {
				printf("no use...\n");
				goto err;
			}
#else
			goto err;
#endif
		}
		fw_rcv_copy(xfer, vec, nvec);
		xfer->resp = 0;
		/* make sure the packet is drained in AT queue */
		oldstate = xfer->state;
		xfer->state = FWXF_RCVD;
		switch (oldstate) {
		case FWXF_SENT:
			fw_xfer_done(xfer);
			break;
		case FWXF_START:
#if 0
			if (firewire_debug)
				printf("not sent yet tl=%x\n", xfer->tl);
#endif
			break;
		default:
			printf("unexpected state %d\n", xfer->state);
		}
		return;
	case FWTCODE_WREQQ:
	case FWTCODE_WREQB:
	case FWTCODE_RREQQ:
	case FWTCODE_RREQB:
	case FWTCODE_LREQ:
		bind = fw_bindlookup(fc, fp->mode.rreqq.dest_hi,
			fp->mode.rreqq.dest_lo);
		if(bind == NULL){
#if __FreeBSD_version >= 500000
			printf("Unknown service addr 0x%08x:0x%08x tcode=%x src=0x%x data=%x\n",
#else
			printf("Unknown service addr 0x%08x:0x%08x tcode=%x src=0x%x data=%lx\n",
#endif
				fp->mode.wreqq.dest_hi,
				fp->mode.wreqq.dest_lo,
				tcode,
				fp->mode.hdr.src,
				ntohl(fp->mode.wreqq.data));
			if (fc->status == FWBUSRESET) {
				printf("fw_rcv: cannot respond(bus reset)!\n");
				goto err;
			}
			xfer = fw_xfer_alloc_buf(M_FWXFER, 16, 0);
			if(xfer == NULL){
				return;
			}
			xfer->spd = spd;
			resfp = (struct fw_pkt *)xfer->send.buf;
			switch (tcode) {
			case FWTCODE_WREQQ:
			case FWTCODE_WREQB:
				resfp->mode.hdr.tcode = FWTCODE_WRES;
				xfer->send.len = 12;
				break;
			case FWTCODE_RREQQ:
				resfp->mode.hdr.tcode = FWTCODE_RRESQ;
				xfer->send.len = 16;
				break;
			case FWTCODE_RREQB:
				resfp->mode.hdr.tcode = FWTCODE_RRESB;
				xfer->send.len = 16;
				break;
			case FWTCODE_LREQ:
				resfp->mode.hdr.tcode = FWTCODE_LRES;
				xfer->send.len = 16;
				break;
			}
			resfp->mode.hdr.dst = fp->mode.hdr.src;
			resfp->mode.hdr.tlrt = fp->mode.hdr.tlrt;
			resfp->mode.hdr.pri = fp->mode.hdr.pri;
			resfp->mode.rresb.rtcode = 7;
			resfp->mode.rresb.extcode = 0;
			resfp->mode.rresb.len = 0;
/*
			xfer->act.hand = fw_asy_callback;
*/
			xfer->act.hand = fw_xfer_free;
			if(fw_asyreq(fc, -1, xfer)){
				fw_xfer_free( xfer);
				return;
			}
			goto err;
		}
		len = 0;
		for (i = 0; i < nvec; i ++)
			len += vec[i].iov_len;
		switch(bind->act_type){
		case FWACT_XFER:
			/* splfw()?? */
			xfer = STAILQ_FIRST(&bind->xferlist);
			if (xfer == NULL) {
				printf("Discard a packet for this bind.\n");
				goto err;
			}
			STAILQ_REMOVE_HEAD(&bind->xferlist, link);
			fw_rcv_copy(xfer, vec, nvec);
			xfer->spd = spd;
			if (fc->status != FWBUSRESET)
				xfer->act.hand(xfer);
			else
				STAILQ_INSERT_TAIL(&fc->pending, xfer, link);
			return;
			break;
		case FWACT_CH:
			if(fc->ir[bind->sub]->queued >=
				fc->ir[bind->sub]->maxq){
				device_printf(fc->bdev,
					"Discard a packet %x %d\n",
					bind->sub,
					fc->ir[bind->sub]->queued);
				goto err;
			}
			xfer = STAILQ_FIRST(&bind->xferlist);
			if (xfer == NULL) {
				printf("Discard packet for this bind\n");
				goto err;
			}
			STAILQ_REMOVE_HEAD(&bind->xferlist, link);
			fw_rcv_copy(xfer, vec, nvec);
			xfer->spd = spd;
			s = splfw();
			fc->ir[bind->sub]->queued++;
			STAILQ_INSERT_TAIL(&fc->ir[bind->sub]->q, xfer, link);
			splx(s);

			wakeup((caddr_t)fc->ir[bind->sub]);

			return;
			break;
		default:
			goto err;
			break;
		}
		break;
	case FWTCODE_STREAM:
	{
		struct fw_xferq *xferq;

		xferq = fc->ir[sub];
#if 0
		printf("stream rcv dma %d len %d off %d spd %d\n",
			sub, len, off, spd);
#endif
		if(xferq->queued >= xferq->maxq) {
			printf("receive queue is full\n");
			goto err;
		}
		/* XXX get xfer from xfer queue, we don't need copy for 
			per packet mode */
		xfer = fw_xfer_alloc_buf(M_FWXFER, 0, /* XXX */
						vec[0].iov_len);
		if(xfer == NULL) goto err;
		fw_rcv_copy(xfer, vec, nvec);
		xfer->spd = spd;
		s = splfw();
		xferq->queued++;
		STAILQ_INSERT_TAIL(&xferq->q, xfer, link);
		splx(s);
		sc = device_get_softc(fc->bdev);
#if __FreeBSD_version >= 500000
		if (SEL_WAITING(&xferq->rsel))
#else
		if (&xferq->rsel.si_pid != 0)
#endif
			selwakeup(&xferq->rsel);
		if (xferq->flag & FWXFERQ_WAKEUP) {
			xferq->flag &= ~FWXFERQ_WAKEUP;
			wakeup((caddr_t)xferq);
		}
		if (xferq->flag & FWXFERQ_HANDLER) {
			xferq->hand(xferq);
		}
		return;
		break;
	}
	default:
		printf("fw_rcv: unknow tcode %d\n", tcode);
		break;
	}
err:
	return;
}

/*
 * Post process for Bus Manager election process.
 */
static void
fw_try_bmr_callback(struct fw_xfer *xfer)
{
	struct fw_pkt *rfp;
	struct firewire_comm *fc;
	int bmr;

	if (xfer == NULL)
		return;
	fc = xfer->fc;
	if (xfer->resp != 0)
		goto error;
	if (xfer->send.buf == NULL)
		goto error;
	if (xfer->recv.buf == NULL)
		goto error;
	rfp = (struct fw_pkt *)xfer->recv.buf;
	if (rfp->mode.lres.rtcode != FWRCODE_COMPLETE)
		goto error;

	bmr = ntohl(rfp->mode.lres.payload[0]);
	if (bmr == 0x3f)
		bmr = fc->nodeid;

	CSRARC(fc, BUS_MGR_ID) = fc->set_bmr(fc, bmr & 0x3f);
	fw_xfer_free(xfer);
	fw_bmr(fc);
	return;

error:
	device_printf(fc->bdev, "bus manager election failed\n");
	fw_xfer_free(xfer);
}


/*
 * To candidate Bus Manager election process.
 */
static void
fw_try_bmr(void *arg)
{
	struct fw_xfer *xfer;
	struct firewire_comm *fc = (struct firewire_comm *)arg;
	struct fw_pkt *fp;
	int err = 0;

	xfer = fw_xfer_alloc_buf(M_FWXFER, 24, 20);
	if(xfer == NULL){
		return;
	}
	xfer->spd = 0;
	fc->status = FWBUSMGRELECT;

	fp = (struct fw_pkt *)xfer->send.buf;
	fp->mode.lreq.dest_hi = 0xffff;
	fp->mode.lreq.tlrt = 0;
	fp->mode.lreq.tcode = FWTCODE_LREQ;
	fp->mode.lreq.pri = 0;
	fp->mode.lreq.src = 0;
	fp->mode.lreq.len = 8;
	fp->mode.lreq.extcode = FW_LREQ_CMPSWAP;
	xfer->dst = FWLOCALBUS | fc->irm;
	fp->mode.lreq.dst = xfer->dst;
	fp->mode.lreq.dest_lo = 0xf0000000 | BUS_MGR_ID;
	fp->mode.lreq.payload[0] = htonl(0x3f);
	fp->mode.lreq.payload[1] = htonl(fc->nodeid);
	xfer->act.hand = fw_try_bmr_callback;

	err = fw_asyreq(fc, -1, xfer);
	if(err){
		fw_xfer_free( xfer);
		return;
	}
	return;
}

#ifdef FW_VMACCESS
/*
 * Software implementation for physical memory block access.
 * XXX:Too slow, usef for debug purpose only.
 */
static void
fw_vmaccess(struct fw_xfer *xfer){
	struct fw_pkt *rfp, *sfp = NULL;
	u_int32_t *ld = (u_int32_t *)xfer->recv.buf;

	printf("vmaccess spd:%2x len:%03x data:%08x %08x %08x %08x\n",
			xfer->spd, xfer->recv.len, ntohl(ld[0]), ntohl(ld[1]), ntohl(ld[2]), ntohl(ld[3]));
	printf("vmaccess          data:%08x %08x %08x %08x\n", ntohl(ld[4]), ntohl(ld[5]), ntohl(ld[6]), ntohl(ld[7]));
	if(xfer->resp != 0){
		fw_xfer_free( xfer);
		return;
	}
	if(xfer->recv.buf == NULL){
		fw_xfer_free( xfer);
		return;
	}
	rfp = (struct fw_pkt *)xfer->recv.buf;
	switch(rfp->mode.hdr.tcode){
		/* XXX need fix for 64bit arch */
		case FWTCODE_WREQB:
			xfer->send.buf = malloc(12, M_FW, M_NOWAIT);
			xfer->send.len = 12;
			sfp = (struct fw_pkt *)xfer->send.buf;
			bcopy(rfp->mode.wreqb.payload,
				(caddr_t)ntohl(rfp->mode.wreqb.dest_lo), ntohs(rfp->mode.wreqb.len));
			sfp->mode.wres.tcode = FWTCODE_WRES;
			sfp->mode.wres.rtcode = 0;
			break;
		case FWTCODE_WREQQ:
			xfer->send.buf = malloc(12, M_FW, M_NOWAIT);
			xfer->send.len = 12;
			sfp->mode.wres.tcode = FWTCODE_WRES;
			*((u_int32_t *)(ntohl(rfp->mode.wreqb.dest_lo))) = rfp->mode.wreqq.data;
			sfp->mode.wres.rtcode = 0;
			break;
		case FWTCODE_RREQB:
			xfer->send.buf = malloc(16 + rfp->mode.rreqb.len, M_FW, M_NOWAIT);
			xfer->send.len = 16 + ntohs(rfp->mode.rreqb.len);
			sfp = (struct fw_pkt *)xfer->send.buf;
			bcopy((caddr_t)ntohl(rfp->mode.rreqb.dest_lo),
				sfp->mode.rresb.payload, (u_int16_t)ntohs(rfp->mode.rreqb.len));
			sfp->mode.rresb.tcode = FWTCODE_RRESB;
			sfp->mode.rresb.len = rfp->mode.rreqb.len;
			sfp->mode.rresb.rtcode = 0;
			sfp->mode.rresb.extcode = 0;
			break;
		case FWTCODE_RREQQ:
			xfer->send.buf = malloc(16, M_FW, M_NOWAIT);
			xfer->send.len = 16;
			sfp = (struct fw_pkt *)xfer->send.buf;
			sfp->mode.rresq.data = *(u_int32_t *)(ntohl(rfp->mode.rreqq.dest_lo));
			sfp->mode.wres.tcode = FWTCODE_RRESQ;
			sfp->mode.rresb.rtcode = 0;
			break;
		default:
			fw_xfer_free( xfer);
			return;
	}
	sfp->mode.hdr.dst = rfp->mode.hdr.src;
	xfer->dst = ntohs(rfp->mode.hdr.src);
	xfer->act.hand = fw_xfer_free;
	xfer->retry_req = fw_asybusy;

	sfp->mode.hdr.tlrt = rfp->mode.hdr.tlrt;
	sfp->mode.hdr.pri = 0;

	fw_asyreq(xfer->fc, -1, xfer);
/**/
	return;
}
#endif 

/*
 * CRC16 check-sum for IEEE1394 register blocks.
 */
u_int16_t
fw_crc16(u_int32_t *ptr, u_int32_t len){
	u_int32_t i, sum, crc = 0;
	int shift;
	len = (len + 3) & ~3;
	for(i = 0 ; i < len ; i+= 4){
		for( shift = 28 ; shift >= 0 ; shift -= 4){
			sum = ((crc >> 12) ^ (ptr[i/4] >> shift)) & 0xf;
			crc = (crc << 4) ^ ( sum << 12 ) ^ ( sum << 5) ^ sum;
		}
		crc &= 0xffff;
	}
	return((u_int16_t) crc);
}

static int
fw_bmr(struct firewire_comm *fc)
{
	struct fw_device fwdev;
	union fw_self_id *self_id;
	int cmstr;

	/* Check to see if the current root node is cycle master capable */
	self_id = &fc->topology_map->self_id[fc->max_node];
	if (fc->max_node > 0) {
		/* XXX check cmc bit of businfo block rather than contender */
		if (self_id->p0.link_active && self_id->p0.contender)
			cmstr = fc->max_node;
		else {
			device_printf(fc->bdev,
				"root node is not cycle master capable\n");
			/* XXX shall we be the cycle master? */
			cmstr = fc->nodeid;
			/* XXX need bus reset */
		}
	} else
		cmstr = -1;

	device_printf(fc->bdev, "bus manager %d ", CSRARC(fc, BUS_MGR_ID));
	if(CSRARC(fc, BUS_MGR_ID) != fc->nodeid) {
		/* We are not the bus manager */
		printf("\n");
		return(0);
	}
	printf("(me)\n");

	/* Optimize gapcount */
	if(fc->max_hop <= MAX_GAPHOP )
		fw_phy_config(fc, cmstr, gap_cnt[fc->max_hop]);
	/* If we are the cycle master, nothing to do */
	if (cmstr == fc->nodeid || cmstr == -1)
		return 0;
	/* Bus probe has not finished, make dummy fwdev for cmstr */
	bzero(&fwdev, sizeof(fwdev));
	fwdev.fc = fc;
	fwdev.dst = cmstr;
	fwdev.speed = 0;
	fwdev.maxrec = 8; /* 512 */
	fwdev.status = FWDEVINIT;
	/* Set cmstr bit on the cycle master */
	fwmem_write_quad(&fwdev, NULL, 0/*spd*/,
		0xffff, 0xf0000000 | STATE_SET, htonl(1 << 8),
		fw_asy_callback_free);

	return 0;
}

static int
fw_modevent(module_t mode, int type, void *data)
{
	int err = 0;
#if __FreeBSD_version >= 500000
	static eventhandler_tag fwdev_ehtag = NULL;
#endif

	switch (type) {
	case MOD_LOAD:
#if __FreeBSD_version >= 500000
		fwdev_ehtag = EVENTHANDLER_REGISTER(dev_clone,
						fwdev_clone, 0, 1000);
#endif
		break;
	case MOD_UNLOAD:
#if __FreeBSD_version >= 500000
		if (fwdev_ehtag != NULL)
			EVENTHANDLER_DEREGISTER(dev_clone, fwdev_ehtag);
#endif
		break;
	case MOD_SHUTDOWN:
		break;
	}
	return (err);
}


DRIVER_MODULE(firewire,fwohci,firewire_driver,firewire_devclass,fw_modevent,0);
MODULE_VERSION(firewire, 1);
