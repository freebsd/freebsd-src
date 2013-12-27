/*-
 * Copyright (c) 2011 Jakub Wojciech Klama <jceel@FreeBSD.org>
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bio.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/endian.h>
#include <sys/kernel.h>
#include <sys/kthread.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/queue.h>
#include <sys/resource.h>
#include <sys/rman.h>
#include <sys/time.h>
#include <sys/timetc.h>
#include <sys/watchdog.h>

#include <dev/spibus/spi.h>

#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include <arm/lpc/lpcreg.h>
#include <arm/lpc/lpcvar.h>

#include "spibus_if.h"

struct ssd1289_softc
{
	device_t		ss_dev;
};

static int ssd1289_probe(device_t);
static int ssd1289_attach(device_t);

static __inline void ssd1289_set_dc(struct ssd1289_softc *, int);
static __inline void ssd1289_spi_send(struct ssd1289_softc *, uint8_t *, int);
static void ssd1289_write_reg(struct ssd1289_softc *, uint16_t, uint16_t);

static struct ssd1289_softc *ssd1289_sc = NULL;

void ssd1289_configure(void);

static int
ssd1289_probe(device_t dev)
{
#if 0
	if (!ofw_bus_is_compatible(dev, "ssd1289"))
		return (ENXIO);
#endif

	device_set_desc(dev, "Solomon Systech SSD1289 LCD controller");
	return (BUS_PROBE_DEFAULT);
}

static int
ssd1289_attach(device_t dev)
{
	struct ssd1289_softc *sc = device_get_softc(dev);

	sc->ss_dev = dev;
	ssd1289_sc = sc;

	return (0);
}

void
ssd1289_configure(void)
{
	struct ssd1289_softc *sc = ssd1289_sc;

	/* XXX will be replaced with commented code */
        ssd1289_write_reg(sc,0x00,0x0001);
        DELAY(20);

        ssd1289_write_reg(sc,0x03,0xA2A4);
        ssd1289_write_reg(sc,0x0C,0x0004);
        ssd1289_write_reg(sc,0x0D,0x0308);
        ssd1289_write_reg(sc,0x0E,0x3000);
        DELAY(50);

        ssd1289_write_reg(sc,0x1E,0x00AF);
        ssd1289_write_reg(sc,0x01,0x2B3F);
        ssd1289_write_reg(sc,0x02,0x0600);
        ssd1289_write_reg(sc,0x10,0x0000);
        ssd1289_write_reg(sc,0x07,0x0233);
        ssd1289_write_reg(sc,0x0B,0x0039);
        ssd1289_write_reg(sc,0x0F,0x0000);
        DELAY(50);

        ssd1289_write_reg(sc,0x30,0x0707);
        ssd1289_write_reg(sc,0x31,0x0204);
        ssd1289_write_reg(sc,0x32,0x0204);
        ssd1289_write_reg(sc,0x33,0x0502);
        ssd1289_write_reg(sc,0x34,0x0507);
        ssd1289_write_reg(sc,0x35,0x0204);
        ssd1289_write_reg(sc,0x36,0x0204);
        ssd1289_write_reg(sc,0x37,0x0502);
        ssd1289_write_reg(sc,0x3A,0x0302);
        ssd1289_write_reg(sc,0x3B,0x0302);

        ssd1289_write_reg(sc,0x23,0x0000);
        ssd1289_write_reg(sc,0x24,0x0000);

        ssd1289_write_reg(sc,0x48,0x0000);
        ssd1289_write_reg(sc,0x49,0x013F);
        ssd1289_write_reg(sc,0x4A,0x0000);
        ssd1289_write_reg(sc,0x4B,0x0000);

        ssd1289_write_reg(sc,0x41,0x0000);
        ssd1289_write_reg(sc,0x42,0x0000);

        ssd1289_write_reg(sc,0x44,0xEF00);
        ssd1289_write_reg(sc,0x45,0x0000);
        ssd1289_write_reg(sc,0x46,0x013F);
        DELAY(50);

        ssd1289_write_reg(sc,0x44,0xEF00);
        ssd1289_write_reg(sc,0x45,0x0000);
        ssd1289_write_reg(sc,0x4E,0x0000);
        ssd1289_write_reg(sc,0x4F,0x0000);
        ssd1289_write_reg(sc,0x46,0x013F);
}

static __inline void
ssd1289_spi_send(struct ssd1289_softc *sc, uint8_t *data, int len)
{
	struct spi_command cmd;
	uint8_t buffer[8];
	cmd.tx_cmd = data;
	cmd.tx_cmd_sz = len;
	cmd.rx_cmd = buffer;
	cmd.rx_cmd_sz = len;
	cmd.tx_data_sz = 0;
	cmd.rx_data_sz = 0;
	SPIBUS_TRANSFER(device_get_parent(sc->ss_dev), sc->ss_dev, &cmd);
}

static __inline void
ssd1289_set_dc(struct ssd1289_softc *sc, int value)
{
	lpc_gpio_set_state(sc->ss_dev, SSD1289_DC_PIN, value);
}

static void
ssd1289_write_reg(struct ssd1289_softc *sc, uint16_t addr, uint16_t value)
{
	uint8_t buffer[2];

	ssd1289_set_dc(sc, 0);
	buffer[0] = 0x00;
	buffer[1] = addr & 0xff;
	ssd1289_spi_send(sc, buffer, 2);

	ssd1289_set_dc(sc, 1);
	buffer[0] = (value >> 8) & 0xff;
	buffer[1] = value & 0xff;
	ssd1289_spi_send(sc, buffer, 2);
}

static device_method_t ssd1289_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		ssd1289_probe),
	DEVMETHOD(device_attach,	ssd1289_attach),

	{ 0, 0 }
};

static devclass_t ssd1289_devclass;

static driver_t ssd1289_driver = {
	"ssd1289",
	ssd1289_methods,
	sizeof(struct ssd1289_softc),
};

DRIVER_MODULE(ssd1289, spibus, ssd1289_driver, ssd1289_devclass, 0, 0);
