/* $FreeBSD$ */
/*	$NetBSD: if_le_ioasic.c,v 1.10 1998/01/19 02:49:48 thorpej Exp $	*/

/*
 * Copyright (c) 1996 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Author: Chris G. Demetriou
 *
 * Permission to use, copy, modify and distribute this software and
 * its documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND
 * FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 */

/*
 * LANCE on DEC IOCTL ASIC.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/bus.h>
#include <sys/mbuf.h>
#include <sys/syslog.h>
#include <sys/socket.h>

#include <net/if.h>
#include <net/netisr.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/ip.h>
#include <netinet/if_ether.h>
#include <net/if_media.h>

#ifdef INET
#include <netinet/in.h>
#include <netinet/if_inarp.h>
#endif

#include <alpha/tc/am7990reg.h>
#include <alpha/tc/am7990var.h>

#include <alpha/tc/if_levar.h>
#include <alpha/tc/tcreg.h>
#include <alpha/tc/tcvar.h>
#include <alpha/tc/ioasicvar.h>

extern caddr_t le_iomem;

static int  le_ioasic_probe(device_t dev);
static int  le_ioasic_attach(device_t dev);
static device_method_t le_ioasic_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		le_ioasic_probe),
	DEVMETHOD(device_attach,	le_ioasic_attach),
	{0, 0}
};
#define LE_IOASIC_SOFTC(dev)	(struct le_softc*) device_get_softc(dev)
static driver_t le_ioasic_driver = {
	"le",
	le_ioasic_methods,
	sizeof(struct le_softc),
};

static  devclass_t      le_ioasic_devclass;

hide void le_ioasic_copytobuf_gap2(struct am7990_softc *, void *,
	    int, int);
hide void le_ioasic_copyfrombuf_gap2(struct am7990_softc *, void *,
	    int, int);

hide void le_ioasic_copytobuf_gap16(struct am7990_softc *, void *,
	    int, int);
hide void le_ioasic_copyfrombuf_gap16(struct am7990_softc *, void *,
	    int, int);
hide void le_ioasic_zerobuf_gap16(struct am7990_softc *, int, int);


static int
le_ioasic_probe(device_t dev)
{
	if(strcmp(device_get_name(device_get_parent(dev)),"ioasic")){
		return ENXIO;
	} else {
		return(0);
	}
	
}

static int
le_ioasic_attach(device_t dev)
{
	register struct le_softc *lesc = LE_IOASIC_SOFTC(dev);
	register struct am7990_softc *sc = &lesc->sc_am7990;
	struct ioasic_dev *d = device_get_ivars(dev);
	lesc->sc_r1 = (struct lereg1 *)
		TC_DENSE_TO_SPARSE(TC_PHYS_TO_UNCACHED(d->iada_addr));
	sc->sc_mem = (void *)TC_PHYS_TO_UNCACHED(le_iomem);
	sc->sc_copytodesc = le_ioasic_copytobuf_gap2;
	sc->sc_copyfromdesc = le_ioasic_copyfrombuf_gap2;
	sc->sc_copytobuf = le_ioasic_copytobuf_gap16;
	sc->sc_copyfrombuf = le_ioasic_copyfrombuf_gap16;
	sc->sc_zerobuf = le_ioasic_zerobuf_gap16;

	if (le_iomem == 0) {
		printf("%s: DMA area not set up\n",
			device_get_nameunit(sc->sc_dev));
		return ENXIO;
	}

	sc->sc_dev = dev;
	sc->unit = device_get_unit(dev);

	dec_le_common_attach(sc, ioasic_lance_ether_address());

	ioasic_intr_establish(device_get_parent(dev), d->iad_cookie, 0,  am7990_intr, sc);
	return(0);
}

/*
 * Special memory access functions needed by ioasic-attached LANCE
 * chips.
 */

/*
 * gap2: two bytes of data followed by two bytes of pad.
 *
 * Buffers must be 4-byte aligned.  The code doesn't worry about
 * doing an extra byte.
 */

