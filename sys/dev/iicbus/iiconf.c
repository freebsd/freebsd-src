/*-
 * Copyright (c) 1998 Nicolas Souchu
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
 *	$Id: iiconf.c,v 1.1.1.11 1998/08/29 17:02:05 son Exp $
 *
 */
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/bus.h>

#include <dev/iicbus/iiconf.h>
#include <dev/iicbus/iicbus.h>
#include "iicbus_if.h"

/*
 * iicbus_intr()
 */
void
iicbus_intr(device_t bus, int event, char *buf)
{
	struct iicbus_softc *sc = (struct iicbus_softc *)device_get_softc(bus);

	/* call owner's intr routine */
	if (sc->owner)
		IICBUS_INTR(sc->owner, event, buf);

	return;
}

/*
 * iicbus_alloc_bus()
 *
 * Allocate a new bus connected to the given parent device
 */
device_t
iicbus_alloc_bus(device_t parent)
{
	device_t child;

	/* add the bus to the parent */
	child = device_add_child(parent, "iicbus", -1, NULL);

	if (child)
		device_set_desc(child, "Philips I2C bus");

	return (child);
}

/*
 * iicbus_request_bus()
 *
 * Allocate the device to perform transfers.
 *
 * how	: IIC_WAIT or IIC_DONTWAIT
 */
int
iicbus_request_bus(device_t bus, device_t dev, int how)
{
	struct iicbus_softc *sc = (struct iicbus_softc *)device_get_softc(bus);
	int s, error = 0;

	while (!error) {
		s = splhigh();	
		if (sc->owner) {
			splx(s);

			switch (how) {
			case (IIC_WAIT | IIC_INTR):
				error = tsleep(sc, IICPRI|PCATCH, "iicreq", 0);
				break;

			case (IIC_WAIT | IIC_NOINTR):
				error = tsleep(sc, IICPRI, "iicreq", 0);
				break;

			default:
				return (EWOULDBLOCK);
				break;
			}

		} else {
			sc->owner = dev;

			splx(s);
			return (0);
		}
	}

	return (error);
}

/*
 * iicbus_release_bus()
 *
 * Release the device allocated with iicbus_request_dev()
 */
int
iicbus_release_bus(device_t bus, device_t dev)
{
	struct iicbus_softc *sc = (struct iicbus_softc *)device_get_softc(bus);
	int s;

	s = splhigh();
	if (sc->owner != dev) {
		splx(s);
		return (EACCES);
	}

	sc->owner = 0;
	splx(s);

	/* wakeup waiting processes */
	wakeup(sc);

	return (0);
}

/*
 * iicbus_block_write()
 *
 * Write a block of data to slave ; start/stop protocol managed
 */
int
iicbus_block_write(device_t bus, u_char slave, char *buf, int len, int *sent)
{
	u_char addr = slave & ~LSB;
	int error;

	if ((error = iicbus_start(bus, addr)))
		return (error);

	error = iicbus_write(bus, buf, len, sent);

	iicbus_stop(bus);

	return (error);
}

/*
 * iicbus_block_read()
 *
 * Read a block of data from slave ; start/stop protocol managed
 */
int
iicbus_block_read(device_t bus, u_char slave, char *buf, int len, int *read)
{
	u_char addr = slave | LSB;
	int error;

	if ((error = iicbus_start(bus, addr)))
		return (error);

	error = iicbus_read(bus, buf, len, read);

	/* STOP condition sent at adapter level */

	return (error);
}

/*
 * iicbus_get_addr()
 *
 * Get the I2C 7 bits address of the device
 */
u_char
iicbus_get_addr(device_t dev)
{
	u_long addr;
	device_t parent = device_get_parent(dev);

	BUS_READ_IVAR(parent, dev, IICBUS_IVAR_ADDR, &addr);

	return ((u_char)addr);
}

u_char
iicbus_get_own_address(device_t bus)
{
	struct iicbus_softc *sc = (struct iicbus_softc *)device_get_softc(bus);

	return (sc->ownaddr);
}
