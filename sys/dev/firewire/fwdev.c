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

#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/conf.h>
#include <sys/uio.h>
#include <sys/poll.h>

#include <sys/bus.h>

#include <sys/ioccom.h>

#include <dev/firewire/firewire.h>
#include <dev/firewire/firewirereg.h>
#include <dev/firewire/fwmem.h>
#include <dev/firewire/iec68113.h>

#define CDEV_MAJOR 127
#define	FWNODE_INVAL 0xffff

static	d_open_t	fw_open;
static	d_close_t	fw_close;
static	d_ioctl_t	fw_ioctl;
static	d_poll_t	fw_poll;
static	d_read_t	fw_read;	/* for Isochronous packet */
static	d_write_t	fw_write;
static	d_mmap_t	fw_mmap;

struct cdevsw firewire_cdevsw = 
{
	fw_open, fw_close, fw_read, fw_write, fw_ioctl,
	fw_poll, fw_mmap, nostrategy, "fw", CDEV_MAJOR, nodump, nopsize, D_MEM
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
#ifdef FWXFERQ_DV
	if(sc->fc->it[sub]->flag & FWXFERQ_DV){
		struct fw_dvbuf *dvbuf;

		if((dvbuf = sc->fc->it[sub]->dvproc) != NULL){
			free(dvbuf->buf, M_FW);
			sc->fc->it[sub]->dvproc = NULL;
		}
		if((dvbuf = sc->fc->it[sub]->dvdma) != NULL){
			free(dvbuf->buf, M_FW);
			sc->fc->it[sub]->dvdma = NULL;
		}
		while((dvbuf = STAILQ_FIRST(&sc->fc->it[sub]->dvvalid)) != NULL){
			STAILQ_REMOVE_HEAD(&sc->fc->it[sub]->dvvalid, link);
			free(dvbuf->buf, M_FW);
		}
		while((dvbuf = STAILQ_FIRST(&sc->fc->it[sub]->dvfree)) != NULL){
			STAILQ_REMOVE_HEAD(&sc->fc->it[sub]->dvfree, link);
			free(dvbuf->buf, M_FW);
		}
		free(sc->fc->it[sub]->dvbuf, M_FW);
		sc->fc->it[sub]->dvbuf = NULL;
	}
#endif
	if(sc->fc->ir[sub]->flag & FWXFERQ_EXTBUF){
		free(sc->fc->ir[sub]->buf, M_FW);
		sc->fc->ir[sub]->buf = NULL;
		free(sc->fc->ir[sub]->bulkxfer, M_FW);
		sc->fc->ir[sub]->bulkxfer = NULL;
		sc->fc->ir[sub]->flag &= ~FWXFERQ_EXTBUF;
		sc->fc->ir[sub]->psize = PAGE_SIZE;
		sc->fc->ir[sub]->maxq = FWMAXQUEUE;
	}
	if(sc->fc->it[sub]->flag & FWXFERQ_EXTBUF){
		free(sc->fc->it[sub]->buf, M_FW);
		sc->fc->it[sub]->buf = NULL;
		free(sc->fc->it[sub]->bulkxfer, M_FW);
		sc->fc->it[sub]->bulkxfer = NULL;
#ifdef FWXFERQ_DV
		sc->fc->it[sub]->dvbuf = NULL;
#endif
		sc->fc->it[sub]->flag &= ~FWXFERQ_EXTBUF;
		sc->fc->it[sub]->psize = 0;
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
		free(fwb, M_FW);
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

	if (ir->flag & FWXFERQ_PACKET) {
		ir->stproc = NULL;
	}
readloop:
	xfer = STAILQ_FIRST(&ir->q);
	if ((ir->flag & FWXFERQ_PACKET) == 0 && ir->stproc == NULL) {
		/* iso bulkxfer */
		ir->stproc = STAILQ_FIRST(&ir->stvalid);
		if (ir->stproc != NULL) {
			s = splfw();
			STAILQ_REMOVE_HEAD(&ir->stvalid, link);
			splx(s);
			ir->queued = 0;
		}
	}
	if (xfer == NULL && ir->stproc == NULL) {
		/* no data avaliable */
		if (slept == 0) {
			slept = 1;
			if ((ir->flag & FWXFERQ_RUNNING) == 0
					&& (ir->flag & FWXFERQ_PACKET)) {
				err = sc->fc->irx_enable(sc->fc, sub);
				if (err)
					return err;
			}
			ir->flag |= FWXFERQ_WAKEUP;
			err = tsleep((caddr_t)ir, FWPRI, "fw_read", hz);
			ir->flag &= ~FWXFERQ_WAKEUP;
			if (err == 0)
				goto readloop;
		} else if (slept == 1)
			err = EIO;
		return err;
	} else if(xfer != NULL) {
		/* per packet mode */
		s = splfw();
		ir->queued --;
		STAILQ_REMOVE_HEAD(&ir->q, link);
		splx(s);
		fp = (struct fw_pkt *)(xfer->recv.buf + xfer->recv.off);
		if(sc->fc->irx_post != NULL)
			sc->fc->irx_post(sc->fc, fp->mode.ld);
		err = uiomove(xfer->recv.buf + xfer->recv.off, xfer->recv.len, uio);
		fw_xfer_free( xfer);
	} else if(ir->stproc != NULL) {
		/* iso bulkxfer */
		fp = (struct fw_pkt *)(ir->stproc->buf + ir->queued * ir->psize);
		if(sc->fc->irx_post != NULL)
			sc->fc->irx_post(sc->fc, fp->mode.ld);
		if(ntohs(fp->mode.stream.len) == 0){
			err = EIO;
			return err;
		}
		err = uiomove((caddr_t)fp,
			ntohs(fp->mode.stream.len) + sizeof(u_int32_t), uio);
#if 0
		fp->mode.stream.len = 0;
#endif
		ir->queued ++;
		if(ir->queued >= ir->bnpacket){
			s = splfw();
			STAILQ_INSERT_TAIL(&ir->stfree, ir->stproc, link);
			splx(s);
			sc->fc->irx_enable(sc->fc, sub);
			ir->stproc = NULL;
		}
		if (uio->uio_resid >= ir->psize) {
			slept = -1;
			goto readloop;
		}
	}
	return err;
}

static int
fw_write (dev_t dev, struct uio *uio, int ioflag)
{
	int err = 0;
	struct firewire_softc *sc;
	int unit = DEV2UNIT(dev);
	int sub = DEV2DMACH(dev);
	int s, slept = 0;
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
		STAILQ_INSERT_TAIL(&it->stfree, it->stproc, link);
		splx(s);
		it->stproc = NULL;
	}
#ifdef FWXFERQ_DV
	if(xferq == NULL && !(it->flag & FWXFERQ_DV)){
#else
	if (xferq == NULL) {
#endif
isoloop:
		if (it->stproc == NULL) {
			it->stproc = STAILQ_FIRST(&it->stfree);
			if (it->stproc != NULL) {
				s = splfw();
				STAILQ_REMOVE_HEAD(&it->stfree, link);
				splx(s);
				it->queued = 0;
			} else if (slept == 0) {
				slept = 1;
				err = sc->fc->itx_enable(sc->fc, sub);
				if (err)
					return err;
				err = tsleep((caddr_t)it, FWPRI,
							"fw_write", hz);
				if (err)
					return err;
				goto isoloop;
			} else {
				err = EIO;
				return err;
			}
		}
		fp = (struct fw_pkt *)
			(it->stproc->buf + it->queued * it->psize);
		err = uiomove((caddr_t)fp, sizeof(struct fw_isohdr), uio);
		err = uiomove((caddr_t)fp->mode.stream.payload,
					ntohs(fp->mode.stream.len), uio);
		it->queued ++;
		if (it->queued >= it->bnpacket) {
			s = splfw();
			STAILQ_INSERT_TAIL(&it->stvalid, it->stproc, link);
			splx(s);
			it->stproc = NULL;
			err = sc->fc->itx_enable(sc->fc, sub);
		}
		if (uio->uio_resid >= sizeof(struct fw_isohdr)) {
			slept = 0;
			goto isoloop;
		}
		return err;
	}
#ifdef FWXFERQ_DV
	if(xferq == NULL && it->flag & FWXFERQ_DV){
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
#if 0 /* What's this for? (it->dvptr? overwritten by the following uiomove)*/
		fp = (struct fw_pkt *)(it->dvproc->buf + it->queued * it->psize);
		fp->mode.stream.len = htons(uio->uio_resid - sizeof(u_int32_t));
#endif
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
#endif
	if(xferq != NULL){
		xfer = fw_xfer_alloc(M_FWXFER);
		if(xfer == NULL){
			err = ENOMEM;
			return err;
		}
		xfer->send.buf = malloc(uio->uio_resid, M_FW, M_NOWAIT);
		if(xfer->send.buf == NULL){
			fw_xfer_free( xfer);
			err = ENOBUFS;
			return err;
		}
		xfer->dst = ntohs(fp->mode.hdr.dst);
#if 0	
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
		xfer->tcode = fp->mode.common.tcode;
		xfer->fc = fc; 
		xfer->q = xferq;
		xfer->act_type = FWACT_XFER;
		xfer->retry_req = fw_asybusy;
#endif
		xfer->send.len = uio->uio_resid; 
		xfer->send.off = 0; 
		xfer->spd = 0;/* XXX: how to setup it */
		xfer->act.hand = fw_asy_callback;
			
		err = uiomove(xfer->send.buf, uio->uio_resid, uio);
		if(err){
			fw_xfer_free( xfer);
			return err;
		}
#if 0
		fw_asystart(xfer);
#else
		fw_asyreq(fc, -1, xfer);
#endif
		err = tsleep((caddr_t)xfer, FWPRI, "fw_write", hz);
		if(xfer->resp == EBUSY)
			return EBUSY;
		fw_xfer_free( xfer);
		return err;
	}
	return EINVAL;
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
	struct fw_devinfo *devinfo;

	struct fw_devlstreq *fwdevlst = (struct fw_devlstreq *)data;
	struct fw_asyreq *asyreq = (struct fw_asyreq *)data;
	struct fw_isochreq *ichreq = (struct fw_isochreq *)data;
	struct fw_isobufreq *ibufreq = (struct fw_isobufreq *)data;
	struct fw_asybindreq *bindreq = (struct fw_asybindreq *)data;
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
#ifdef FWXFERQ_DV
	case FW_SSTDV:
		ibufreq = (struct fw_isobufreq *)
			malloc(sizeof(struct fw_isobufreq), M_FW, M_NOWAIT);
		if(ibufreq == NULL){
			err = ENOMEM;
			break;
		}
#if DV_PAL
#define FWDVPACKET 300
#else
#define FWDVPACKET 250
#endif
#define FWDVPMAX 512
		ibufreq->rx.nchunk = 8;
		ibufreq->rx.npacket = 50;
		ibufreq->rx.psize = FWDVPMAX;

		ibufreq->tx.nchunk = 5;
		ibufreq->tx.npacket = FWDVPACKET + 30;	/* > 320 or 267 */
		ibufreq->tx.psize = FWDVPMAX;

		err = fw_ioctl(dev, FW_SSTBUF, (caddr_t)ibufreq, flag, td);
		sc->fc->it[sub]->dvpacket = FWDVPACKET;
		free(ibufreq, M_FW);
/* reserve a buffer space */
#define NDVCHUNK 8
		sc->fc->it[sub]->dvproc = NULL;
		sc->fc->it[sub]->dvdma = NULL;
		sc->fc->it[sub]->flag |= FWXFERQ_DV;
		/* XXX check malloc failure */
		sc->fc->it[sub]->dvbuf
			= (struct fw_dvbuf *)malloc(sizeof(struct fw_dvbuf) * NDVCHUNK, M_FW, M_NOWAIT);
		STAILQ_INIT(&sc->fc->it[sub]->dvvalid);
		STAILQ_INIT(&sc->fc->it[sub]->dvfree);
		for( i = 0 ; i < NDVCHUNK ; i++){
			/* XXX check malloc failure */
			sc->fc->it[sub]->dvbuf[i].buf
				= malloc(FWDVPMAX * sc->fc->it[sub]->dvpacket, M_FW, M_NOWAIT);
			STAILQ_INSERT_TAIL(&sc->fc->it[sub]->dvfree,
					&sc->fc->it[sub]->dvbuf[i], link);
		}
		break;
#endif
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
			= (struct fw_bulkxfer *)malloc(sizeof(struct fw_bulkxfer) * ibufreq->rx.nchunk, M_FW, 0);
		if(ir->bulkxfer == NULL){
			return(ENOMEM);
		}
		it->bulkxfer
			= (struct fw_bulkxfer *)malloc(sizeof(struct fw_bulkxfer) * ibufreq->tx.nchunk, M_FW, 0);
		if(it->bulkxfer == NULL){
			return(ENOMEM);
		}
		ir->buf = malloc(
			ibufreq->rx.nchunk * ibufreq->rx.npacket
			/* XXX psize must be 2^n and less or
						equal to PAGE_SIZE */
			* ((ibufreq->rx.psize + 3) &~3),
			M_FW, 0);
		if(ir->buf == NULL){
			free(ir->bulkxfer, M_FW);
			free(it->bulkxfer, M_FW);
			ir->bulkxfer = NULL;
			it->bulkxfer = NULL;
			it->buf = NULL;
			return(ENOMEM);
		}
		it->buf = malloc(
			ibufreq->tx.nchunk * ibufreq->tx.npacket
			/* XXX psize must be 2^n and less or
						equal to PAGE_SIZE */
			* ((ibufreq->tx.psize + 3) &~3),
			M_FW, 0);
		if(it->buf == NULL){
			free(ir->bulkxfer, M_FW);
			free(it->bulkxfer, M_FW);
			free(ir->buf, M_FW);
			ir->bulkxfer = NULL;
			it->bulkxfer = NULL;
			it->buf = NULL;
			return(ENOMEM);
		}

		ir->bnchunk = ibufreq->rx.nchunk;
		ir->bnpacket = ibufreq->rx.npacket;
		ir->psize = (ibufreq->rx.psize + 3) & ~3;
		ir->queued = 0;

		it->bnchunk = ibufreq->tx.nchunk;
		it->bnpacket = ibufreq->tx.npacket;
		it->psize = (ibufreq->tx.psize + 3) & ~3;
		it->queued = 0;

#ifdef FWXFERQ_DV
		it->dvdbc = 0;
		it->dvdiff = 0;
		it->dvsync = 0;
		it->dvoffset = 0;
#endif

		STAILQ_INIT(&ir->stvalid);
		STAILQ_INIT(&ir->stfree);
		STAILQ_INIT(&ir->stdma);
		ir->stproc = NULL;

		STAILQ_INIT(&it->stvalid);
		STAILQ_INIT(&it->stfree);
		STAILQ_INIT(&it->stdma);
		it->stproc = NULL;

		for(i = 0 ; i < sc->fc->ir[sub]->bnchunk; i++){
			ir->bulkxfer[i].buf =
				ir->buf +
				i * sc->fc->ir[sub]->bnpacket *
			  	sc->fc->ir[sub]->psize;
			STAILQ_INSERT_TAIL(&ir->stfree,
					&ir->bulkxfer[i], link);
			ir->bulkxfer[i].npacket = ir->bnpacket;
		}
		for(i = 0 ; i < sc->fc->it[sub]->bnchunk; i++){
			it->bulkxfer[i].buf =
				it->buf +
				i * sc->fc->it[sub]->bnpacket *
			  	sc->fc->it[sub]->psize;
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
		xfer = fw_xfer_alloc(M_FWXFER);
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
			fwdev = fw_noderesolve_eui64(sc->fc,
						asyreq->req.dst.eui);
			if (fwdev == NULL) {
				device_printf(sc->fc->bdev,
					"cannot find node\n");
				err = EINVAL;
				goto error;
			}
			xfer->dst = fwdev->dst;
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
		xfer->send.buf = malloc(xfer->send.len, M_FW, M_NOWAIT);
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
		free(fwb, M_FW);
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
		fwb = (struct fw_bind *)malloc(sizeof (struct fw_bind), M_FW, M_NOWAIT);
		if(fwb == NULL){
			err = ENOMEM;
			break;
		}
		fwb->start_hi = bindreq->start.hi;
		fwb->start_lo = bindreq->start.lo;
		fwb->addrlen = bindreq->len;

		xfer = fw_xfer_alloc(M_FWXFER);
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
		i = len = 1;
		/* myself */
		devinfo = &fwdevlst->dev[0];
		devinfo->dst = sc->fc->nodeid;
		devinfo->status = 0;	/* XXX */
		devinfo->eui.hi = sc->fc->eui.hi;
		devinfo->eui.lo = sc->fc->eui.lo;
		STAILQ_FOREACH(fwdev, &sc->fc->devices, link) {
			if(len < FW_MAX_DEVLST){
				devinfo = &fwdevlst->dev[len++];
				devinfo->dst = fwdev->dst;
				devinfo->status = 
					(fwdev->status == FWDEVINVAL)?0:1;
				devinfo->eui.hi = fwdev->eui.hi;
				devinfo->eui.lo = fwdev->eui.lo;
			}
			i++;
		}
		fwdevlst->n = i;
		fwdevlst->info_len = len;
		break;
	case FW_GTPMAP:
		bcopy(sc->fc->topology_map, data,
				(sc->fc->topology_map->crc_len + 1) * 4);
		break;
	case FW_GCROM:
		STAILQ_FOREACH(fwdev, &sc->fc->devices, link)
			if (FW_EUI64_EQUAL(fwdev->eui, crom_buf->eui))
				break;
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