void
le_ioasic_copytobuf_gap2(sc, fromv, boff, len)
	struct am7990_softc *sc;  
	void *fromv;
	int boff;
	register int len;
{
	volatile caddr_t buf = sc->sc_mem;
	register caddr_t from = fromv;
	register volatile u_int16_t *bptr;  

	if (boff & 0x1) {
		/* handle unaligned first byte */
		bptr = ((volatile u_int16_t *)buf) + (boff - 1);
		*bptr = (*from++ << 8) | (*bptr & 0xff);
		bptr += 2;  
		len--;
	} else
		bptr = ((volatile u_int16_t *)buf) + boff;
	while (len > 1) {
		*bptr = (from[1] << 8) | (from[0] & 0xff);
		bptr += 2;
		from += 2;
		len -= 2;
	}
	if (len == 1)
		*bptr = (u_int16_t)*from;
}

void
le_ioasic_copyfrombuf_gap2(sc, tov, boff, len)
	struct am7990_softc *sc;
	void *tov;
	int boff, len;
{
	volatile caddr_t buf = sc->sc_mem;
	register caddr_t to = tov;
	register volatile u_int16_t *bptr;
	register u_int16_t tmp;

	if (boff & 0x1) {
		/* handle unaligned first byte */
		bptr = ((volatile u_int16_t *)buf) + (boff - 1);
		*to++ = (*bptr >> 8) & 0xff;
		bptr += 2;
		len--;
	} else
		bptr = ((volatile u_int16_t *)buf) + boff;
	while (len > 1) {
		tmp = *bptr;
		*to++ = tmp & 0xff;
		*to++ = (tmp >> 8) & 0xff;
		bptr += 2;
		len -= 2;
	}
	if (len == 1)
		*to = *bptr & 0xff;
}

/*
 * gap16: 16 bytes of data followed by 16 bytes of pad.
 *
 * Buffers must be 32-byte aligned.
 */

void
le_ioasic_copytobuf_gap16(sc, fromv, boff, len)
	struct am7990_softc *sc;
	void *fromv;
	int boff;
	register int len;
{
	volatile caddr_t buf = sc->sc_mem;
	register caddr_t from = fromv;
	register caddr_t bptr;

	bptr = buf + ((boff << 1) & ~0x1f);
	boff &= 0xf;

	/*
	 * Dispose of boff so destination of subsequent copies is
	 * 16-byte aligned.
	 */
	if (boff) {
		register int xfer;
		xfer = min(len, 16 - boff);
		bcopy(from, bptr + boff, xfer);
		from += xfer;
		bptr += 32;
		len -= xfer;
	}

	/* Destination of  copies is now 16-byte aligned. */
	if (len >= 16)
		switch ((u_long)from & (sizeof(u_int32_t) -1)) {
		case 2:
			/*  Ethernet headers make this the dominant case. */
		do {
			register u_int32_t *dst = (u_int32_t*)bptr;
			register u_int16_t t0;
			register u_int32_t t1,  t2, t3, t4;

			/* read from odd-16-bit-aligned, cached src */
			t0 = *(u_int16_t*)from;
			t1 = *(u_int32_t*)(from+2);
			t2 = *(u_int32_t*)(from+6);
			t3 = *(u_int32_t*)(from+10);
			t4 = *(u_int16_t*)(from+14);

			/* DMA buffer is uncached on mips */
			dst[0] =         t0 |  (t1 << 16);
			dst[1] = (t1 >> 16) |  (t2 << 16);
			dst[2] = (t2 >> 16) |  (t3 << 16);
			dst[3] = (t3 >> 16) |  (t4 << 16);

			from += 16;
			bptr += 32;
			len -= 16;
		} while (len >= 16);
		break;

		case 0:
		do {
			register u_int32_t *src = (u_int32_t*)from;
			register u_int32_t *dst = (u_int32_t*)bptr;
			register u_int32_t t0, t1, t2, t3;

			t0 = src[0]; t1 = src[1]; t2 = src[2]; t3 = src[3];
			dst[0] = t0; dst[1] = t1; dst[2] = t2; dst[3] = t3;

			from += 16;
			bptr += 32;
			len -= 16;
		} while (len >= 16);
		break;

		default: 
		/* Does odd-aligned case ever happen? */
		do {
			bcopy(from, bptr, 16);
			from += 16;
			bptr += 32;
			len -= 16;
		} while (len >= 16);
		break;
	}
	if (len)
		bcopy(from, bptr, len);
}

