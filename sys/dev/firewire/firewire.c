/*
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
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/socketvar.h>

#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/conf.h>
#include <sys/uio.h>
#include <sys/sysctl.h>
#include <sys/poll.h>

#include <machine/cpufunc.h>    /* for rdtsc proto for clock.h below */
#include <machine/clock.h>
#include <pci/pcivar.h>
#include <pci/pcireg.h>

#include <vm/vm.h>
#include <vm/pmap.h>            /* for vtophys proto */
#include <vm/vm_extern.h> 

#include <sys/bus.h>		/* used by smbus and newbus */

#include <machine/bus.h>	/* used by newbus */
#include <sys/rman.h>		/* used by newbus */
#include <machine/resource.h>	/* used by newbus */

#include <sys/signal.h>
#include <sys/mman.h>
#include <sys/ioccom.h>

#include <dev/firewire/firewire.h>
#include <dev/firewire/firewirereg.h>
#include <dev/firewire/fwmem.h>
#include <dev/firewire/iec13213.h>
#include <dev/firewire/iec68113.h>

int firewire_debug=0;
SYSCTL_NODE(_hw, OID_AUTO, firewire, CTLFLAG_RD, 0, "Firewire Subsystem");
SYSCTL_INT(_debug, OID_AUTO, firewire_debug, CTLFLAG_RW, &firewire_debug, 0,
	"Firewire driver debug flag");

#define CDEV_MAJOR 127
#define FW_MAXASYRTY 4
#define FW_MAXDEVRCNT 4
#define	FWNODE_INVAL 0xffff

#define XFER_TIMEOUT 0

static	d_open_t	fw_open;
static	d_close_t	fw_close;
static	d_ioctl_t	fw_ioctl;
static	d_poll_t	fw_poll;
static	d_read_t	fw_read;	/* for Isochronous packet */
static	d_write_t	fw_write;
static	d_mmap_t	fw_mmap;

devclass_t firewire_devclass;


static int firewire_match      __P((device_t));
static int firewire_attach      __P((device_t));
static int firewire_detach      __P((device_t));
#if 0
static int firewire_shutdown    __P((device_t));
#endif
static device_t firewire_add_child   __P((device_t, int, const char *, int));
static struct fw_bind *fw_bindlookup __P((struct firewire_comm *, u_int32_t, u_int32_t));
static void fw_try_bmr __P((void *));
static void fw_try_bmr_callback __P((struct fw_xfer *));
static u_int16_t fw_noderesolve __P((struct firewire_comm *, struct fw_eui64));
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

static device_method_t firewire_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		firewire_match),
	DEVMETHOD(device_attach,	firewire_attach),
	DEVMETHOD(device_detach,	firewire_detach),
	DEVMETHOD(device_shutdown,	bus_generic_shutdown),

	/* Bus interface */
	DEVMETHOD(bus_add_child,	firewire_add_child),
	DEVMETHOD(bus_print_child,	bus_generic_print_child),

	{ 0, 0 }
};
char linkspeed[7][0x10]={"S100","S200","S400","S800","S1600","S3200","Unknown"};
u_int maxrec[6]={512,1024,2048,4096,8192,0};

#define MAX_GAPHOP  16
u_int gap_cnt[] = {1, 1, 4, 6, 9, 12, 14, 17,
			20, 23, 25, 28, 31, 33, 36, 39, 42};
/*
 * The probe routine.
 */
struct cdevsw firewire_cdevsw = 
{
	fw_open, fw_close, fw_read, fw_write, fw_ioctl,
	fw_poll, fw_mmap, nostrategy, "fw", CDEV_MAJOR, nodump, nopsize, D_MEM
};
static driver_t firewire_driver = {
	"firewire",
	firewire_methods,
	sizeof(struct firewire_softc),
};

static int
fw_open (dev_t dev, int flags, int fmt, fw_proc *td)
{
	struct firewire_softc *sc;
	int unit = DEV2UNIT(dev);
	int sub = DEV2DMACH(dev);

	int err = 0;

	if (DEV_FWMEM(dev))
		return fwmem_open(dev, flags, fmt, td);

	sc = devclass_get_softc(firewire_devclass, unit);
	if(sc->fc->ir[sub]->flag & FWXFERQ_OPEN){
		err = EBUSY;
		return err;
	}
	if(sc->fc->it[sub]->flag & FWXFERQ_OPEN){
		err = EBUSY;
		return err;
	}
	if(sc->fc->ir[sub]->flag & FWXFERQ_MODEMASK){
		err = EBUSY;
		return err;
	}
/* Default is per packet mode */
	sc->fc->ir[sub]->flag |= FWXFERQ_OPEN;
	sc->fc->it[sub]->flag |= FWXFERQ_OPEN;
	sc->fc->ir[sub]->flag |= FWXFERQ_PACKET;
	return err;
}
static int
fw_close (dev_t dev, int flags, int fmt, fw_proc *td)
{
	struct firewire_softc *sc;
	int unit = DEV2UNIT(dev);
	int sub = DEV2DMACH(dev);
	struct fw_xfer *xfer;
	struct fw_dvbuf *dvbuf;
	struct fw_bind *fwb;
	int err = 0;

	if (DEV_FWMEM(dev))
		return fwmem_close(dev, flags, fmt, td);

	sc = devclass_get_softc(firewire_devclass, unit);
	if(!(sc->fc->ir[sub]->flag & FWXFERQ_OPEN)){
		err = EINVAL;
		return err;
	}
	sc->fc->ir[sub]->flag &= ~FWXFERQ_OPEN;
	if(!(sc->fc->it[sub]->flag & FWXFERQ_OPEN)){
		err = EINVAL;
		return err;
	}
	sc->fc->it[sub]->flag &= ~FWXFERQ_OPEN;

	if(sc->fc->ir[sub]->flag & FWXFERQ_RUNNING){
		sc->fc->irx_disable(sc->fc, sub);
	}
	if(sc->fc->it[sub]->flag & FWXFERQ_RUNNING){
		sc->fc->it[sub]->flag &= ~FWXFERQ_RUNNING;
		sc->fc->itx_disable(sc->fc, sub);
	}
	if(sc->fc->it[sub]->flag & FWXFERQ_DV){
		if((dvbuf = sc->fc->it[sub]->dvproc) != NULL){
			free(dvbuf->buf, M_DEVBUF);
			sc->fc->it[sub]->dvproc = NULL;
		}
		if((dvbuf = sc->fc->it[sub]->dvdma) != NULL){
			free(dvbuf->buf, M_DEVBUF);
			sc->fc->it[sub]->dvdma = NULL;
		}
		while((dvbuf = STAILQ_FIRST(&sc->fc->it[sub]->dvvalid)) != NULL){
			STAILQ_REMOVE_HEAD(&sc->fc->it[sub]->dvvalid, link);
			free(dvbuf->buf, M_DEVBUF);
		}
		while((dvbuf = STAILQ_FIRST(&sc->fc->it[sub]->dvfree)) != NULL){
			STAILQ_REMOVE_HEAD(&sc->fc->it[sub]->dvfree, link);
			free(dvbuf->buf, M_DEVBUF);
		}
		free(sc->fc->it[sub]->dvbuf, M_DEVBUF);
		sc->fc->it[sub]->dvbuf = NULL;
	}
	if(sc->fc->ir[sub]->flag & FWXFERQ_EXTBUF){
		free(sc->fc->ir[sub]->buf, M_DEVBUF);
		sc->fc->ir[sub]->buf = NULL;
		free(sc->fc->ir[sub]->bulkxfer, M_DEVBUF);
		sc->fc->ir[sub]->bulkxfer = NULL;
		sc->fc->ir[sub]->flag &= ~FWXFERQ_EXTBUF;
		sc->fc->ir[sub]->psize = FWPMAX_S400;
		sc->fc->ir[sub]->maxq = FWMAXQUEUE;
	}
	if(sc->fc->it[sub]->flag & FWXFERQ_EXTBUF){
		free(sc->fc->it[sub]->buf, M_DEVBUF);
		sc->fc->it[sub]->buf = NULL;
		free(sc->fc->it[sub]->bulkxfer, M_DEVBUF);
		sc->fc->it[sub]->bulkxfer = NULL;
		sc->fc->it[sub]->dvbuf = NULL;
		sc->fc->it[sub]->flag &= ~FWXFERQ_EXTBUF;
		sc->fc->it[sub]->psize = FWPMAX_S400;
		sc->fc->it[sub]->maxq = FWMAXQUEUE;
	}
	for(xfer = STAILQ_FIRST(&sc->fc->ir[sub]->q);
		xfer != NULL; xfer = STAILQ_FIRST(&sc->fc->ir[sub]->q)){
		sc->fc->ir[sub]->queued--;
		STAILQ_REMOVE_HEAD(&sc->fc->ir[sub]->q, link);

		xfer->resp = 0;
		switch(xfer->act_type){
		case FWACT_XFER:
			fw_xfer_done(xfer);
			break;
		default:
			break;
		}
		fw_xfer_free(xfer);
	}
	for(fwb = STAILQ_FIRST(&sc->fc->ir[sub]->binds); fwb != NULL;
		fwb = STAILQ_FIRST(&sc->fc->ir[sub]->binds)){
		STAILQ_REMOVE(&sc->fc->binds, fwb, fw_bind, fclist);
		STAILQ_REMOVE_HEAD(&sc->fc->ir[sub]->binds, chlist);
		free(fwb, M_DEVBUF);
	}
	sc->fc->ir[sub]->flag &= ~FWXFERQ_MODEMASK;
	sc->fc->it[sub]->flag &= ~FWXFERQ_MODEMASK;
	return err;
}
/*
 * read request.
 */
