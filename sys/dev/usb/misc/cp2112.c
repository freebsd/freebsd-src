/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) Andriy Gapon
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions, and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

/*
 * Hardware information links:
 * - CP2112 Datasheet
 *   https://www.silabs.com/documents/public/data-sheets/cp2112-datasheet.pdf
 * - AN495: CP2112 Interface Specification
 *   https://www.silabs.com/documents/public/application-notes/an495-cp2112-interface-specification.pdf
 * - CP2112 Errata
 *   https://www.silabs.com/documents/public/errata/cp2112-errata.pdf
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/condvar.h>
#include <sys/bus.h>
#include <sys/gpio.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/sdt.h>
#include <sys/sx.h>

#include <dev/gpio/gpiobusvar.h>

#include <dev/iicbus/iiconf.h>
#include <dev/iicbus/iicbus.h>
#include "iicbus_if.h"

#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>
#include <dev/usb/usbdi_util.h>
#include <dev/usb/usbhid.h>
#include "usbdevs.h"

#define	USB_DEBUG_VAR usb_debug
#include <dev/usb/usb_debug.h>

#define	SIZEOF_FIELD(_s, _f)	sizeof(((struct _s *)NULL)->_f)

#define	CP2112GPIO_LOCK(sc)	sx_xlock(&sc->gpio_lock)
#define	CP2112GPIO_UNLOCK(sc)	sx_xunlock(&sc->gpio_lock)
#define	CP2112GPIO_LOCKED(sc)	sx_assert(&sc->gpio_lock, SX_XLOCKED)

#define	CP2112_PART_NUM			0x0c
#define	CP2112_GPIO_COUNT		8
#define	CP2112_REPORT_SIZE		64

#define	CP2112_REQ_RESET		0x1
#define	CP2112_REQ_GPIO_CFG		0x2
#define	CP2112_REQ_GPIO_GET		0x3
#define	CP2112_REQ_GPIO_SET		0x4
#define	CP2112_REQ_VERSION		0x5
#define	CP2112_REQ_SMB_CFG		0x6

#define	CP2112_REQ_SMB_READ		0x10
#define	CP2112_REQ_SMB_WRITE_READ	0x11
#define	CP2112_REQ_SMB_READ_FORCE_SEND	0x12
#define	CP2112_REQ_SMB_READ_RESPONSE	0x13
#define	CP2112_REQ_SMB_WRITE		0x14
#define	CP2112_REQ_SMB_XFER_STATUS_REQ	0x15
#define	CP2112_REQ_SMB_XFER_STATUS_RESP	0x16
#define	CP2112_REQ_SMB_CANCEL		0x17

#define	CP2112_REQ_LOCK			0x20
#define	CP2112_REQ_USB_CFG		0x21

#define	CP2112_IIC_MAX_READ_LEN		512
#define	CP2112_IIC_REPSTART_VER		2	/* Erratum CP2112_E10. */

#define	CP2112_GPIO_SPEC_CLK7		1	/* Pin 7 is clock output. */
#define	CP2112_GPIO_SPEC_TX0		2	/* Pin 0 pulses on USB TX. */
#define	CP2112_GPIO_SPEC_RX1		4	/* Pin 1 pulses on USB RX. */

#define	CP2112_IIC_STATUS0_IDLE		0
#define	CP2112_IIC_STATUS0_BUSY		1
#define	CP2112_IIC_STATUS0_CMP		2
#define	CP2112_IIC_STATUS0_ERROR	3

#define	CP2112_IIC_STATUS1_TIMEOUT_NACK	0
#define	CP2112_IIC_STATUS1_TIMEOUT_BUS	1
#define	CP2112_IIC_STATUS1_ARB_LOST	2

/* CP2112_REQ_VERSION */
struct version_request {
	uint8_t id;
	uint8_t part_num;
	uint8_t version;
} __packed;

/* CP2112_REQ_GPIO_GET */
struct gpio_get_req {
	uint8_t id;
	uint8_t state;
} __packed;

/* CP2112_REQ_GPIO_SET */
struct gpio_set_req {
	uint8_t id;
	uint8_t state;
	uint8_t mask;
} __packed;

/* CP2112_REQ_GPIO_CFG */
struct gpio_config_req {
	uint8_t id;
	uint8_t output;
	uint8_t pushpull;
	uint8_t special;
	uint8_t divider;
} __packed;

/* CP2112_REQ_SMB_XFER_STATUS_REQ */
struct i2c_xfer_status_req {
	uint8_t id;
	uint8_t request;
} __packed;

/* CP2112_REQ_SMB_XFER_STATUS_RESP */
struct i2c_xfer_status_resp {
	uint8_t id;
	uint8_t status0;
	uint8_t status1;
	uint16_t status2;
	uint16_t status3;
} __packed;

/* CP2112_REQ_SMB_READ_FORCE_SEND */
struct i2c_data_read_force_send_req {
	uint8_t id;
	uint16_t len;
} __packed;

/* CP2112_REQ_SMB_READ_RESPONSE */
struct i2c_data_read_resp {
	uint8_t id;
	uint8_t status;
	uint8_t len;
	uint8_t data[61];
} __packed;

/* CP2112_REQ_SMB_READ */
struct i2c_write_read_req {
	uint8_t id;
	uint8_t slave;
	uint16_t rlen;
	uint8_t wlen;
	uint8_t wdata[16];
} __packed;

/* CP2112_REQ_SMB_WRITE */
struct i2c_read_req {
	uint8_t id;
	uint8_t slave;
	uint16_t len;
} __packed;

/* CP2112_REQ_SMB_WRITE_READ */
struct i2c_write_req {
	uint8_t id;
	uint8_t slave;
	uint8_t len;
	uint8_t data[61];
} __packed;

/* CP2112_REQ_SMB_CFG */
struct i2c_cfg_req {
	uint8_t		id;
	uint32_t	speed;		/* Hz */
	uint8_t		slave_addr;	/* ACK only */
	uint8_t		auto_send_read;	/* boolean */
	uint16_t	write_timeout;	/* 0-1000 ms, 0 ~ no timeout */
	uint16_t	read_timeout;	/* 0-1000 ms, 0 ~ no timeout */
	uint8_t		scl_low_timeout;/* boolean */
	uint16_t	retry_count;	/* 1-1000, 0 ~ forever */
} __packed;

