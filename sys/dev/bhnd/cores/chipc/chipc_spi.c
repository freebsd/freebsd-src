/*-
 * Copyright (c) 2016 Michael Zhilin <mizhka@gmail.com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer,
 *    without modification.
 * 2. Redistributions in binary form must reproduce at minimum a disclaimer
 *    similar to the "NO WARRANTY" disclaimer below ("Disclaimer") and any
 *    redistribution must be conditioned upon including a substantially
 *    similar Disclaimer requirement for further binary redistribution.
 *
 * NO WARRANTY
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF NONINFRINGEMENT, MERCHANTIBILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
 * THE COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
 * IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGES.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/errno.h>
#include <sys/rman.h>
#include <sys/bus.h>

#include <machine/bus.h>

#include <dev/bhnd/bhndvar.h>
/*
 * SPI BUS interface
 */
#include <dev/spibus/spi.h>

#include "spibus_if.h"

#include "chipcreg.h"
#include "chipcvar.h"
#include "chipc_spi.h"
#include "bhnd_chipc_if.h"

/*
 * Flash slicer
 */
#include "chipc_slicer.h"

/*
 * **************************** PROTOTYPES ****************************
 */

static void	chipc_spi_identify(driver_t *driver, device_t parent);
static int	chipc_spi_probe(device_t dev);
static int	chipc_spi_attach(device_t dev);
static int	chipc_spi_transfer(device_t dev, device_t child,
		    struct spi_command *cmd);
static int	chipc_spi_txrx(struct chipc_spi_softc *sc, uint8_t in,
		    uint8_t* out);
static int	chipc_spi_wait(struct chipc_spi_softc *sc);

/*
 * **************************** IMPLEMENTATION ************************
 */

static void
chipc_spi_identify(driver_t *driver, device_t parent)
{
	struct chipc_caps	*caps;
	device_t	 	 spidev;
	device_t	 	 spibus;
	device_t 		 flash;
	char*		 	 flash_name;
	int		 	 err;

	flash_name = NULL;

	if (device_find_child(parent, "spi", -1) != NULL)
		return;

	caps = BHND_CHIPC_GET_CAPS(parent);
	if (caps == NULL) {
		BHND_ERROR_DEV(parent, "can't retrieve ChipCommon capabilities");
		return;
	}

	switch (caps->flash_type) {
	case CHIPC_SFLASH_AT:
		flash_name = "at45d";
		break;
	case CHIPC_SFLASH_ST:
		flash_name = "mx25l";
		break;
	default:
		return;
	}

	spidev = BUS_ADD_CHILD(parent, 0, "spi", -1);
	if (spidev == NULL) {
		BHND_ERROR_DEV(parent, "can't add chipc_spi to ChipCommon");
		return;
	}

	err = device_probe_and_attach(spidev);
	if (err) {
		BHND_ERROR_DEV(spidev, "failed attach chipc_spi: %d", err);
		return;
	}

	spibus = device_find_child(spidev, "spibus", -1);
	if (spibus == NULL) {
		BHND_ERROR_DEV(spidev, "can't find spibus under chipc_spi");
		return;
	}

	flash = BUS_ADD_CHILD(spibus, 0, flash_name, -1);
	if (flash == NULL) {
		BHND_ERROR_DEV(spibus, "can't add %s to spibus", flash_name);
		return;
	}

	err = device_probe_and_attach(flash);
	if (err)
		BHND_ERROR_DEV(flash, "failed attach flash %s: %d", flash_name,
		    err);

	return;
}

static int
chipc_spi_probe(device_t dev)
{
	device_set_desc(dev, "ChipCommon SPI");
	return (BUS_PROBE_DEFAULT);
}

struct resource_spec	spec_mem[] = {
		{SYS_RES_MEMORY, 0, RF_ACTIVE},
		{SYS_RES_MEMORY, 1, RF_ACTIVE},
		{ -1, -1, 0 }
	};

