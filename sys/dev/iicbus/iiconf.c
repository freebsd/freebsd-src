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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/mutex.h>
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

static int
iicbus_poll(struct iicbus_softc *sc, int how)
{
	int error;

	IICBUS_ASSERT_LOCKED(sc);
	switch (how) {
	case IIC_WAIT | IIC_INTR:
		error = mtx_sleep(sc, &sc->lock, IICPRI|PCATCH, "iicreq", 0);
		break;

	case IIC_WAIT | IIC_NOINTR:
		error = mtx_sleep(sc, &sc->lock, IICPRI, "iicreq", 0);
		break;

	default:
		return (EWOULDBLOCK);
	}

	return (error);
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
	int error = 0;

	IICBUS_LOCK(sc);

	while ((error == 0) && (sc->owner != NULL))
		error = iicbus_poll(sc, how);

	if (error == 0) {
		sc->owner = dev;
		/* 
		 * Drop the lock around the call to the bus driver. 
		 * This call should be allowed to sleep in the IIC_WAIT case.
		 * Drivers might also need to grab locks that would cause LOR
		 * if our lock is held.
		 */
		IICBUS_UNLOCK(sc);
		/* Ask the underlying layers if the request is ok */
		error = IICBUS_CALLBACK(device_get_parent(bus),
		    IIC_REQUEST_BUS, (caddr_t)&how);
		IICBUS_LOCK(sc);

		if (error != 0) {
			sc->owner = NULL;
			wakeup_one(sc);
		}
	}


	IICBUS_UNLOCK(sc);

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
	int error;

	IICBUS_LOCK(sc);

	if (sc->owner != dev) {
		IICBUS_UNLOCK(sc);
		return (EACCES);
	}

	/* 
	 * Drop the lock around the call to the bus driver. 
	 * This call should be allowed to sleep in the IIC_WAIT case.
	 * Drivers might also need to grab locks that would cause LOR
	 * if our lock is held.
	 */
	IICBUS_UNLOCK(sc);
	/* Ask the underlying layers if the release is ok */
	error = IICBUS_CALLBACK(device_get_parent(bus), IIC_RELEASE_BUS, NULL);

	if (error == 0) {
		IICBUS_LOCK(sc);
		sc->owner = NULL;

		/* wakeup a waiting thread */
		wakeup_one(sc);
		IICBUS_UNLOCK(sc);
	}

	return (error);
}

/*
 * iicbus_started()
 *
 * Test if the iicbus is started by the controller
 */
int
iicbus_started(device_t bus)
{
	struct iicbus_softc *sc = (struct iicbus_softc *)device_get_softc(bus);

	return (sc->started);
}

/*
 * iicbus_start()
 *
 * Send start condition to the slave addressed by 'slave'
 */
int
iicbus_start(device_t bus, u_char slave, int timeout)
{
	struct iicbus_softc *sc = (struct iicbus_softc *)device_get_softc(bus);
	int error = 0;

	if (sc->started)
		return (EINVAL);		/* bus already started */

	if (!(error = IICBUS_START(device_get_parent(bus), slave, timeout)))
		sc->started = slave;
	else
		sc->started = 0;

	return (error);
}

/*
 * iicbus_repeated_start()
 *
 * Send start condition to the slave addressed by 'slave'
 */
int
iicbus_repeated_start(device_t bus, u_char slave, int timeout)
{
	struct iicbus_softc *sc = (struct iicbus_softc *)device_get_softc(bus);
	int error = 0;

	if (!sc->started)
		return (EINVAL);     /* bus should have been already started */

	if (!(error = IICBUS_REPEATED_START(device_get_parent(bus), slave, timeout)))
		sc->started = slave;
	else
		sc->started = 0;

	return (error);
}

/*
 * iicbus_stop()
 *
 * Send stop condition to the bus
 */
int
iicbus_stop(device_t bus)
{
	struct iicbus_softc *sc = (struct iicbus_softc *)device_get_softc(bus);
	int error = 0;

	if (!sc->started)
		return (EINVAL);		/* bus not started */

	error = IICBUS_STOP(device_get_parent(bus));

	/* refuse any further access */
	sc->started = 0;

	return (error);
}

/*
 * iicbus_write()
 *
 * Write a block of data to the slave previously started by
 * iicbus_start() call
 */
