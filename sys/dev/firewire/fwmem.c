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
 * $FreeBSD$
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/types.h>

#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/conf.h>
#include <sys/sysctl.h>

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

	xfer = fw_xfer_alloc_buf(M_FWXFER, slen, rlen);
	if (xfer == NULL)
		return NULL;

	xfer->fc = fwdev->fc;
	xfer->dst = FWLOCALBUS | fwdev->dst;
	if (spd < 0)
		xfer->spd = fwdev->speed;
	else
		xfer->spd = min(spd, fwdev->speed);
	xfer->act.hand = hand;
	xfer->retry_req = fw_asybusy;
	xfer->sc = sc;

	return xfer;
}

struct fw_xfer *
fwmem_read_quad(
	struct fw_device *fwdev,
	caddr_t	sc,
	u_int8_t spd,
	u_int16_t dst_hi,
	u_int32_t dst_lo,
	void (*hand)(struct fw_xfer *))
{
	struct fw_xfer *xfer;
	struct fw_pkt *fp;

	xfer = fwmem_xfer_req(fwdev, sc, spd, 12, 16, hand);
	if (xfer == NULL)
		return NULL;

	fp = (struct fw_pkt *)xfer->send.buf;
	fp->mode.rreqq.tcode = FWTCODE_RREQQ;
	fp->mode.rreqq.dst = xfer->dst;
	fp->mode.rreqq.dest_hi = dst_hi;
	fp->mode.rreqq.dest_lo = dst_lo;

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
	u_int32_t data,
	void (*hand)(struct fw_xfer *))
{
	struct fw_xfer *xfer;
	struct fw_pkt *fp;

	xfer = fwmem_xfer_req(fwdev, sc, spd, 16, 12, hand);
	if (xfer == NULL)
		return NULL;

	fp = (struct fw_pkt *)xfer->send.buf;
	fp->mode.wreqq.tcode = FWTCODE_WREQQ;
	fp->mode.wreqq.dst = xfer->dst;
	fp->mode.wreqq.dest_hi = dst_hi;
	fp->mode.wreqq.dest_lo = dst_lo;

	fp->mode.wreqq.data = data;

	if (fwmem_debug)
		printf("fwmem_write_quad: %d %04x:%08x %08x\n", fwdev->dst,
			dst_hi, dst_lo, data);

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
	void (*hand)(struct fw_xfer *))
{
	struct fw_xfer *xfer;
	struct fw_pkt *fp;

	xfer = fwmem_xfer_req(fwdev, sc, spd, 16, roundup2(16+len,4), hand);
	if (xfer == NULL)
		return NULL;

	fp = (struct fw_pkt *)xfer->send.buf;
	fp->mode.rreqb.tcode = FWTCODE_RREQB;
	fp->mode.rreqb.dst = xfer->dst;
	fp->mode.rreqb.dest_hi = dst_hi;
	fp->mode.rreqb.dest_lo = dst_lo;
	fp->mode.rreqb.len = len;

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
	char *data,
	void (*hand)(struct fw_xfer *))
{
	struct fw_xfer *xfer;
	struct fw_pkt *fp;

	xfer = fwmem_xfer_req(fwdev, sc, spd, roundup(16+len, 4), 12, hand);
	if (xfer == NULL)
		return NULL;

	fp = (struct fw_pkt *)xfer->send.buf;
	fp->mode.wreqb.tcode = FWTCODE_WREQB;
	fp->mode.wreqb.dst = xfer->dst;
	fp->mode.wreqb.dest_hi = dst_hi;
	fp->mode.wreqb.dest_lo = dst_lo;
	fp->mode.wreqb.len = len;
	bcopy(data, &fp->mode.wreqb.payload[0], len);

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

	return (0);
}

int
fwmem_close (dev_t dev, int flags, int fmt, fw_proc *td)
{
	free(dev->si_drv1, M_FW);
	dev->si_drv1 = NULL;

	return (0);
}

