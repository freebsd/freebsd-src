/* $NetBSD: am79c930.c,v 1.5 2000/03/23 13:57:58 onoe Exp $ */
/*-
 * Copyright (c) 1999 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Bill Sommerfeld
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
 *        This product includes software developed by the NetBSD
 *        Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

/*
 * Am79c930 chip driver.
 *
 * This is used by the awi driver to use the shared
 * memory attached to the 79c930 to communicate with the firmware running
 * in the 930's on-board 80188 core.
 *
 * The 79c930 can be mapped into just I/O space, or also have a
 * memory mapping; the mapping must be set up by the bus front-end
 * before am79c930_init is called.
 */

/*
 * operations:
 *
 * read_8, read_16, read_32, read_64, read_bytes
 * write_8, write_16, write_32, write_64, write_bytes
 * (two versions, depending on whether memory-space or i/o space is in use).
 *
 * interrupt E.C.
 * start isr
 * end isr
 */

#include <sys/param.h>
#include <sys/systm.h>
#ifndef __FreeBSD__
#include <sys/device.h>
#endif

#include <machine/cpu.h>
#ifdef __FreeBSD__
#include <machine/bus_pio.h>
#include <machine/bus_memio.h>
#endif
#include <machine/bus.h>
#ifdef __NetBSD__
#include <machine/intr.h>
#endif

#ifdef __NetBSD__
#include <dev/ic/am79c930reg.h>
#include <dev/ic/am79c930var.h>
#endif
#ifdef __FreeBSD__
#include <dev/awi/am79c930reg.h>
#include <dev/awi/am79c930var.h>
#endif

#define AM930_DELAY(x) /*nothing*/

void am79c930_regdump(struct am79c930_softc *sc);

static void io_write_1(struct am79c930_softc *, u_int32_t, u_int8_t);
static void io_write_2(struct am79c930_softc *, u_int32_t, u_int16_t);
static void io_write_4(struct am79c930_softc *, u_int32_t, u_int32_t);
static void io_write_bytes(struct am79c930_softc *, u_int32_t, u_int8_t *, size_t);

static u_int8_t io_read_1(struct am79c930_softc *, u_int32_t);
static u_int16_t io_read_2(struct am79c930_softc *, u_int32_t);
static u_int32_t io_read_4(struct am79c930_softc *, u_int32_t);
static void io_read_bytes(struct am79c930_softc *, u_int32_t, u_int8_t *, size_t);

static void mem_write_1(struct am79c930_softc *, u_int32_t, u_int8_t);
static void mem_write_2(struct am79c930_softc *, u_int32_t, u_int16_t);
static void mem_write_4(struct am79c930_softc *, u_int32_t, u_int32_t);
static void mem_write_bytes(struct am79c930_softc *, u_int32_t, u_int8_t *, size_t);

static u_int8_t mem_read_1(struct am79c930_softc *, u_int32_t);
static u_int16_t mem_read_2(struct am79c930_softc *, u_int32_t);
static u_int32_t mem_read_4(struct am79c930_softc *, u_int32_t);
static void mem_read_bytes(struct am79c930_softc *, u_int32_t, u_int8_t *, size_t);

static struct am79c930_ops iospace_ops = {
	io_write_1,
	io_write_2,
	io_write_4,
	io_write_bytes,
	io_read_1,
	io_read_2,
	io_read_4,
	io_read_bytes
};

struct am79c930_ops memspace_ops = {
	mem_write_1,
	mem_write_2,
	mem_write_4,
	mem_write_bytes,
	mem_read_1,
	mem_read_2,
	mem_read_4,
	mem_read_bytes
};

