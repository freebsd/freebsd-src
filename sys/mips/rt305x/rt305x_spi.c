/*-
 * Copyright (c) 2009, Oleksandr Tymoshenko <gonzo@FreeBSD.org>
 * Copyright (c) 2011, Aleksandr Rybalko <ray@FreeBSD.org>
 * Copyright (c) 2013, Alexander A. Mityaev <sansan@adm.ua>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
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
#include <sys/bus.h>

#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/rman.h>
#include <sys/lock.h>
#include <sys/mutex.h>

#include <machine/bus.h>
#include <machine/cpu.h>
//#include <machine/pmap.h>

#include <dev/spibus/spi.h>
#include <dev/spibus/spibusvar.h>
#include "spibus_if.h"

#include "opt_platform.h"

#ifdef FDT
#include <dev/ofw/openfirm.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>
#endif

#include <mips/rt305x/rt305xreg.h>
#include <dev/flash/mx25lreg.h>

#undef RT305X_SPI_DEBUG
#ifdef RT305X_SPI_DEBUG
#define dprintf printf
#else
#define dprintf(x, arg...)
#endif

/*
 * register space access macros
 */
#define SPI_WRITE(sc, reg, val)	do {	\
		bus_write_4(sc->sc_mem_res, (reg), (val)); \
	} while (0)

#define SPI_READ(sc, reg)	 bus_read_4(sc->sc_mem_res, (reg))

#define SPI_SET_BITS(sc, reg, bits)	\
	SPI_WRITE(sc, reg, SPI_READ(sc, (reg)) | (bits))

#define SPI_CLEAR_BITS(sc, reg, bits)	\
	SPI_WRITE(sc, reg, SPI_READ(sc, (reg)) & ~(bits))

struct rt305x_spi_softc {
	device_t		sc_dev;
	struct resource		*sc_mem_res;
};

static int	rt305x_spi_probe(device_t);
static int	rt305x_spi_attach(device_t);
static int	rt305x_spi_detach(device_t);
static int	rt305x_spi_wait(struct rt305x_spi_softc *);
static void	rt305x_spi_chip_activate(struct rt305x_spi_softc *);
static void	rt305x_spi_chip_deactivate(struct rt305x_spi_softc *);
static uint8_t	rt305x_spi_txrx(struct rt305x_spi_softc *, uint8_t *, int);
static int	rt305x_spi_transfer(device_t, device_t, struct spi_command *);
#ifdef FDT
static phandle_t rt305x_spi_get_node(device_t, device_t);
#endif

static int
rt305x_spi_probe(device_t dev)
{
#ifdef FDT
        if (!ofw_bus_is_compatible(dev, "ralink,rt305x-spi"))
            return(ENXIO);
#endif
	device_set_desc(dev, "RT305X SPI");
	return (0);
}

static int
rt305x_spi_attach(device_t dev)
{
	struct rt305x_spi_softc *sc = device_get_softc(dev);
	int rid;

	sc->sc_dev = dev;
        rid = 0;
	sc->sc_mem_res = bus_alloc_resource_any(dev, SYS_RES_MEMORY, &rid,
	    RF_ACTIVE);
	if (!sc->sc_mem_res) {
		device_printf(dev, "Could not map memory\n");
		return (ENXIO);
	}

	if (rt305x_spi_wait(sc)) {
		bus_release_resource(dev, SYS_RES_MEMORY, 0, sc->sc_mem_res);
		return (EBUSY);
	}

	SPI_WRITE(sc, RT305X_SPICFG, MSBFIRST | SPICLKPOL | TX_ON_CLK_FALL |
	    SPI_CLK_DIV8); /* XXX: make it configurable */
	    /*
	     * W25Q64CV max 104MHz, bus 120-192 MHz, so divide by 2.
	     * Update: divide by 4, DEV2 to fast for flash.
	     */

	device_add_child(dev, "spibus", 0);
	return (bus_generic_attach(dev));
}

static int
rt305x_spi_detach(device_t dev)
{
	struct rt305x_spi_softc *sc = device_get_softc(dev);

	SPI_SET_BITS(sc, RT305X_SPICTL, HIZSMOSI | CS_HIGH);

	if (sc->sc_mem_res)
		bus_release_resource(dev, SYS_RES_MEMORY, 0, sc->sc_mem_res);

	return (0);
}

static void
rt305x_spi_chip_activate(struct rt305x_spi_softc *sc)
{
//        printf("%s\n", __func__);
        rt305x_spi_wait(sc);
	/*
	 * Put all CSx to low
	 */
	SPI_CLEAR_BITS(sc, RT305X_SPICTL, CS_HIGH | HIZSMOSI);
}

static void
rt305x_spi_chip_deactivate(struct rt305x_spi_softc *sc)
{
//        printf("%s\n", __func__);
        rt305x_spi_wait(sc);
	/*
	 * Put all CSx to high
	 */
	SPI_SET_BITS(sc, RT305X_SPICTL, CS_HIGH | HIZSMOSI);
}

static int
rt305x_spi_wait(struct rt305x_spi_softc *sc)
{
	int i = 1000;

	while (i--) {
		if (!SPI_READ(sc, RT305X_SPIBUSY))
			break;
	}
	if (i == 0) {
		printf("busy\n");
		return (1);
	}

	return (0);
}

