/*-
 * Copyright (c) 1999 Kazutaka YOKOTA <yokota@zodiac.mech.utsunomiya-u.ac.jp>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer as
 *    the first lines of this file unmodified.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $Id$
 */

#include "sc.h"
#include "opt_syscons.h"

#if NSC > 0

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/bus.h>

#include <machine/console.h>

#include <dev/syscons/syscons.h>

#include <isa/isareg.h>
#include <isa/isavar.h>

devclass_t	sc_devclass;

static int	scprobe(device_t dev);
static int	scattach(device_t dev);

static device_method_t sc_methods[] = {
	DEVMETHOD(device_probe,         scprobe),
	DEVMETHOD(device_attach,        scattach),
	{ 0, 0 }
};

static driver_t sc_driver = {
	"sc",
	sc_methods,
	DRIVER_TYPE_TTY,
	1,                          /* XXX */
};

static int
scprobe(device_t dev)
{
	device_set_desc(dev, "System console");
	return sc_probe_unit(device_get_unit(dev), isa_get_flags(dev));
}

static int
scattach(device_t dev)
{
	return sc_attach_unit(device_get_unit(dev), isa_get_flags(dev));
}

DRIVER_MODULE(sc, isa, sc_driver, sc_devclass, 0, 0);

#endif /* NSC > 0 */
