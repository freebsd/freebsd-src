/*
 * Copyright (C) 2002
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
#include <sys/uio.h>
#include <sys/sysctl.h>

#include <sys/bus.h>

#include <sys/signal.h>
#include <sys/mman.h>
#include <sys/ioccom.h>

#include <dev/firewire/firewire.h>
#include <dev/firewire/firewirereg.h>
#include <dev/firewire/fwmem.h>

static int fwmem_node=0, fwmem_speed=2, fwmem_debug=0;
SYSCTL_DECL(_hw_firewire);
SYSCTL_NODE(_hw_firewire, OID_AUTO, fwmem, CTLFLAG_RD, 0,
	"Firewire Memory Access");
SYSCTL_INT(_hw_firewire_fwmem, OID_AUTO, node, CTLFLAG_RW, &fwmem_node, 0,
	"Fwmem target node");
SYSCTL_INT(_hw_firewire_fwmem, OID_AUTO, speed, CTLFLAG_RW, &fwmem_speed, 0,
	"Fwmem link speed");
SYSCTL_INT(_debug, OID_AUTO, fwmem_debug, CTLFLAG_RW, &fwmem_debug, 0,
	"Fwmem driver debug flag");

struct fw_xfer *
fwmem_read_quad(
	struct firewire_comm *fc,
	u_int8_t spd,
	int dst,
	u_int16_t dst_hi,
	u_int32_t dst_lo,
	void (*hand)(struct fw_xfer *))
{
	struct fw_xfer *xfer;
	struct fw_pkt *fp;

	xfer = fw_xfer_alloc();
	if (xfer == NULL)
		return NULL;

	xfer->fc = fc;
	xfer->dst = FWLOCALBUS | dst;
	xfer->spd = spd;
	xfer->send.len = 12;
	xfer->send.buf = malloc(xfer->send.len, M_DEVBUF, M_NOWAIT | M_ZERO);

	if (xfer->send.buf == NULL)
		goto error;

	xfer->send.off = 0; 
	xfer->act.hand = hand;
	xfer->retry_req = fw_asybusy;
	xfer->sc = NULL;

	fp = (struct fw_pkt *)xfer->send.buf;
	fp->mode.rreqq.tcode = FWTCODE_RREQQ;
	fp->mode.rreqq.dst = htons(xfer->dst);
	fp->mode.rreqq.dest_hi = htons(dst_hi);
	fp->mode.rreqq.dest_lo = htonl(dst_lo);

	if (fwmem_debug)
		printf("fwmem: %d %04x:%08x\n", dst, dst_hi, dst_lo);

	if (fw_asyreq(fc, -1, xfer) == 0)
		return xfer;

error:
	fw_xfer_free(xfer);
	return NULL;
}

struct fw_xfer *
fwmem_read_block(
	struct firewire_comm *fc,
	u_int8_t spd,
	int dst,
	u_int16_t dst_hi,
	u_int32_t dst_lo,
	int len,
	void (*hand)(struct fw_xfer *))
{
	struct fw_xfer *xfer;
	struct fw_pkt *fp;

	xfer = fw_xfer_alloc();
	if (xfer == NULL)
		return NULL;

	xfer->fc = fc;
	xfer->dst = FWLOCALBUS | dst;
	xfer->spd = spd;
	xfer->send.len = 16;
	xfer->send.buf = malloc(xfer->send.len, M_DEVBUF, M_NOWAIT | M_ZERO);

	if (xfer->send.buf == NULL)
		goto error;

	xfer->send.off = 0; 
	xfer->act.hand = fw_asy_callback;
	xfer->retry_req = fw_asybusy;
	xfer->sc = NULL;

	fp = (struct fw_pkt *)xfer->send.buf;
	fp->mode.rreqb.tcode = FWTCODE_RREQB;
	fp->mode.rreqb.dst = htons(xfer->dst);
	fp->mode.rreqb.dest_hi = htons(dst_hi);
	fp->mode.rreqb.dest_lo = htonl(dst_lo);
	fp->mode.rreqb.len = htons(len);

	if (fwmem_debug)
		printf("fwmem: %d %04x:%08x %d\n", dst, dst_hi, dst_lo, len);
	if (fw_asyreq(fc, -1, xfer) == 0)
		return xfer;

error:
	fw_xfer_free(xfer);
	return NULL;
}

int
fwmem_open (dev_t dev, int flags, int fmt, fw_proc *td)
{
	int err = 0;
	return err;
}

int
fwmem_close (dev_t dev, int flags, int fmt, fw_proc *td)
{
	int err = 0;
	return err;
}

#define MAXLEN 2048
#define USE_QUAD 0
int
fwmem_read (dev_t dev, struct uio *uio, int ioflag)
{
	struct firewire_softc *sc;
	struct firewire_comm *fc;
	struct fw_xfer *xfer;
	int err = 0, pad;
        int unit = DEV2UNIT(dev);
	u_int16_t dst_hi;
	u_int32_t dst_lo;
	off_t offset;
	int len;

	sc = devclass_get_softc(firewire_devclass, unit);
	fc = sc->fc;

	pad = uio->uio_offset % 4;
	if  (fwmem_debug && pad != 0)
		printf("unaligned\n");
	while(uio->uio_resid > 0) {
		offset = uio->uio_offset;
		offset -= pad;
		dst_hi = (offset >> 32) & 0xffff;
		dst_lo = offset & 0xffffffff;
#if USE_QUAD
		xfer = fwmem_read_quad(fc, fwmem_speed, fwmem_node,
				dst_hi, dst_lo, fw_asy_callback);
		if (xfer == NULL)
			return EINVAL;
		err = tsleep((caddr_t)xfer, FWPRI, "fwmem", hz);
		if (err !=0 || xfer->resp != 0 || xfer->recv.buf == NULL)
			return EINVAL; /* XXX */
		err = uiomove(xfer->recv.buf + xfer->recv.off + 4*3 + pad,
			4 - pad, uio);
#else
		len = uio->uio_resid;
		if (len > MAXLEN)
			len = MAXLEN;
		xfer = fwmem_read_block(fc, fwmem_speed, fwmem_node,
				dst_hi, dst_lo, len, fw_asy_callback);
		if (xfer == NULL)
			return EINVAL;
		err = tsleep((caddr_t)xfer, FWPRI, "fwmem", hz);
		if (err != 0 || xfer->resp != 0 || xfer->recv.buf == NULL)
			return EINVAL; /* XXX */
		err = uiomove(xfer->recv.buf + xfer->recv.off + 4*4 + pad,
			len - pad, uio);
#endif
		if (err)
			return err;
		fw_xfer_free(xfer);
		pad = 0;
	}
	return err;
}
int
fwmem_write (dev_t dev, struct uio *uio, int ioflag)
{
	return EINVAL;
}
int
fwmem_ioctl (dev_t dev, u_long cmd, caddr_t data, int flag, fw_proc *td)
{
	return EINVAL;
}
int
fwmem_poll (dev_t dev, int events, fw_proc *td)
{  
	return EINVAL;
}
int
fwmem_mmap (dev_t dev, vm_offset_t offset, int nproto)
{  
	return EINVAL;
}