static int
fw_read (dev_t dev, struct uio *uio, int ioflag)
{
	struct firewire_softc *sc;
	struct fw_xferq *ir;
	struct fw_xfer *xfer;
	int err = 0, s, slept = 0;
	int unit = DEV2UNIT(dev);
	int sub = DEV2DMACH(dev);
	struct fw_pkt *fp;

	if (DEV_FWMEM(dev))
		return fwmem_read(dev, uio, ioflag);

	sc = devclass_get_softc(firewire_devclass, unit);

	ir = sc->fc->ir[sub];

	if(ir->flag & FWXFERQ_PACKET){
		ir->stproc = NULL;
	}
readloop:
	xfer = STAILQ_FIRST(&ir->q);
	if(!(ir->flag & FWXFERQ_PACKET) && ir->stproc == NULL){
		ir->stproc = STAILQ_FIRST(&ir->stvalid);
		if(ir->stproc != NULL){
			s = splfw();
			STAILQ_REMOVE_HEAD(&ir->stvalid, link);
			splx(s);
			ir->queued = 0;
		}
	}

	if(xfer == NULL && ir->stproc == NULL){
		if(slept == 0){
			slept = 1;
			if(!(ir->flag & FWXFERQ_RUNNING)
				&& (ir->flag & FWXFERQ_PACKET)){
				err = sc->fc->irx_enable(sc->fc, sub);
			}
			if(err){
				return err;
			}
			ir->flag |= FWXFERQ_WAKEUP;
			err = tsleep((caddr_t)ir, FWPRI, "fw_read", hz);
			if(err){
				ir->flag &= ~FWXFERQ_WAKEUP;
				return err;
			}
			goto readloop;
		}else{
			err = EIO;
			return err;
		}
	}else if(xfer != NULL){
		s = splfw();
		ir->queued --;
		STAILQ_REMOVE_HEAD(&ir->q, link);
		splx(s);
		fp = (struct fw_pkt *)(xfer->recv.buf + xfer->recv.off);
		if(sc->fc->irx_post != NULL)
			sc->fc->irx_post(sc->fc, fp->mode.ld);
		err = uiomove(xfer->recv.buf + xfer->recv.off, xfer->recv.len, uio);
		fw_xfer_free( xfer);
	}else if(ir->stproc != NULL){
		fp = (struct fw_pkt *)(ir->stproc->buf + ir->queued * ir->psize);
		if(sc->fc->irx_post != NULL)
			sc->fc->irx_post(sc->fc, fp->mode.ld);
		if(ntohs(fp->mode.stream.len) == 0){
			err = EIO;
			return err;
		}
		err = uiomove((caddr_t)fp, ntohs(fp->mode.stream.len) + sizeof(u_int32_t), uio);
		fp->mode.stream.len = 0;
		ir->queued ++;
		if(ir->queued >= ir->bnpacket){
			s = splfw();
			ir->stproc->flag = 0;
			STAILQ_INSERT_TAIL(&ir->stfree, ir->stproc, link);
			splx(s);
			ir->stproc = NULL;
		}
	}
#if 0
	if(STAILQ_FIRST(&ir->q) == NULL &&
		(ir->flag & FWXFERQ_RUNNING) && (ir->flag & FWXFERQ_PACKET)){
		err = sc->fc->irx_enable(sc->fc, sub);
	}
#endif
#if 0
	if(STAILQ_FIRST(&ir->stvalid) == NULL &&
		(ir->flag & FWXFERQ_RUNNING) && !(ir->flag & FWXFERQ_PACKET)){
		err = sc->fc->irx_enable(sc->fc, sub);
	}
#endif
	return err;
}
static int
fw_write (dev_t dev, struct uio *uio, int ioflag)
{
	int err = 0;
	struct firewire_softc *sc;
	int unit = DEV2UNIT(dev);
	int sub = DEV2DMACH(dev);
	int tl, s, slept = 0;
	struct fw_pkt *fp;
	struct fw_xfer *xfer;
	struct fw_xferq *xferq;
	struct firewire_comm *fc;
	struct fw_xferq *it;

	if (DEV_FWMEM(dev))
		return fwmem_write(dev, uio, ioflag);

	sc = devclass_get_softc(firewire_devclass, unit);
	fc = sc->fc;
	it = sc->fc->it[sub];

	fp = (struct fw_pkt *)uio->uio_iov->iov_base;
	switch(fp->mode.common.tcode){
	case FWTCODE_RREQQ:
	case FWTCODE_RREQB:
	case FWTCODE_LREQ:
		err = EINVAL;
		return err;
	case FWTCODE_WREQQ:
	case FWTCODE_WREQB:
		xferq = fc->atq;
		break;
	case FWTCODE_STREAM:
		if(it->flag & FWXFERQ_PACKET){
			xferq = fc->atq;
		}else{
			xferq = NULL;
		}
		break;
	case FWTCODE_WRES:
	case FWTCODE_RRESQ:
	case FWTCODE_RRESB:
	case FWTCODE_LRES:
		xferq = fc->ats;
		break;
	default:
		err = EINVAL;
		return err;
	}
	/* Discard unsent buffered stream packet, when sending Asyrequrst */
	if(xferq != NULL && it->stproc != NULL){
		s = splfw();
		it->stproc->flag = 0;
		STAILQ_INSERT_TAIL(&it->stfree, it->stproc, link);
		splx(s);
		it->stproc = NULL;
	}
	if(xferq == NULL && !(it->flag & FWXFERQ_DV)){
isoloop:
		if(it->stproc == NULL){
			it->stproc = STAILQ_FIRST(&it->stfree);
			if(it->stproc != NULL){
				s = splfw();
				STAILQ_REMOVE_HEAD(&it->stfree, link);
				splx(s);
				it->queued = 0;
			}else if(slept == 0){
				slept = 1;
				err = sc->fc->itx_enable(sc->fc, sub);
				if(err){
					return err;
				}
				err = tsleep((caddr_t)it, FWPRI, "fw_write", hz);
				if(err){
					return err;
				}
				goto isoloop;
			}else{
				err = EIO;
				return err;
			}
		}
		fp = (struct fw_pkt *)(it->stproc->buf + it->queued * it->psize);
		fp->mode.stream.len = htons(uio->uio_resid - sizeof(u_int32_t));
		err = uiomove(it->stproc->buf + it->queued * it->psize,
							uio->uio_resid, uio);
		it->queued ++;
		if(it->queued >= it->btpacket){
			s = splfw();
			STAILQ_INSERT_TAIL(&it->stvalid, it->stproc, link);
			splx(s);
			it->stproc = NULL;
			fw_tbuf_update(sc->fc, sub, 0);
			err = sc->fc->itx_enable(sc->fc, sub);
		}
		return err;
	} if(xferq == NULL && it->flag & FWXFERQ_DV){
dvloop:
		if(it->dvproc == NULL){
			it->dvproc = STAILQ_FIRST(&it->dvfree);
			if(it->dvproc != NULL){
				s = splfw();
				STAILQ_REMOVE_HEAD(&it->dvfree, link);
				splx(s);
				it->dvptr = 0;
			}else if(slept == 0){
				slept = 1;
				err = sc->fc->itx_enable(sc->fc, sub);
				if(err){
					return err;
				}
				err = tsleep((caddr_t)it, FWPRI, "fw_write", hz);
				if(err){
					return err;
				}
				goto dvloop;
			}else{
				err = EIO;
				return err;
			}
		}
		fp = (struct fw_pkt *)(it->dvproc->buf + it->queued * it->psize);
		fp->mode.stream.len = htons(uio->uio_resid - sizeof(u_int32_t));
		err = uiomove(it->dvproc->buf + it->dvptr,
							uio->uio_resid, uio);
		it->dvptr += it->psize;
		if(err){
			return err;
		}
		if(it->dvptr >= it->psize * it->dvpacket){
			s = splfw();
			STAILQ_INSERT_TAIL(&it->dvvalid, it->dvproc, link);
			splx(s);
			it->dvproc = NULL;
			err = fw_tbuf_update(sc->fc, sub, 0);
			if(err){
				return err;
			}
			err = sc->fc->itx_enable(sc->fc, sub);
		}
		return err;
	}
	if(xferq != NULL){
		xfer = fw_xfer_alloc();
		if(xfer == NULL){
			err = ENOMEM;
			return err;
		}
		xfer->send.buf = malloc(uio->uio_resid, M_DEVBUF, M_NOWAIT);
		if(xfer->send.buf == NULL){
			fw_xfer_free( xfer);
			err = ENOBUFS;
			return err;
		}
		xfer->dst = ntohs(fp->mode.hdr.dst);
	
		switch(fp->mode.common.tcode){
		case FWTCODE_WREQQ:
		case FWTCODE_WREQB:
			if((tl = fw_get_tlabel(fc, xfer)) == -1 ){
				fw_xfer_free( xfer);
				err = EAGAIN;
				return err;
			}
			fp->mode.hdr.tlrt = tl << 2;
		default:
			break;
		}
	
		xfer->tl = fp->mode.hdr.tlrt >> 2;
		xfer->send.len = uio->uio_resid; 
		xfer->send.off = 0; 
		xfer->tcode = fp->mode.common.tcode;
		xfer->spd = 0;/* XXX: how to setup it */
		xfer->fc = fc; 
		xfer->q = xferq;
		xfer->act_type = FWACT_XFER;
		xfer->act.hand = fw_asy_callback;
		xfer->retry_req = fw_asybusy;
			
		err = uiomove(xfer->send.buf, uio->uio_resid, uio);
		if(err){
			return err;
		}
		fw_asystart(xfer);
		err = tsleep((caddr_t)xfer, FWPRI, "fw_write", hz);
		if(xfer->resp == EBUSY)
			return EBUSY;
		fw_xfer_free( xfer);
		return err;
	}
	return EINVAL;
}
/*
 * transmitter buffer update.
 */