static void io_write_1 (sc, off, val)
	struct am79c930_softc *sc;
	u_int32_t off;
	u_int8_t val;
{
	AM930_DELAY(1);
	bus_space_write_1(sc->sc_iot, sc->sc_ioh, AM79C930_LMA_HI,
	    ((off>>8)& 0x7f));
	AM930_DELAY(1);
	bus_space_write_1(sc->sc_iot, sc->sc_ioh, AM79C930_LMA_LO, (off&0xff));
	AM930_DELAY(1);
	bus_space_write_1(sc->sc_iot, sc->sc_ioh, AM79C930_IODPA, val);
	AM930_DELAY(1);
}

static void io_write_2 (sc, off, val)
	struct am79c930_softc *sc;
	u_int32_t off;
	u_int16_t val;
{
	AM930_DELAY(1);
	bus_space_write_1(sc->sc_iot, sc->sc_ioh, AM79C930_LMA_HI,
	    ((off>>8)& 0x7f));
	AM930_DELAY(1);
	bus_space_write_1(sc->sc_iot,sc->sc_ioh,AM79C930_LMA_LO, (off&0xff));
	AM930_DELAY(1);
	bus_space_write_1(sc->sc_iot,sc->sc_ioh,AM79C930_IODPA, val & 0xff);
	AM930_DELAY(1);
	bus_space_write_1(sc->sc_iot,sc->sc_ioh,AM79C930_IODPA, (val>>8)&0xff);
	AM930_DELAY(1);
}

static void io_write_4 (sc, off, val)
	struct am79c930_softc *sc;
	u_int32_t off;
	u_int32_t val;
{
	AM930_DELAY(1);
	bus_space_write_1(sc->sc_iot, sc->sc_ioh, AM79C930_LMA_HI,
	    ((off>>8)& 0x7f));
	AM930_DELAY(1);
	bus_space_write_1(sc->sc_iot,sc->sc_ioh,AM79C930_LMA_LO, (off&0xff));
	AM930_DELAY(1);
	bus_space_write_1(sc->sc_iot,sc->sc_ioh,AM79C930_IODPA,val & 0xff);
	AM930_DELAY(1);
	bus_space_write_1(sc->sc_iot,sc->sc_ioh,AM79C930_IODPA,(val>>8)&0xff);
	AM930_DELAY(1);
	bus_space_write_1(sc->sc_iot,sc->sc_ioh,AM79C930_IODPA,(val>>16)&0xff);
	AM930_DELAY(1);
	bus_space_write_1(sc->sc_iot,sc->sc_ioh,AM79C930_IODPA,(val>>24)&0xff);
	AM930_DELAY(1);
}

static void io_write_bytes (sc, off, ptr, len)
	struct am79c930_softc *sc;
	u_int32_t off;
	u_int8_t *ptr;
	size_t len;
{
	int i;

	AM930_DELAY(1);
	bus_space_write_1(sc->sc_iot, sc->sc_ioh, AM79C930_LMA_HI,
	    ((off>>8)& 0x7f));
	AM930_DELAY(1);
	bus_space_write_1(sc->sc_iot,sc->sc_ioh,AM79C930_LMA_LO, (off&0xff));
	AM930_DELAY(1);
	for (i=0; i<len; i++)
		bus_space_write_1(sc->sc_iot,sc->sc_ioh,AM79C930_IODPA,ptr[i]);
}

static u_int8_t io_read_1 (sc, off)
	struct am79c930_softc *sc;
	u_int32_t off;
{
	u_int8_t val;
	
	bus_space_write_1(sc->sc_iot, sc->sc_ioh, AM79C930_LMA_HI,
	    ((off>>8)& 0x7f));
	AM930_DELAY(1);
	bus_space_write_1(sc->sc_iot, sc->sc_ioh, AM79C930_LMA_LO, (off&0xff));
	AM930_DELAY(1);
	val = bus_space_read_1(sc->sc_iot, sc->sc_ioh, AM79C930_IODPA);
	AM930_DELAY(1);
	return val;
}

