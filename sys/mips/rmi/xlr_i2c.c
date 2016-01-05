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
#include <sys/lock.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/bus.h>
#include <sys/rman.h>


#include <dev/iicbus/iiconf.h>
#include <dev/iicbus/iicbus.h>

#include <mips/rmi/board.h>
#include <mips/rmi/iomap.h>
#include <mips/include/resource.h>

#include "iicbus_if.h"

/* XLR I2C REGISTERS */
#define XLR_I2C_CFG            0x00
#define XLR_I2C_CLKDIV         0x01
#define XLR_I2C_DEVADDR        0x02
#define XLR_I2C_ADDR           0x03
#define XLR_I2C_DATAOUT        0x04
#define XLR_I2C_DATAIN         0x05
#define XLR_I2C_STATUS         0x06
#define XLR_I2C_STARTXFR       0x07
#define XLR_I2C_BYTECNT        0x08
#define XLR_I2C_HDSTATIM       0x09

/* XLR I2C REGISTERS FLAGS */
#define XLR_I2C_BUS_BUSY	0x01
#define XLR_I2C_SDOEMPTY	0x02
#define XLR_I2C_RXRDY       	0x04
#define XLR_I2C_ACK_ERR		0x08
#define XLR_I2C_ARB_STARTERR	0x30

/* Register Programming Values!! Change as required */
#define XLR_I2C_CFG_ADDR	0xF8	/* 8-Bit dev Addr + POR Values */
#define XLR_I2C_CFG_NOADDR	0xFA	/* 8-Bit reg Addr + POR Values  : No dev addr */
#define XLR_I2C_STARTXFR_ND	0x02	/* No data , only addr */
#define XLR_I2C_STARTXFR_RD	0x01	/* Read */
#define XLR_I2C_STARTXFR_WR	0x00	/* Write */
#define XLR_I2C_CLKDIV_DEF	0x14A	/* 0x00000052 */
#define XLR_I2C_HDSTATIM_DEF	0x107	/* 0x00000000 */

#define MAXTIME 0x10000
#define ARIZONA_I2C_BUS 1

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
static int xlr_i2c_write(device_t dev, const char *buf, int len, int *sent, int timeout);
static int xlr_i2c_callback(device_t dev, int index, caddr_t data);
static int xlr_i2c_repeated_start(device_t dev, u_char slave, int timeout);
static int xlr_i2c_transfer(device_t bus, struct iic_msg *msgs, uint32_t nmsgs);

struct xlr_i2c_softc {
	device_t dev;		/* Self */
	struct resource *mem_res;	/* Memory resource */
	volatile int flags;
	int sc_started;
	uint8_t i2cdev_addr;
	xlr_reg_t *iobase_i2c_regs;
	device_t iicbus;
	struct mtx sc_mtx;
};

static void  
set_i2c_base(device_t dev)
{
	struct xlr_i2c_softc *sc;

	sc = device_get_softc(dev);
	if (device_get_unit(dev) == 0)
		sc->iobase_i2c_regs = xlr_io_mmio(XLR_IO_I2C_0_OFFSET);
	else
		sc->iobase_i2c_regs = xlr_io_mmio(XLR_IO_I2C_1_OFFSET);
}

static void 
xlr_i2c_dev_write(device_t dev, int reg, int value)
{
	struct xlr_i2c_softc *sc;

	sc = device_get_softc(dev);
	xlr_write_reg(sc->iobase_i2c_regs, reg, value);
	return;
}


static int 
xlr_i2c_dev_read(device_t dev, int reg)
{
	uint32_t val;
	struct xlr_i2c_softc *sc;

	sc = device_get_softc(dev);
	val = xlr_read_reg(sc->iobase_i2c_regs, reg);
	return ((int)val);
}


static int
xlr_i2c_probe(device_t dev)
{
	device_set_desc(dev, "XLR/XLS I2C bus controller");

	return (0);
}