int
iicbus_write(device_t bus, const char *buf, int len, int *sent, int timeout)
{
	struct iicbus_softc *sc = (struct iicbus_softc *)device_get_softc(bus);
	
	/* a slave must have been started for writing */
	if (sc->started == 0 || (sc->strict != 0 && (sc->started & LSB) != 0))
		return (EINVAL);

	return (IICBUS_WRITE(device_get_parent(bus), buf, len, sent, timeout));
}

/*
 * iicbus_read()
 *
 * Read a block of data from the slave previously started by
 * iicbus_read() call
 */
int 
iicbus_read(device_t bus, char *buf, int len, int *read, int last, int delay)
{
	struct iicbus_softc *sc = (struct iicbus_softc *)device_get_softc(bus);
	
	/* a slave must have been started for reading */
	if (sc->started == 0 || (sc->strict != 0 && (sc->started & LSB) == 0))
		return (EINVAL);

	return (IICBUS_READ(device_get_parent(bus), buf, len, read, last, delay));
}

/*
 * iicbus_write_byte()
 *
 * Write a byte to the slave previously started by iicbus_start() call
 */
int
iicbus_write_byte(device_t bus, char byte, int timeout)
{
	char data = byte;
	int sent;

	return (iicbus_write(bus, &data, 1, &sent, timeout));
}

/*
 * iicbus_read_byte()
 *
 * Read a byte from the slave previously started by iicbus_start() call
 */
int
iicbus_read_byte(device_t bus, char *byte, int timeout)
{
	int read;

	return (iicbus_read(bus, byte, 1, &read, IIC_LAST_READ, timeout));
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

	if ((error = iicbus_start(bus, addr, 0)))
		return (error);

	error = iicbus_write(bus, buf, len, sent, 0);

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

	if ((error = iicbus_start(bus, addr, 0)))
		return (error);

	error = iicbus_read(bus, buf, len, read, IIC_LAST_READ, 0);

	iicbus_stop(bus);

	return (error);
}

/*
 * iicbus_transfer()
 *
 * Do an aribtrary number of transfers on the iicbus.  We pass these
 * raw requests to the bridge driver.  If the bridge driver supports
 * them directly, then it manages all the details.  If not, it can use
 * the helper function iicbus_transfer_gen() which will do the
 * transfers at a low level.
 *
 * Pointers passed in as part of iic_msg must be kernel pointers.
 * Callers that have user addresses to manage must do so on their own.
 */
int
iicbus_transfer(device_t bus, struct iic_msg *msgs, uint32_t nmsgs)
{
	return (IICBUS_TRANSFER(device_get_parent(bus), msgs, nmsgs));
}

/*
 * Generic version of iicbus_transfer that calls the appropriate
 * routines to accomplish this.  See note above about acceptable
 * buffer addresses.
 */
int
iicbus_transfer_gen(device_t dev, struct iic_msg *msgs, uint32_t nmsgs)
{
	int i, error, lenread, lenwrote, nkid, rpstart, addr;
	device_t *children, bus;
	bool nostop;

	if ((error = device_get_children(dev, &children, &nkid)) != 0)
		return (error);
	if (nkid != 1) {
		free(children, M_TEMP);
		return (EIO);
	}
	bus = children[0];
	rpstart = 0;
	free(children, M_TEMP);
	nostop = iicbus_get_nostop(dev);
	for (i = 0, error = 0; i < nmsgs && error == 0; i++) {
		addr = msgs[i].slave;
		if (msgs[i].flags & IIC_M_RD)
			addr |= LSB;
		else
			addr &= ~LSB;

		if (!(msgs[i].flags & IIC_M_NOSTART)) {
			if (rpstart)
				error = iicbus_repeated_start(bus, addr, 0);
			else
				error = iicbus_start(bus, addr, 0);
		}
		if (error != 0)
			break;

		if (msgs[i].flags & IIC_M_RD)
			error = iicbus_read(bus, msgs[i].buf, msgs[i].len,
			    &lenread, IIC_LAST_READ, 0);
		else
			error = iicbus_write(bus, msgs[i].buf, msgs[i].len,
			    &lenwrote, 0);
		if (error != 0)
			break;

		if ((msgs[i].flags & IIC_M_NOSTOP) != 0 ||
		    (nostop && i + 1 < nmsgs)) {
			rpstart = 1;	/* Next message gets repeated start */
		} else {
			rpstart = 0;
			iicbus_stop(bus);
		}
	}
	if (error != 0 && !nostop)
		iicbus_stop(bus);
	return (error);
}