static u_int16_t io_read_2 (sc, off)
	struct am79c930_softc *sc;
	u_int32_t off;
{
	u_int16_t val;

	bus_space_write_1(sc->sc_iot, sc->sc_ioh, AM79C930_LMA_HI,
	    ((off>>8)& 0x7f));
	AM930_DELAY(1);
	bus_space_write_1(sc->sc_iot, sc->sc_ioh, AM79C930_LMA_LO, (off&0xff));
	AM930_DELAY(1);
	val = bus_space_read_1(sc->sc_iot, sc->sc_ioh, AM79C930_IODPA);
	AM930_DELAY(1);
	val |= bus_space_read_1(sc->sc_iot, sc->sc_ioh, AM79C930_IODPA) << 8;
	AM930_DELAY(1);	
	return val;
}

static u_int32_t io_read_4 (sc, off)
	struct am79c930_softc *sc;
	u_int32_t off;
{
	u_int32_t val;

	bus_space_write_1(sc->sc_iot, sc->sc_ioh, AM79C930_LMA_HI,
	    ((off>>8)& 0x7f));
	AM930_DELAY(1);
	bus_space_write_1(sc->sc_iot, sc->sc_ioh, AM79C930_LMA_LO, (off&0xff));
	AM930_DELAY(1);
	val = bus_space_read_1(sc->sc_iot, sc->sc_ioh, AM79C930_IODPA);
	AM930_DELAY(1);
	val |= bus_space_read_1(sc->sc_iot, sc->sc_ioh, AM79C930_IODPA) << 8;
	AM930_DELAY(1);	
	val |= bus_space_read_1(sc->sc_iot, sc->sc_ioh, AM79C930_IODPA) << 16;
	AM930_DELAY(1);	
	val |= bus_space_read_1(sc->sc_iot, sc->sc_ioh, AM79C930_IODPA) << 24;
	AM930_DELAY(1);	
	return val;
}

static void io_read_bytes (sc, off, ptr, len)
	struct am79c930_softc *sc;
	u_int32_t off;
	u_int8_t *ptr;
	size_t len;
{
	int i;
	
	bus_space_write_1(sc->sc_iot, sc->sc_ioh, AM79C930_LMA_HI,
	    ((off>>8)& 0x7f));
	AM930_DELAY(1);
	bus_space_write_1(sc->sc_iot, sc->sc_ioh, AM79C930_LMA_LO, (off&0xff));
	AM930_DELAY(1);
	for (i=0; i<len; i++) 
		ptr[i] = bus_space_read_1(sc->sc_iot, sc->sc_ioh,
		    AM79C930_IODPA);
}

static void mem_write_1 (sc, off, val)
	struct am79c930_softc *sc;
	u_int32_t off;
	u_int8_t val;
{
	bus_space_write_1(sc->sc_memt, sc->sc_memh, off, val);
}

static void mem_write_2 (sc, off, val)
	struct am79c930_softc *sc;
	u_int32_t off;
	u_int16_t val;
{
	bus_space_tag_t t = sc->sc_memt;
	bus_space_handle_t h = sc->sc_memh;

	/* could be unaligned */
	if ((off & 0x1) == 0)
		bus_space_write_2(t, h, off,    val);
	else {
		bus_space_write_1(t, h, off,    val        & 0xff);
		bus_space_write_1(t, h, off+1, (val >>  8) & 0xff);
	}
}

static void mem_write_4 (sc, off, val)
	struct am79c930_softc *sc;
	u_int32_t off;
	u_int32_t val;
{
	bus_space_tag_t t = sc->sc_memt;
	bus_space_handle_t h = sc->sc_memh;

	/* could be unaligned */
	if ((off & 0x3) == 0)
		bus_space_write_4(t, h, off,    val);
	else {
		bus_space_write_1(t, h, off,    val        & 0xff);
		bus_space_write_1(t, h, off+1, (val >>  8) & 0xff);
		bus_space_write_1(t, h, off+2, (val >> 16) & 0xff);
		bus_space_write_1(t, h, off+3, (val >> 24) & 0xff);
	}
}