/*
 * We add all the devices which we know about.
 * The generic attach routine will attach them if they are alive.
 */
static int
xlr_i2c_attach(device_t dev)
{
	int rid;
	struct xlr_i2c_softc *sc;
	device_t tmpd;

	if(device_get_unit(dev)!=ARIZONA_I2C_BUS) {
		device_printf(dev, "unused iicbus instance\n");
		return 0;
	}

	sc = device_get_softc(dev);
	set_i2c_base(dev);

	mtx_init(&sc->sc_mtx, "xlr_i2c", "xlr_i2c", MTX_DEF);

	sc->mem_res = bus_alloc_resource_any(dev, SYS_RES_MEMORY, &rid, RF_ACTIVE);
	if (sc->mem_res == NULL) {
		printf("not able to allocate the bus resource\n");
	}
	if ((sc->iicbus = device_add_child(dev, "iicbus", -1)) == NULL) {
		printf("could not allocate iicbus instance\n");
		return -1;
	}
	if(xlr_board_info.xlr_i2c_device[I2C_RTC].enabled == 1) {
		tmpd = device_add_child(sc->iicbus, "ds1374_rtc", 0);
		device_set_ivars(tmpd, &xlr_board_info.xlr_i2c_device[I2C_RTC]);
	}
	if(xlr_board_info.xlr_i2c_device[I2C_THERMAL].enabled == 1) {
		tmpd = device_add_child(sc->iicbus, "max6657", 0);
		device_set_ivars(tmpd, &xlr_board_info.xlr_i2c_device[I2C_THERMAL]);
	}
	if(xlr_board_info.xlr_i2c_device[I2C_EEPROM].enabled == 1) {
		tmpd = device_add_child(sc->iicbus, "at24co2n", 0);
		device_set_ivars(tmpd, &xlr_board_info.xlr_i2c_device[I2C_EEPROM]);
	}

	bus_generic_attach(dev);

	return (0);
}

static int
xlr_i2c_detach(device_t dev)
{
	bus_generic_detach(dev);

	return (0);
}

static int 
xlr_i2c_start(device_t dev, u_char slave, int timeout)
{
	int error = 0;
	struct xlr_i2c_softc *sc;

	sc = device_get_softc(dev);
        mtx_lock(&sc->sc_mtx);
	sc->sc_started = 1;
	sc->i2cdev_addr = (slave >> 1);
	return error;

}

static int 
xlr_i2c_stop(device_t dev)
{
	int error = 0;
	struct xlr_i2c_softc *sc;

	sc = device_get_softc(dev);
	mtx_unlock(&sc->sc_mtx);
	return error;

}

static int 
xlr_i2c_read(device_t dev, char *buf, int len, int *read, int last,
    int delay)
{
	volatile uint32_t i2c_status = 0;
	int pos=0;
	int timeout = 0;
	
	xlr_i2c_dev_write(dev, XLR_I2C_CFG, XLR_I2C_CFG_NOADDR);
	xlr_i2c_dev_write(dev, XLR_I2C_BYTECNT, len);

retry:
	xlr_i2c_dev_write(dev, XLR_I2C_STARTXFR, XLR_I2C_STARTXFR_RD);

	timeout = 0;
	while(1) {
		if(timeout++ > MAXTIME)
			return -1;

		i2c_status = xlr_i2c_dev_read(dev, XLR_I2C_STATUS);
		if (i2c_status & XLR_I2C_RXRDY)
			buf[pos++] = (uint8_t) xlr_i2c_dev_read(dev, XLR_I2C_DATAIN);

		/* ACKERR -- bail */
		if (i2c_status & XLR_I2C_ACK_ERR) 
			 return -1;      /* ACK_ERROR */

		/* LOST ARB or STARTERR -- repeat */
		if (i2c_status & XLR_I2C_ARB_STARTERR)
			goto retry;

		/* Wait for busy bit to go away */
		if (i2c_status & XLR_I2C_BUS_BUSY)
			continue;

		if (pos == len)
			break;
	}	
	*read = pos;
	return 0;

}

