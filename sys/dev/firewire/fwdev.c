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
#include <sys/mbuf.h>

#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/conf.h>
#include <sys/poll.h>

#include <sys/bus.h>
#include <machine/bus.h>

#include <sys/ioccom.h>

#include <dev/firewire/firewire.h>
#include <dev/firewire/firewirereg.h>
#include <dev/firewire/fwdma.h>
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
#if __FreeBSD_version >= 500104
	.d_open =	fw_open,
	.d_close =	fw_close,
	.d_read =	fw_read,
	.d_write =	fw_write,
	.d_ioctl =	fw_ioctl,
	.d_poll =	fw_poll,
	.d_mmap =	fw_mmap,
	.d_name =	"fw",
	.d_maj =	CDEV_MAJOR,
	.d_flags =	D_MEM
#else
	fw_open, fw_close, fw_read, fw_write, fw_ioctl,
	fw_poll, fw_mmap, nostrategy, "fw", CDEV_MAJOR, nodump, nopsize, D_MEM
#endif
};

struct fw_drv1 {
	struct fw_xferq *ir;
	struct fw_xferq *it;
	struct fw_isobufreq bufreq;
};

static int
fwdev_allocbuf(struct firewire_comm *fc, struct fw_xferq *q,
	struct fw_bufspec *b)
{
	int i;

	if (q->flag & (FWXFERQ_RUNNING | FWXFERQ_EXTBUF))
		return(EBUSY);

	q->bulkxfer = (struct fw_bulkxfer *) malloc(
		sizeof(struct fw_bulkxfer) * b->nchunk,
		M_FW, M_WAITOK);
	if (q->bulkxfer == NULL)
		return(ENOMEM);

	b->psize = roundup2(b->psize, sizeof(u_int32_t));
	q->buf = fwdma_malloc_multiseg(fc, sizeof(u_int32_t),
			b->psize, b->nchunk * b->npacket, BUS_DMA_WAITOK);

	if (q->buf == NULL) {
		free(q->bulkxfer, M_FW);
		q->bulkxfer = NULL;
		return(ENOMEM);
	}
	q->bnchunk = b->nchunk;
	q->bnpacket = b->npacket;
	q->psize = (b->psize + 3) & ~3;
	q->queued = 0;

	STAILQ_INIT(&q->stvalid);
	STAILQ_INIT(&q->stfree);
	STAILQ_INIT(&q->stdma);
	q->stproc = NULL;

	for(i = 0 ; i < q->bnchunk; i++){
		q->bulkxfer[i].poffset = i * q->bnpacket;
		q->bulkxfer[i].mbuf = NULL;
		STAILQ_INSERT_TAIL(&q->stfree, &q->bulkxfer[i], link);
	}

	q->flag &= ~FWXFERQ_MODEMASK;
	q->flag |= FWXFERQ_STREAM;
	q->flag |= FWXFERQ_EXTBUF;

	return (0);
}

static int
fwdev_freebuf(struct fw_xferq *q)
{
	if (q->flag & FWXFERQ_EXTBUF) {
		if (q->buf != NULL)
			fwdma_free_multiseg(q->buf);
		q->buf = NULL;
		free(q->bulkxfer, M_FW);
		q->bulkxfer = NULL;
		q->flag &= ~FWXFERQ_EXTBUF;
		q->psize = 0;
		q->maxq = FWMAXQUEUE;
	}
	return (0);
}


static int
fw_open (dev_t dev, int flags, int fmt, fw_proc *td)
{
	int unit = DEV2UNIT(dev);
	int sub = DEV2SUB(dev);

	int err = 0;

	if (dev->si_drv1 != NULL)
		return (EBUSY);

	if (DEV_FWMEM(dev))
		return fwmem_open(dev, flags, fmt, td);

#if __FreeBSD_version >= 500000
	if ((dev->si_flags & SI_NAMED) == 0)
#endif
		make_dev(&firewire_cdevsw, minor(dev),
			UID_ROOT, GID_OPERATOR, 0660,
			"fw%d.%d", unit, sub);

	dev->si_drv1 = malloc(sizeof(struct fw_drv1), M_FW, M_WAITOK | M_ZERO);

	return err;
}