int
fw_tbuf_update(struct firewire_comm *fc, int sub, int flag){
	struct fw_bulkxfer *bulkxfer, *bulkxfer2 = NULL;
	struct fw_dvbuf *dvbuf = NULL;
	struct fw_xferq *it;
	int s, err = 0, i, j, chtag;
	struct fw_pkt *fp;
	u_int64_t tmpsync, dvsync;

	it = fc->it[sub];
	
	s = splfw();
	if(it->stdma == NULL){
		bulkxfer = STAILQ_FIRST(&it->stvalid);
	}else if(flag != 0){
		bulkxfer = STAILQ_FIRST(&it->stvalid);
		if(bulkxfer == it->stdma){
			STAILQ_REMOVE_HEAD(&it->stvalid, link);
			it->stdma->flag = 0;
			STAILQ_INSERT_TAIL(&it->stfree, it->stdma, link);
			if(!(it->flag & FWXFERQ_DV))
				wakeup(it);
		}
		bulkxfer = STAILQ_FIRST(&it->stvalid);
	}else{
		bulkxfer = it->stdma;
	}
	splx(s);
	if(bulkxfer != NULL){
		s = splfw();
		bulkxfer2 = STAILQ_NEXT(bulkxfer, link);
#if 0
		if(it->flag & FWXFERQ_DV && bulkxfer2 == NULL){
			bulkxfer2 = STAILQ_FIRST(&it->stfree);
			STAILQ_REMOVE_HEAD(&it->stfree, link);
			splx(s);
			bcopy(bulkxfer->buf, bulkxfer2->buf,
					it->psize * it->btpacket);
			s = splfw();
			STAILQ_INSERT_TAIL(&it->stvalid, bulkxfer2, link);
		}
#endif
		splx(s);
	}
	it->stdma = bulkxfer;
	it->stdma2 = bulkxfer2;

	if(it->flag & FWXFERQ_DV){
		chtag = it->flag & 0xff;
dvloop:
		if(it->dvdma == NULL){
			dvbuf = STAILQ_FIRST(&it->dvvalid);
			if(dvbuf != NULL){
				s = splfw();
				STAILQ_REMOVE_HEAD(&it->dvvalid, link);
				it->dvdma = dvbuf;
				splx(s);
				it->queued = 0;
			}
		}
		if(it->dvdma == NULL)
			return err;

		it->stproc = STAILQ_FIRST(&it->stfree);
		if(it->stproc != NULL){
			s = splfw();
			STAILQ_REMOVE_HEAD(&it->stfree, link);
			splx(s);
		}else{
			return err;
		}
/*
 * Insert least significant 12 bits timestamp value by computation. 
 * Highest significant 4 bits is insert at just before packet sending.
 */
		fp = (struct fw_pkt *)(it->stproc->buf);
/* XXX: Parameter relies on NTSC type DV video */
		tmpsync = (u_int64_t)3072 * 8000 * 100 / 2997;
		tmpsync *= it->dvsync;
		dvsync = tmpsync;
		dvsync %= 0xc00;
		fp->mode.ld[2] = htonl(0x80000000 | (dvsync % 0xc00));
		it->dvsync ++;
		it->dvsync %= 2997;

		for( i = 0, j = 0 ; i < it->dvpacket ; i++){
			bcopy(it->dvdma->buf + it->queued * it->psize,
				it->stproc->buf + j * it->psize, it->psize);
			fp = (struct fw_pkt *)(it->stproc->buf + j * it->psize);
			fp->mode.stream.len = htons(488);
			fp->mode.stream.chtag = chtag;
			fp->mode.stream.tcode = FWTCODE_STREAM;
			fp->mode.ld[1] = htonl((fc->nodeid << 24) | 0x00780000 | it->dvdbc);
			it->dvdbc++;
			it->dvdbc %= 256;
			it->queued ++;
			j++;
/* XXX: Parameter relies on NTSC type DV video */
#if 1
#define DVDIFF 203
#define DVFRAC 2997
#else
#define DVDIFF 127
#define DVFRAC 1875
#endif
			it->dvdiff += DVDIFF;
			if(it->dvdiff >= DVFRAC){
				it->dvdiff %= DVFRAC;
				fp = (struct fw_pkt *)(it->stproc->buf + j * it->psize);

				fp->mode.stream.len = htons(0x8);
				fp->mode.stream.chtag = chtag;
				fp->mode.stream.tcode = FWTCODE_STREAM;
				fp->mode.ld[1] = htonl((fc->nodeid << 24) | 
					 0x00780000 | it->dvdbc);
				j++;
			}
		}
		it->stproc->npacket = j;
		s = splfw();
		STAILQ_INSERT_TAIL(&it->stvalid, it->stproc, link);
		splx(s);
		if(it->queued >= it->dvpacket){
			s = splfw();
			STAILQ_INSERT_TAIL(&it->dvfree, it->dvdma, link);
			it->dvdma = NULL;
			splx(s);
			wakeup(it);
			goto dvloop;
		}
	}
	return err;
}
/*
 * receving buffer update.
 */
int
fw_rbuf_update(struct firewire_comm *fc, int sub, int flag){
	struct fw_bulkxfer *bulkxfer, *bulkxfer2 = NULL;
	struct fw_xferq *ir;
	int s, err = 0;

	ir = fc->ir[sub];
	s = splfw();
	if(ir->stdma != NULL){
		if(flag != 0){
			STAILQ_INSERT_TAIL(&ir->stvalid, ir->stdma, link);
		}else{
			ir->stdma->flag = 0;
			STAILQ_INSERT_TAIL(&ir->stfree, ir->stdma, link);
		}
	}
	if(ir->stdma2 != NULL){
		bulkxfer = ir->stdma2;
		bulkxfer2 = STAILQ_FIRST(&ir->stfree);
		if(bulkxfer2 != NULL){
			STAILQ_REMOVE_HEAD(&ir->stfree, link);
		}
	}else{
		bulkxfer = STAILQ_FIRST(&ir->stfree);
		if(bulkxfer != NULL){
			STAILQ_REMOVE_HEAD(&ir->stfree, link);
			bulkxfer2 = STAILQ_FIRST(&ir->stfree);
			if(bulkxfer2 != NULL){
				STAILQ_REMOVE_HEAD(&ir->stfree, link);
			}
		}else{
			bulkxfer = STAILQ_FIRST(&ir->stvalid);
			STAILQ_REMOVE_HEAD(&ir->stvalid, link);
		}
	}
	splx(s);
	ir->stdma = bulkxfer;
	ir->stdma2 = bulkxfer2;
	return err;
}
/*
 * ioctl support.
 */
