/*-
 * Copyright (c) 2003 Poul-Henning Kamp
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
 * 3. The names of the authors may not be used to endorse or promote
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/timetc.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <dev/pci/pcivar.h>

static unsigned	cba;
static unsigned	geode_counter;

static unsigned
geode_get_timecount(struct timecounter *tc)
{
	return (inl(geode_counter));
}

static struct timecounter geode_timecounter = {
	geode_get_timecount,
	NULL,
	0xffffffff,
	27000000,
	"Geode",
	1000
};


static int
geode_probe(device_t self)
{

	if (pci_get_devid(self) != 0x0515100b)
		return (ENXIO);
	if (geode_counter != 0)
		return (ENXIO);
	cba = pci_read_config(self, 0x64, 4);
	printf("Geode CBA@ 0x%x\n", cba);
	geode_counter = cba + 0x08;
	outl(cba + 0x0d, 2);
	tc_init(&geode_timecounter);
	return (ENXIO);
}

static int
geode_attach(device_t self)
{

	return(ENODEV);
}

static device_method_t geode_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		geode_probe),
	DEVMETHOD(device_attach,	geode_attach),
	DEVMETHOD(device_suspend,	bus_generic_suspend),
	DEVMETHOD(device_resume,	bus_generic_resume),
	DEVMETHOD(device_shutdown,	bus_generic_shutdown),
	{0, 0}
};
 
static driver_t geode_driver = {
	"geode",
	geode_methods,
	0,
};

static devclass_t geode_devclass;

DRIVER_MODULE(geode, pci, geode_driver, geode_devclass, 0, 0);