static int
chipc_spi_attach(device_t dev)
{
	int 			 err;
	struct chipc_spi_softc	*sc;
	struct resource		*mem[2];

	sc = device_get_softc(dev);
	err = bus_alloc_resources(dev, spec_mem, mem);
	if (err != 0)
		return (ENXIO);

	sc->sc_res = mem[0];
	sc->sc_mem_res = mem[1];

	flash_register_slicer(chipc_slicer_spi);
	device_add_child(dev, "spibus", 0);
	return (bus_generic_attach(dev));
}

static int
chipc_spi_wait(struct chipc_spi_softc *sc)
{
	int i;

	for (i = CHIPC_SPI_MAXTRIES; i > 0; i--)
		if (!(SPI_READ(sc, CHIPC_SPI_FLASHCTL) & CHIPC_SPI_FLASHCTL_START))
			break;

	if (i > 0)
		return (0);

	BHND_DEBUG_DEV(sc->dev, "busy");
	return (-1);
}

static int
chipc_spi_txrx(struct chipc_spi_softc *sc, uint8_t out, uint8_t* in)
{
	uint32_t ctl;

	ctl = CHIPC_SPI_FLASHCTL_START | CHIPC_SPI_FLASHCTL_CSACTIVE | out;
	SPI_BARRIER_WRITE(sc);
	SPI_WRITE(sc, CHIPC_SPI_FLASHCTL, ctl);
	SPI_BARRIER_WRITE(sc);

	if (chipc_spi_wait(sc))
		return (-1);

	*in = SPI_READ(sc, CHIPC_SPI_FLASHDATA) & 0xff;
	return (0);
}

static int
chipc_spi_transfer(device_t dev, device_t child, struct spi_command *cmd)
{
	struct chipc_spi_softc	*sc;
	uint8_t		*buf_in;
	uint8_t		*buf_out;
	int		 i;

	sc = device_get_softc(dev);
	KASSERT(cmd->tx_cmd_sz == cmd->rx_cmd_sz,
	    ("TX/RX command sizes should be equal"));
	KASSERT(cmd->tx_data_sz == cmd->rx_data_sz,
	    ("TX/RX data sizes should be equal"));

	if (cmd->tx_cmd_sz == 0) {
		BHND_DEBUG_DEV(child, "size of command is ZERO");
		return (EIO);
	}

	SPI_BARRIER_WRITE(sc);
	SPI_WRITE(sc, CHIPC_SPI_FLASHADDR, 0);
	SPI_BARRIER_WRITE(sc);

	/*
	 * Transfer command
	 */
	buf_out = (uint8_t *)cmd->tx_cmd;
	buf_in = (uint8_t *)cmd->rx_cmd;
	for (i = 0; i < cmd->tx_cmd_sz; i++)
		 if (chipc_spi_txrx(sc, buf_out[i], &(buf_in[i])))
			 return (EIO);

	/*
	 * Receive/transmit data
	 */
	buf_out = (uint8_t *)cmd->tx_data;
	buf_in = (uint8_t *)cmd->rx_data;
	for (i = 0; i < cmd->tx_data_sz; i++)
		if (chipc_spi_txrx(sc, buf_out[i], &(buf_in[i])))
			return (EIO);

	/*
	 * Clear CS bit and whole control register
	 */
	SPI_BARRIER_WRITE(sc);
	SPI_WRITE(sc, CHIPC_SPI_FLASHCTL, 0);
	SPI_BARRIER_WRITE(sc);

	return (0);
}

/*
 * **************************** METADATA ************************
 */
static device_method_t chipc_spi_methods[] = {
		DEVMETHOD(device_identify,	chipc_spi_identify),
		DEVMETHOD(device_probe,		chipc_spi_probe),
		DEVMETHOD(device_attach,	chipc_spi_attach),
		/* SPI */
		DEVMETHOD(spibus_transfer,	chipc_spi_transfer),
		DEVMETHOD_END
};

static driver_t chipc_spi_driver = {
	"spi",
	chipc_spi_methods,
	sizeof(struct chipc_spi_softc),
};

static devclass_t chipc_spi_devclass;

DRIVER_MODULE(chipc_spi, bhnd_chipc, chipc_spi_driver, chipc_spi_devclass,
		0, 0);