enum cp2112_out_mode {
	OUT_OD,
	OUT_PP,
	OUT_KEEP
};

enum {
	CP2112_INTR_OUT = 0,
	CP2112_INTR_IN,
	CP2112_N_TRANSFER,
};

struct cp2112_softc {
	device_t		sc_gpio_dev;
	device_t		sc_iic_dev;
	struct usb_device	*sc_udev;
	uint8_t			sc_iface_index;
	uint8_t			sc_version;
};

struct cp2112gpio_softc {
	struct sx		gpio_lock;
	device_t		busdev;
	int			gpio_caps;
	struct gpio_pin		pins[CP2112_GPIO_COUNT];
};

struct cp2112iic_softc {
	device_t	dev;
	device_t	iicbus_dev;
	struct usb_xfer	*xfers[CP2112_N_TRANSFER];
	u_char		own_addr;
	struct {
		struct mtx	lock;
		struct cv	cv;
		struct {
			uint8_t		*data;
			int		len;
			int		done;
			int		error;
		}		in;
		struct {
			const uint8_t	*data;
			int		len;
			int		done;
			int		error;
		}		out;
	}		io;
};

static int cp2112_detach(device_t dev);
static int cp2112gpio_detach(device_t dev);
static int cp2112iic_detach(device_t dev);

static const STRUCT_USB_HOST_ID cp2112_devs[] = {
	{ USB_VP(USB_VENDOR_SILABS, USB_PRODUCT_SILABS_CP2112) },
	{ USB_VP(0x1009, USB_PRODUCT_SILABS_CP2112) }, /* XXX */
};

static int
cp2112_get_report(device_t dev, uint8_t id, void *data, uint16_t len)
{
	struct cp2112_softc *sc;
	int err;

	sc = device_get_softc(dev);
	err = usbd_req_get_report(sc->sc_udev, NULL, data,
	    len, sc->sc_iface_index, UHID_FEATURE_REPORT, id);
	return (err);
}

static int
cp2112_set_report(device_t dev, uint8_t id, void *data, uint16_t len)
{
	struct cp2112_softc *sc;
	int err;

	sc = device_get_softc(dev);
	*(uint8_t *)data = id;
	err = usbd_req_set_report(sc->sc_udev, NULL, data,
	    len, sc->sc_iface_index, UHID_FEATURE_REPORT, id);
	return (err);
}

static int
cp2112_probe(device_t dev)
{
	struct usb_attach_arg *uaa;

	uaa = device_get_ivars(dev);
	if (uaa->usb_mode != USB_MODE_HOST)
		return (ENXIO);
	if (uaa->info.bInterfaceClass != UICLASS_HID)
		return (ENXIO);

	if (usbd_lookup_id_by_uaa(cp2112_devs, sizeof(cp2112_devs), uaa) == 0)
		return (BUS_PROBE_DEFAULT);
	return (ENXIO);
}

static int
cp2112_attach(device_t dev)
{
	struct version_request vdata;
	struct usb_attach_arg *uaa;
	struct cp2112_softc *sc;
	int err;

	uaa = device_get_ivars(dev);
	sc = device_get_softc(dev);

	device_set_usb_desc(dev);

	sc->sc_udev = uaa->device;
	sc->sc_iface_index = uaa->info.bIfaceIndex;

	err = cp2112_get_report(dev, CP2112_REQ_VERSION, &vdata, sizeof(vdata));
	if (err != 0)
		goto detach;
	device_printf(dev, "part number 0x%02x, version 0x%02x\n",
	    vdata.part_num, vdata.version);
	if (vdata.part_num != CP2112_PART_NUM) {
		device_printf(dev, "unsupported part number\n");
		goto detach;
	}
	sc->sc_version = vdata.version;
	sc->sc_gpio_dev = device_add_child(dev, "gpio", -1);
	if (sc->sc_gpio_dev != NULL) {
		err = device_probe_and_attach(sc->sc_gpio_dev);
		if (err != 0) {
			device_printf(dev, "failed to attach gpio child\n");
		}
	} else {
		device_printf(dev, "failed to create gpio child\n");
	}

	sc->sc_iic_dev = device_add_child(dev, "iichb", -1);
	if (sc->sc_iic_dev != NULL) {
		err = device_probe_and_attach(sc->sc_iic_dev);
		if (err != 0) {
			device_printf(dev, "failed to attach iic child\n");
		}
	} else {
		device_printf(dev, "failed to create iic child\n");
	}

	return (0);

detach:
	cp2112_detach(dev);
	return (ENXIO);
}

static int
cp2112_detach(device_t dev)
{
	int err;

	err = bus_generic_detach(dev);
	if (err != 0)
		return (err);
	device_delete_children(dev);
	return (0);
}

static int
cp2112_gpio_read_pin(device_t dev, uint32_t pin_num, bool *on)
{
	struct gpio_get_req data;
	struct cp2112gpio_softc *sc __diagused;
	int err;

	sc = device_get_softc(dev);
	CP2112GPIO_LOCKED(sc);

	err = cp2112_get_report(device_get_parent(dev),
	    CP2112_REQ_GPIO_GET, &data, sizeof(data));
	if (err != 0)
		return (err);
	*on = (data.state & ((uint8_t)1 << pin_num)) != 0;
	return (0);

}

static int
cp2112_gpio_write_pin(device_t dev, uint32_t pin_num, bool on)
{
	struct gpio_set_req data;
	struct cp2112gpio_softc *sc __diagused;
	int err;
	bool actual;

	sc = device_get_softc(dev);
	CP2112GPIO_LOCKED(sc);

	data.state = (uint8_t)on << pin_num;
	data.mask = (uint8_t)1 << pin_num;
	err = cp2112_set_report(device_get_parent(dev),
	    CP2112_REQ_GPIO_SET, &data, sizeof(data));
	if (err != 0)
		return (err);
	err = cp2112_gpio_read_pin(dev, pin_num, &actual);
	if (err != 0)
		return (err);
	if (actual != on)
		return (EIO);
	return (0);
}

