/*-
 * Copyright (c) 2024 Denis Bodor <dbodor@rollmops.ninja>
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * i2c-tiny-usb, DIY USB to IIC bridge (using AVR or RP2040) from
 * Till Harbaum & Nicolai Electronics
 * See :
 *   https://github.com/harbaum/I2C-Tiny-USB
 * and
 *   https://github.com/Nicolai-Electronics/rp2040-i2c-interface
 */

#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/bus.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/condvar.h>
#include <sys/unistd.h>
#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>
#include <dev/usb/usbhid.h>
#include <dev/usb/usb_device.h>

#include <dev/iicbus/iiconf.h>
#include <dev/iicbus/iicbus.h>
#include "iicbus_if.h"

// commands via USB, must match command ids in the firmware
#define CMD_ECHO		0
#define CMD_GET_FUNC		1
#define CMD_SET_DELAY		2
#define CMD_GET_STATUS		3
#define CMD_I2C_IO		4
#define CMD_SET_LED		8
#define CMD_I2C_IO_BEGIN	(1 << 0)
#define CMD_I2C_IO_END		(1 << 1)
#define STATUS_IDLE		0
#define STATUS_ADDRESS_ACK	1
#define STATUS_ADDRESS_NAK	2

struct i2ctinyusb_softc {
	struct usb_device	*sc_udev;
	device_t		sc_iic_dev;
	device_t		iicbus_dev;
	struct mtx		sc_mtx;
};

#define USB_VENDOR_EZPROTOTYPES	0x1c40
#define USB_VENDOR_FTDI		0x0403

static const STRUCT_USB_HOST_ID i2ctinyusb_devs[] = {
	{ USB_VPI(USB_VENDOR_EZPROTOTYPES, 0x0534, 0) },
	{ USB_VPI(USB_VENDOR_FTDI, 0xc631, 0) },
};

/* Prototypes. */
static int i2ctinyusb_probe(device_t dev);
static int i2ctinyusb_attach(device_t dev);
static int i2ctinyusb_detach(device_t dev);
static int i2ctinyusb_transfer(device_t dev, struct iic_msg *msgs,
		uint32_t nmsgs);
static int i2ctinyusb_reset(device_t dev, u_char speed, u_char addr,
		u_char *oldaddr);

static int
usb_read(struct i2ctinyusb_softc *sc, int cmd, int value, int index,
		void *data, int len)
{
	int error;
	struct usb_device_request req;
	uint16_t actlen;

	req.bmRequestType = UT_READ_VENDOR_INTERFACE;
	req.bRequest = cmd;
	USETW(req.wValue, value);
	USETW(req.wIndex, (index >> 1));
	USETW(req.wLength, len);

	error = usbd_do_request_flags(sc->sc_udev, &sc->sc_mtx, &req, data, 0,
			&actlen, 2000);

	if (error)
		actlen = -1;

	return (actlen);
}

static int
usb_write(struct i2ctinyusb_softc *sc, int cmd, int value, int index,
		void *data, int len)
{
	int error;
	struct usb_device_request req;
	uint16_t actlen;

	req.bmRequestType = UT_WRITE_VENDOR_INTERFACE;
	req.bRequest = cmd;
	USETW(req.wValue, value);
	USETW(req.wIndex, (index >> 1));
	USETW(req.wLength, len);

	error = usbd_do_request_flags(sc->sc_udev, &sc->sc_mtx, &req, data, 0,
			&actlen, 2000);

	if (error) {
		printf(">>> usbd_do_request_flags error!\n");
		actlen = -1;
	}

	return (actlen);
}

static int
i2ctinyusb_probe(device_t dev)
{
	struct usb_attach_arg *uaa;

	uaa = device_get_ivars(dev);

	if (uaa->usb_mode != USB_MODE_HOST)
		return (ENXIO);

	if (usbd_lookup_id_by_uaa(i2ctinyusb_devs, sizeof(i2ctinyusb_devs),
				uaa) == 0) {
		device_set_desc(dev, "I2C-Tiny-USB I2C interface");
		return (BUS_PROBE_DEFAULT);
	}

	return (ENXIO);
}