#define MAXLEN 2048
#define USE_QUAD 0
int
fwmem_read (dev_t dev, struct uio *uio, int ioflag)
{
	struct firewire_softc *sc;
	struct fw_device *fwdev;
	struct fw_xfer *xfer;
	int err = 0;
        int unit = DEV2UNIT(dev);
	u_int16_t dst_hi;
	u_int32_t dst_lo;
	off_t offset;
	int len, s;

	sc = devclass_get_softc(firewire_devclass, unit);
	fwdev = fw_noderesolve_eui64(sc->fc, (struct fw_eui64 *)dev->si_drv1);
	if (fwdev == NULL) {
		if (fwmem_debug)
			printf("fwmem: no such device ID:%08x%08x\n",
					fwmem_eui64.hi, fwmem_eui64.lo);
		return EINVAL;
	}

	while(uio->uio_resid > 0 && !err) {
		offset = uio->uio_offset;
		dst_hi = (offset >> 32) & 0xffff;
		dst_lo = offset & 0xffffffff;
		len = uio->uio_resid;
		if (len == 4 && (dst_lo & 3) == 0) {
			s = splfw();
			xfer = fwmem_read_quad(fwdev, NULL, fwmem_speed,
				dst_hi, dst_lo, fw_asy_callback);
			if (xfer == NULL) {
				err = EINVAL;
				splx(s);
				break;
			}
			err = tsleep((caddr_t)xfer, FWPRI, "fwmrq", 0);
			splx(s);
			if (xfer->recv.buf == NULL)
				err = EIO;
			else if (xfer->resp != 0)
				err = xfer->resp;
			else if (err == 0)
				err = uiomove(xfer->recv.buf + 4*3, 4, uio);
		} else {
			if (len > MAXLEN)
				len = MAXLEN;
			s = splfw();
			xfer = fwmem_read_block(fwdev, NULL, fwmem_speed,
				dst_hi, dst_lo, len, fw_asy_callback);
			if (xfer == NULL) {
				err = EINVAL;
				splx(s);
				break;
			}
			err = tsleep((caddr_t)xfer, FWPRI, "fwmrb", 0);
			splx(s);
			if (xfer->recv.buf == NULL)
				err = EIO;
			else if (xfer->resp != 0)
				err = xfer->resp;
			else if (err == 0)
				err = uiomove(xfer->recv.buf + 4*4, len, uio);
		}
		fw_xfer_free(xfer);
	}
	return err;
}
int
fwmem_write (dev_t dev, struct uio *uio, int ioflag)
{
	struct firewire_softc *sc;
	struct fw_device *fwdev;
	struct fw_xfer *xfer;
	int err = 0;
        int unit = DEV2UNIT(dev);
	u_int16_t dst_hi;
	u_int32_t dst_lo, quad;
	char *data;
	off_t offset;
	int len, s;

	sc = devclass_get_softc(firewire_devclass, unit);
	fwdev = fw_noderesolve_eui64(sc->fc, (struct fw_eui64 *)dev->si_drv1);
	if (fwdev == NULL) {
		if (fwmem_debug)
			printf("fwmem: no such device ID:%08x%08x\n",
					fwmem_eui64.hi, fwmem_eui64.lo);
		return EINVAL;
	}

	data = malloc(MAXLEN, M_FW, M_WAITOK);
	if (data == NULL)
		return ENOMEM;

	while(uio->uio_resid > 0 && !err) {
		offset = uio->uio_offset;
		dst_hi = (offset >> 32) & 0xffff;
		dst_lo = offset & 0xffffffff;
		len = uio->uio_resid;
		if (len == 4 && (dst_lo & 3) == 0) {
			err = uiomove((char *)&quad, sizeof(quad), uio);
			s = splfw();
			xfer = fwmem_write_quad(fwdev, NULL, fwmem_speed,
				dst_hi, dst_lo, quad, fw_asy_callback);
			if (xfer == NULL) {
				err = EINVAL;
				splx(s);
				break;
			}
			err = tsleep((caddr_t)xfer, FWPRI, "fwmwq", 0);
			splx(s);
			if (xfer->resp != 0)
				err = xfer->resp;
		} else {
			if (len > MAXLEN)
				len = MAXLEN;
			err = uiomove(data, len, uio);
			if (err)
				break;
			s = splfw();
			xfer = fwmem_write_block(fwdev, NULL, fwmem_speed,
				dst_hi, dst_lo, len, data, fw_asy_callback);
			if (xfer == NULL) {
				err = EINVAL;
				splx(s);
				break;
			}
			err = tsleep((caddr_t)xfer, FWPRI, "fwmwb", 0);
			splx(s);
			if (xfer->resp != 0)
				err = xfer->resp;
		}
		fw_xfer_free(xfer);
	}
	free(data, M_FW);
	return err;
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