static int
cp2112_gpio_configure_write_pin(device_t dev, uint32_t pin_num,
    bool output, enum cp2112_out_mode *mode)
{
	struct gpio_config_req data;
	struct cp2112gpio_softc *sc __diagused;
	int err;
	uint8_t mask;

	sc = device_get_softc(dev);
	CP2112GPIO_LOCKED(sc);

	err = cp2112_get_report(device_get_parent(dev),
	    CP2112_REQ_GPIO_CFG, &data, sizeof(data));
	if (err != 0)
		return (err);

	mask = (uint8_t)1 << pin_num;
	if (output) {
		data.output |= mask;
		switch (*mode) {
		case OUT_PP:
			data.pushpull |= mask;
			break;
		case OUT_OD:
			data.pushpull &= ~mask;
			break;
		default:
			break;
		}
	} else {
		data.output &= ~mask;
	}

	err = cp2112_set_report(device_get_parent(dev),
	    CP2112_REQ_GPIO_CFG, &data, sizeof(data));
	if (err != 0)
		return (err);

	/* Read back and verify. */
	err = cp2112_get_report(device_get_parent(dev),
	    CP2112_REQ_GPIO_CFG, &data, sizeof(data));
	if (err != 0)
		return (err);

	if (((data.output & mask) != 0) != output)
		return (EIO);
	if (output) {
		switch (*mode) {
		case OUT_PP:
			if ((data.pushpull & mask) == 0)
				return (EIO);
			break;
		case OUT_OD:
			if ((data.pushpull & mask) != 0)
				return (EIO);
			break;
		default:
			*mode = (data.pushpull & mask) != 0 ?
			    OUT_PP : OUT_OD;
			break;
		}
	}
	return (0);
}

static device_t
cp2112_gpio_get_bus(device_t dev)
{
	struct cp2112gpio_softc *sc;

	sc = device_get_softc(dev);
	return (sc->busdev);
}

static int
cp2112_gpio_pin_max(device_t dev, int *maxpin)
{

	*maxpin = CP2112_GPIO_COUNT - 1;
	return (0);
}

static int
cp2112_gpio_pin_set(device_t dev, uint32_t pin_num, uint32_t pin_value)
{
	struct cp2112gpio_softc *sc;
	int err;

	if (pin_num >= CP2112_GPIO_COUNT)
		return (EINVAL);

	sc = device_get_softc(dev);
	CP2112GPIO_LOCK(sc);
	err = cp2112_gpio_write_pin(dev, pin_num, pin_value != 0);
	CP2112GPIO_UNLOCK(sc);

	return (err);
}

static int
cp2112_gpio_pin_get(device_t dev, uint32_t pin_num, uint32_t *pin_value)
{
	struct cp2112gpio_softc *sc;
	int err;
	bool on;

	if (pin_num >= CP2112_GPIO_COUNT)
		return (EINVAL);

	sc = device_get_softc(dev);
	CP2112GPIO_LOCK(sc);
	err = cp2112_gpio_read_pin(dev, pin_num, &on);
	CP2112GPIO_UNLOCK(sc);

	if (err == 0)
		*pin_value = on;
	return (err);
}

static int
cp2112_gpio_pin_toggle(device_t dev, uint32_t pin_num)
{
	struct cp2112gpio_softc *sc;
	int err;
	bool on;

	if (pin_num >= CP2112_GPIO_COUNT)
		return (EINVAL);

	sc = device_get_softc(dev);
	CP2112GPIO_LOCK(sc);
	err = cp2112_gpio_read_pin(dev, pin_num, &on);
	if (err == 0)
		err = cp2112_gpio_write_pin(dev, pin_num, !on);
	CP2112GPIO_UNLOCK(sc);

	return (err);
}

static int
cp2112_gpio_pin_getcaps(device_t dev, uint32_t pin_num, uint32_t *caps)
{
	struct cp2112gpio_softc *sc;

	if (pin_num >= CP2112_GPIO_COUNT)
		return (EINVAL);

	sc = device_get_softc(dev);
	CP2112GPIO_LOCK(sc);
	*caps = sc->gpio_caps;
	CP2112GPIO_UNLOCK(sc);

	return (0);
}

static int
cp2112_gpio_pin_getflags(device_t dev, uint32_t pin_num, uint32_t *flags)
{
	struct cp2112gpio_softc *sc;

	if (pin_num >= CP2112_GPIO_COUNT)
		return (EINVAL);

	sc = device_get_softc(dev);
	CP2112GPIO_LOCK(sc);
	*flags = sc->pins[pin_num].gp_flags;
	CP2112GPIO_UNLOCK(sc);

	return (0);
}

static int
cp2112_gpio_pin_getname(device_t dev, uint32_t pin_num, char *name)
{
	struct cp2112gpio_softc *sc;

	if (pin_num >= CP2112_GPIO_COUNT)
		return (EINVAL);

	sc = device_get_softc(dev);
	CP2112GPIO_LOCK(sc);
	memcpy(name, sc->pins[pin_num].gp_name, GPIOMAXNAME);
	CP2112GPIO_UNLOCK(sc);

	return (0);
}