void
le_ioasic_copyfrombuf_gap16(sc, tov, boff, len)
	struct am7990_softc *sc;
	void *tov;
	int boff, len;
{
	volatile caddr_t buf = sc->sc_mem;
	register caddr_t to = tov;
	register caddr_t bptr;

	bptr = buf + ((boff << 1) & ~0x1f);
	boff &= 0xf;

	/* Dispose of boff. source of copy is subsequently 16-byte aligned. */
	if (boff) {
		register int xfer;
		xfer = min(len, 16 - boff);
		bcopy(bptr+boff, to, xfer);
		to += xfer;
		bptr += 32;
		len -= xfer;
	}
	if (len >= 16)
	switch ((u_long)to & (sizeof(u_int32_t) -1)) {
	case 2:
		/*
		 * to is aligned to an odd 16-bit boundary.  Ethernet headers
		 * make this the dominant case (98% or more).
		 */
		do {
			register u_int32_t *src = (u_int32_t*)bptr;
			register u_int32_t t0, t1, t2, t3;

			/* read from uncached aligned DMA buf */
			t0 = src[0]; t1 = src[1]; t2 = src[2]; t3 = src[3];

			/* write to odd-16-bit-word aligned dst */
			*(u_int16_t *) (to+0)  = (u_short)  t0;
			*(u_int32_t *) (to+2)  = (t0 >> 16) |  (t1 << 16);
			*(u_int32_t *) (to+6)  = (t1 >> 16) |  (t2 << 16);
			*(u_int32_t *) (to+10) = (t2 >> 16) |  (t3 << 16);
			*(u_int16_t *) (to+14) = (t3 >> 16);
			bptr += 32;
			to += 16;
			len -= 16;
		} while (len > 16);
		break;
	case 0:
		/* 32-bit aligned aligned copy. Rare. */
		do {
			register u_int32_t *src = (u_int32_t*)bptr;
			register u_int32_t *dst = (u_int32_t*)to;
			register u_int32_t t0, t1, t2, t3;

			t0 = src[0]; t1 = src[1]; t2 = src[2]; t3 = src[3];
			dst[0] = t0; dst[1] = t1; dst[2] = t2; dst[3] = t3;
			to += 16;
			bptr += 32;
			len -= 16;
		} while (len  > 16);
		break;

	/* XXX Does odd-byte-aligned case ever happen? */
	default:
		do {
			bcopy(bptr, to, 16);
			to += 16;
			bptr += 32;
			len -= 16;
		} while (len  > 16);
		break;
	}
	if (len)
		bcopy(bptr, to, len);
}

void
le_ioasic_zerobuf_gap16(sc, boff, len)
	struct am7990_softc *sc;
	int boff, len;
{
	volatile caddr_t buf = sc->sc_mem;
	register caddr_t bptr;
	register int xfer;

	bptr = buf + ((boff << 1) & ~0x1f);
	boff &= 0xf;
	xfer = min(len, 16 - boff);
	while (len > 0) {
		bzero(bptr + boff, xfer);
		bptr += 32;
		boff = 0;
		len -= xfer;
		xfer = min(len, 16);
	}
}



DRIVER_MODULE(le_ioasic, ioasic, le_ioasic_driver, le_ioasic_devclass, 0, 0);