static int
fw_close (dev_t dev, int flags, int fmt, fw_proc *td)
{
	struct firewire_softc *sc;
	struct firewire_comm *fc;
	struct fw_drv1 *d;
	int unit = DEV2UNIT(dev);
	struct fw_xfer *xfer;
	struct fw_bind *fwb;
	int err = 0;

	if (DEV_FWMEM(dev))
		return fwmem_close(dev, flags, fmt, td);

	sc = devclass_get_softc(firewire_devclass, unit);
	fc = sc->fc;
	d = (struct fw_drv1 *)dev->si_drv1;

	if (d->ir != NULL) {
		struct fw_xferq *ir = d->ir;

		if ((ir->flag & FWXFERQ_OPEN) == 0)
			return (EINVAL);
		if (ir->flag & FWXFERQ_RUNNING) {
			ir->flag &= ~FWXFERQ_RUNNING;
			fc->irx_disable(fc, ir->dmach);
		}
		/* free extbuf */
		fwdev_freebuf(ir);
		/* drain receiving buffer */
		for (xfer = STAILQ_FIRST(&ir->q);
			xfer != NULL; xfer = STAILQ_FIRST(&ir->q)) {
			ir->queued --;
			STAILQ_REMOVE_HEAD(&ir->q, link);

			xfer->resp = 0;
			fw_xfer_done(xfer);
		}
		/* remove binding */
		for (fwb = STAILQ_FIRST(&ir->binds); fwb != NULL;
				fwb = STAILQ_FIRST(&ir->binds)) {
			STAILQ_REMOVE(&fc->binds, fwb, fw_bind, fclist);
			STAILQ_REMOVE_HEAD(&ir->binds, chlist);
			free(fwb, M_FW);
		}
		ir->flag &= ~(FWXFERQ_OPEN |
			FWXFERQ_MODEMASK | FWXFERQ_CHTAGMASK);
		d->ir = NULL;

	}
	if (d->it != NULL) {
		struct fw_xferq *it = d->it;

		if ((it->flag & FWXFERQ_OPEN) == 0)
			return (EINVAL);
		if (it->flag & FWXFERQ_RUNNING) {
			it->flag &= ~FWXFERQ_RUNNING;
			fc->itx_disable(fc, it->dmach);
		}
		/* free extbuf */
		fwdev_freebuf(it);
		it->flag &= ~(FWXFERQ_OPEN |
			FWXFERQ_MODEMASK | FWXFERQ_CHTAGMASK);
		d->it = NULL;
	}
	free(dev->si_drv1, M_FW);
	dev->si_drv1 = NULL;

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
	struct fw_pkt *fp;

	if (DEV_FWMEM(dev))
		return fwmem_read(dev, uio, ioflag);

	sc = devclass_get_softc(firewire_devclass, unit);

	ir = ((struct fw_drv1 *)dev->si_drv1)->ir;
	if (ir == NULL || ir->buf == NULL)
		return (EIO);

readloop:
	xfer = STAILQ_FIRST(&ir->q);
	if (ir->stproc == NULL) {
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
			ir->flag |= FWXFERQ_WAKEUP;
			err = tsleep(ir, FWPRI, "fw_read", hz);
			ir->flag &= ~FWXFERQ_WAKEUP;
			if (err == 0)
				goto readloop;
		} else if (slept == 1)
			err = EIO;
		return err;
	} else if(xfer != NULL) {
		/* per packet mode or FWACT_CH bind?*/
		s = splfw();
		ir->queued --;
		STAILQ_REMOVE_HEAD(&ir->q, link);
		splx(s);
		fp = (struct fw_pkt *)xfer->recv.buf;
		if(sc->fc->irx_post != NULL)
			sc->fc->irx_post(sc->fc, fp->mode.ld);
		err = uiomove(xfer->recv.buf, xfer->recv.len, uio);
		/* XXX we should recycle this xfer */
		fw_xfer_free( xfer);
	} else if(ir->stproc != NULL) {
		/* iso bulkxfer */
		fp = (struct fw_pkt *)fwdma_v_addr(ir->buf, 
				ir->stproc->poffset + ir->queued);
		if(sc->fc->irx_post != NULL)
			sc->fc->irx_post(sc->fc, fp->mode.ld);
		if(fp->mode.stream.len == 0){
			err = EIO;
			return err;
		}
		err = uiomove((caddr_t)fp,
			fp->mode.stream.len + sizeof(u_int32_t), uio);
		ir->queued ++;
		if(ir->queued >= ir->bnpacket){
			s = splfw();
			STAILQ_INSERT_TAIL(&ir->stfree, ir->stproc, link);
			splx(s);
			sc->fc->irx_enable(sc->fc, ir->dmach);
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
	int s, slept = 0;
	struct fw_pkt *fp;
	struct firewire_comm *fc;
	struct fw_xferq *it;

	if (DEV_FWMEM(dev))
		return fwmem_write(dev, uio, ioflag);

	sc = devclass_get_softc(firewire_devclass, unit);
	fc = sc->fc;
	it = ((struct fw_drv1 *)dev->si_drv1)->it;
	if (it == NULL || it->buf == NULL)
		return (EIO);
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
			err = sc->fc->itx_enable(sc->fc, it->dmach);
			if (err)
				return err;
			err = tsleep(it, FWPRI, "fw_write", hz);
			if (err)
				return err;
			goto isoloop;
		} else {
			err = EIO;
			return err;
		}
	}
	fp = (struct fw_pkt *)fwdma_v_addr(it->buf,
			it->stproc->poffset + it->queued);
	err = uiomove((caddr_t)fp, sizeof(struct fw_isohdr), uio);
	err = uiomove((caddr_t)fp->mode.stream.payload,
				fp->mode.stream.len, uio);
	it->queued ++;
	if (it->queued >= it->bnpacket) {
		s = splfw();
		STAILQ_INSERT_TAIL(&it->stvalid, it->stproc, link);
		splx(s);
		it->stproc = NULL;
		err = sc->fc->itx_enable(sc->fc, it->dmach);
	}
	if (uio->uio_resid >= sizeof(struct fw_isohdr)) {
		slept = 0;
		goto isoloop;
	}
	return err;
}
/*
 * ioctl support.
 */
