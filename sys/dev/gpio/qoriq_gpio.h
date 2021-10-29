/*-
 * Copyright (c) 2020 Alstom Group.
 * Copyright (c) 2020 Semihalf.
 * Copyright (c) 2015 Justin Hibbits
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
 */

#define MAXPIN	(31)

#define BIT(x)	(1 << (x))

#define VALID_PIN(u)	((u) >= 0 && (u) <= MAXPIN)
#define DEFAULT_CAPS	(GPIO_PIN_INPUT | GPIO_PIN_OUTPUT | \
			 GPIO_PIN_OPENDRAIN | GPIO_PIN_PUSHPULL)

#define GPIO_LOCK(sc)		mtx_lock_spin(&(sc)->sc_mtx)
#define	GPIO_UNLOCK(sc)		mtx_unlock_spin(&(sc)->sc_mtx)
#define GPIO_LOCK_INIT(sc) \
	mtx_init(&(sc)->sc_mtx, device_get_nameunit((sc)->dev),	\
	    "gpio", MTX_SPIN)
#define GPIO_LOCK_DESTROY(_sc)	mtx_destroy(&_sc->sc_mtx);

#define	GPIO_GPDIR	0x0
#define	GPIO_GPODR	0x4
#define	GPIO_GPDAT	0x8
#define	GPIO_GPIER	0xc
#define	GPIO_GPIMR	0x10
#define	GPIO_GPICR	0x14
#define	GPIO_GPIBE	0x18

struct qoriq_gpio_softc {
	device_t	dev;
	device_t	busdev;
	struct mtx	sc_mtx;
	struct resource *sc_mem;
	struct gpio_pin	 sc_pins[MAXPIN + 1];
};

device_attach_t qoriq_gpio_attach;
device_detach_t qoriq_gpio_detach;

DECLARE_CLASS(qoriq_gpio_driver);
