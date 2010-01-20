/*-
 * Copyright (c) 2003-2009 RMI Corporation
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
 * 3. Neither the name of RMI Corporation, nor the names of its contributors,
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
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
 * RMI_BSD */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

/*
 * I2C driver for the Palm-BK3220 I2C Host adapter on the RMI XLR.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/bus.h>
#include <sys/rman.h>


#include <dev/iicbus/iiconf.h>
#include <dev/iicbus/iicbus.h>

#include <mips/rmi/iomap.h>
#include <mips/include/resource.h>

#include "iicbus_if.h"

#define DEVTOIICBUS(dev) ((struct iicbus_device*)device_get_ivars(dev))

#define I2C_PALM_CFG            0x00
#define I2C_PALM_CLKDIV         0x01
#define I2C_PALM_DEVADDR        0x02
#define I2C_PALM_ADDR           0x03
#define I2C_PALM_DATAOUT        0x04
#define I2C_PALM_DATAIN         0x05
#define I2C_PALM_STATUS         0x06
#define I2C_PALM_STARTXFR       0x07
#define I2C_PALM_BYTECNT        0x08
#define I2C_PALM_HDSTATIM       0x09

/* TEST Values!! Change as required */
#define I2C_PALM_CFG_DEF        0x000000F8	/* 8-Bit Addr + POR Values */
#define I2C_PALM_CLKDIV_DEF     0x14A //0x00000052
#define I2C_PALM_HDSTATIM_DEF       0x107 //0x00000000

#define I2C_PALM_STARTXFR_RD        0x00000001
#define I2C_PALM_STARTXFR_WR        0x00000000


#define PHOENIX_IO_I2C_0_OFFSET           0x16000
#define PHOENIX_IO_I2C_1_OFFSET           0x17000

#define ARIZONA_I2c_BUS 1

int bus = 1;


uint8_t current_slave;
uint8_t read_address;
static xlr_reg_t *iobase_i2c_regs;

static devclass_t xlr_i2c_devclass;

/*
 * Device methods
 */
static int xlr_i2c_probe(device_t);
static int xlr_i2c_attach(device_t);
static int xlr_i2c_detach(device_t);

static int xlr_i2c_start(device_t dev, u_char slave, int timeout);
static int xlr_i2c_stop(device_t dev);
static int xlr_i2c_read(device_t dev, char *buf, int len, int *read, int last, int delay);
static int xlr_i2c_write(device_t dev, char *buf, int len, int *sent, int timeout);


struct xlr_i2c_softc {
	device_t dev;		/* Myself */
	struct resource *mem_res;	/* Memory resource */
	volatile int flags;
#define RXRDY       4
#define TXRDY       0x10
	int sc_started;
	int twi_addr;
	device_t iicbus;
};


#define MDELAY(a){ \
    unsigned long  local_loop = 0xfffff; \
    while(local_loop--); \
}\

static void 
get_i2c_base(void)
{
	if (bus == 0)
		iobase_i2c_regs = xlr_io_mmio(PHOENIX_IO_I2C_0_OFFSET);
	else
		iobase_i2c_regs = xlr_io_mmio(PHOENIX_IO_I2C_1_OFFSET);
	return;
}

static void 
palm_write(int reg, int value)
{
	get_i2c_base();
	xlr_write_reg(iobase_i2c_regs, reg, value);
	return;
}


static int 
palm_read(int reg)
{
	uint32_t val;

	get_i2c_base();
	val = xlr_read_reg(iobase_i2c_regs, reg);
	return ((int)val);
}

static int 
palm_addr_only(uint8_t addr, uint8_t offset)
{
	volatile uint32_t regVal = 0x00;

	palm_write(I2C_PALM_ADDR, offset);
	palm_write(I2C_PALM_DEVADDR, addr);
	palm_write(I2C_PALM_CFG, 0xfa);
	palm_write(I2C_PALM_STARTXFR, 0x02);
	regVal = palm_read(I2C_PALM_STATUS);
	if (regVal & 0x0008) {
		printf("palm_addr_only: ACKERR. Aborting...\n");
		return -1;
	}
	return 0;
}