static int
cp2112_gpio_pin_setflags(device_t dev, uint32_t pin_num, uint32_t flags)
{
	struct cp2112gpio_softc *sc;
	struct gpio_pin *pin;
	enum cp2112_out_mode out_mode;
	int err;

	if (pin_num >= CP2112_GPIO_COUNT)
		return (EINVAL);

	sc = device_get_softc(dev);
	if ((flags & sc->gpio_caps) != flags)
		return (EINVAL);

	if ((flags & (GPIO_PIN_INPUT | GPIO_PIN_OUTPUT)) == 0)
			return (EINVAL);
	if ((flags & (GPIO_PIN_INPUT | GPIO_PIN_OUTPUT)) ==
		(GPIO_PIN_INPUT | GPIO_PIN_OUTPUT)) {
			return (EINVAL);
	}
	if ((flags & GPIO_PIN_INPUT) != 0) {
		if ((flags & (GPIO_PIN_OPENDRAIN | GPIO_PIN_PUSHPULL)) != 0)
			return (EINVAL);
	} else {
		if ((flags & (GPIO_PIN_OPENDRAIN | GPIO_PIN_PUSHPULL)) ==
		    (GPIO_PIN_OPENDRAIN | GPIO_PIN_PUSHPULL))
			return (EINVAL);
	}

	/*
	 * If neither push-pull or open-drain is explicitly requested, then
	 * preserve the current state.
	 */
	out_mode = OUT_KEEP;
	if ((flags & GPIO_PIN_OUTPUT) != 0) {
		if ((flags & GPIO_PIN_OPENDRAIN) != 0)
			out_mode = OUT_OD;
		if ((flags & GPIO_PIN_PUSHPULL) != 0)
			out_mode = OUT_PP;
	}

	CP2112GPIO_LOCK(sc);
	pin = &sc->pins[pin_num];
	err = cp2112_gpio_configure_write_pin(dev, pin_num,
	    (flags & GPIO_PIN_OUTPUT) != 0, &out_mode);
	if (err == 0) {
		/*
		 * If neither open-drain or push-pull was requested, then see
		 * what hardware actually had.  Otherwise, it has been
		 * reconfigured as requested.
		 */
		if ((flags & GPIO_PIN_OUTPUT) != 0 &&
		    (flags & (GPIO_PIN_OPENDRAIN | GPIO_PIN_PUSHPULL)) == 0) {
			KASSERT(out_mode != OUT_KEEP,
			    ("impossible current output mode"));
			if (out_mode == OUT_OD)
				flags |= GPIO_PIN_OPENDRAIN;
			else
				flags |= GPIO_PIN_PUSHPULL;
		}
		pin->gp_flags = flags;
	}
	CP2112GPIO_UNLOCK(sc);

	return (err);
}

static int
cp2112gpio_probe(device_t dev)
{
	device_set_desc(dev, "CP2112 GPIO interface");
	return (BUS_PROBE_SPECIFIC);
}

static int
cp2112gpio_attach(device_t dev)
{
	struct gpio_config_req data;
	struct cp2112gpio_softc *sc;
	device_t cp2112;
	int err;
	int i;
	uint8_t mask;

	cp2112 = device_get_parent(dev);
	sc = device_get_softc(dev);
	sx_init(&sc->gpio_lock, "cp2112 lock");

	sc->gpio_caps = GPIO_PIN_INPUT | GPIO_PIN_OUTPUT | GPIO_PIN_OPENDRAIN |
	    GPIO_PIN_PUSHPULL;

	err = cp2112_get_report(cp2112, CP2112_REQ_GPIO_CFG,
	    &data, sizeof(data));
	if (err != 0)
		goto detach;

	for (i = 0; i < CP2112_GPIO_COUNT; i++) {
		struct gpio_pin *pin;

		mask = (uint8_t)1 << i;
		pin = &sc->pins[i];
		pin->gp_flags = 0;

		snprintf(pin->gp_name, GPIOMAXNAME, "GPIO%u", i);
		pin->gp_name[GPIOMAXNAME - 1] = '\0';

		if ((i == 0 && (data.special & CP2112_GPIO_SPEC_TX0) != 0) ||
		    (i == 1 && (data.special & CP2112_GPIO_SPEC_RX1) != 0) ||
		    (i == 7 && (data.special & CP2112_GPIO_SPEC_CLK7) != 0)) {
			/* Special mode means that a pin is not for GPIO. */
		} else if ((data.output & mask) != 0) {
			pin->gp_flags |= GPIO_PIN_OUTPUT;
			if ((data.pushpull & mask) != 0)
				pin->gp_flags |= GPIO_PIN_PUSHPULL;
			else
				pin->gp_flags |= GPIO_PIN_OPENDRAIN;
		} else {
			pin->gp_flags |= GPIO_PIN_INPUT;
		}
	}

	sc->busdev = gpiobus_attach_bus(dev);
	if (sc->busdev == NULL) {
		device_printf(dev, "gpiobus_attach_bus failed\n");
		goto detach;
	}
	return (0);

detach:
	cp2112gpio_detach(dev);
	return (ENXIO);
}

static int
cp2112gpio_detach(device_t dev)
{
	struct cp2112gpio_softc *sc;

	sc = device_get_softc(dev);
	if (sc->busdev != NULL)
		gpiobus_detach_bus(dev);
	sx_destroy(&sc->gpio_lock);
	return (0);
}

static void
cp2112iic_intr_write_callback(struct usb_xfer *xfer, usb_error_t error)
{
	struct cp2112iic_softc *sc;
	struct usb_page_cache *pc;

	sc = usbd_xfer_softc(xfer);

	mtx_assert(&sc->io.lock, MA_OWNED);

	switch (USB_GET_STATE(xfer)) {
	case USB_ST_SETUP:
		pc = usbd_xfer_get_frame(xfer, 0);
		usbd_copy_in(pc, 0, sc->io.out.data, sc->io.out.len);
		usbd_xfer_set_frame_len(xfer, 0, sc->io.out.len);
		usbd_xfer_set_frames(xfer, 1);
		usbd_transfer_submit(xfer);
		break;
	case USB_ST_TRANSFERRED:
		sc->io.out.error = 0;
		sc->io.out.done = 1;
		cv_signal(&sc->io.cv);
		break;
	default:			/* Error */
		device_printf(sc->dev, "write intr state %d error %d\n",
		    USB_GET_STATE(xfer), error);
		sc->io.out.error = IIC_EBUSERR;
		cv_signal(&sc->io.cv);
		if (error != USB_ERR_CANCELLED) {
			/* try to clear stall first */
			usbd_xfer_set_stall(xfer);
		}
		break;
	}
}