int
fw_ioctl (dev_t dev, u_long cmd, caddr_t data, int flag, fw_proc *td)
{
	struct firewire_softc *sc;
	int unit = DEV2UNIT(dev);
	int sub = DEV2DMACH(dev);
	int i, len, err = 0;
	struct fw_device *fwdev;
	struct fw_bind *fwb;
	struct fw_xferq *ir, *it;
	struct fw_xfer *xfer;
	struct fw_pkt *fp;

	struct fw_devlstreq *fwdevlst = (struct fw_devlstreq *)data;
	struct fw_asyreq *asyreq = (struct fw_asyreq *)data;
	struct fw_isochreq *ichreq = (struct fw_isochreq *)data;
	struct fw_isobufreq *ibufreq = (struct fw_isobufreq *)data;
	struct fw_asybindreq *bindreq = (struct fw_asybindreq *)data;
#if 0
	struct fw_map_buf *map_buf = (struct fw_map_buf *)data;
#endif
	struct fw_crom_buf *crom_buf = (struct fw_crom_buf *)data;

	if (DEV_FWMEM(dev))
		return fwmem_ioctl(dev, cmd, data, flag, td);

	sc = devclass_get_softc(firewire_devclass, unit);
	if (!data)
		return(EINVAL);

	switch (cmd) {
	case FW_STSTREAM:
		sc->fc->it[sub]->flag &= ~0xff;
		sc->fc->it[sub]->flag |= (0x3f & ichreq->ch);
		sc->fc->it[sub]->flag |= ((0x3 & ichreq->tag) << 6);
		err = 0;
		break;
	case FW_GTSTREAM:
		ichreq->ch = sc->fc->it[sub]->flag & 0x3f;
		ichreq->tag =(sc->fc->it[sub]->flag) >> 2 & 0x3;
		err = 0;
		break;
	case FW_SRSTREAM:
		sc->fc->ir[sub]->flag &= ~0xff;
		sc->fc->ir[sub]->flag |= (0x3f & ichreq->ch);
		sc->fc->ir[sub]->flag |= ((0x3 & ichreq->tag) << 6);
		err = sc->fc->irx_enable(sc->fc, sub);
		break;
	case FW_GRSTREAM:
		ichreq->ch = sc->fc->ir[sub]->flag & 0x3f;
		ichreq->tag =(sc->fc->ir[sub]->flag) >> 2 & 0x3;
		err = 0;
		break;
	case FW_SSTDV:
		ibufreq = (struct fw_isobufreq *)
			malloc(sizeof(struct fw_isobufreq), M_DEVBUF, M_NOWAIT);
		if(ibufreq == NULL){
			err = ENOMEM;
			break;
		}
#define FWDVPACKET 250
#define FWDVPMAX 512
		ibufreq->rx.nchunk = 8;
		ibufreq->rx.npacket = 50;
		ibufreq->rx.psize = FWDVPMAX;

		ibufreq->tx.nchunk = 5;
		ibufreq->tx.npacket = 300;
		ibufreq->tx.psize = FWDVPMAX;

		err = fw_ioctl(dev, FW_SSTBUF, (caddr_t)ibufreq, flag, td);
		sc->fc->it[sub]->dvpacket = FWDVPACKET;
		free(ibufreq, M_DEVBUF);
/* reserve a buffer space */
#define NDVCHUNK 8
		sc->fc->it[sub]->dvproc = NULL;
		sc->fc->it[sub]->dvdma = NULL;
		sc->fc->it[sub]->flag |= FWXFERQ_DV;
		sc->fc->it[sub]->dvbuf
			= (struct fw_dvbuf *)malloc(sizeof(struct fw_dvbuf) * NDVCHUNK, M_DEVBUF, M_DONTWAIT);
		STAILQ_INIT(&sc->fc->it[sub]->dvvalid);
		STAILQ_INIT(&sc->fc->it[sub]->dvfree);
		for( i = 0 ; i < NDVCHUNK ; i++){
			sc->fc->it[sub]->dvbuf[i].buf
				= malloc(FWDVPMAX * sc->fc->it[sub]->dvpacket, M_DEVBUF, M_DONTWAIT);
			STAILQ_INSERT_TAIL(&sc->fc->it[sub]->dvfree,
					&sc->fc->it[sub]->dvbuf[i], link);
		}
		break;
	case FW_SSTBUF:
		ir = sc->fc->ir[sub];
		it = sc->fc->it[sub];

		if(ir->flag & FWXFERQ_RUNNING || it->flag & FWXFERQ_RUNNING){
			return(EBUSY);
		}
		if((ir->flag & FWXFERQ_EXTBUF) || (it->flag & FWXFERQ_EXTBUF)){
			return(EBUSY);
		}
		if((ibufreq->rx.nchunk *
			ibufreq->rx.psize * ibufreq->rx.npacket) +
		   (ibufreq->tx.nchunk *
			ibufreq->tx.psize * ibufreq->tx.npacket) <= 0){
				return(EINVAL);
		}
		if(ibufreq->rx.nchunk > FWSTMAXCHUNK ||
				ibufreq->tx.nchunk > FWSTMAXCHUNK){
			return(EINVAL);
		}
		ir->bulkxfer
			= (struct fw_bulkxfer *)malloc(sizeof(struct fw_bulkxfer) * ibufreq->rx.nchunk, M_DEVBUF, M_DONTWAIT);
		if(ir->bulkxfer == NULL){
			return(ENOMEM);
		}
		it->bulkxfer
			= (struct fw_bulkxfer *)malloc(sizeof(struct fw_bulkxfer) * ibufreq->tx.nchunk, M_DEVBUF, M_DONTWAIT);
		if(it->bulkxfer == NULL){
			return(ENOMEM);
		}
		ir->buf = malloc(
			ibufreq->rx.nchunk * ibufreq->rx.npacket
			* ((ibufreq->rx.psize + 3) &~3),
			M_DEVBUF, M_DONTWAIT);
		if(ir->buf == NULL){
			free(ir->bulkxfer, M_DEVBUF);
			free(it->bulkxfer, M_DEVBUF);
			ir->bulkxfer = NULL;
			it->bulkxfer = NULL;
			it->buf = NULL;
			return(ENOMEM);
		}
		it->buf = malloc(
			ibufreq->tx.nchunk * ibufreq->tx.npacket
			* ((ibufreq->tx.psize + 3) &~3),
			M_DEVBUF, M_DONTWAIT);
		if(it->buf == NULL){
			free(ir->bulkxfer, M_DEVBUF);
			free(it->bulkxfer, M_DEVBUF);
			free(ir->buf, M_DEVBUF);
			ir->bulkxfer = NULL;
			it->bulkxfer = NULL;
			it->buf = NULL;
			return(ENOMEM);
		}

		ir->bnchunk = ibufreq->rx.nchunk;
		ir->bnpacket = ibufreq->rx.npacket;
		ir->btpacket = ibufreq->rx.npacket;
		ir->psize = (ibufreq->rx.psize + 3) & ~3;
		ir->queued = 0;

		it->bnchunk = ibufreq->tx.nchunk;
		it->bnpacket = ibufreq->tx.npacket;
		it->btpacket = ibufreq->tx.npacket;
		it->psize = (ibufreq->tx.psize + 3) & ~3;
		ir->queued = 0;
		it->dvdbc = 0;
		it->dvdiff = 0;
		it->dvsync = 0;

		STAILQ_INIT(&ir->stvalid);
		STAILQ_INIT(&ir->stfree);
		ir->stdma = NULL;
		ir->stdma2 = NULL;
		ir->stproc = NULL;

		STAILQ_INIT(&it->stvalid);
		STAILQ_INIT(&it->stfree);
		it->stdma = NULL;
		it->stdma2 = NULL;
		it->stproc = NULL;

		for(i = 0 ; i < sc->fc->ir[sub]->bnchunk; i++){
			ir->bulkxfer[i].buf =
				ir->buf +
				i * sc->fc->ir[sub]->bnpacket *
			  	sc->fc->ir[sub]->psize;
			ir->bulkxfer[i].flag = 0;
			STAILQ_INSERT_TAIL(&ir->stfree,
					&ir->bulkxfer[i], link);
			ir->bulkxfer[i].npacket = ir->bnpacket;
		}
		for(i = 0 ; i < sc->fc->it[sub]->bnchunk; i++){
			it->bulkxfer[i].buf =
				it->buf +
				i * sc->fc->it[sub]->bnpacket *
			  	sc->fc->it[sub]->psize;
			it->bulkxfer[i].flag = 0;
			STAILQ_INSERT_TAIL(&it->stfree,
					&it->bulkxfer[i], link);
			it->bulkxfer[i].npacket = it->bnpacket;
		}
		ir->flag &= ~FWXFERQ_MODEMASK;
		ir->flag |= FWXFERQ_STREAM;
		ir->flag |= FWXFERQ_EXTBUF;

		it->flag &= ~FWXFERQ_MODEMASK;
		it->flag |= FWXFERQ_STREAM;
		it->flag |= FWXFERQ_EXTBUF;
		err = 0;
		break;
	case FW_GSTBUF:
		ibufreq->rx.nchunk = sc->fc->ir[sub]->bnchunk;
		ibufreq->rx.npacket = sc->fc->ir[sub]->bnpacket;
		ibufreq->rx.psize = sc->fc->ir[sub]->psize;

		ibufreq->tx.nchunk = sc->fc->it[sub]->bnchunk;
		ibufreq->tx.npacket = sc->fc->it[sub]->bnpacket;
		ibufreq->tx.psize = sc->fc->it[sub]->psize;
		break;
	case FW_ASYREQ:
		xfer = fw_xfer_alloc();
		if(xfer == NULL){
			err = ENOMEM;
			return err;
		}
		fp = &asyreq->pkt;
		switch (asyreq->req.type) {
		case FWASREQNODE:
			xfer->dst = ntohs(fp->mode.hdr.dst);
			break;
		case FWASREQEUI:
			xfer->dst = fw_noderesolve(sc->fc, asyreq->req.dst.eui);
			if(xfer->dst == FWNODE_INVAL ){
				printf("%s:cannot found node\n",
					device_get_nameunit(sc->fc->dev));
				err = EINVAL;
				goto error;
			}
			fp->mode.hdr.dst = htons(FWLOCALBUS | xfer->dst);
			break;
		case FWASRESTL:
			/* XXX what's this? */
			break;
		case FWASREQSTREAM:
			/* nothing to do */
			break;
		}
		xfer->spd = asyreq->req.sped;
		xfer->send.len = asyreq->req.len;
		xfer->send.buf = malloc(xfer->send.len, M_DEVBUF, M_NOWAIT);
		if(xfer->send.buf == NULL){
			return ENOMEM;
		}
		xfer->send.off = 0; 
		bcopy(fp, xfer->send.buf, xfer->send.len);
		xfer->act.hand = fw_asy_callback;
		err = fw_asyreq(sc->fc, sub, xfer);
		if(err){
			fw_xfer_free( xfer);
			return err;
		}
		err = tsleep((caddr_t)xfer, FWPRI, "asyreq", hz);
		if(err == 0){
			if(asyreq->req.len >= xfer->recv.len){
				asyreq->req.len = xfer->recv.len;
			}else{
				err = EINVAL;
			}
			bcopy(xfer->recv.buf + xfer->recv.off, fp, asyreq->req.len);
		}
error:
		fw_xfer_free( xfer);
		break;
	case FW_IBUSRST:
		sc->fc->ibr(sc->fc);
		break;
	case FW_CBINDADDR:
		fwb = fw_bindlookup(sc->fc,
				bindreq->start.hi, bindreq->start.lo);
		if(fwb == NULL){
			err = EINVAL;
			break;
		}
		STAILQ_REMOVE(&sc->fc->binds, fwb, fw_bind, fclist);
		STAILQ_REMOVE(&sc->fc->ir[sub]->binds, fwb, fw_bind, chlist);
		free(fwb, M_DEVBUF);
		break;
	case FW_SBINDADDR:
		if(bindreq->len <= 0 ){
			err = EINVAL;
			break;
		}
		if(bindreq->start.hi > 0xffff ){
			err = EINVAL;
			break;
		}
		fwb = (struct fw_bind *)malloc(sizeof (struct fw_bind), M_DEVBUF, M_DONTWAIT);
		if(fwb == NULL){
			err = ENOMEM;
			break;
		}
		fwb->start_hi = bindreq->start.hi;
		fwb->start_lo = bindreq->start.lo;
		fwb->addrlen = bindreq->len;

		xfer = fw_xfer_alloc();
		if(xfer == NULL){
			err = ENOMEM;
			return err;
		}
		xfer->act_type = FWACT_CH;
		xfer->sub = sub;
		xfer->fc = sc->fc;

		fwb->xfer = xfer;
		err = fw_bindadd(sc->fc, fwb);
		break;
	case FW_GDEVLST:
		i = 0;
		for(fwdev = TAILQ_FIRST(&sc->fc->devices); fwdev != NULL;
			fwdev = TAILQ_NEXT(fwdev, link)){
			if(i < fwdevlst->n){
				fwdevlst->dst[i] = fwdev->dst;
				fwdevlst->status[i] = 
					(fwdev->status == FWDEVATTACHED)?1:0;
				fwdevlst->eui[i].hi = fwdev->eui.hi;
				fwdevlst->eui[i].lo = fwdev->eui.lo;
			}
			i++;
		}
		fwdevlst->n = i;
		break;
	case FW_GTPMAP:
		bcopy(sc->fc->topology_map, data,
				(sc->fc->topology_map->crc_len + 1) * 4);
		break;
	case FW_GSPMAP:
		/* speed_map is larger than a page */
		err = copyout(sc->fc->speed_map, *(void **)data,
				(sc->fc->speed_map->crc_len + 1) * 4);
		break;
	case FW_GCROM:
		for (fwdev = TAILQ_FIRST(&sc->fc->devices); fwdev != NULL;
			fwdev = TAILQ_NEXT(fwdev, link)) {
			if (fwdev->eui.hi == crom_buf->eui.hi && 
					fwdev->eui.lo == crom_buf->eui.lo)
				break;
		}
		if (fwdev == NULL) {
			err = FWNODE_INVAL;
			break;
		}
#if 0
		if (fwdev->csrrom[0] >> 24 == 1)
			len = 4;
		else
			len = (1 + ((fwdev->csrrom[0] >> 16) & 0xff)) * 4;
#else
		if (fwdev->rommax < CSRROMOFF)
			len = 0;
		else
			len = fwdev->rommax - CSRROMOFF + 4;
#endif
		if (crom_buf->len < len)
			len = crom_buf->len;
		else
			crom_buf->len = len;
		err = copyout(&fwdev->csrrom[0], crom_buf->ptr, len);
		break;
	default:
		sc->fc->ioctl (dev, cmd, data, flag, td);
		break;
	}
	return err;
}
int
fw_poll(dev_t dev, int events, fw_proc *td)
{
	int revents;
	int tmp;
	int unit = DEV2UNIT(dev);
	int sub = DEV2DMACH(dev);
	struct firewire_softc *sc;

	if (DEV_FWMEM(dev))
		return fwmem_poll(dev, events, td);

	sc = devclass_get_softc(firewire_devclass, unit);
	revents = 0;
	tmp = POLLIN | POLLRDNORM;
	if (events & tmp) {
		if (STAILQ_FIRST(&sc->fc->ir[sub]->q) != NULL)
			revents |= tmp;
		else
			selrecord(td, &sc->fc->ir[sub]->rsel);
	}
	tmp = POLLOUT | POLLWRNORM;
	if (events & tmp) {
		/* XXX should be fixed */	
		revents |= tmp;
	}

	return revents;
}

/*
 * To lookup node id. from EUI64.
 */