static int 
palm_rx(uint8_t addr, uint8_t offset, uint8_t len,
    uint8_t * buf)
{
	volatile uint32_t regVal = 0x00, ctr = 0x00;
	int timeOut, numBytes = 0x00;

	palm_write(I2C_PALM_CFG, 0xfa);
	palm_write(I2C_PALM_BYTECNT, len);
	palm_write(I2C_PALM_DEVADDR, addr);
	//DEVADDR = 0x4c, 0x68
	    MDELAY(1);

	for (numBytes = 0x00; numBytes < len; numBytes++) {
		palm_write(I2C_PALM_ADDR, offset + numBytes);
//I2C_PALM_ADDR:offset
		    MDELAY(1);
		if (!ctr) {
			/* Trigger a READ Transaction */
			palm_write(I2C_PALM_STARTXFR, I2C_PALM_STARTXFR_RD);
			ctr++;
		}
		/* Error Conditions [Begin] */
		regVal = palm_read(I2C_PALM_STATUS);
		MDELAY(1);
		if (regVal & 0x0008) {
			printf("palm_rx: ACKERR. Aborting...\n");
			return -1;
		}
		timeOut = 10;
		while ((regVal & 0x0030) && timeOut--) {
			palm_write(I2C_PALM_STARTXFR, I2C_PALM_STARTXFR_RD);
			regVal = palm_read(I2C_PALM_STATUS);
		}
		if (timeOut == 0x00) {
			printf("palm_rx: TimedOut on Valid STARTXFR/Arbitration\n");
			return -1;
		}
		timeOut = 10;
		/* Do we have valid data from the device yet..? */
		regVal &= 0x0004;
		while (!regVal && timeOut--) {
			regVal = palm_read(I2C_PALM_STATUS) & 0x0004;
		}
		if (timeOut == 0x00) {
			printf("palm_rx: TimedOut Waiting for Valid Data\n");
			return -1;
		}
		/* Error Conditions [End] */
		/* Read the data */
		buf[numBytes] = (uint8_t) palm_read(I2C_PALM_DATAIN);
	}
	return 0;
}



static int 
wait_for_idle(void)
{
	int timeOut = 0x1000;
	volatile uint32_t regVal = 0x00;

	regVal = palm_read(I2C_PALM_STATUS) & 0x0001;
	while (regVal && timeOut--) {
		regVal = palm_read(I2C_PALM_STATUS) & 0x0001;
	}
	if (timeOut == 0x00)
		return -1;	/* Timed Out */
	else
		return 0;
}


static int 
palm_tx(uint8_t addr, uint8_t offset, uint8_t * buf, uint8_t len)
{
	volatile uint32_t regVal = 0x00;
	int timeOut, ctr = 0x00, numBytes = len;

	for (ctr = 0x00; ctr < len; ctr++) {
		if (wait_for_idle() < 0) {
			printf("TimedOut on Waiting for I2C Bus Idle.\n");
			return -EIO;
		}
		palm_write(I2C_PALM_CFG, 0xF8);
		palm_write(I2C_PALM_BYTECNT, 0x00);
		palm_write(I2C_PALM_DEVADDR, addr);
		//0x4c, 0x68
		    palm_write(I2C_PALM_ADDR, offset + numBytes - 1);
		//offset
		    palm_write(I2C_PALM_DATAOUT, buf[ctr]);
		palm_write(I2C_PALM_STARTXFR, I2C_PALM_STARTXFR_WR);
		MDELAY(1);

		regVal = palm_read(I2C_PALM_STATUS);
		MDELAY(1);
		if (regVal & 0x0008) {
			printf("palm_tx: ACKERR. Aborting...\n");
			return -1;
		}
		timeOut = 0x1000;
		while (!(regVal & 0x0002) && timeOut) {
			regVal = palm_read(I2C_PALM_STATUS);
			timeOut--;
		}
		if (timeOut == 0x00) {
			printf("palm_tx: [TimeOut] SDOEMPTY Not Set\n");
			return -1;
		}
		timeOut = 1000;
		while ((regVal & 0x0030) && timeOut) {
			palm_write(I2C_PALM_STARTXFR, I2C_PALM_STARTXFR_WR);
			regVal = palm_read(I2C_PALM_STATUS);
			timeOut--;
		}
		if (timeOut == 0x00) {
			printf("palm_rx: TimedOut on Valid STARTXFR/Arbitration\n");
			return -1;
		}
		numBytes--;
	}
	return 0;
}