static int
i2ctinyusb_attach(device_t dev)
{
	struct i2ctinyusb_softc *sc;
	struct usb_attach_arg *uaa;
	int err;

	sc = device_get_softc(dev);

	uaa = device_get_ivars(dev);
	device_set_usb_desc(dev);

	sc->sc_udev = uaa->device;
	mtx_init(&sc->sc_mtx, "i2ctinyusb lock", NULL, MTX_DEF | MTX_RECURSE);

	sc->iicbus_dev = device_add_child(dev, "iicbus", -1);
	if (sc->iicbus_dev == NULL) {
		device_printf(dev, "iicbus creation failed\n");
		err = ENXIO;
		goto detach;
	}
	err = bus_generic_attach(dev);

	return (0);

detach:
	i2ctinyusb_detach(dev);
	return (err);
}

static int
i2ctinyusb_detach(device_t dev)
{
	struct i2ctinyusb_softc *sc;
	int err;

	sc = device_get_softc(dev);

	err = bus_generic_detach(dev);
	if (err != 0)
		return (err);
	device_delete_children(dev);

	mtx_destroy(&sc->sc_mtx);

	return (0);
}

static int
i2ctinyusb_transfer(device_t dev, struct iic_msg *msgs, uint32_t nmsgs)
{
	struct i2ctinyusb_softc *sc;
	uint32_t i;
	int ret = 0;
	int cmd = CMD_I2C_IO;
	struct iic_msg *pmsg;
	unsigned char pstatus;

	sc = device_get_softc(dev);

	mtx_lock(&sc->sc_mtx);

	for (i = 0; i < nmsgs; i++) {
		pmsg = &msgs[i];
		if (i == 0)
			cmd |= CMD_I2C_IO_BEGIN;
		if (i == nmsgs - 1)
			cmd |= CMD_I2C_IO_END;

		if ((msgs[i].flags & IIC_M_RD) != 0) {
			if ((ret = usb_read(sc, cmd, pmsg->flags, pmsg->slave, pmsg->buf,
							pmsg->len)) != pmsg->len) {
				printf("Read error: got %u\n", ret);
				ret = EIO;
				goto out;
			}
		} else {
			if ((ret = usb_write(sc, cmd, pmsg->flags, pmsg->slave, pmsg->buf,
							pmsg->len)) != pmsg->len) {
				printf("Write error: got %u\n", ret);
				ret = EIO;
				goto out;
			}

		}
		// check status
		if ((ret = usb_read(sc, CMD_GET_STATUS, 0, 0, &pstatus, 1)) != 1) {
			ret = EIO;
			goto out;
		}

		if (pstatus == STATUS_ADDRESS_NAK) {
			ret = EIO;
			goto out;
		}
	}

	ret = 0;

out:
	mtx_unlock(&sc->sc_mtx);
	return (ret);
}

static int
i2ctinyusb_reset(device_t dev, u_char speed, u_char addr, u_char *oldaddr)
{
	struct i2ctinyusb_softc *sc;
	int ret;

	sc = device_get_softc(dev);

	mtx_lock(&sc->sc_mtx);
	ret = usb_write(sc, CMD_SET_DELAY, 10, 0, NULL, 0);
	mtx_unlock(&sc->sc_mtx);

	if (ret < 0)
		printf("i2ctinyusb_reset error!\n");

	return (0);
}

static device_method_t i2ctinyusb_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe, i2ctinyusb_probe),
	DEVMETHOD(device_attach, i2ctinyusb_attach),
	DEVMETHOD(device_detach, i2ctinyusb_detach),

	/* I2C methods */
	DEVMETHOD(iicbus_transfer, i2ctinyusb_transfer),
	DEVMETHOD(iicbus_reset, i2ctinyusb_reset),
	DEVMETHOD(iicbus_callback, iicbus_null_callback),

	DEVMETHOD_END
};

static driver_t i2ctinyusb_driver = {
	.name = "iichb",
	.methods = i2ctinyusb_methods,
	.size = sizeof(struct i2ctinyusb_softc),
};

DRIVER_MODULE(i2ctinyusb, uhub, i2ctinyusb_driver, NULL, NULL);
MODULE_DEPEND(i2ctinyusb, usb, 1, 1, 1);
MODULE_DEPEND(i2ctinyusb, iicbus, IICBUS_MINVER, IICBUS_PREFVER, IICBUS_MAXVER);
MODULE_VERSION(i2ctinyusb, 1);

/* vi: set ts=8 sw=8: */