static uint8_t
rt305x_spi_txrx(struct rt305x_spi_softc *sc, uint8_t *data, int write)
{

	if (rt305x_spi_wait(sc))
		return (EBUSY);

	if (write == RT305X_SPI_WRITE) {
		SPI_WRITE(sc, RT305X_SPIDATA, *data);
		SPI_SET_BITS(sc, RT305X_SPICTL, START_WRITE);
		//printf("%s(W:%d)\n", __func__, *data);
	} else {/* RT305X_SPI_READ */
		SPI_SET_BITS(sc, RT305X_SPICTL, START_READ);
		if (rt305x_spi_wait(sc))
			return (EBUSY);

		*data = SPI_READ(sc, RT305X_SPIDATA) & 0xff;
		//printf("%s(R:%d)\n", __func__, *data);
	}
	return (0);
}

static int
rt305x_spi_transfer(device_t dev, device_t child, struct spi_command *cmd)
{
	struct rt305x_spi_softc *sc;
	uint8_t *buf, byte, *tx_buf;
	struct spibus_ivar *devi = SPIBUS_IVAR(child);
	int i, sz, error = 0, write = 0;

	sc = device_get_softc(dev);

	if (devi->cs != 0)
		/* Only 1 CS */
		return (ENXIO);

        /* There is always a command to transfer. */
        tx_buf = (uint8_t *)(cmd->tx_cmd);
        
        /* Perform some fixup because RT305X dont support duplex SPI */
        switch(tx_buf[0]) {
                case CMD_READ_IDENT:
                        cmd->tx_cmd_sz = 1;
                        cmd->rx_cmd_sz = 3;
                        break;
                case CMD_WRITE_ENABLE:
                case CMD_WRITE_DISABLE:
                        cmd->tx_cmd_sz = 1;
                        cmd->rx_cmd_sz = 0;
                        break;
                case CMD_READ_STATUS:
                        cmd->tx_cmd_sz = 1;
                        cmd->rx_cmd_sz = 1;
                        break;
                case CMD_READ:
                        cmd->tx_cmd_sz = 4;
                case CMD_FAST_READ:
                        cmd->tx_cmd_sz = 5;
                        cmd->rx_cmd_sz = cmd->tx_data_sz = 0;
                        break;
                case CMD_SECTOR_ERASE:
                        cmd->tx_cmd_sz = 4;
                        cmd->rx_cmd_sz = cmd->tx_data_sz = 0;
                        break;
                case CMD_PAGE_PROGRAM:
                        cmd->tx_cmd_sz = 4;
                        cmd->rx_cmd_sz = cmd->rx_data_sz = 0;
                        break;
        }      
        
	rt305x_spi_chip_activate(sc);

	if (cmd->tx_cmd_sz + cmd->rx_cmd_sz) {
		buf = (uint8_t *)(cmd->rx_cmd);
		tx_buf = (uint8_t *)(cmd->tx_cmd);
		sz = cmd->tx_cmd_sz + cmd->rx_cmd_sz;

		for (i = 0; i < sz; i++) {
                        if(i < cmd->tx_cmd_sz) {
			        byte = tx_buf[i];
        			error = rt305x_spi_txrx(sc, &byte,
		        	    RT305X_SPI_WRITE);
        			if (error)
        				goto rt305x_spi_transfer_fail;
                                continue;
                        }
                        error = rt305x_spi_txrx(sc, &byte,
		            RT305X_SPI_READ);
        		if (error)
        			goto rt305x_spi_transfer_fail;
			buf[i] = byte;
		}
	}
	
	/*
	 * Transfer/Receive data
	 */
	
	if (cmd->tx_data_sz + cmd->rx_data_sz) {
		write = (cmd->tx_data_sz > 0)?1:0;
		buf = (uint8_t *)(write ? cmd->tx_data : cmd->rx_data);
		sz = write ? cmd->tx_data_sz : cmd->rx_data_sz;

		for (i = 0; i < sz; i++) {
			byte = buf[i];
			error = rt305x_spi_txrx(sc, &byte,
			    write?RT305X_SPI_WRITE:RT305X_SPI_READ);
			if (error)
				goto rt305x_spi_transfer_fail;
			buf[i] = byte;
		}
	}
rt305x_spi_transfer_fail:
	rt305x_spi_chip_deactivate(sc);

	return (error);
}

#ifdef FDT
static phandle_t
rt305x_spi_get_node(device_t bus, device_t dev)
{

	/* We only have one child, the SPI bus, which needs our own node. */
	return (ofw_bus_get_node(bus));
}
#endif

static device_method_t rt305x_spi_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		rt305x_spi_probe),
	DEVMETHOD(device_attach,	rt305x_spi_attach),
	DEVMETHOD(device_detach,	rt305x_spi_detach),

	DEVMETHOD(spibus_transfer,	rt305x_spi_transfer),

#ifdef FDT
	/* ofw_bus interface */
	DEVMETHOD(ofw_bus_get_node,	rt305x_spi_get_node),
#endif

	DEVMETHOD_END
};

static driver_t rt305x_spi_driver = {
	.name = "spi",
	.methods = rt305x_spi_methods,
	.size = sizeof(struct rt305x_spi_softc),
};

static devclass_t rt305x_spi_devclass;

DRIVER_MODULE(rt305x_spi, obio, rt305x_spi_driver, rt305x_spi_devclass, 0, 0);
#ifdef FDT
DRIVER_MODULE(rt305x_spi, simplebus, rt305x_spi_driver, rt305x_spi_devclass, 0, 0);
#endif
