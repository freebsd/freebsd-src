/*-
 * Copyright (c) 1999 Doug Rabson
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
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	$FreeBSD$
 */

#include <dev/sound/pcm/sound.h>
#include <dev/sound/isa/sb.h>

#ifdef __alpha__
static int
es1888_dspready(u_int32_t port)
{
	return ((inb(port + SBDSP_STATUS) & 0x80) == 0);
}

static int
es1888_dspwr(u_int32_t port, u_char val)
{
    	int  i;

    	for (i = 0; i < 1000; i++) {
		if (es1888_dspready(port)) {
	    		outb(port + SBDSP_CMD, val);
	    		return 0;
		}
		if (i > 10) DELAY((i > 100)? 1000 : 10);
    	}
    	return ENXIO;
}

static u_int
es1888_get_byte(u_int32_t port)
{
    	int i;

    	for (i = 1000; i > 0; i--) {
		if (inb(port + DSP_DATA_AVAIL) & 0x80)
			return inb(port + DSP_READ);
		else
			DELAY(20);
    	}
    	return 0xffff;
}

static int
es1888_reset(u_int32_t port)
{
    	outb(port + SBDSP_RST, 3);
    	DELAY(100);
    	outb(port + SBDSP_RST, 0);
    	if (es1888_get_byte(port) != 0xAA) {
		return ENXIO;	/* Sorry */
    	}
    	return 0;
}

static void
es1888_configuration_mode(void)
{
	/*
	 * Emit the Read-Sequence-Key to enter configuration
	 * mode. Note this only works after a reset (or after bit 2 of
	 * mixer register 0x40 is set).
	 *
	 * 3 reads from 0x229 in a row guarantees reset of key
	 * sequence to beginning.
	 */
	inb(0x229);
	inb(0x229);
	inb(0x229);

	inb(0x22b);		/* state 1 */
	inb(0x229);		/* state 2 */
	inb(0x22b);		/* state 3 */
	inb(0x229);		/* state 4 */
	inb(0x229);		/* state 5 */
	inb(0x22b);		/* state 6 */
	inb(0x229);		/* state 7 */
}

static void
es1888_set_port(u_int32_t port)
{
	es1888_configuration_mode();
	inb(port);
}
#endif

static void
es1888_identify(driver_t *driver, device_t parent)
{
/*
 * Only use this on alpha since PNPBIOS is a better solution on x86.
 */
#ifdef __alpha__
	u_int32_t lo, hi;
	device_t dev;

	es1888_set_port(0x220);
	if (es1888_reset(0x220))
		return;

	/*
	 * Check identification bytes for es1888.
	 */
	if (es1888_dspwr(0x220, 0xe7))
		return;
	hi = es1888_get_byte(0x220);
	lo = es1888_get_byte(0x220);
	if (hi != 0x68 || (lo & 0xf0) != 0x80)
		return;

	/*
	 * Program irq and drq.
	 */
	if (es1888_dspwr(0x220, 0xc6) /* enter extended mode */
	    || es1888_dspwr(0x220, 0xb1) /* write register b1 */
	    || es1888_dspwr(0x220, 0x14) /* enable irq 5 */
	    || es1888_dspwr(0x220, 0xb2) /* write register b1 */
	    || es1888_dspwr(0x220, 0x18)) /* enable drq 1 */
		return;

	/*
	 * Create the device and program its resources.
	 */
	dev = BUS_ADD_CHILD(parent, ISA_ORDER_PNP, NULL, -1);
	bus_set_resource(dev, SYS_RES_IOPORT, 0, 0x220, 0x10);
	bus_set_resource(dev, SYS_RES_IRQ, 0, 5, 1);
	bus_set_resource(dev, SYS_RES_DRQ, 0, 1, 1);
	isa_set_vendorid(dev, PNP_EISAID("ESS1888"));
	isa_set_logicalid(dev, PNP_EISAID("ESS1888"));
#endif
}

static device_method_t es1888_methods[] = {
	/* Device interface */
	DEVMETHOD(device_identify,	es1888_identify),

	{ 0, 0 }
};

static driver_t es1888_driver = {
	"pcm",
	es1888_methods,
	1,			/* no softc */
};

DRIVER_MODULE(snd_es1888, isa, es1888_driver, pcm_devclass, 0, 0);
MODULE_DEPEND(snd_es1888, snd_pcm, PCM_MINVER, PCM_PREFVER, PCM_MAXVER);
MODULE_VERSION(snd_es1888, 1);