static void
cp2112iic_intr_read_callback(struct usb_xfer *xfer, usb_error_t error)
{
	struct cp2112iic_softc *sc = usbd_xfer_softc(xfer);
	struct usb_page_cache *pc;
	int act_len, len;

	mtx_assert(&sc->io.lock, MA_OWNED);
	usbd_xfer_status(xfer, &act_len, NULL, NULL, NULL);

	switch (USB_GET_STATE(xfer)) {
	case USB_ST_TRANSFERRED:
		if (sc->io.in.done) {
			device_printf(sc->dev,
			    "interrupt while previous is pending, ignored\n");
		} else if (sc->io.in.len == 0) {
			uint8_t buf[8];

			/*
			 * There is a spurious Transfer Status Response and
			 * zero-length Read Response during hardware
			 * configuration.  Possibly they carry some information
			 * about the initial bus state.
			 */
			if (device_is_attached(sc->dev)) {
				device_printf(sc->dev,
				    "unsolicited interrupt, ignored\n");
				if (bootverbose) {
					pc = usbd_xfer_get_frame(xfer, 0);
					len = MIN(sizeof(buf), act_len);
					usbd_copy_out(pc, 0, buf, len);
					device_printf(sc->dev, "data: %*D\n",
					    len, buf, " ");
				}
			} else {
				pc = usbd_xfer_get_frame(xfer, 0);
				len = MIN(sizeof(buf), act_len);
				usbd_copy_out(pc, 0, buf, len);
				if (buf[0] == CP2112_REQ_SMB_XFER_STATUS_RESP) {
					device_printf(sc->dev,
					    "initial bus status0 = 0x%02x, "
					    "status1 = 0x%02x\n",
					    buf[1], buf[2]);
				}
			}
		} else if (act_len == CP2112_REPORT_SIZE) {
			pc = usbd_xfer_get_frame(xfer, 0);
			usbd_copy_out(pc, 0, sc->io.in.data, sc->io.in.len);
			sc->io.in.error = 0;
			sc->io.in.done = 1;
		} else {
			device_printf(sc->dev,
			    "unexpected input report length %u\n", act_len);
			sc->io.in.error = IIC_EBUSERR;
			sc->io.in.done = 1;
		}
		cv_signal(&sc->io.cv);
	case USB_ST_SETUP:
tr_setup:
		usbd_xfer_set_frame_len(xfer, 0, usbd_xfer_max_len(xfer));
		usbd_transfer_submit(xfer);
		break;

	default:			/* Error */
		device_printf(sc->dev, "read intr state %d error %d\n",
		    USB_GET_STATE(xfer), error);

		sc->io.in.error = IIC_EBUSERR;
		sc->io.in.done = 1;
		cv_signal(&sc->io.cv);
		if (error != USB_ERR_CANCELLED) {
			/* try to clear stall first */
			usbd_xfer_set_stall(xfer);
			goto tr_setup;
		}
		break;
	}
}

static const struct usb_config cp2112iic_config[CP2112_N_TRANSFER] = {
	[CP2112_INTR_OUT] = {
		.type = UE_INTERRUPT,
		.endpoint = UE_ADDR_ANY,
		.direction = UE_DIR_OUT,
		.flags = { .pipe_bof = 1, .no_pipe_ok = 1, },
		.bufsize = 0,	/* use wMaxPacketSize */
		.callback = &cp2112iic_intr_write_callback,
	},
	[CP2112_INTR_IN] = {
		.type = UE_INTERRUPT,
		.endpoint = UE_ADDR_ANY,
		.direction = UE_DIR_IN,
		.flags = { .pipe_bof = 1, .short_xfer_ok = 1, },
		.bufsize = 0,	/* use wMaxPacketSize */
		.callback = &cp2112iic_intr_read_callback,
	},
};

static int
cp2112iic_send_req(struct cp2112iic_softc *sc, const void *data,
    uint16_t len)
{
	int err;

	mtx_assert(&sc->io.lock, MA_OWNED);
	KASSERT(sc->io.out.done == 0, ("%s: conflicting request", __func__));

	sc->io.out.data = data;
	sc->io.out.len = len;

	DTRACE_PROBE1(send__req, uint8_t, *(const uint8_t *)data);

	usbd_transfer_start(sc->xfers[CP2112_INTR_OUT]);

	while (!sc->io.out.done)
		cv_wait(&sc->io.cv, &sc->io.lock);

	usbd_transfer_stop(sc->xfers[CP2112_INTR_OUT]);

	sc->io.out.done = 0;
	sc->io.out.data = NULL;
	sc->io.out.len = 0;
	err = sc->io.out.error;
	if (err != 0) {
		device_printf(sc->dev, "output report 0x%02x failed: %d\n",
		    *(const uint8_t*)data, err);
	}
	return (err);
}

static int
cp2112iic_req_resp(struct cp2112iic_softc *sc, const void *req_data,
    uint16_t req_len, void *resp_data, uint16_t resp_len)
{
	int err;

	mtx_assert(&sc->io.lock, MA_OWNED);

	/*
	 * Prepare to receive a response interrupt even before the
	 * request transfer is confirmed (USB_ST_TRANSFERED).
	 */
	KASSERT(sc->io.in.done == 0, ("%s: conflicting request", __func__));
	sc->io.in.len = resp_len;
	sc->io.in.data = resp_data;

	err = cp2112iic_send_req(sc, req_data, req_len);
	if (err != 0) {
		sc->io.in.len = 0;
		sc->io.in.data = NULL;
		return (err);
	}

	while (!sc->io.in.done)
		cv_wait(&sc->io.cv, &sc->io.lock);

	err = sc->io.in.error;
	sc->io.in.done = 0;
	sc->io.in.error = 0;
	sc->io.in.len = 0;
	sc->io.in.data = NULL;
	return (err);
}

