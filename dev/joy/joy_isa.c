/*-
 * Copyright (c) 1995 Jean-Marc Zucconi
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in this position and unchanged.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
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
 *
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/uio.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/bus.h>
#include <machine/bus.h>
#include <machine/resource.h>
#include <sys/rman.h>
#include <sys/time.h>
#include <dev/joy/joyvar.h>

#include <isa/isavar.h>
#include "isa_if.h"

static int joy_isa_probe (device_t);

static struct isa_pnp_id joy_ids[] = {
    {0x0100630e, "CSC0001 PnP Joystick"},	/* CSC0001 */
    {0x0101630e, "CSC0101 PnP Joystick"},	/* CSC0101 */
    {0x01100002, "ALS0110 PnP Joystick"},	/* @P@1001 */
    {0x01200002, "ALS0120 PnP Joystick"},	/* @P@2001 */
    {0x01007316, "ESS0001 PnP Joystick"},	/* ESS0001 */
    {0x2fb0d041, "Generic PnP Joystick"},	/* PNPb02f */
    {0x2200a865, "YMH0022 PnP Joystick"},	/* YMH0022 */
    {0x82719304, NULL},    			/* ADS7182 */
    {0}
};

static int
joy_isa_probe(device_t dev)
{
    if (ISA_PNP_PROBE(device_get_parent(dev), dev, joy_ids) == ENXIO)
        return ENXIO;
    return (joy_probe(dev));
}

static device_method_t joy_methods[] = {
    DEVMETHOD(device_probe,	joy_isa_probe),
    DEVMETHOD(device_attach,	joy_attach),
    DEVMETHOD(device_detach,	joy_detach),
    { 0, 0 }
};

static driver_t joy_isa_driver = {
    "joy",
    joy_methods,
    sizeof (struct joy_softc)
};

DRIVER_MODULE(joy, isa, joy_isa_driver, joy_devclass, 0, 0);
DRIVER_MODULE(joy, acpi, joy_isa_driver, joy_devclass, 0, 0);