static int
xlr_i2c_probe(device_t dev)
{
	device_set_desc(dev, "I2C bus controller");

	return (0);
}


/*
 * We add all the devices which we know about.
 * The generic attach routine will attach them if they are alive.
 */
static int
xlr_i2c_attach(device_t dev)
{
	struct xlr_i2c_softc *sc;
	int rid;

	sc = device_get_softc(dev);
	sc->mem_res = bus_alloc_resource_any(dev, SYS_RES_MEMORY, &rid,
	    RF_ACTIVE);
	if (sc->mem_res == NULL) {
		printf("not able to allocate the bus resource\n");
	}
	if ((sc->iicbus = device_add_child(dev, "iicbus", -1)) == NULL)
		printf("could not allocate iicbus instance\n");

	bus_generic_attach(dev);

	return (0);
}

static int
xlr_i2c_detach(device_t dev)
{
	bus_generic_detach(dev);

	return (0);
}

/*
static int
xlr_i2c_add_child(device_t dev, int order, const char *name, int unit)
{
    printf("********* %s ********  \n", __FUNCTION__);
	device_add_child_ordered(dev, order, name, unit);

	bus_generic_attach(dev);

	return (0);
}
*/

static int 
xlr_i2c_start(device_t dev, u_char slave, int timeout)
{
	int error = 0;
	struct xlr_i2c_softc *sc;

	sc = device_get_softc(dev);
	sc->sc_started = 1;

	current_slave = (slave >> 1);
	return error;

}

static int 
xlr_i2c_stop(device_t dev)
{
	int error = 0;

	return error;

}

static int 
xlr_i2c_read(device_t dev, char *buf, int len, int *read, int last,
    int delay)
{
	int error = 0;

	if (palm_addr_only(current_slave, read_address) == -1) {
		printf("I2C ADDRONLY Phase Fail.\n");
		return -1;
	}
	if (palm_rx(current_slave, read_address, len, buf) == -1) {
		printf("I2C Read Fail.\n");
		return -1;
	}
	*read = len;
	return error;

}


static int 
xlr_i2c_write(device_t dev, char *buf, int len, int *sent, int timeout /* us */ )
{

	int error = 0;
	uint8_t write_address;

	if (len == 1) {
		/* address for the next read */
		read_address = buf[0];
		return error;
	}
	if (len < 2)
		return (-1);

	write_address = buf[0];

	/*
	 * for write operation, buf[0] contains the register offset and
	 * buf[1] onwards contains the value
	 */
	palm_tx(current_slave, write_address, &buf[1], len - 1);

	return error;

}

static int
xlr_i2c_callback(device_t dev, int index, caddr_t *data)
{
	return 0;
}

static int
xlr_i2c_repeated_start(device_t dev, u_char slave, int timeout)
{
	return 0;
}


static device_method_t xlr_i2c_methods[] = {
	/* device interface */
	DEVMETHOD(device_probe, xlr_i2c_probe),
	DEVMETHOD(device_attach, xlr_i2c_attach),
	DEVMETHOD(device_detach, xlr_i2c_detach),

	/* iicbus interface */
	DEVMETHOD(iicbus_callback, xlr_i2c_callback),
	DEVMETHOD(iicbus_repeated_start, xlr_i2c_repeated_start),
	DEVMETHOD(iicbus_start, xlr_i2c_start),
	DEVMETHOD(iicbus_stop, xlr_i2c_stop),
	DEVMETHOD(iicbus_write, xlr_i2c_write),
	DEVMETHOD(iicbus_read, xlr_i2c_read),
	{0, 0}
};

static driver_t xlr_i2c_driver = {
	"xlr_i2c",
	xlr_i2c_methods,
	sizeof(struct xlr_i2c_softc),
};

DRIVER_MODULE(xlr_i2c, iodi, xlr_i2c_driver, xlr_i2c_devclass, 0, 0);
