/*	$NetBSD: iq31244_7seg.c,v 1.2 2003/07/15 00:25:01 lukem Exp $	*/

/*-
 * Copyright (c) 2001, 2002, 2003 Wasabi Systems, Inc.
 * All rights reserved.
 *
 * Written by Jason R. Thorpe for Wasabi Systems, Inc.
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
 *	This product includes software developed for the NetBSD Project by
 *	Wasabi Systems, Inc.
 * 4. The name of Wasabi Systems, Inc. may not be used to endorse
 *    or promote products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY WASABI SYSTEMS, INC. ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL WASABI SYSTEMS, INC
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Support for the 7-segment display on the Intel IQ81342.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/sys/arm/xscale/i8134x/iq81342_7seg.c,v 1.1 2007/09/22 16:25:43 cognet Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/bus.h>
#include <sys/sysctl.h>

#include <machine/bus.h>

#include <arm/xscale/i8134x/i81342reg.h>
#include <arm/xscale/i8134x/iq81342reg.h>
#include <arm/xscale/i8134x/iq81342var.h>

#define	WRITE(x, v)	*((__volatile uint8_t *) (x)) = (v)

static int snakestate;

/*
 * The 7-segment display looks like so:
 *
 *         A
 *	+-----+
 *	|     |
 *    F	|     | B
 *	|  G  |
 *	+-----+
 *	|     |
 *    E	|     | C
 *	|  D  |
 *	+-----+ o  DP
 *
 * Setting a bit clears the corresponding segment on the
 * display.
 */
#define	SEG_A			(1 << 1)
#define	SEG_B			(1 << 2)
#define	SEG_C			(1 << 3)
#define	SEG_D			(1 << 4)
#define	SEG_E			(1 << 5)
#define	SEG_F			(1 << 6)
#define	SEG_G			(1 << 7)
#define	SEG_DP			(1 << 0)

static const uint8_t digitmap[] = {
/*	+#####+
 *	#     #
 *	#     #
 *	#     #
 *	+-----+
 *	#     #
 *	#     #
 *	#     #
 *	+#####+
 */
	(unsigned char)~SEG_G,

/*	+-----+
 *	|     #
 *	|     #
 *	|     #
 *	+-----+
 *	|     #
 *	|     #
 *	|     #
 *	+-----+
 */
	SEG_B|SEG_C,

/*	+#####+
 *	|     #
 *	|     #
 *	|     #
 *	+#####+
 *	#     |
 *	#     |
 *	#     |
 *	+#####+
 */
	~(SEG_C|SEG_F),

/*	+#####+
 *	|     #
 *	|     #
 *	|     #
 *	+#####+
 *	|     #
 *	|     #
 *	|     #
 *	+#####+
 */
	~(SEG_E|SEG_F),

/*	+-----+
 *	#     #
 *	#     #
 *	#     #
 *	+#####+
 *	|     #
 *	|     #
 *	|     #
 *	+-----+
 */
	~(SEG_A|SEG_D|SEG_E),

/*	+#####+
 *	#     |
 *	#     |
 *	#     |
 *	+#####+
 *	|     #
 *	|     #
 *	|     #
 *	+#####+
 */
	~(SEG_B|SEG_E),

/*	+#####+
 *	#     |
 *	#     |
 *	#     |
 *	+#####+
 *	#     #
 *	#     #
 *	#     #
 *	+#####+
 */
	~(SEG_B),

/*	+#####+
 *	|     #
 *	|     #
 *	|     #
 *	+-----+
 *	|     #
 *	|     #
 *	|     #
 *	+-----+
 */
	~(SEG_D|SEG_E|SEG_F),

/*	+#####+
 *	#     #
 *	#     #
 *	#     #
 *	+#####+
 *	#     #
 *	#     #
 *	#     #
 *	+#####+
 */
	~0,

/*	+#####+
 *	#     #
 *	#     #
 *	#     #
 *	+#####+
 *	|     #
 *	|     #
 *	|     #
 *	+-----+
 */
	~(SEG_D|SEG_E),
};

static uint8_t 
iq81342_7seg_xlate(char c)
{
	uint8_t rv;

	if (c >= '0' && c <= '9')
		rv = digitmap[c - '0'];
	else if (c == '.')
		rv = (uint8_t) ~SEG_DP;
	else
		rv = 0xff;

	return (rv);
}