static void mem_write_bytes (sc, off, ptr, len)
	struct am79c930_softc *sc;
	u_int32_t off;
	u_int8_t *ptr;
	size_t len;
{
	bus_space_write_region_1 (sc->sc_memt, sc->sc_memh, off, ptr, len);
}


static u_int8_t mem_read_1 (sc, off)
	struct am79c930_softc *sc;
	u_int32_t off;
{
	return bus_space_read_1(sc->sc_memt, sc->sc_memh, off);
}

static u_int16_t mem_read_2 (sc, off)
	struct am79c930_softc *sc;
	u_int32_t off;
{
	/* could be unaligned */
	if ((off & 0x1) == 0)
		return bus_space_read_2(sc->sc_memt, sc->sc_memh, off);
	else
		return
		     bus_space_read_1(sc->sc_memt, sc->sc_memh, off  )       |
		    (bus_space_read_1(sc->sc_memt, sc->sc_memh, off+1) << 8);
}

static u_int32_t mem_read_4 (sc, off)
	struct am79c930_softc *sc;
	u_int32_t off;
{
	/* could be unaligned */
	if ((off & 0x3) == 0)
		return bus_space_read_4(sc->sc_memt, sc->sc_memh, off);
	else
		return
		     bus_space_read_1(sc->sc_memt, sc->sc_memh, off  )       |
		    (bus_space_read_1(sc->sc_memt, sc->sc_memh, off+1) << 8) |
		    (bus_space_read_1(sc->sc_memt, sc->sc_memh, off+2) <<16) |
		    (bus_space_read_1(sc->sc_memt, sc->sc_memh, off+3) <<24);
}



static void mem_read_bytes (sc, off, ptr, len)
	struct am79c930_softc *sc;
	u_int32_t off;
	u_int8_t *ptr;
	size_t len;
{
	bus_space_read_region_1 (sc->sc_memt, sc->sc_memh, off, ptr, len);
}




/*
 * Set bits in GCR.
 */

void am79c930_gcr_setbits (sc, bits)
	struct am79c930_softc *sc;
	u_int8_t bits;
{
	u_int8_t gcr = bus_space_read_1 (sc->sc_iot, sc->sc_ioh, AM79C930_GCR);

	gcr |= bits;
	
	bus_space_write_1(sc->sc_iot, sc->sc_ioh, AM79C930_GCR, gcr);
}

/*
 * Clear bits in GCR.
 */

void am79c930_gcr_clearbits (sc, bits)
	struct am79c930_softc *sc;
	u_int8_t bits;
{
	u_int8_t gcr = bus_space_read_1 (sc->sc_iot, sc->sc_ioh, AM79C930_GCR);

	gcr &= ~bits;
	
	bus_space_write_1(sc->sc_iot, sc->sc_ioh, AM79C930_GCR, gcr);
}

u_int8_t am79c930_gcr_read (sc)
	struct am79c930_softc *sc;
{
	return bus_space_read_1 (sc->sc_iot, sc->sc_ioh, AM79C930_GCR);
}

#if 0 
void am79c930_regdump (sc) 
	struct am79c930_softc *sc;
{
	u_int8_t buf[8];
	int i;

	AM930_DELAY(5);
	for (i=0; i<8; i++) {
		buf[i] = bus_space_read_1 (sc->sc_iot, sc->sc_ioh, i);
		AM930_DELAY(5);
	}
	printf("am79c930: regdump:");
	for (i=0; i<8; i++) {
		printf(" %02x", buf[i]);
	}
	printf("\n");
}
#endif

void am79c930_chip_init (sc, how)
	struct am79c930_softc *sc;
	int how;
{
	/* zero the bank select register, and leave it that way.. */
	bus_space_write_1(sc->sc_iot, sc->sc_ioh, AM79C930_BSS, 0);
	if (how)
	  	sc->sc_ops = &memspace_ops;
	else
	  	sc->sc_ops = &iospace_ops;
}