static int
cp2112iic_check_req_status(struct cp2112iic_softc *sc)
{
	struct i2c_xfer_status_req xfer_status_req;
	struct i2c_xfer_status_resp xfer_status_resp;
	int err;

	mtx_assert(&sc->io.lock, MA_OWNED);

	do {
		xfer_status_req.id = CP2112_REQ_SMB_XFER_STATUS_REQ;
		xfer_status_req.request = 1;
		err = cp2112iic_req_resp(sc,
		    &xfer_status_req, sizeof(xfer_status_req),
		    &xfer_status_resp, sizeof(xfer_status_resp));

		if (xfer_status_resp.id != CP2112_REQ_SMB_XFER_STATUS_RESP) {
			device_printf(sc->dev,
			    "unexpected response 0x%02x to status request\n",
			    xfer_status_resp.id);
			err = IIC_EBUSERR;
			goto out;
		}

		DTRACE_PROBE4(xfer__status, uint8_t, xfer_status_resp.status0,
		    uint8_t, xfer_status_resp.status1,
		    uint16_t, be16toh(xfer_status_resp.status2),
		    uint16_t, be16toh(xfer_status_resp.status3));

		switch (xfer_status_resp.status0) {
		case CP2112_IIC_STATUS0_IDLE:
			err = IIC_ESTATUS;
			break;
		case CP2112_IIC_STATUS0_BUSY:
			err = ERESTART;	/* non-I2C, special handling */
			break;
		case CP2112_IIC_STATUS0_CMP:
			err = IIC_NOERR;
			break;
		case CP2112_IIC_STATUS0_ERROR:
			switch (xfer_status_resp.status1) {
			case CP2112_IIC_STATUS1_TIMEOUT_NACK:
				err = IIC_ENOACK;
				break;
			case CP2112_IIC_STATUS1_TIMEOUT_BUS:
				err = IIC_ETIMEOUT;
				break;
			case CP2112_IIC_STATUS1_ARB_LOST:
				err = IIC_EBUSBSY;
				break;
			default:
				device_printf(sc->dev,
				    "i2c error, status = 0x%02x\n",
				    xfer_status_resp.status1);
				err = IIC_ESTATUS;
				break;
			}
			break;
		default:
			device_printf(sc->dev,
			    "unknown i2c xfer status0 0x%02x\n",
			    xfer_status_resp.status0);
			err = IIC_EBUSERR;
			break;
		}

	} while (err == ERESTART);
out:
	return (err);
}

static int
cp2112iic_read_data(struct cp2112iic_softc *sc, void *data, uint16_t in_len,
    uint16_t *out_len)
{
	struct i2c_data_read_force_send_req data_read_force_send;
	struct i2c_data_read_resp data_read_resp;
	int err;

	mtx_assert(&sc->io.lock, MA_OWNED);

	/*
	 * Prepare to receive a response interrupt even before the request
	 * transfer is confirmed (USB_ST_TRANSFERED).
	 */

	if (in_len > sizeof(data_read_resp.data))
		in_len = sizeof(data_read_resp.data);
	data_read_force_send.id = CP2112_REQ_SMB_READ_FORCE_SEND;
	data_read_force_send.len = htobe16(in_len);
	err = cp2112iic_req_resp(sc,
	    &data_read_force_send, sizeof(data_read_force_send),
	    &data_read_resp, sizeof(data_read_resp));
	if (err != 0)
		goto out;

	if (data_read_resp.id != CP2112_REQ_SMB_READ_RESPONSE) {
		device_printf(sc->dev,
		    "unexpected response 0x%02x to data read request\n",
		    data_read_resp.id);
		err = IIC_EBUSERR;
		goto out;
	}

	DTRACE_PROBE2(read__response, uint8_t, data_read_resp.status,
	    uint8_t, data_read_resp.len);

	/*
	 * We expect either the request completed status or, more typical for
	 * this driver, the bus idle status because of the preceding
	 * Force Read Status command (which is not an I2C request).
	 */
	if (data_read_resp.status != CP2112_IIC_STATUS0_CMP &&
	    data_read_resp.status != CP2112_IIC_STATUS0_IDLE) {
		err = IIC_EBUSERR;
		goto out;
	}
	if (data_read_resp.len > in_len) {
		device_printf(sc->dev, "device returns more data than asked\n");
		err = IIC_EOVERFLOW;
		goto out;
	}

	*out_len = data_read_resp.len;
	if (*out_len > 0)
		memcpy(data, data_read_resp.data, *out_len);
out:
	return (err);
}

