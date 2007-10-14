/*	$FreeBSD$	*/
/*	$OpenBSD: itvar.h,v 1.4 2007/03/22 16:55:31 deraadt Exp $	*/

/*-
 * Copyright (c) 2003 Julien Bordet <zejames@greyhats.org>
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _DEV_ISA_ITVAR_H
#define _DEV_ISA_ITVAR_H

#define IT_NUM_SENSORS	15

/* chip ids */
#define IT_ID_IT87	0x90

/* ctl registers */

#define ITC_ADDR	0x05
#define ITC_DATA	0x06

/* data registers */

#define ITD_CONFIG	0x00
#define ITD_ISR1	0x01
#define ITD_ISR2	0x02
#define ITD_ISR3	0x03
#define ITD_SMI1	0x04
#define ITD_SMI2	0x05
#define ITD_SMI3	0x06
#define ITD_IMR1	0x07
#define ITD_IMR2	0x08
#define ITD_IMR3	0x09
#define ITD_VID		0x0a
#define ITD_FAN		0x0b

#define ITD_FANMINBASE	0x10
#define ITD_FANENABLE	0x13

#define ITD_SENSORFANBASE	0x0d	/* Fan from 0x0d to 0x0f */
#define ITD_SENSORVOLTBASE	0x20	/* Fan from 0x20 to 0x28 */
#define ITD_SENSORTEMPBASE	0x29	/* Fan from 0x29 to 0x2b */

#define ITD_VOLTMAXBASE	0x30
#define ITD_VOLTMINBASE	0x31

#define ITD_TEMPMAXBASE 0x40
#define ITD_TEMPMINBASE 0x41

#define ITD_SBUSADDR	0x48
#define ITD_VOLTENABLE	0x50
#define ITD_TEMPENABLE	0x51

#define ITD_CHIPID	0x58

#define IT_VREF		(4096) /* Vref = 4.096 V */

struct it_softc {
	struct device *sc_dev;

	struct resource *sc_iores;
	int sc_iorid;
	bus_space_tag_t sc_iot;
	bus_space_handle_t sc_ioh;

	struct ksensor sensors[IT_NUM_SENSORS];
	struct ksensordev sensordev;
	u_int numsensors;
	void (*refresh_sensor_data)(struct it_softc *);

	u_int8_t (*it_readreg)(struct it_softc *, int);
	void (*it_writereg)(struct it_softc *, int, int);
};

#endif