void
iq81342_7seg(char a, char b)
{
	uint8_t msb, lsb;

	msb = iq81342_7seg_xlate(a);
	lsb = iq81342_7seg_xlate(b);

	snakestate = 0;

	WRITE(IQ8134X_7SEG_MSB, msb);
	WRITE(IQ8134X_7SEG_LSB, lsb);
}

static const uint8_t snakemap[][2] = {

/*	+#####+		+#####+
 *	|     |		|     |
 *	|     |		|     |
 *	|     |		|     |
 *	+-----+		+-----+
 *	|     |		|     |
 *	|     |		|     |
 *	|     |		|     |
 *	+-----+		+-----+
 */
	{ SEG_A,	SEG_A },

/*	+-----+		+-----+
 *	#     |		|     #
 *	#     |		|     #
 *	#     |		|     #
 *	+-----+		+-----+
 *	|     |		|     |
 *	|     |		|     |
 *	|     |		|     |
 *	+-----+		+-----+
 */
	{ SEG_F,	SEG_B },

/*	+-----+		+-----+
 *	|     |		|     |
 *	|     |		|     |
 *	|     |		|     |
 *	+#####+		+#####+
 *	|     |		|     |
 *	|     |		|     |
 *	|     |		|     |
 *	+-----+		+-----+
 */
	{ SEG_G,	SEG_G },

/*	+-----+		+-----+
 *	|     |		|     |
 *	|     |		|     |
 *	|     |		|     |
 *	+-----+		+-----+
 *	|     #		#     |
 *	|     #		#     |
 *	|     #		#     |
 *	+-----+		+-----+
 */
	{ SEG_C,	SEG_E },

/*	+-----+		+-----+
 *	|     |		|     |
 *	|     |		|     |
 *	|     |		|     |
 *	+-----+		+-----+
 *	|     |		|     |
 *	|     |		|     |
 *	|     |		|     |
 *	+#####+		+#####+
 */
	{ SEG_D,	SEG_D },

/*	+-----+		+-----+
 *	|     |		|     |
 *	|     |		|     |
 *	|     |		|     |
 *	+-----+		+-----+
 *	#     |		|     #
 *	#     |		|     #
 *	#     |		|     #
 *	+-----+		+-----+
 */
	{ SEG_E,	SEG_C },

/*	+-----+		+-----+
 *	|     |		|     |
 *	|     |		|     |
 *	|     |		|     |
 *	+#####+		+#####+
 *	|     |		|     |
 *	|     |		|     |
 *	|     |		|     |
 *	+-----+		+-----+
 */
	{ SEG_G,	SEG_G },

/*	+-----+		+-----+
 *	|     #		#     |
 *	|     #		#     |
 *	|     #		#     |
 *	+-----+		+-----+
 *	|     |		|     |
 *	|     |		|     |
 *	|     |		|     |
 *	+-----+		+-----+
 */
	{ SEG_B,	SEG_F },
};

SYSCTL_NODE(_hw, OID_AUTO, sevenseg, CTLFLAG_RD, 0, "7 seg");
static int freq = 20;
SYSCTL_INT(_hw_sevenseg, OID_AUTO, freq, CTLFLAG_RW, &freq, 0, 
    "7 Seg update frequency");
static void
iq81342_7seg_snake(void)
{
	static int snakefreq;
	int cur = snakestate;

	snakefreq++;
	if ((snakefreq % freq))
		return;
	WRITE(IQ8134X_7SEG_MSB, snakemap[cur][0]);
	WRITE(IQ8134X_7SEG_LSB, snakemap[cur][1]);

	snakestate = (cur + 1) & 7;
}

struct iq81342_7seg_softc {
	device_t	dev;
};

static int
iq81342_7seg_probe(device_t dev)
{

	device_set_desc(dev, "IQ81342 7seg");
	return (0);
}

extern void (*i80321_hardclock_hook)(void);
static int
iq81342_7seg_attach(device_t dev)
{

	i80321_hardclock_hook = iq81342_7seg_snake;
	return (0);
}

static device_method_t iq81342_7seg_methods[] = {
	DEVMETHOD(device_probe, iq81342_7seg_probe),
	DEVMETHOD(device_attach, iq81342_7seg_attach),
	{0, 0},
};

static driver_t iq81342_7seg_driver = {
	"iqseg",
	iq81342_7seg_methods,
	sizeof(struct iq81342_7seg_softc),
};
static devclass_t iq81342_7seg_devclass;

DRIVER_MODULE(iqseg, iq, iq81342_7seg_driver, iq81342_7seg_devclass, 0, 0);