static int
cp2112iic_transfer(device_t dev, struct iic_msg *msgs, uint32_t nmsgs)
{
	struct cp2112iic_softc *sc = device_get_softc(dev);
	struct cp2112_softc *psc = device_get_softc(device_get_parent(dev));
	const char *reason = NULL;
	uint32_t i;
	uint16_t read_off, to_read;
	int err;

	/*
	 * The hardware interface imposes limits on allowed I2C messages.
	 * It is not possible to explicitly send a start or stop.
	 * It is not possible to do a zero length transfer.
	 * For this reason it's impossible to send a message with no data
	 * at all (like an SMBus quick message).
	 * Each read or write transfer beginning with the start condition
	 * and ends with the stop condition.  The only exception is that
	 * it is possible to have a write transfer followed by a read
	 * transfer to the same slave with the repeated start condition
	 * between them.
	 */
	for (i = 0; i < nmsgs; i++) {
		if (i == 0 && (msgs[i].flags & IIC_M_NOSTART) != 0) {
			reason = "first message without start";
			break;
		}
		if (i == nmsgs - 1 && (msgs[i].flags & IIC_M_NOSTOP) != 0) {
			reason = "last message without stop";
			break;
		}
		if (msgs[i].len == 0) {
			reason = "message with no data";
			break;
		}
		if ((msgs[i].flags & IIC_M_RD) != 0 &&
		    msgs[i].len > CP2112_IIC_MAX_READ_LEN) {
			reason = "too long read";
			break;
		}
		if ((msgs[i].flags & IIC_M_RD) == 0 &&
		    msgs[i].len > SIZEOF_FIELD(i2c_write_req, data)) {
			reason = "too long write";
			break;
		}
		if ((msgs[i].flags & IIC_M_NOSTART) != 0) {
			reason = "message without start or repeated start";
			break;
		}
		if ((msgs[i].flags & IIC_M_NOSTOP) != 0 &&
		    (msgs[i].flags & IIC_M_RD) != 0) {
			reason = "read without stop";
			break;
		}
		if ((msgs[i].flags & IIC_M_NOSTOP) != 0 &&
		    psc->sc_version < CP2112_IIC_REPSTART_VER) {
			reason = "write without stop";
			break;
		}
		if ((msgs[i].flags & IIC_M_NOSTOP) != 0 &&
		    msgs[i].len > SIZEOF_FIELD(i2c_write_read_req, wdata)) {
			reason = "too long write without stop";
			break;
		}
		if (i > 0) {
			if ((msgs[i - 1].flags & IIC_M_NOSTOP) != 0 &&
			    msgs[i].slave != msgs[i - 1].slave) {
				reason = "change of slave without stop";
				break;
			}
			if ((msgs[i - 1].flags & IIC_M_NOSTOP) != 0 &&
			    (msgs[i].flags & IIC_M_RD) == 0) {
				reason = "write after repeated start";
				break;
			}
		}
	}
	if (reason != NULL) {
		if (bootverbose)
			device_printf(dev, "unsupported i2c message: %s\n",
			    reason);
		return (IIC_ENOTSUPP);
	}

	mtx_lock(&sc->io.lock);

	for (i = 0; i < nmsgs; i++) {
		if (i + 1 < nmsgs && (msgs[i].flags & IIC_M_NOSTOP) != 0) {
			/*
			 * Combine <write><repeated start><read> into a single
			 * CP2112 operation.
			 */
			struct i2c_write_read_req req;

			KASSERT((msgs[i].flags & IIC_M_RD) == 0,
			    ("read without stop"));
			KASSERT((msgs[i + 1].flags & IIC_M_RD) != 0,
			    ("write after write without stop"));
			req.id = CP2112_REQ_SMB_WRITE_READ;
			req.slave = msgs[i].slave & ~LSB;
			to_read = msgs[i + 1].len;
			req.rlen = htobe16(to_read);
			req.wlen = msgs[i].len;
			memcpy(req.wdata, msgs[i].buf, msgs[i].len);
			err = cp2112iic_send_req(sc, &req, msgs[i].len + 5);

			/*
			 * The next message is already handled.
			 * Also needed for read data to go into the right msg.
			 */
			i++;
		} else if ((msgs[i].flags & IIC_M_RD) != 0) {
			struct i2c_read_req req;

			req.id = CP2112_REQ_SMB_READ;
			req.slave = msgs[i].slave & ~LSB;
			to_read = msgs[i].len;
			req.len = htobe16(to_read);
			err = cp2112iic_send_req(sc, &req, sizeof(req));
		} else {
			struct i2c_write_req req;

			req.id = CP2112_REQ_SMB_WRITE;
			req.slave = msgs[i].slave & ~LSB;
			req.len = msgs[i].len;
			memcpy(req.data, msgs[i].buf, msgs[i].len);
			to_read = 0;
			err = cp2112iic_send_req(sc, &req, msgs[i].len + 3);
		}
		if (err != 0)
			break;

		err = cp2112iic_check_req_status(sc);
		if (err != 0)
			break;

		read_off = 0;
		while (to_read > 0) {
			uint16_t act_read;

			err = cp2112iic_read_data(sc, msgs[i].buf + read_off,
			    to_read, &act_read);
			if (err != 0)
				break;
			KASSERT(act_read <= to_read, ("cp2112iic_read_data "
			    "returned more data than asked"));
			read_off += act_read;
			to_read -= act_read;
		}
		if (err != 0)
			break;
	}

	mtx_unlock(&sc->io.lock);
	return (err);
}

static int
cp2112iic_reset(device_t dev, u_char speed, u_char addr, u_char *oldaddr)
{
	struct i2c_cfg_req i2c_cfg;
	struct cp2112iic_softc *sc;
	device_t cp2112;
	u_int busfreq;
	int err;

	sc = device_get_softc(dev);
	cp2112 = device_get_parent(dev);
	if (sc->iicbus_dev == NULL)
		busfreq = 100000;
	else
		busfreq = IICBUS_GET_FREQUENCY(sc->iicbus_dev, speed);

	err = cp2112_get_report(cp2112, CP2112_REQ_SMB_CFG,
	    &i2c_cfg, sizeof(i2c_cfg));
	if (err != 0) {
		device_printf(dev, "failed to get CP2112_REQ_SMB_CFG report\n");
		return (err);
	}

	if (oldaddr != NULL)
		*oldaddr = i2c_cfg.slave_addr;
	/*
	 * For simplicity we do not enable Auto Send Read
	 * because of erratum CP2112_E101 (fixed in version 3).
	 *
	 * TODO: set I2C parameters based on configuration preferences:
	 * - read and write timeouts (no timeout by default),
	 * - SCL low timeout (disabled by default),
	 * etc.
	 *
	 * TODO: should the device reset request (0x01) be sent?
	 * If the device disconnects as a result, then no.
	 */
	i2c_cfg.speed = htobe32(busfreq);
	if (addr != 0)
		i2c_cfg.slave_addr = addr;
	i2c_cfg.auto_send_read = 0;
	i2c_cfg.retry_count = htobe16(1);
	i2c_cfg.scl_low_timeout = 0;
	if (bootverbose) {
		device_printf(dev, "speed %d Hz\n", be32toh(i2c_cfg.speed));
		device_printf(dev, "slave addr 0x%02x\n", i2c_cfg.slave_addr);
		device_printf(dev, "auto send read %s\n",
		    i2c_cfg.auto_send_read ? "on" : "off");
		device_printf(dev, "write timeout %d ms (0 - disabled)\n",
		    be16toh(i2c_cfg.write_timeout));
		device_printf(dev, "read timeout %d ms (0 - disabled)\n",
		    be16toh(i2c_cfg.read_timeout));
		device_printf(dev, "scl low timeout %s\n",
		    i2c_cfg.scl_low_timeout ? "on" : "off");
		device_printf(dev, "retry count %d (0 - no limit)\n",
		    be16toh(i2c_cfg.retry_count));
	}
	err = cp2112_set_report(cp2112, CP2112_REQ_SMB_CFG,
	    &i2c_cfg, sizeof(i2c_cfg));
	if (err != 0) {
		device_printf(dev, "failed to set CP2112_REQ_SMB_CFG report\n");
		return (err);
	}
	return (0);
}

