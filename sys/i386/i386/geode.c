/*-
 * Copyright (c) 2003-2004 Poul-Henning Kamp
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/timetc.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/watchdog.h>
#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/led/led.h>
#include <machine/pc/bios.h>

static unsigned	cba;
static unsigned	gpio;
static unsigned	geode_counter;

static struct cdev *led1, *led2, *led3;
static int 	led1b, led2b, led3b;

static void
led_func(void *ptr, int onoff)
{
	uint32_t u;
	int bit;

	bit = *(int *)ptr;
	if (bit < 0) {
		bit = -bit;
		onoff = !onoff;
	}

	u = inl(gpio + 4);
	if (onoff)
		u |= 1 << bit;
	else
		u &= ~(1 << bit);
	outl(gpio, u);
}


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

/*
 * The GEODE watchdog runs from a 32kHz frequency.  One period of that is
 * 31250 nanoseconds which we round down to 2^14 nanoseconds.  The watchdog
 * consists of a power-of-two prescaler and a 16 bit counter, so the math
 * is quite simple.  The max timeout is 14 + 16 + 13 = 2^43 nsec ~= 2h26m.
 */
static void
geode_watchdog(void *foo __unused, u_int cmd, int *error)
{
	u_int u, p, r;

	u = cmd & WD_INTERVAL;
	if (cmd && u >= 14 && u <= 43) {
		u -= 14;
		if (u > 16) {
			p = u - 16;
			u -= p;
		} else {
			p = 0;
		}
		if (u == 16)
			u = (1 << u) - 1;
		else
			u = 1 << u;
		r = inw(cba + 2) & 0xff00;
		outw(cba + 2, p | 0xf0 | r);
		outw(cba, u);
		*error = 0;
	} else {
		outw(cba, 0);
	}
}

static int
geode_probe(device_t self)
{

	if (pci_get_devid(self) == 0x0515100b) {
		if (geode_counter == 0) {
			/*
			 * The address of the CBA is written to this register
			 * by the bios, see p161 in data sheet.
			 */
			cba = pci_read_config(self, 0x64, 4);
			printf("Geode CBA@ 0x%x\n", cba);
			geode_counter = cba + 0x08;
			outl(cba + 0x0d, 2);
			printf("Geode rev: %02x %02x\n",
				inb(cba + 0x3c), inb(cba + 0x3d));
			tc_init(&geode_timecounter);
			EVENTHANDLER_REGISTER(watchdog_list, geode_watchdog,
			    NULL, 0);
		}
	} else if (pci_get_devid(self) == 0x0510100b) {
		gpio = pci_read_config(self, PCIR_BAR(0), 4);
		gpio &= ~0x1f;
		printf("Geode GPIO@ = %x\n", gpio);
		if (NULL != 
		    bios_string(0xf0000, 0xf0100, "Soekris Engineering", 0)) {
			printf("Soekris Engineering NET4801 platform\n");
			led1b = 20;
			led1 = led_create(led_func, &led1b, "error");
		} else if (NULL !=
		    bios_string(0xf9000, 0xf9000, "PC Engines WRAP.1C ", 0)) {
			printf("PC Engines WRAP.1C platfrom\n");
			led1b = -2;
			led2b = -3;
			led3b = -18;
			led1 = led_create(led_func, &led1b, "led1");
			led2 = led_create(led_func, &led2b, "led2");
			led3 = led_create(led_func, &led3b, "led3");
			/*
			 * Turn on first LED so we don't make people think
			 * their box just died.
			 */
			led_func(&led1b, 1);
		}
	}
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
