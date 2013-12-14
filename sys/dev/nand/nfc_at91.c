/*-
 * Copyright (C) 2013 Ian Lepore.
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

/*
 * Atmel at91-family integrated NAND controller driver.
 *
 * This code relies on the board setup code (in at91/board_whatever.c) having
 * set up the EBI and SMC registers appropriately for whatever type of nand part
 * is on the board.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/malloc.h>
#include <sys/rman.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/time.h>

#include <machine/bus.h>

#include <dev/nand/nand.h>
#include <dev/nand/nandbus.h>
#include "nfc_if.h"

/*
 * Data cycles are triggered by access to any address within the EBI CS3 region
 * that has A21 and A22 clear.  Command cycles are any access with bit A21
 * asserted. Address cycles are any access with bit A22 asserted.
 *
 * XXX The atmel docs say that any address bits can be used instead of A21 and
 * A22; these values should be configurable.
 */
#define	AT91_NAND_DATA		0
#define	AT91_NAND_COMMAND	(1 << 21)
#define	AT91_NAND_ADDRESS	(1 << 22)

struct at91_nand_softc {
	struct nand_softc	nand_sc;
	struct resource		*res;
};

static int	at91_nand_attach(device_t);
static int	at91_nand_probe(device_t);
static uint8_t	at91_nand_read_byte(device_t);
static void	at91_nand_read_buf(device_t, void *, uint32_t);
static int	at91_nand_read_rnb(device_t);
static int	at91_nand_select_cs(device_t, uint8_t);
static int	at91_nand_send_command(device_t, uint8_t);
static int	at91_nand_send_address(device_t, uint8_t);
static void	at91_nand_write_buf(device_t, void *, uint32_t);

static inline u_int8_t
dev_read_1(struct at91_nand_softc *sc, bus_size_t offset)
{
	return bus_read_1(sc->res, offset);
}

static inline void
dev_write_1(struct at91_nand_softc *sc, bus_size_t offset, u_int8_t value)
{
	bus_write_1(sc->res, offset, value);
}

static int
at91_nand_probe(device_t dev)
{

	device_set_desc(dev, "AT91 Integrated NAND controller");
	return (BUS_PROBE_DEFAULT);
}

static int
at91_nand_attach(device_t dev)
{
	struct at91_nand_softc *sc;
	int err, rid;

	sc = device_get_softc(dev);
	rid = 0;
	sc->res = bus_alloc_resource_any(dev, SYS_RES_MEMORY, &rid,
	    RF_ACTIVE);
	if (sc->res == NULL) {
		device_printf(dev, "could not allocate resources!\n");
		return (ENXIO);
	}

	nand_init(&sc->nand_sc, dev, NAND_ECC_SOFT, 0, 0, NULL, NULL);

	err = nandbus_create(dev);

	return (err);
}

static int
at91_nand_send_command(device_t dev, uint8_t command)
{
	struct at91_nand_softc *sc;

        /* nand_debug(NDBG_DRV,"at91_nand_send_command: 0x%02x", command); */

	sc = device_get_softc(dev);
	dev_write_1(sc, AT91_NAND_COMMAND, command);
	return (0);
}

static int
at91_nand_send_address(device_t dev, uint8_t addr)
{
	struct at91_nand_softc *sc;

        /* nand_debug(NDBG_DRV,"at91_nand_send_address: x%02x", addr); */

	sc = device_get_softc(dev);
	dev_write_1(sc, AT91_NAND_ADDRESS, addr);
	return (0);
}

static uint8_t
at91_nand_read_byte(device_t dev)
{
	struct at91_nand_softc *sc;
	uint8_t data;

	sc = device_get_softc(dev);
	data = dev_read_1(sc, AT91_NAND_DATA);

        /* nand_debug(NDBG_DRV,"at91_nand_read_byte: 0x%02x", data); */

	return (data);
}


static void
at91_nand_dump_buf(const char *op, void* buf, uint32_t len)
{
	int i;
	uint8_t *b = buf;

	printf("at91_nand_%s_buf (hex):", op);
	for (i = 0; i < len; i++) {
		if ((i & 0x01f) == 0)
			printf("\n");
		printf(" %02x", b[i]);
	}
	printf("\n");
}

static void
at91_nand_read_buf(device_t dev, void* buf, uint32_t len)
{
	struct at91_nand_softc *sc;

	sc = device_get_softc(dev);

	bus_read_multi_1(sc->res, AT91_NAND_DATA, buf, len);

	if (nand_debug_flag & NDBG_DRV)
		at91_nand_dump_buf("read", buf, len);
}

static void
at91_nand_write_buf(device_t dev, void* buf, uint32_t len)
{
	struct at91_nand_softc *sc;

	sc = device_get_softc(dev);

	if (nand_debug_flag & NDBG_DRV)
		at91_nand_dump_buf("write", buf, len);

	bus_write_multi_1(sc->res, AT91_NAND_DATA, buf, len);
}

static int
at91_nand_select_cs(device_t dev, uint8_t cs)
{

	if (cs > 0)
		return (ENODEV);

	return (0);
}

static int
at91_nand_read_rnb(device_t dev)
{
#if 0
	/*
         * XXX There's no way for this code to know which GPIO pin (if any) is
         * attached to the chip's RNB line.  Not to worry, nothing calls this;
         * at higher layers, all the nand code uses status commands.
         */
	uint32_t bits;

	bits = at91_pio_gpio_get(AT91RM92_PIOD_BASE, AT91C_PIO_PD15);
	nand_debug(NDBG_DRV,"at91_nand: read_rnb: %#x", bits);
	return (bits != 0); /* ready */
#endif	
	panic("at91_nand_read_rnb() is not implemented\n");
	return (0);
}

static device_method_t at91_nand_methods[] = {
	DEVMETHOD(device_probe,		at91_nand_probe),
	DEVMETHOD(device_attach,	at91_nand_attach),

	DEVMETHOD(nfc_send_command,	at91_nand_send_command),
	DEVMETHOD(nfc_send_address,	at91_nand_send_address),
	DEVMETHOD(nfc_read_byte,	at91_nand_read_byte),
	DEVMETHOD(nfc_read_buf,		at91_nand_read_buf),
	DEVMETHOD(nfc_write_buf,	at91_nand_write_buf),
	DEVMETHOD(nfc_select_cs,	at91_nand_select_cs),
	DEVMETHOD(nfc_read_rnb,		at91_nand_read_rnb),

	DEVMETHOD_END
};

static driver_t at91_nand_driver = {
	"nand",
	at91_nand_methods,
	sizeof(struct at91_nand_softc),
};

static devclass_t at91_nand_devclass;
DRIVER_MODULE(at91_nand, atmelarm, at91_nand_driver, at91_nand_devclass, 0, 0);