static int
cp2112iic_probe(device_t dev)
{
	device_set_desc(dev, "CP2112 I2C interface");
	return (BUS_PROBE_SPECIFIC);
}

static int
cp2112iic_attach(device_t dev)
{
	struct cp2112iic_softc *sc;
	struct cp2112_softc *psc;
	device_t cp2112;
	int err;

	sc = device_get_softc(dev);
	sc->dev = dev;
	cp2112 = device_get_parent(dev);
	psc = device_get_softc(cp2112);

	mtx_init(&sc->io.lock, "cp2112iic lock", NULL, MTX_DEF | MTX_RECURSE);
	cv_init(&sc->io.cv, "cp2112iic cv");

	err = usbd_transfer_setup(psc->sc_udev,
	    &psc->sc_iface_index, sc->xfers, cp2112iic_config,
	    nitems(cp2112iic_config), sc, &sc->io.lock);
	if (err != 0) {
		device_printf(dev, "usbd_transfer_setup failed %d\n", err);
		goto detach;
	}

	/* Prepare to receive interrupts. */
	mtx_lock(&sc->io.lock);
	usbd_transfer_start(sc->xfers[CP2112_INTR_IN]);
	mtx_unlock(&sc->io.lock);

	sc->iicbus_dev = device_add_child(dev, "iicbus", -1);
	if (sc->iicbus_dev == NULL) {
		device_printf(dev, "iicbus creation failed\n");
		err = ENXIO;
		goto detach;
	}
	bus_generic_attach(dev);
	return (0);

detach:
	cp2112iic_detach(dev);
	return (err);
}

static int
cp2112iic_detach(device_t dev)
{
	struct cp2112iic_softc *sc;
	int err;

	sc = device_get_softc(dev);
	err = bus_generic_detach(dev);
	if (err != 0)
		return (err);
	device_delete_children(dev);

	mtx_lock(&sc->io.lock);
	usbd_transfer_stop(sc->xfers[CP2112_INTR_IN]);
	mtx_unlock(&sc->io.lock);
	usbd_transfer_unsetup(sc->xfers, nitems(cp2112iic_config));

	cv_destroy(&sc->io.cv);
	mtx_destroy(&sc->io.lock);

	return (0);
}

static device_method_t cp2112hid_methods[] = {
	DEVMETHOD(device_probe,		cp2112_probe),
	DEVMETHOD(device_attach,	cp2112_attach),
	DEVMETHOD(device_detach,	cp2112_detach),

	DEVMETHOD_END
};

static driver_t cp2112hid_driver = {
	.name = "cp2112hid",
	.methods = cp2112hid_methods,
	.size = sizeof(struct cp2112_softc),
};

DRIVER_MODULE(cp2112hid, uhub, cp2112hid_driver, NULL, NULL);
MODULE_DEPEND(cp2112hid, usb, 1, 1, 1);
MODULE_VERSION(cp2112hid, 1);
USB_PNP_HOST_INFO(cp2112_devs);

static device_method_t cp2112gpio_methods[] = {
	/* Device */
	DEVMETHOD(device_probe,		cp2112gpio_probe),
	DEVMETHOD(device_attach,	cp2112gpio_attach),
	DEVMETHOD(device_detach,	cp2112gpio_detach),

	/* GPIO */
	DEVMETHOD(gpio_get_bus,		cp2112_gpio_get_bus),
	DEVMETHOD(gpio_pin_max,		cp2112_gpio_pin_max),
	DEVMETHOD(gpio_pin_get,		cp2112_gpio_pin_get),
	DEVMETHOD(gpio_pin_set,		cp2112_gpio_pin_set),
	DEVMETHOD(gpio_pin_toggle,	cp2112_gpio_pin_toggle),
	DEVMETHOD(gpio_pin_getname,	cp2112_gpio_pin_getname),
	DEVMETHOD(gpio_pin_getcaps,	cp2112_gpio_pin_getcaps),
	DEVMETHOD(gpio_pin_getflags,	cp2112_gpio_pin_getflags),
	DEVMETHOD(gpio_pin_setflags,	cp2112_gpio_pin_setflags),

	DEVMETHOD_END
};

static driver_t cp2112gpio_driver = {
	.name = "gpio",
	.methods = cp2112gpio_methods,
	.size = sizeof(struct cp2112gpio_softc),
};

DRIVER_MODULE(cp2112gpio, cp2112hid, cp2112gpio_driver, NULL, NULL);
MODULE_DEPEND(cp2112gpio, cp2112hid, 1, 1, 1);
MODULE_DEPEND(cp2112gpio, gpiobus, 1, 1, 1);
MODULE_VERSION(cp2112gpio, 1);

static device_method_t cp2112iic_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe, cp2112iic_probe),
	DEVMETHOD(device_attach, cp2112iic_attach),
	DEVMETHOD(device_detach, cp2112iic_detach),

	/* I2C methods */
	DEVMETHOD(iicbus_transfer, cp2112iic_transfer),
	DEVMETHOD(iicbus_reset, cp2112iic_reset),
	DEVMETHOD(iicbus_callback, iicbus_null_callback),

	DEVMETHOD_END
};

static driver_t cp2112iic_driver = {
	"iichb",
	cp2112iic_methods,
	sizeof(struct cp2112iic_softc)
};

DRIVER_MODULE(cp2112iic, cp2112hid, cp2112iic_driver, NULL, NULL);
MODULE_DEPEND(cp2112iic, cp2112hid, 1, 1, 1);
MODULE_DEPEND(cp2112iic, iicbus, IICBUS_MINVER, IICBUS_PREFVER, IICBUS_MAXVER);
MODULE_VERSION(cp2112iic, 1);
