/*-
 * Copyright (c) 1999 Kazutaka YOKOTA (yokota@zodiac.mech.utsunomiya-u.ac.jp)
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
 * 3. The name of the author may not be used to endorse or promote 
 *    products derived from this software without specific prior written 
 *    permission.
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
 * $Id: $
 */

#include "atkbdc.h"
#include "opt_kbd.h"

#if NATKBDC > 0

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>

#include <dev/kbd/atkbdcreg.h>

#include <i386/isa/isa.h>
#include <i386/isa/isa_device.h>

static int	atkbdc_probe(struct isa_device *dev);
static int	atkbdc_attach(struct isa_device *dev);

struct isa_driver atkbdcdriver = {
	atkbdc_probe,
	atkbdc_attach,
	ATKBDC_DRIVER_NAME,
	0,
};

static int
atkbdc_probe(struct isa_device *dev)
{
	atkbdc_softc_t *sc;
	int error;

	sc = atkbdc_get_softc(dev->id_unit);
	if (sc == NULL)
		return 0;

	error = atkbdc_probe_unit(sc, dev->id_unit, dev->id_iobase);
	if (error)
		return 0;
	if (dev->id_iobase <= 0)
		dev->id_iobase = sc->port;
	return IO_KBDSIZE;
}

static int
atkbdc_attach(struct isa_device *dev)
{
	atkbdc_softc_t *sc;
	
	sc = atkbdc_get_softc(dev->id_unit);
	return ((sc == NULL) ? 0 : 1);
}

#endif /* NATKBDC > 0 */