static int 
xlr_i2c_write(device_t dev, const char *buf, int len, int *sent, int timeout /* us */ )
{
	volatile uint32_t i2c_status = 0x00;
	uint8_t devaddr, addr;
	struct xlr_i2c_softc *sc;
	int pos;

	sc = device_get_softc(dev);

	/* the first byte of write is  addr (of register in device) */
	addr = buf[0];
	devaddr = sc->i2cdev_addr;
	xlr_i2c_dev_write(dev, XLR_I2C_ADDR, addr);
	xlr_i2c_dev_write(dev, XLR_I2C_DEVADDR, devaddr);
	xlr_i2c_dev_write(dev, XLR_I2C_CFG, XLR_I2C_CFG_ADDR);
	xlr_i2c_dev_write(dev, XLR_I2C_BYTECNT, len - 1);

retry:
	pos = 1;
	if (len == 1) /* there is no data only address */
		xlr_i2c_dev_write(dev, XLR_I2C_STARTXFR, XLR_I2C_STARTXFR_ND);
	else {
		xlr_i2c_dev_write(dev, XLR_I2C_STARTXFR, XLR_I2C_STARTXFR_WR);
		xlr_i2c_dev_write(dev, XLR_I2C_DATAOUT, buf[pos]);
	}

	while (1) {
		i2c_status = xlr_i2c_dev_read(dev, XLR_I2C_STATUS);
		
		/* sdo empty send next byte */
		if (i2c_status & XLR_I2C_SDOEMPTY) {
			pos++;
			xlr_i2c_dev_write(dev, XLR_I2C_DATAOUT, buf[pos]);
		}

		/* LOST ARB or STARTERR -- repeat */
		if (i2c_status & XLR_I2C_ARB_STARTERR) 
			goto retry;

		/* ACKERR -- bail */
		if (i2c_status & XLR_I2C_ACK_ERR) {
			printf("ACK ERR : exiting\n ");
			return -1;
		}
	
		/* busy try again */	
		if (i2c_status & XLR_I2C_BUS_BUSY)
			continue;

		if (pos >= len)
			break;
	}
	*sent = len - 1;
	return 0;
}



static int
xlr_i2c_callback(device_t dev, int index, caddr_t data)
{
	return 0;
}

static int
xlr_i2c_repeated_start(device_t dev, u_char slave, int timeout)
{
	return 0;
}

/*
 * I2C bus transfer for RMI boards and devices.
 * Generic version of iicbus_transfer that calls the appropriate
 * routines to accomplish this.  See note above about acceptable
 * buffer addresses.
 */
int
xlr_i2c_transfer(device_t bus, struct iic_msg *msgs, uint32_t nmsgs)
{       
        int i, error, lenread, lenwrote;
        u_char addr;
 
	addr = msgs[0].slave | LSB;
	error = xlr_i2c_start(bus, addr, 0);
        for (i = 0, error = 0; i < nmsgs && error == 0; i++) {
                if (msgs[i].flags & IIC_M_RD) {
		        error = xlr_i2c_read((bus), msgs[i].buf, msgs[i].len, &lenread, IIC_LAST_READ, 0);
		}
                else {    
		        error = xlr_i2c_write((bus), msgs[i].buf, msgs[i].len, &lenwrote, 0);
		}
        }	
	error = xlr_i2c_stop(bus);
        return (error);
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
	DEVMETHOD(iicbus_transfer, xlr_i2c_transfer),
	{0, 0}
};

static driver_t xlr_i2c_driver = {
	"xlr_i2c",
	xlr_i2c_methods,
	sizeof(struct xlr_i2c_softc),
};

DRIVER_MODULE(xlr_i2c, iodi, xlr_i2c_driver, xlr_i2c_devclass, 0, 0);
DRIVER_MODULE(iicbus, xlr_i2c, iicbus_driver, iicbus_devclass, 0, 0);