int
fw_ioctl (dev_t dev, u_long cmd, caddr_t data, int flag, fw_proc *td)
{
	struct firewire_softc *sc;
	struct firewire_comm *fc;
	struct fw_drv1 *d;
	int unit = DEV2UNIT(dev);
	int s, i, len, err = 0;
	struct fw_device *fwdev;
	struct fw_bind *fwb;
	struct fw_xferq *ir, *it;
	struct fw_xfer *xfer;
	struct fw_pkt *fp;
	struct fw_devinfo *devinfo;
	void *ptr;

	struct fw_devlstreq *fwdevlst = (struct fw_devlstreq *)data;
	struct fw_asyreq *asyreq = (struct fw_asyreq *)data;
	struct fw_isochreq *ichreq = (struct fw_isochreq *)data;
	struct fw_isobufreq *ibufreq = (struct fw_isobufreq *)data;
	struct fw_asybindreq *bindreq = (struct fw_asybindreq *)data;
	struct fw_crom_buf *crom_buf = (struct fw_crom_buf *)data;

	if (DEV_FWMEM(dev))
		return fwmem_ioctl(dev, cmd, data, flag, td);

	if (!data)
		return(EINVAL);

	sc = devclass_get_softc(firewire_devclass, unit);
	fc = sc->fc;
	d = (struct fw_drv1 *)dev->si_drv1;
	ir = d->ir;
	it = d->it;

	switch (cmd) {
	case FW_STSTREAM:
		if (it == NULL) {
			for (i = 0; i < fc->nisodma; i ++) {
				it = fc->it[i];
				if ((it->flag & FWXFERQ_OPEN) == 0)
					 break;
	                }	
			if (i >= fc->nisodma) {
				err = EBUSY;
				break;
			}
			err = fwdev_allocbuf(fc, it, &d->bufreq.tx);
			if (err)
				break;
			it->flag |=  FWXFERQ_OPEN;
		}
		it->flag &= ~0xff;
		it->flag |= (0x3f & ichreq->ch);
		it->flag |= ((0x3 & ichreq->tag) << 6);
		d->it = it;
		err = 0;
		break;
	case FW_GTSTREAM:
		if (it != NULL) {
			ichreq->ch = it->flag & 0x3f;
			ichreq->tag = it->flag >> 2 & 0x3;
			err = 0;
		} else
			err = EINVAL;
		break;
	case FW_SRSTREAM:
		if (ir == NULL) {
			for (i = 0; i < fc->nisodma; i ++) {
				ir = fc->ir[i];
				if ((ir->flag & FWXFERQ_OPEN) == 0)
					break;
			}	
			if (i >= fc->nisodma) {
				err = EBUSY;
				break;
			}
			err = fwdev_allocbuf(fc, ir, &d->bufreq.rx);
			if (err)
				break;
			ir->flag |=  FWXFERQ_OPEN;
		}
		ir->flag &= ~0xff;
		ir->flag |= (0x3f & ichreq->ch);
		ir->flag |= ((0x3 & ichreq->tag) << 6);
		d->ir = ir;
		err = fc->irx_enable(fc, ir->dmach);
		break;
	case FW_GRSTREAM:
		if (d->ir != NULL) {
			ichreq->ch = ir->flag & 0x3f;
			ichreq->tag = ir->flag >> 2 & 0x3;
			err = 0;
		} else
			err = EINVAL;
		break;
	case FW_SSTBUF:
		bcopy(ibufreq, &d->bufreq, sizeof(d->bufreq));
		err = 0;
		break;
	case FW_GSTBUF:
		bzero(&ibufreq->rx, sizeof(ibufreq->rx));
		if (ir != NULL) {
			ibufreq->rx.nchunk = ir->bnchunk;
			ibufreq->rx.npacket = ir->bnpacket;
			ibufreq->rx.psize = ir->psize;
		}
		bzero(&ibufreq->tx, sizeof(ibufreq->tx));
		if (it != NULL) {
			ibufreq->tx.nchunk = it->bnchunk;
			ibufreq->tx.npacket = it->bnpacket;
			ibufreq->tx.psize = it->psize;
		}
		break;
	case FW_ASYREQ:
		xfer = fw_xfer_alloc_buf(M_FWXFER, asyreq->req.len,
							PAGE_SIZE /* XXX */);
		if(xfer == NULL){
			err = ENOMEM;
			return err;
		}
		fp = &asyreq->pkt;
		switch (asyreq->req.type) {
		case FWASREQNODE:
			xfer->dst = fp->mode.hdr.dst;
			break;
		case FWASREQEUI:
			fwdev = fw_noderesolve_eui64(sc->fc,
						&asyreq->req.dst.eui);
			if (fwdev == NULL) {
				device_printf(sc->fc->bdev,
					"cannot find node\n");
				err = EINVAL;
				goto error;
			}
			xfer->dst = FWLOCALBUS | fwdev->dst;
			fp->mode.hdr.dst = xfer->dst;
			break;
		case FWASRESTL:
			/* XXX what's this? */
			break;
		case FWASREQSTREAM:
			/* nothing to do */
			break;
		}
		xfer->spd = asyreq->req.sped;
		bcopy(fp, xfer->send.buf, xfer->send.len);
		xfer->act.hand = fw_asy_callback;
		err = fw_asyreq(sc->fc, -1, xfer);
		if(err){
			fw_xfer_free( xfer);
			return err;
		}
		err = tsleep(xfer, FWPRI, "asyreq", hz);
		if(err == 0){
			if(asyreq->req.len >= xfer->recv.len){
				asyreq->req.len = xfer->recv.len;
			}else{
				err = EINVAL;
			}
			bcopy(xfer->recv.buf, fp, asyreq->req.len);
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
		STAILQ_REMOVE(&ir->binds, fwb, fw_bind, chlist);
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
		/* XXX */
		fwb->sub = ir->dmach;
		fwb->act_type = FWACT_CH;

		xfer = fw_xfer_alloc(M_FWXFER);
		if(xfer == NULL){
			err = ENOMEM;
			return err;
		}
		xfer->fc = sc->fc;

		s = splfw();
		/* XXX broken. need multiple xfer */
		STAILQ_INIT(&fwb->xferlist);
		STAILQ_INSERT_TAIL(&fwb->xferlist, xfer, link);
		splx(s);
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
			if (!FW_EUI64_EQUAL(sc->fc->eui, crom_buf->eui)) {
				err = FWNODE_INVAL;
				break;
			}
			/* myself */
			ptr = malloc(CROMSIZE, M_FW, M_WAITOK);
			len = CROMSIZE;
			for (i = 0; i < CROMSIZE/4; i++)
				((u_int32_t *)ptr)[i]
					= ntohl(sc->fc->config_rom[i]);
		} else {
			/* found */
			ptr = (void *)&fwdev->csrrom[0];
			if (fwdev->rommax < CSRROMOFF)
				len = 0;
			else
				len = fwdev->rommax - CSRROMOFF + 4;
		}
		if (crom_buf->len < len)
			len = crom_buf->len;
		else
			crom_buf->len = len;
		err = copyout(ptr, crom_buf->ptr, len);
		if (fwdev == NULL)
			/* myself */
			free(ptr, M_FW);
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
	struct firewire_softc *sc;
	struct fw_xferq *ir;
	int revents;
	int tmp;
	int unit = DEV2UNIT(dev);

	if (DEV_FWMEM(dev))
		return fwmem_poll(dev, events, td);

	sc = devclass_get_softc(firewire_devclass, unit);
	ir = ((struct fw_drv1 *)dev->si_drv1)->ir;
	revents = 0;
	tmp = POLLIN | POLLRDNORM;
	if (events & tmp) {
		if (STAILQ_FIRST(&ir->q) != NULL)
			revents |= tmp;
		else
			selrecord(td, &ir->rsel);
	}
	tmp = POLLOUT | POLLWRNORM;
	if (events & tmp) {
		/* XXX should be fixed */	
		revents |= tmp;
	}

	return revents;
}

static int
#if __FreeBSD_version < 500102
fw_mmap (dev_t dev, vm_offset_t offset, int nproto)
#else
fw_mmap (dev_t dev, vm_offset_t offset, vm_paddr_t *paddr, int nproto)
#endif
{  
	struct firewire_softc *fc;
	int unit = DEV2UNIT(dev);

	if (DEV_FWMEM(dev))
#if __FreeBSD_version < 500102
		return fwmem_mmap(dev, offset, nproto);
#else
		return fwmem_mmap(dev, offset, paddr, nproto);
#endif

	fc = devclass_get_softc(firewire_devclass, unit);

	return EINVAL;
}
