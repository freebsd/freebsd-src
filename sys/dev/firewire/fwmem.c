/*
 * Copyright (c) 2002-2003
 * 	Hidetoshi Shimokawa. All rights reserved.
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
 *
 *	This product includes software developed by Hidetoshi Shimokawa.
 *
 * 4. Neither the name of the author nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 * 
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/types.h>

#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/conf.h>
#include <sys/sysctl.h>
#if __FreeBSD_version < 500000
#include <sys/buf.h>
#else
#include <sys/bio.h>
#endif

#include <sys/bus.h>
#include <machine/bus.h>

#include <sys/signal.h>
#include <sys/mman.h>
#include <sys/ioccom.h>

#include <dev/firewire/firewire.h>
#include <dev/firewire/firewirereg.h>
#include <dev/firewire/fwmem.h>

static int fwmem_speed=2, fwmem_debug=0;
static struct fw_eui64 fwmem_eui64;
SYSCTL_DECL(_hw_firewire);
SYSCTL_NODE(_hw_firewire, OID_AUTO, fwmem, CTLFLAG_RD, 0,
	"FireWire Memory Access");
SYSCTL_UINT(_hw_firewire_fwmem, OID_AUTO, eui64_hi, CTLFLAG_RW,
	&fwmem_eui64.hi, 0, "Fwmem target EUI64 high");
SYSCTL_UINT(_hw_firewire_fwmem, OID_AUTO, eui64_lo, CTLFLAG_RW,
	&fwmem_eui64.lo, 0, "Fwmem target EUI64 low");
SYSCTL_INT(_hw_firewire_fwmem, OID_AUTO, speed, CTLFLAG_RW, &fwmem_speed, 0,
	"Fwmem link speed");
SYSCTL_INT(_debug, OID_AUTO, fwmem_debug, CTLFLAG_RW, &fwmem_debug, 0,
	"Fwmem driver debug flag");

#define MAXLEN (512 << fwmem_speed)

static struct fw_xfer *
fwmem_xfer_req(
	struct fw_device *fwdev,
	caddr_t sc,
	int spd,
	int slen,
	int rlen,
	void *hand)
{
	struct fw_xfer *xfer;

	xfer = fw_xfer_alloc(M_FWXFER);
	if (xfer == NULL)
		return NULL;

	xfer->fc = fwdev->fc;
	xfer->send.hdr.mode.hdr.dst = FWLOCALBUS | fwdev->dst;
	if (spd < 0)
		xfer->send.spd = fwdev->speed;
	else
		xfer->send.spd = min(spd, fwdev->speed);
	xfer->act.hand = hand;
	xfer->retry_req = fw_asybusy;
	xfer->sc = sc;
	xfer->send.pay_len = slen;
	xfer->recv.pay_len = rlen;

	return xfer;
}

struct fw_xfer *
fwmem_read_quad(
	struct fw_device *fwdev,
	caddr_t	sc,
	u_int8_t spd,
	u_int16_t dst_hi,
	u_int32_t dst_lo,
	void *data,
	void (*hand)(struct fw_xfer *))
{
	struct fw_xfer *xfer;
	struct fw_pkt *fp;

	xfer = fwmem_xfer_req(fwdev, (void *)sc, spd, 0, 4, hand);
	if (xfer == NULL) {
		return NULL;
	}

	fp = &xfer->send.hdr;
	fp->mode.rreqq.tcode = FWTCODE_RREQQ;
	fp->mode.rreqq.dest_hi = dst_hi;
	fp->mode.rreqq.dest_lo = dst_lo;

	xfer->send.payload = NULL;
	xfer->recv.payload = (u_int32_t *)data;

	if (fwmem_debug)
		printf("fwmem_read_quad: %d %04x:%08x\n", fwdev->dst,
				dst_hi, dst_lo);

	if (fw_asyreq(xfer->fc, -1, xfer) == 0)
		return xfer;

	fw_xfer_free(xfer);
	return NULL;
}

struct fw_xfer *
fwmem_write_quad(
	struct fw_device *fwdev,
	caddr_t	sc,
	u_int8_t spd,
	u_int16_t dst_hi,
	u_int32_t dst_lo,
	void *data,
	void (*hand)(struct fw_xfer *))
{
	struct fw_xfer *xfer;
	struct fw_pkt *fp;

	xfer = fwmem_xfer_req(fwdev, sc, spd, 0, 0, hand);
	if (xfer == NULL)
		return NULL;

	fp = &xfer->send.hdr;
	fp->mode.wreqq.tcode = FWTCODE_WREQQ;
	fp->mode.wreqq.dest_hi = dst_hi;
	fp->mode.wreqq.dest_lo = dst_lo;
	fp->mode.wreqq.data = *(u_int32_t *)data;

	xfer->send.payload = xfer->recv.payload = NULL;

	if (fwmem_debug)
		printf("fwmem_write_quad: %d %04x:%08x %08x\n", fwdev->dst,
			dst_hi, dst_lo, *(u_int32_t *)data);

	if (fw_asyreq(xfer->fc, -1, xfer) == 0)
		return xfer;

	fw_xfer_free(xfer);
	return NULL;
}

struct fw_xfer *
fwmem_read_block(
	struct fw_device *fwdev,
	caddr_t	sc,
	u_int8_t spd,
	u_int16_t dst_hi,
	u_int32_t dst_lo,
	int len,
	void *data,
	void (*hand)(struct fw_xfer *))
{
	struct fw_xfer *xfer;
	struct fw_pkt *fp;
	
	xfer = fwmem_xfer_req(fwdev, sc, spd, 0, roundup2(len, 4), hand);
	if (xfer == NULL)
		return NULL;

	fp = &xfer->send.hdr;
	fp->mode.rreqb.tcode = FWTCODE_RREQB;
	fp->mode.rreqb.dest_hi = dst_hi;
	fp->mode.rreqb.dest_lo = dst_lo;
	fp->mode.rreqb.len = len;
	fp->mode.rreqb.extcode = 0;

	xfer->send.payload = NULL;
	xfer->recv.payload = data;

	if (fwmem_debug)
		printf("fwmem_read_block: %d %04x:%08x %d\n", fwdev->dst,
				dst_hi, dst_lo, len);
	if (fw_asyreq(xfer->fc, -1, xfer) == 0)
		return xfer;

	fw_xfer_free(xfer);
	return NULL;
}

struct fw_xfer *
fwmem_write_block(
	struct fw_device *fwdev,
	caddr_t	sc,
	u_int8_t spd,
	u_int16_t dst_hi,
	u_int32_t dst_lo,
	int len,
	void *data,
	void (*hand)(struct fw_xfer *))
{
	struct fw_xfer *xfer;
	struct fw_pkt *fp;

	xfer = fwmem_xfer_req(fwdev, sc, spd, len, 0, hand);
	if (xfer == NULL)
		return NULL;

	fp = &xfer->send.hdr;
	fp->mode.wreqb.tcode = FWTCODE_WREQB;
	fp->mode.wreqb.dest_hi = dst_hi;
	fp->mode.wreqb.dest_lo = dst_lo;
	fp->mode.wreqb.len = len;
	fp->mode.wreqb.extcode = 0;

	xfer->send.payload = data;
	xfer->recv.payload = NULL;

	if (fwmem_debug)
		printf("fwmem_write_block: %d %04x:%08x %d\n", fwdev->dst,
				dst_hi, dst_lo, len);
	if (fw_asyreq(xfer->fc, -1, xfer) == 0)
		return xfer;

	fw_xfer_free(xfer);
	return NULL;
}


int
fwmem_open (dev_t dev, int flags, int fmt, fw_proc *td)
{
	struct fw_eui64 *eui;

	if (dev->si_drv1 != NULL)
		return (EBUSY);

	eui = (struct fw_eui64 *)malloc(sizeof(struct fw_eui64),
							M_FW, M_WAITOK);
	if (eui == NULL)
		return ENOMEM;
	bcopy(&fwmem_eui64, eui, sizeof(struct fw_eui64));
	dev->si_drv1 = (void *)eui;
	dev->si_iosize_max = DFLTPHYS;

	return (0);
}

int
fwmem_close (dev_t dev, int flags, int fmt, fw_proc *td)
{
	free(dev->si_drv1, M_FW);
	dev->si_drv1 = NULL;

	return (0);
}


static void
fwmem_biodone(struct fw_xfer *xfer)
{
	struct bio *bp;

	bp = (struct bio *)xfer->sc;
	bp->bio_error = xfer->resp;

	if (bp->bio_error != 0) {
		if (fwmem_debug)
			printf("%s: err=%d\n", __FUNCTION__, bp->bio_error);
		bp->bio_flags |= BIO_ERROR;
		bp->bio_resid = bp->bio_bcount;
	}

	fw_xfer_free(xfer);
	biodone(bp);
}

void
fwmem_strategy(struct bio *bp)
{
	struct firewire_softc *sc;
	struct fw_device *fwdev;
	struct fw_xfer *xfer;
	dev_t dev;
	int unit, err=0, s, iolen;

	dev = bp->bio_dev;
	/* XXX check request length */

        unit = DEV2UNIT(dev);
	sc = devclass_get_softc(firewire_devclass, unit);

	s = splfw();
	fwdev = fw_noderesolve_eui64(sc->fc, (struct fw_eui64 *)dev->si_drv1);
	if (fwdev == NULL) {
		if (fwmem_debug)
			printf("fwmem: no such device ID:%08x%08x\n",
					fwmem_eui64.hi, fwmem_eui64.lo);
		err = EINVAL;
		goto error;
	}

	iolen = MIN(bp->bio_bcount, MAXLEN);
	if ((bp->bio_cmd & BIO_READ) == BIO_READ) {
		if (iolen == 4 && (bp->bio_offset & 3) == 0)
			xfer = fwmem_read_quad(fwdev,
			    (void *) bp, fwmem_speed,
			    bp->bio_offset >> 32, bp->bio_offset & 0xffffffff,
			    bp->bio_data, fwmem_biodone);
		else
			xfer = fwmem_read_block(fwdev,
			    (void *) bp, fwmem_speed,
			    bp->bio_offset >> 32, bp->bio_offset & 0xffffffff,
			    iolen, bp->bio_data, fwmem_biodone);
	} else {
		if (iolen == 4 && (bp->bio_offset & 3) == 0)
			xfer = fwmem_write_quad(fwdev,
			    (void *)bp, fwmem_speed,
			    bp->bio_offset >> 32, bp->bio_offset & 0xffffffff,
			    bp->bio_data, fwmem_biodone);
		else
			xfer = fwmem_write_block(fwdev,
			    (void *)bp, fwmem_speed,
			    bp->bio_offset >> 32, bp->bio_offset & 0xffffffff,
			    iolen, bp->bio_data, fwmem_biodone);
	}
	if (xfer == NULL) {
		err = EIO;
		goto error;
	}
	/* XXX */
	bp->bio_resid = bp->bio_bcount - iolen;
error:
	splx(s);
	if (err != 0) {
		if (fwmem_debug)
			printf("%s: err=%d\n", __FUNCTION__, err);
		bp->bio_error = err;
		bp->bio_flags |= BIO_ERROR;
		bp->bio_resid = bp->bio_bcount;
		biodone(bp);
	}
}

int
fwmem_ioctl (dev_t dev, u_long cmd, caddr_t data, int flag, fw_proc *td)
{
	int err = 0;
	switch (cmd) {
	case FW_SDEUI64:
		bcopy(data, dev->si_drv1, sizeof(struct fw_eui64));
		break;
	case FW_GDEUI64:
		bcopy(dev->si_drv1, data, sizeof(struct fw_eui64));
		break;
	default:
		err = EINVAL;
	}
	return(err);
}
int
fwmem_poll (dev_t dev, int events, fw_proc *td)
{  
	return EINVAL;
}
int
#if __FreeBSD_version < 500102
fwmem_mmap (dev_t dev, vm_offset_t offset, int nproto)
#else
fwmem_mmap (dev_t dev, vm_offset_t offset, vm_paddr_t *paddr, int nproto)
#endif
{  
	return EINVAL;
}