u_int16_t
fw_noderesolve(struct firewire_comm *fc, struct fw_eui64 eui)
{
	struct fw_device *fwdev;
	for(fwdev = TAILQ_FIRST(&fc->devices); fwdev != NULL;
		fwdev = TAILQ_NEXT(fwdev, link)){
		if(fwdev->eui.hi == eui.hi && fwdev->eui.lo == eui.lo){
			break;
		}
	}
	if(fwdev == NULL) return FWNODE_INVAL;
	if(fwdev->status != FWDEVATTACHED) return FWNODE_INVAL;
	return fwdev->dst;
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
	if(xfer->send.len > fc->maxrec){
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
		len += ntohs(fp->mode.stream.len);
	else if (info->flag & FWTI_BLOCK_ASY)
		len += ntohs(fp->mode.rresb.len);
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
		printf("%s:Discard a packet (queued=%d)\n",
			device_get_nameunit(fc->dev), xferq->queued);
		return EINVAL;
	}


	if (info->flag & FWTI_TLABEL) {
		if((tl = fw_get_tlabel(fc, xfer)) == -1 )
			return EIO;
		fp->mode.hdr.tlrt = tl << 2;
	}

	xfer->tl = tl;
	xfer->tcode = tcode;
	xfer->resp = 0;
	xfer->fc = fc;
	xfer->q = xferq;
	xfer->act_type = FWACT_XFER;
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
#if 0
	printf("fw_asybusy\n");
#endif
#if XFER_TIMEOUT
	untimeout(fw_xfer_timeout, (void *)xfer, xfer->ch);
#endif
/*
	xfer->ch =  timeout((timeout_t *)fw_asystart, (void *)xfer, 20000);
*/
	DELAY(20000);
	fw_asystart(xfer);
	return;
}
#if XFER_TIMEOUT
/*
 * Post timeout for async. request.
 */
void
fw_xfer_timeout(void *arg)
{
	int s;
	struct fw_xfer *xfer;

	xfer = (struct fw_xfer *)arg;
	printf("fw_xfer_timeout status=%d resp=%d\n", xfer->state, xfer->resp);
	/* XXX set error code */
	s = splfw();
	xfer->act.hand(xfer);
	splx(s);
}
#endif
/*
 * Async. request with given xfer structure.
 */
