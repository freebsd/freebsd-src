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
 * $FreeBSD$
 *
 */
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/bus.h>

#include <dev/smbus/smbconf.h>
#include <dev/smbus/smbus.h>
#include "smbus_if.h"

/*
 * smbus_intr()
 */
void
smbus_intr(device_t bus, u_char devaddr, char low, char high, int error)
{
	struct smbus_softc *sc = (struct smbus_softc *)device_get_softc(bus);

	/* call owner's intr routine */
	if (sc->owner)
		SMBUS_INTR(sc->owner, devaddr, low, high, error);

	return;
}

/*
 * smbus_error()
 *
 * Converts an smbus error to a unix error.
 */
int
smbus_error(int smb_error)
{
	int error = 0;

	if (smb_error == SMB_ENOERR)
		return (0);
	
	if (smb_error & (SMB_ENOTSUPP)) {
		error = ENODEV;
	} else if (smb_error & (SMB_ENOACK)) {
		error = ENXIO;
	} else if (smb_error & (SMB_ETIMEOUT)) {
		error = EWOULDBLOCK;
	} else if (smb_error & (SMB_EBUSY)) {
		error = EBUSY;
	} else {
		error = EINVAL;
	}

	return (error);
}

/*
 * smbus_alloc_bus()
 *
 * Allocate a new bus connected to the given parent device
 */
device_t
smbus_alloc_bus(device_t parent)
{
	device_t child;

	/* add the bus to the parent */
	child = device_add_child(parent, "smbus", -1);

	return (child);
}

static int
smbus_poll(struct smbus_softc *sc, int how)
{
	int error;

	switch (how) {
	case (SMB_WAIT | SMB_INTR):
		error = tsleep(sc, SMBPRI|PCATCH, "smbreq", 0);
		break;

	case (SMB_WAIT | SMB_NOINTR):
		error = tsleep(sc, SMBPRI, "smbreq", 0);
		break;

	default:
		return (EWOULDBLOCK);
		break;
	}

	return (error);
}

/*
 * smbus_request_bus()
 *
 * Allocate the device to perform transfers.
 *
 * how	: SMB_WAIT or SMB_DONTWAIT
 */
int
smbus_request_bus(device_t bus, device_t dev, int how)
{
	struct smbus_softc *sc = (struct smbus_softc *)device_get_softc(bus);
	int s, error = 0;

	/* first, ask the underlying layers if the request is ok */
	do {
		error = SMBUS_CALLBACK(device_get_parent(bus),
						SMB_REQUEST_BUS, (caddr_t)&how);
		if (error)
			error = smbus_poll(sc, how);
	} while (error == EWOULDBLOCK);

	while (!error) {
		s = splhigh();	
		if (sc->owner && sc->owner != dev) {
			splx(s);

			error = smbus_poll(sc, how);
		} else {
			sc->owner = dev;

			splx(s);
			return (0);
		}

		/* free any allocated resource */
		if (error)
			SMBUS_CALLBACK(device_get_parent(bus), SMB_RELEASE_BUS,
					(caddr_t)&how);
	}

	return (error);
}

/*
 * smbus_release_bus()
 *
 * Release the device allocated with smbus_request_dev()
 */
int
smbus_release_bus(device_t bus, device_t dev)
{
	struct smbus_softc *sc = (struct smbus_softc *)device_get_softc(bus);
	int s, error;

	/* first, ask the underlying layers if the release is ok */
	error = SMBUS_CALLBACK(device_get_parent(bus), SMB_RELEASE_BUS, NULL);

	if (error)
		return (error);

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
 * smbus_get_addr()
 *
 * Get the I2C 7 bits address of the device
 */
u_char
smbus_get_addr(device_t dev)
{
	uintptr_t addr;
	device_t parent = device_get_parent(dev);

	BUS_READ_IVAR(parent, dev, SMBUS_IVAR_ADDR, &addr);

	return ((u_char)addr);
}
