/*-
 * Copyright (c) 2009 Oleksandr Tymoshenko <gonzo@freebsd.org>
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
 * $FreeBSD$
 *
 */

#ifndef	__GPIOBUS_H__
#define	__GPIOBUS_H__

#include <sys/param.h>
#include <sys/lock.h>
#include <sys/mutex.h>

#define GPIOBUS_IVAR(d) (struct gpiobus_ivar *) device_get_ivars(d)
#define GPIOBUS_SOFTC(d) (struct gpiobus_softc *) device_get_softc(d)

struct gpiobus_softc
{
	struct mtx	sc_mtx;		/* bus mutex */
	device_t	sc_busdev;	/* bus device */
	device_t	sc_owner;	/* bus owner */
	device_t	sc_dev;		/* driver device */
	int		sc_npins;	/* total pins on bus */
	int		*sc_pins_mapped; /* mark mapped pins */
};


struct gpiobus_ivar
{
	uint32_t	npins;	/* pins total */
	uint32_t	*pins;	/* pins map */
};

#endif	/* __GPIOBUS_H__ */