static void
fw_asystart(struct fw_xfer *xfer)
{
	struct firewire_comm *fc = xfer->fc;
	int s;
	if(xfer->retry++ >= fc->max_asyretry){
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
#if XFER_TIMEOUT
	if (xfer->act.hand != NULL)
		xfer->ch = timeout(fw_xfer_timeout, (void *)xfer, hz);
#endif
	return;
}

static int
fw_mmap (dev_t dev, vm_offset_t offset, int nproto)
{  
	struct firewire_softc *fc;
	int unit = DEV2UNIT(dev);

	if (DEV_FWMEM(dev))
		return fwmem_mmap(dev, offset, nproto);

	fc = devclass_get_softc(firewire_devclass, unit);

	return EINVAL;
}

static int
firewire_match( device_t dev )
{
	device_set_desc(dev, "IEEE1394(Firewire) bus");
	return -140;
}

/*
 * The attach routine.
 */
static int
firewire_attach( device_t dev )
{
	int i, unitmask, mn;
	struct firewire_softc *sc = device_get_softc(dev);
	device_t pa = device_get_parent(dev);
	struct firewire_comm *fc;
	dev_t d;

	fc = (struct firewire_comm *)device_get_softc(pa);
	sc->fc = fc;
	sc->fc->dev = dev;

	unitmask = UNIT2MIN(device_get_unit(dev));

	if( fc->nisodma > FWMAXNDMA) fc->nisodma = FWMAXNDMA;
	for ( i = 0 ; i < fc->nisodma ; i++ ){
		mn = unitmask | i;
		/* XXX device name should be improved */
		d = make_dev(&firewire_cdevsw, unit2minor(mn),
			UID_ROOT, GID_OPERATOR, 0770,
			"fw%x", mn);
#if __FreeBSD_version >= 500000
		if (i == 0)
			sc->dev = d;
		else
			dev_depends(sc->dev, d);
#else
		sc->dev[i] = d;
#endif
	}
	d = make_dev(&firewire_cdevsw, unit2minor(unitmask | FWMEM_FLAG),
			UID_ROOT, GID_OPERATOR, 0770,
			"fwmem%d", device_get_unit(dev));
#if __FreeBSD_version >= 500000
	dev_depends(sc->dev, d);
#else
	sc->dev[i] = d;
#endif
	printf("%s: firewire bus attach\n", device_get_nameunit(sc->fc->dev));
	sc->fc->timeouthandle = timeout((timeout_t *)sc->fc->timeout, (void *)sc->fc, hz * 10);

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

/*
 * Dettach it.
 */
static int
firewire_detach( device_t dev )
{
	struct firewire_softc *sc;

	sc = (struct firewire_softc *)device_get_softc(dev);

#if __FreeBSD_version >= 500000
	destroy_dev(sc->dev);
#else
	{
		int j;
		for (j = 0 ; j < sc->fc->nisodma + 1; j++)
			destroy_dev(sc->dev[j]);
	}
#endif
	/* XXX xfree_free and untimeout on all xfers */
	untimeout((timeout_t *)sc->fc->timeout, sc->fc, sc->fc->timeouthandle);
	free(sc->fc->topology_map, M_DEVBUF);
	free(sc->fc->speed_map, M_DEVBUF);
	bus_generic_detach(dev);
	return(0);
}
#if 0
static int
firewire_shutdown( device_t dev )
{
	return 0;
}
#endif

/*
 * Called after bus reset.
 */
void
fw_busreset(struct firewire_comm *fc)
{
	int i;
	struct fw_xfer *xfer;

	switch(fc->status){
	case FWBUSMGRELECT:
		untimeout((timeout_t *)fw_try_bmr, (void *)fc, fc->bmrhandle);
		break;
	default:
		break;
	}
	fc->status = FWBUSRESET;
/* XXX: discard all queued packet */
	while((xfer = STAILQ_FIRST(&fc->atq->q)) != NULL){
		STAILQ_REMOVE_HEAD(&fc->atq->q, link);
		xfer->resp = EAGAIN;
		switch(xfer->act_type){
		case FWACT_XFER:
			fw_xfer_done(xfer);
			break;
		default:
			break;
		}
		fw_xfer_free( xfer);
	}
	while((xfer = STAILQ_FIRST(&fc->ats->q)) != NULL){
		STAILQ_REMOVE_HEAD(&fc->ats->q, link);
		xfer->resp = EAGAIN;
		switch(xfer->act_type){
		case FWACT_XFER:
			fw_xfer_done(xfer);
		default:
			break;
		}
		fw_xfer_free( xfer);
	}
	for(i = 0; i < fc->nisodma; i++)
		while((xfer = STAILQ_FIRST(&fc->it[i]->q)) != NULL){
			STAILQ_REMOVE_HEAD(&fc->it[i]->q, link);
			xfer->resp = 0;
			switch(xfer->act_type){
			case FWACT_XFER:
				fw_xfer_done(xfer);
				break;
			default:
				break;
			}
			fw_xfer_free( xfer);
		}

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

	fc->arq->psize = FWPMAX_S400;
	fc->ars->psize = FWPMAX_S400;
	fc->atq->psize = FWPMAX_S400;
	fc->ats->psize = FWPMAX_S400;


	fc->arq->buf = NULL;
	fc->ars->buf = NULL;
	fc->atq->buf = NULL;
	fc->ats->buf = NULL;

	fc->arq->flag = FWXFERQ_PACKET;
	fc->ars->flag = FWXFERQ_PACKET;
	fc->atq->flag = FWXFERQ_PACKET;
	fc->ats->flag = FWXFERQ_PACKET;

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
				M_DEVBUF, M_DONTWAIT | M_ZERO);
	fc->speed_map = (struct fw_speed_map *)malloc(
				sizeof(struct fw_speed_map),
				M_DEVBUF, M_DONTWAIT | M_ZERO);
	CSRARC(fc, TOPO_MAP) = 0x3f1 << 16;
	CSRARC(fc, TOPO_MAP + 4) = 1;
	CSRARC(fc, SPED_MAP) = 0x3f1 << 16;
	CSRARC(fc, SPED_MAP + 4) = 1;

	TAILQ_INIT(&fc->devices);
	STAILQ_INIT(&fc->pending);

/* Initialize csr ROM work space */
	SLIST_INIT(&fc->ongocsr);
	SLIST_INIT(&fc->csrfree);
	for( i = 0 ; i < FWMAXCSRDIR ; i++){
		csrd = (struct csrdir *) malloc(sizeof(struct csrdir), M_DEVBUF,M_DONTWAIT);
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


#ifdef FW_VMACCESS
	xfer = fw_xfer_alloc();
	if(xfer == NULL) return;

	fwb = (struct fw_bind *)malloc(sizeof (struct fw_bind), M_DEVBUF, M_DONTWAIT);
	if(fwb == NULL){
		fw_xfer_free(xfer);
	}
	xfer->act.hand = fw_vmaccess;
	xfer->act_type = FWACT_XFER;
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
static struct fw_bind *
fw_bindlookup(struct firewire_comm *fc, u_int32_t dest_hi, u_int32_t dest_lo)
{
	struct fw_bind *tfw;
	for(tfw = STAILQ_FIRST(&fc->binds) ; tfw != NULL ;
		tfw = STAILQ_NEXT(tfw, fclist)){
		if(tfw->xfer->act_type != FWACT_NULL &&
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
	if(!err && fwb->xfer->act_type == FWACT_CH){
		STAILQ_INSERT_HEAD(&fc->ir[fwb->xfer->sub]->binds, fwb, chlist);
	}
	return err;
}

/*
 * To free IEEE1394 address block.
 */
int
fw_bindremove(struct firewire_comm *fc, struct fw_bind *fwb)
{
	int s;

	s = splfw();
	/* shall we check the existance? */
	STAILQ_REMOVE(&fc->binds, fwb, fw_bind, fclist);
	splx(s);
	if (fwb->xfer)
		fw_xfer_free(fwb->xfer);

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
			free(tl, M_DEVBUF);
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
			return(xfer);
		}
	}
	splx(s);
	return(NULL);
}

/*
 * To allocate IEEE1394 XFER structure.
 */
struct fw_xfer *
fw_xfer_alloc()
{
	struct fw_xfer *xfer;

	xfer = malloc(sizeof(struct fw_xfer), M_DEVBUF, M_DONTWAIT | M_ZERO);
	if (xfer == NULL)
		return xfer;

	xfer->time = time_second;
	xfer->sub = -1;

	return xfer;
}

/*
 * IEEE1394 XFER post process.
 */
void
fw_xfer_done(struct fw_xfer *xfer)
{
	if (xfer->act.hand == NULL)
		return;

#if XFER_TIMEOUT
	untimeout(fw_xfer_timeout, (void *)xfer, xfer->ch);
#endif

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

/*
 * To free IEEE1394 XFER structure. 
 */
void
fw_xfer_free( struct fw_xfer* xfer)
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
	if(xfer->fc != NULL){
		if(xfer->state == FWXF_START){
#if 0 /* this could happen if we call fwohci_arcv() before fwohci_txd() */
			printf("fw_xfer_free FWXF_START\n");
#endif
			s = splfw();
			xfer->q->drain(xfer->fc, xfer);
			splx(s);
		}
	}
	if(xfer->send.buf != NULL){
		free(xfer->send.buf, M_DEVBUF);
	}
	if(xfer->recv.buf != NULL){
		free(xfer->recv.buf, M_DEVBUF);
	}
	if(xfer->fc != NULL){
		fw_tl_free(xfer->fc, xfer);
	}
	free(xfer, M_DEVBUF);
}

/*
 * Callback for PHY configuration. 
 */
static void
fw_phy_config_callback(struct fw_xfer *xfer)
{
#if 0
	printf("phy_config done state=%d resp=%d\n",
				xfer->state, xfer->resp);
#endif
	fw_xfer_free(xfer);
	/* XXX need bus reset ?? */
	/* sc->fc->ibr(xfer->fc);  LOOP */
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

	DELAY(100000);
	xfer = fw_xfer_alloc();
	xfer->send.len = 12;
	xfer->send.off = 0;
	xfer->fc = fc;
	xfer->retry_req = fw_asybusy;
	xfer->act.hand = fw_phy_config_callback;

	xfer->send.buf = malloc(sizeof(u_int32_t),
					M_DEVBUF, M_DONTWAIT | M_ZERO);
	fp = (struct fw_pkt *)xfer->send.buf;
	fp->mode.ld[1] = 0;
	if (root_node >= 0)
		fp->mode.ld[1] |= htonl((root_node & 0x3f) << 24 | 1 << 23);
	if (gap_count >= 0)
		fp->mode.ld[1] |= htonl(1 << 22 | (gap_count & 0x3f) << 16);
	fp->mode.ld[2] = ~fp->mode.ld[1];
/* XXX Dangerous, how to pass PHY packet to device driver */
	fp->mode.common.tcode |= FWTCODE_PHY;

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
void fw_sidrcv(struct firewire_comm* fc, caddr_t buf, u_int len, u_int off)
{
	u_int32_t *p, *sid = (u_int32_t *)(buf + off);
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
	printf("%s: %d nodes", device_get_nameunit(fc->dev), fc->max_node + 1);
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
#if 1
	printf(", maxhop <= %d", fc->max_hop);
#endif
		
	if(fc->irm == -1 ){
		printf(", Not found IRM capable node");
	}else{
		printf(", cable IRM = %d", fc->irm);
		if (fc->irm == fc->nodeid)
			printf(" (me)\n");
		else
			printf("\n");
	}

	if((fc->irm != -1) && (CSRARC(fc, BUS_MGR_ID) == 0x3f) ){
		if(fc->irm == ((CSRARC(fc, NODE_IDS) >> 16 ) & 0x3f)){
			fc->status = FWBUSMGRDONE;
			CSRARC(fc, BUS_MGR_ID) = fc->set_bmr(fc, fc->irm);
		}else{
			fc->status = FWBUSMGRELECT;
			fc->bmrhandle = timeout((timeout_t *)fw_try_bmr,(void *)fc, hz / 8);
		}
	}else{
		fc->status = FWBUSMGRDONE;
		printf("%s: BMR = %x\n", device_get_nameunit(fc->dev), CSRARC(fc, BUS_MGR_ID));
	}
	free(buf, M_DEVBUF);
#if 1
	/* XXX optimize gap_count, if I am BMGR */
	if(fc->irm == ((CSRARC(fc, NODE_IDS) >> 16 ) & 0x3f)){
		fw_phy_config(fc, -1, gap_cnt[fc->max_hop]);
	}
#endif
#if 1
	timeout((timeout_t *)fw_bus_probe, (void *)fc, hz/4);
#else
	fw_bus_probe(fc);
#endif
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
	for(fwdev = TAILQ_FIRST(&fc->devices); fwdev != NULL; fwdev = next) {
		next = TAILQ_NEXT(fwdev, link);
		if(fwdev->status != FWDEVINVAL){
			fwdev->status = FWDEVINVAL;
			fwdev->rcnt = 0;
		}else if(fwdev->rcnt < FW_MAXDEVRCNT){
			fwdev->rcnt ++;
		}else{
			TAILQ_REMOVE(&fc->devices, fwdev, link);
			free(fwdev, M_DEVBUF);
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
	struct fw_device *fwdev, *tfwdev;
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
		printf("fw_bus_explore: node %d link down\n", fc->ongonode);
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
		for(fwdev = TAILQ_FIRST(&fc->devices); fwdev != NULL;
			fwdev = TAILQ_NEXT(fwdev, link)){
			if(fwdev->eui.hi == fc->ongoeui.hi && fwdev->eui.lo == fc->ongoeui.lo){
				break;
			}
		}
		if(fwdev != NULL){
			fwdev->dst = fc->ongonode;
			fwdev->status = FWDEVATTACHED;
			fc->ongonode++;
			fc->ongoaddr = CSRROMOFF;
			fc->ongodev = NULL;
			fc->ongoeui.hi = 0xffffffff; fc->ongoeui.lo = 0xffffffff;
			goto loop;
		}
		fwdev = malloc(sizeof(struct fw_device), M_DEVBUF, M_DONTWAIT);
		if(fwdev == NULL)
			return;
		fwdev->rommax = 0;
		fwdev->dst = fc->ongonode;
		fwdev->eui.hi = fc->ongoeui.hi; fwdev->eui.lo = fc->ongoeui.lo;
		fwdev->status = FWDEVINIT;
#if 0
		fwdev->speed = CSRARC(fc, SPED_MAP + 8 + fc->ongonode / 4)
			>> ((3 - (fc->ongonode % 4)) * 8);
#else
		fwdev->speed = fc->speed_map->speed[fc->nodeid][fc->ongonode];
#endif

		tfwdev = TAILQ_FIRST(&fc->devices);
		while( tfwdev != NULL &&
			(tfwdev->eui.hi > fwdev->eui.hi) &&
			((tfwdev->eui.hi == fwdev->eui.hi) &&
				tfwdev->eui.lo > fwdev->eui.lo)){
			tfwdev = TAILQ_NEXT( tfwdev, link);
		}
		if(tfwdev == NULL){
			TAILQ_INSERT_TAIL(&fc->devices, fwdev, link);
		}else{
			TAILQ_INSERT_BEFORE(tfwdev, fwdev, link);
		}

		printf("%s:Discover new %s device ID:%08x%08x\n", device_get_nameunit(fc->dev), linkspeed[fwdev->speed], fc->ongoeui.hi, fc->ongoeui.lo);

		fc->ongodev = fwdev;
		fc->ongoaddr = CSRROMOFF;
		addr = 0xf0000000 | fc->ongoaddr;
	}else{
		addr = 0xf0000000 | fc->ongoaddr;
	}
#if 0
	xfer = asyreqq(fc, FWSPD_S100, 0, 0,
		((FWLOCALBUS | fc->ongonode) << 16) | 0xffff , addr,
		fw_bus_explore_callback);
	if(xfer == NULL) goto done;
#else
	xfer = fw_xfer_alloc();
	if(xfer == NULL){
		goto done;
	}
	xfer->send.len = 16;
	xfer->spd = 0;
	xfer->send.buf = malloc(16, M_DEVBUF, M_DONTWAIT);
	if(xfer->send.buf == NULL){
		fw_xfer_free( xfer);
		return;
	}

	xfer->send.off = 0; 
	fp = (struct fw_pkt *)xfer->send.buf;
	fp->mode.rreqq.dest_hi = htons(0xffff);
	fp->mode.rreqq.tlrt = 0;
	fp->mode.rreqq.tcode = FWTCODE_RREQQ;
	fp->mode.rreqq.pri = 0;
	fp->mode.rreqq.src = 0;
	xfer->dst = FWLOCALBUS | fc->ongonode;
	fp->mode.rreqq.dst = htons(xfer->dst);
	fp->mode.rreqq.dest_lo = htonl(addr);
	xfer->act.hand = fw_bus_explore_callback;

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

	xfer = fw_xfer_alloc();
	if(xfer == NULL){
		return NULL;
	}
	xfer->send.len = 16;
	xfer->spd = spd; /* XXX:min(spd, fc->spd) */
	xfer->send.buf = malloc(16, M_DEVBUF, M_DONTWAIT);
	if(xfer->send.buf == NULL){
		fw_xfer_free( xfer);
		return NULL;
	}

	xfer->send.off = 0; 
	fp = (struct fw_pkt *)xfer->send.buf;
	fp->mode.rreqq.dest_hi = htons(addr_hi & 0xffff);
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
	fp->mode.rreqq.dst = htons(xfer->dst);
	fp->mode.rreqq.dest_lo = htonl(addr_lo);
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

	
	if(xfer == NULL) return;
	fc = xfer->fc;
	if(xfer->resp != 0){
		printf("resp != 0: node=%d addr=0x%x\n",
			fc->ongonode, fc->ongoaddr);
		fc->retry_count++;
		goto nextnode;
	}

	if(xfer->send.buf == NULL){
		printf("send.buf == NULL: node=%d addr=0x%x\n",
			fc->ongonode, fc->ongoaddr);
		printf("send.buf == NULL\n");
		fc->retry_count++;
		goto nextnode;
	}
	sfp = (struct fw_pkt *)xfer->send.buf;

	if(xfer->recv.buf == NULL){
		printf("recv.buf == NULL: node=%d addr=0x%x\n",
			fc->ongonode, fc->ongoaddr);
		fc->retry_count++;
		goto nextnode;
	}
	rfp = (struct fw_pkt *)xfer->recv.buf;
#if 0
	{
		u_int32_t *qld;
		int i;
		qld = (u_int32_t *)xfer->recv.buf;
		printf("len:%d\n", xfer->recv.len);
		for( i = 0 ; i <= xfer->recv.len && i < 32; i+= 4){
			printf("0x%08x ", ntohl(rfp->mode.ld[i/4]));
			if((i % 16) == 15) printf("\n");
		}
		if((i % 16) != 15) printf("\n");
	}
#endif
	if(fc->ongodev == NULL){
		if(sfp->mode.rreqq.dest_lo == htonl((0xf0000000 | CSRROMOFF))){
			rfp->mode.rresq.data = ntohl(rfp->mode.rresq.data);
			chdr = (struct csrhdr *)(&rfp->mode.rresq.data);
/* If CSR is minimul confinguration, more investgation is not needed. */
			if(chdr->info_len == 1){
				goto nextnode;
			}else{
				fc->ongoaddr = CSRROMOFF + 0xc;
			}
		}else if(sfp->mode.rreqq.dest_lo == htonl((0xf0000000 |(CSRROMOFF + 0xc)))){
			fc->ongoeui.hi = ntohl(rfp->mode.rresq.data);
			fc->ongoaddr = CSRROMOFF + 0x10;
		}else if(sfp->mode.rreqq.dest_lo == htonl((0xf0000000 |(CSRROMOFF + 0x10)))){
			fc->ongoeui.lo = ntohl(rfp->mode.rresq.data);
			if (fc->ongoeui.hi == 0 && fc->ongoeui.lo == 0)
				goto nextnode;
			fc->ongoaddr = CSRROMOFF;
		}
	}else{
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
#if 0
/*
 * Async. write responce support for kernel internal use. 
 */
int
fw_writeres(struct firewire_comm *fc, u_int32_t dst, u_int32_t tlrt)
{
	int err = 0;
	struct fw_xfer *xfer;
	struct fw_pkt *fp;

	xfer = fw_xfer_alloc();
	if(xfer == NULL){
		err = ENOMEM;
		return err;
	}
	xfer->send.len = 12;
	xfer->spd = 0;
	xfer->send.buf = malloc(xfer->send.len, M_DEVBUF, M_NOWAIT);
	if(xfer->send.buf == NULL){
		return ENOMEM;
	}
	xfer->send.off = 0; 
	fp = (struct fw_pkt *)xfer->send.buf;
	 
	fp->mode.wres.tlrt = tlrt;
	fp->mode.wres.tcode = FWTCODE_WRES;
	fp->mode.wres.pri = 0;
	fp->mode.wres.dst = htons(dst);

	xfer->act.hand = fw_asy_callback;
	err = fw_asyreq(fc, -1, xfer);
	if(err){
		fw_xfer_free( xfer);
		return err;
	}
	err = tsleep((caddr_t)xfer, FWPRI, "asyreq", 0);
	fw_xfer_free( xfer);

	return err;
}

/*
 * Async. read responce block support for kernel internal use. 
 */
int
fw_readresb(struct firewire_comm *fc, u_int32_t dst, u_int32_t tlrt,
	u_int32_t len, u_int32_t *buf)
{
	int err = 0;
	struct fw_xfer *xfer ;
	struct fw_pkt *fp;

	xfer = fw_xfer_alloc();
	if(xfer == NULL){
		err = ENOMEM;
		return err;
	}
	xfer->send.len = sizeof(struct fw_pkt) + len;
	xfer->spd = 0;
	xfer->send.buf = malloc(sizeof(struct fw_pkt) + 1024, M_DEVBUF, M_DONTWAIT);
	if(xfer->send.buf == NULL){
		return ENOMEM;
	}
	xfer->send.off = 0; 
	fp = (struct fw_pkt *)xfer->send.buf;
	fp->mode.rresb.tlrt = tlrt;
	fp->mode.rresb.tcode = FWTCODE_RRESB;
	fp->mode.rresb.pri = 0;
	fp->mode.rresb.dst = htons(dst);
	fp->mode.rresb.rtcode = 0;
	fp->mode.rresb.extcode = 0;
	fp->mode.rresb.len = htons(len);
	bcopy(buf, fp->mode.rresb.payload, len);
	xfer->act.hand = fw_asy_callback;
	err = fw_asyreq(fc, -1, xfer);
	if(err){
		fw_xfer_free( xfer);
		return err;
	}
	err = tsleep((caddr_t)xfer, FWPRI, "asyreq", 0);

	fw_xfer_free( xfer);
	return err;
}

/*
 * Async. write request block support for kernel internal use. 
 */
int
fw_writereqb(struct firewire_comm *fc, u_int32_t addr_hi, u_int32_t addr_lo,
	u_int len, u_int32_t *buf)
{
	int err = 0;
	struct fw_xfer *xfer ;
	struct fw_pkt *fp;

	xfer = fw_xfer_alloc();
	if(xfer == NULL){
		err = ENOMEM;
		return err;
	}
	xfer->send.len = sizeof(struct fw_pkt) + len;
	xfer->spd = 0;
	xfer->send.buf = malloc(sizeof(struct fw_pkt) + 1024, M_DEVBUF, M_DONTWAIT);
	if(xfer->send.buf == NULL){
		return ENOMEM;
	}
	xfer->send.off = 0; 
	fp = (struct fw_pkt *)xfer->send.buf;
	fp->mode.wreqb.dest_hi = htonl(addr_hi & 0xffff);
	fp->mode.wreqb.tlrt = 0;
	fp->mode.wreqb.tcode = FWTCODE_WREQB;
	fp->mode.wreqb.pri = 0;
	fp->mode.wreqb.dst = htons(addr_hi >> 16);
	fp->mode.wreqb.dest_lo = htonl(addr_lo);
	fp->mode.wreqb.len = htons(len);
	fp->mode.wreqb.extcode = 0;
	bcopy(buf, fp->mode.wreqb.payload, len);
	xfer->act.hand = fw_asy_callback;
	err = fw_asyreq(fc, -1, xfer);
	if(err){
		fw_xfer_free( xfer);
		return err;
	}
	err = tsleep((caddr_t)xfer, FWPRI, "asyreq", 0);

	fw_xfer_free( xfer);
	return err;
}

/*
 * Async. read request support for kernel internal use. 
 */
int
fw_readreqq(struct firewire_comm *fc, u_int32_t addr_hi, u_int32_t addr_lo, u_int32_t *ret){
	int err = 0;
	struct fw_xfer *xfer ;
	struct fw_pkt *fp, *rfp;

	xfer = fw_xfer_alloc();
	if(xfer == NULL){
		err = ENOMEM;
		return err;
	}
	xfer->send.len = 16;
	xfer->spd = 0;
	xfer->send.buf = malloc(16, M_DEVBUF, M_DONTWAIT);
	if(xfer->send.buf == NULL){
		return ENOMEM;
	}
	xfer->send.off = 0; 
	fp = (struct fw_pkt *)xfer->send.buf;
	fp->mode.rreqq.dest_hi = htonl(addr_hi & 0xffff);
	fp->mode.rreqq.tlrt = 0;
	fp->mode.rreqq.tcode = FWTCODE_RREQQ;
	fp->mode.rreqq.pri = 0;
	xfer->dst = addr_hi >> 16;
	fp->mode.rreqq.dst = htons(xfer->dst);
	fp->mode.rreqq.dest_lo = htonl(addr_lo);
	xfer->act.hand = fw_asy_callback;
	err = fw_asyreq(fc, -1, xfer);
	if(err){
		fw_xfer_free( xfer);
		return err;
	}
	err = tsleep((caddr_t)xfer, FWPRI, "asyreq", 0);

	if(err == 0 && xfer->recv.buf != NULL){
		rfp = (struct fw_pkt *)xfer->recv.buf;
		*ret = ntohl(rfp->mode.rresq.data);
	}
	fw_xfer_free( xfer);
	return err;
}
#endif
/*
 * To obtain CSR register values.
 */
u_int32_t getcsrdata(struct fw_device *fwdev, u_int8_t key)
{
	int i;
	struct csrhdr *chdr;
	struct csrreg *creg;
	chdr = (struct csrhdr *)&fwdev->csrrom[0];
	for( i = chdr->info_len + 4; i <= fwdev->rommax - CSRROMOFF; i+=4){
		creg = (struct csrreg *)&fwdev->csrrom[i/4];
		if(creg->key == key){
			return (u_int32_t)creg->val;
		}
	}
	return 0;
}
/*
 * To attach sub-devices layer onto IEEE1394 bus.
 */
static void fw_attach_dev(struct firewire_comm *fc)
{
	struct fw_device *fwdev;
	struct fw_xfer *xfer;
	int i, err;
	device_t *devlistp;
	int devcnt;
	struct firewire_dev_comm *fdc;

	for(fwdev = TAILQ_FIRST(&fc->devices); fwdev != NULL;
			fwdev = TAILQ_NEXT(fwdev, link)){
		if(fwdev->status == FWDEVINIT){
			fwdev->spec = getcsrdata(fwdev, CSRKEY_SPEC);
			if(fwdev->spec == 0)
				continue;
			fwdev->ver = getcsrdata(fwdev, CSRKEY_VER);
			if(fwdev->ver == 0)
				continue;
			fwdev->maxrec = (fwdev->csrrom[2] >> 12) & 0xf;

			switch(fwdev->spec){
			case CSRVAL_ANSIT10:
				switch(fwdev->ver){
				case CSRVAL_T10SBP2:
					printf("Device SBP-II");
					break;
				default:
					break;
				}
				break;
			case CSRVAL_1394TA:
				switch(fwdev->ver){
				case CSR_PROTAVC:
					printf("Device AV/C");
					break;
				case CSR_PROTCAL:
					printf("Device CAL");
					break;
				case CSR_PROTEHS:
					printf("Device EHS");
					break;
				case CSR_PROTHAVI:
					printf("Device HAVi");
					break;
				case CSR_PROTCAM104:
					printf("Device 1394 Cam 1.04");
					break;
				case CSR_PROTCAM120:
					printf("Device 1394 Cam 1.20");
					break;
				case CSR_PROTCAM130:
					printf("Device 1394 Cam 1.30");
					break;
				case CSR_PROTDPP:
					printf("Device 1394 Direct print");
					break;
				case CSR_PROTIICP:
					printf("Device Industrial & Instrument");
					break;
				default:
					printf("Device unkwon 1394TA");
					break;
				}
				break;
			default:
				break;
			}
			fwdev->status = FWDEVATTACHED;
			printf("\n");
		}
	}
	err = device_get_children(fc->dev, &devlistp, &devcnt);
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
		printf("retry_count = %d\n", fc->retry_count);
		fc->retry_probe_handle = timeout((timeout_t *)fc->ibr,
							(void *)fc, hz*2);
	}
	return;
}
/*
 * To allocate uniq transaction label.
 */
static int fw_get_tlabel(struct firewire_comm *fc, struct fw_xfer *xfer)
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
			tl = malloc(sizeof(struct tlabel),M_DEVBUF,M_DONTWAIT);
			if (tl == NULL) {
				splx(s);
				return (-1);
			}
			tl->xfer = xfer;
			STAILQ_INSERT_TAIL(&fc->tlabels[label], tl, link);
			splx(s);
			return(label);
		}
	}
	splx(s);

	printf("fw_get_tlabel: no free tlabel\n");
	return(-1);
}
/*
 * Generic packet receving process.
 */
void fw_rcv(struct firewire_comm* fc, caddr_t buf, u_int len, u_int sub, u_int off, u_int spd)
{
	struct fw_pkt *fp, *resfp;
	struct fw_xfer *xfer;
	struct fw_bind *bind;
	struct firewire_softc *sc;
	int s;
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
	fp = (struct fw_pkt *)(buf + off);
	switch(fp->mode.common.tcode){
	case FWTCODE_WRES:
	case FWTCODE_RRESQ:
	case FWTCODE_RRESB:
	case FWTCODE_LRES:
		xfer = fw_tl2xfer(fc, ntohs(fp->mode.hdr.src),
					fp->mode.hdr.tlrt >> 2);
		if(xfer == NULL) {
			printf("fw_rcv: unknown response "
					"tcode=%d src=0x%x tl=%x rt=%d data=0x%x\n",
					fp->mode.common.tcode,
					ntohs(fp->mode.hdr.src),
					fp->mode.hdr.tlrt >> 2,
					fp->mode.hdr.tlrt & 3,
					fp->mode.rresq.data);
#if 1
			printf("try ad-hoc work around!!\n");
			xfer = fw_tl2xfer(fc, ntohs(fp->mode.hdr.src),
					(fp->mode.hdr.tlrt >> 2)^3);
			if (xfer == NULL) {
				printf("no use...\n");
				goto err;
			}
#else
			goto err;
#endif
		}
		switch(xfer->act_type){
		case FWACT_XFER:
			if((xfer->sub >= 0) &&
				((fc->ir[xfer->sub]->flag & FWXFERQ_MODEMASK ) == 0)){
				xfer->resp = EINVAL;
				fw_xfer_done(xfer);
				goto err;
			}
			xfer->recv.len = len;
			xfer->recv.off = off;
			xfer->recv.buf = buf;
			xfer->resp = 0;
			fw_xfer_done(xfer);
			return;
			break;
		case FWACT_CH:
		default:
			goto err;
			break;
		}
		break;
	case FWTCODE_WREQQ:
	case FWTCODE_WREQB:
	case FWTCODE_RREQQ:
	case FWTCODE_RREQB:
	case FWTCODE_LREQ:
		bind = fw_bindlookup(fc, ntohs(fp->mode.rreqq.dest_hi),
			ntohl(fp->mode.rreqq.dest_lo));
		if(bind == NULL){
			printf("Unknown service addr 0x%08x:0x%08x tcode=%x\n",
				ntohs(fp->mode.rreqq.dest_hi),
				ntohl(fp->mode.rreqq.dest_lo),
				fp->mode.common.tcode);
			if (fc->status == FWBUSRESET) {
				printf("fw_rcv: cannot response(bus reset)!\n");
				goto err;
			}
			xfer = fw_xfer_alloc();
			if(xfer == NULL){
				return;
			}
			xfer->spd = spd;
			xfer->send.buf = malloc(16, M_DEVBUF, M_DONTWAIT);
			resfp = (struct fw_pkt *)xfer->send.buf;
			switch(fp->mode.common.tcode){
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
		switch(bind->xfer->act_type){
		case FWACT_XFER:
			xfer = fw_xfer_alloc();
			if(xfer == NULL) goto err;
			xfer->fc = bind->xfer->fc;
			xfer->sc = bind->xfer->sc;
			xfer->recv.buf = buf;
			xfer->recv.len = len;
			xfer->recv.off = off;
			xfer->spd = spd;
			xfer->act.hand = bind->xfer->act.hand;
			if (fc->status != FWBUSRESET)
				xfer->act.hand(xfer);
			else
				STAILQ_INSERT_TAIL(&fc->pending, xfer, link);
			return;
			break;
		case FWACT_CH:
			if(fc->ir[bind->xfer->sub]->queued >=
				fc->ir[bind->xfer->sub]->maxq){
				printf("%s:Discard a packet %x %d\n",
					device_get_nameunit(fc->dev),
					bind->xfer->sub,
					fc->ir[bind->xfer->sub]->queued);
				goto err;
			}
			xfer = fw_xfer_alloc();
			if(xfer == NULL) goto err;
			xfer->recv.buf = buf;
			xfer->recv.len = len;
			xfer->recv.off = off;
			xfer->spd = spd;
			s = splfw();
			fc->ir[bind->xfer->sub]->queued++;
			STAILQ_INSERT_TAIL(&fc->ir[bind->xfer->sub]->q, xfer, link);
			splx(s);

			wakeup((caddr_t)fc->ir[bind->xfer->sub]);

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
		xfer = fw_xfer_alloc();
		if(xfer == NULL) goto err;
		xfer->recv.buf = buf;
		xfer->recv.len = len;
		xfer->recv.off = off;
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
		printf("fw_rcv: unknow tcode\n");
		break;
	}
err:
	free(buf, M_DEVBUF);
}
/*
 * Post process for Bus Manager election process.
 */
static void
fw_try_bmr_callback(struct fw_xfer *xfer)
{
	struct fw_pkt *sfp,*rfp;
	struct firewire_comm *fc;

	if(xfer == NULL) return;
	fc = xfer->fc;
	if(xfer->resp != 0){
		goto error;
	}

	if(xfer->send.buf == NULL){
		goto error;
	}
	sfp = (struct fw_pkt *)xfer->send.buf;

	if(xfer->recv.buf == NULL){
		goto error;
	}
	rfp = (struct fw_pkt *)xfer->recv.buf;
	CSRARC(fc, BUS_MGR_ID)
		= fc->set_bmr(fc, ntohl(rfp->mode.lres.payload[0]) & 0x3f);
	printf("%s: new bus manager %d ",
		device_get_nameunit(fc->dev), CSRARC(fc, BUS_MGR_ID));
	if((htonl(rfp->mode.lres.payload[0]) & 0x3f) == fc->nodeid){
		printf("(me)\n");
/* If I am bus manager, optimize gapcount */
		if(fc->max_hop <= MAX_GAPHOP ){
			fw_phy_config(fc, -1, gap_cnt[fc->max_hop]);
		}
	}else{
		printf("\n");
	}
error:
	fw_xfer_free(xfer);
}
/*
 * To candidate Bus Manager election process.
 */
void fw_try_bmr(void *arg)
{
	struct fw_xfer *xfer;
	struct firewire_comm *fc = (struct firewire_comm *)arg;
	struct fw_pkt *fp;
	int err = 0;

	xfer = fw_xfer_alloc();
	if(xfer == NULL){
		return;
	}
	xfer->send.len = 24;
	xfer->spd = 0;
	xfer->send.buf = malloc(24, M_DEVBUF, M_DONTWAIT);
	if(xfer->send.buf == NULL){
		fw_xfer_free( xfer);
		return;
	}

	fc->status = FWBUSMGRELECT;

	xfer->send.off = 0; 
	fp = (struct fw_pkt *)xfer->send.buf;
	fp->mode.lreq.dest_hi = htons(0xffff);
	fp->mode.lreq.tlrt = 0;
	fp->mode.lreq.tcode = FWTCODE_LREQ;
	fp->mode.lreq.pri = 0;
	fp->mode.lreq.src = 0;
	fp->mode.lreq.len = htons(8);
	fp->mode.lreq.extcode = htons(FW_LREQ_CMPSWAP);
	xfer->dst = FWLOCALBUS | fc->irm;
	fp->mode.lreq.dst = htons(xfer->dst);
	fp->mode.lreq.dest_lo = htonl(0xf0000000 | BUS_MGR_ID);
	fp->mode.lreq.payload[0] = 0x3f;
	fp->mode.lreq.payload[1] = fc->nodeid;
	xfer->act_type = FWACT_XFER;
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
static void fw_vmaccess(struct fw_xfer *xfer){
	struct fw_pkt *rfp, *sfp = NULL;
	u_int32_t *ld = (u_int32_t *)(xfer->recv.buf + xfer->recv.off);

	printf("vmaccess spd:%2x len:%03x %d data:%08x %08x %08x %08x\n",
			xfer->spd, xfer->recv.len, xfer->recv.off, ntohl(ld[0]), ntohl(ld[1]), ntohl(ld[2]), ntohl(ld[3]));
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
			xfer->send.buf = malloc(12, M_DEVBUF, M_NOWAIT);
			xfer->send.len = 12;
			sfp = (struct fw_pkt *)xfer->send.buf;
			bcopy(rfp->mode.wreqb.payload,
				(caddr_t)ntohl(rfp->mode.wreqb.dest_lo), ntohs(rfp->mode.wreqb.len));
			sfp->mode.wres.tcode = FWTCODE_WRES;
			sfp->mode.wres.rtcode = 0;
			break;
		case FWTCODE_WREQQ:
			xfer->send.buf = malloc(12, M_DEVBUF, M_NOWAIT);
			xfer->send.len = 12;
			sfp->mode.wres.tcode = FWTCODE_WRES;
			*((u_int32_t *)(ntohl(rfp->mode.wreqb.dest_lo))) = rfp->mode.wreqq.data;
			sfp->mode.wres.rtcode = 0;
			break;
		case FWTCODE_RREQB:
			xfer->send.buf = malloc(16 + rfp->mode.rreqb.len, M_DEVBUF, M_NOWAIT);
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
			xfer->send.buf = malloc(16, M_DEVBUF, M_NOWAIT);
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
	xfer->send.off = 0;
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
u_int16_t fw_crc16(u_int32_t *ptr, u_int32_t len){
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
DRIVER_MODULE(firewire,fwohci,firewire_driver,firewire_devclass,0,0);
MODULE_VERSION(firewire, 1);
