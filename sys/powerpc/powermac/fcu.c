/*-
 * Copyright (c) 2010 Andreas Tobler
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/systm.h>
#include <sys/module.h>
#include <sys/callout.h>
#include <sys/conf.h>
#include <sys/cpu.h>
#include <sys/ctype.h>
#include <sys/kernel.h>
#include <sys/reboot.h>
#include <sys/rman.h>
#include <sys/sysctl.h>
#include <sys/limits.h>

#include <machine/bus.h>
#include <machine/md_var.h>

#include <dev/iicbus/iicbus.h>
#include <dev/iicbus/iiconf.h>

#include <dev/ofw/openfirm.h>
#include <dev/ofw/ofw_bus.h>

/* FCU registers
 * /u3@0,f8000000/i2c@f8001000/fan@15e
 */
#define FCU_RPM_FAIL      0x0b      /* fans states in bits 0<1-6>7 */
#define FCU_RPM_AVAILABLE 0x0c
#define FCU_RPM_ACTIVE    0x0d
#define FCU_RPM_READ(x)   0x11 + (x) * 2
#define FCU_RPM_SET(x)    0x10 + (x) * 2

#define FCU_PWM_FAIL      0x2b
#define FCU_PWM_AVAILABLE 0x2c
#define FCU_PWM_ACTIVE    0x2d
#define FCU_PWM_READ(x)   0x31 + (x) * 2
#define FCU_PWM_SET(x)    0x30 + (x) * 2

struct fcu_fan {
	int     id;
	cell_t	min_rpm;
	cell_t	max_rpm;
	char	location[32];
	enum {
		FCU_FAN_RPM,
		FCU_FAN_PWM
	} type;
	int     setpoint;
};

struct fcu_softc {
	device_t		sc_dev;
	struct intr_config_hook enum_hook;
	uint32_t                sc_addr;
	struct fcu_fan		*sc_fans;
	int			sc_nfans;
};

static int fcu_rpm_shift;

/* Regular bus attachment functions */
static int  fcu_probe(device_t);
static int  fcu_attach(device_t);

/* Utility functions */
static void fcu_attach_fans(device_t dev);
static int  fcu_fill_fan_prop(device_t dev);
static int  fcu_fan_set_rpm(device_t dev, struct fcu_fan *fan, int rpm);
static int  fcu_fan_get_rpm(device_t dev, struct fcu_fan *fan, int *rpm);
static int  fcu_fanrpm_sysctl(SYSCTL_HANDLER_ARGS);
static void fcu_start(void *xdev);
static int  fcu_write(device_t dev, uint32_t addr, uint8_t reg, uint8_t *buf,
		      int len);
static int  fcu_read_1(device_t dev, uint32_t addr, uint8_t reg, uint8_t *data);

static device_method_t  fcu_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		fcu_probe),
	DEVMETHOD(device_attach,	fcu_attach),
	{ 0, 0 },
};

static driver_t fcu_driver = {
	"fcu",
	fcu_methods,
	sizeof(struct fcu_softc)
};

static devclass_t fcu_devclass;

DRIVER_MODULE(fcu, iicbus, fcu_driver, fcu_devclass, 0, 0);
MALLOC_DEFINE(M_FCU, "fcu", "FCU Sensor Information");

static int
fcu_write(device_t dev, uint32_t addr, uint8_t reg, uint8_t *buff,
	  int len)
{
	unsigned char buf[4];
	struct iic_msg msg[] = {
		{ addr, IIC_M_WR, 0, buf }
	};

	msg[0].len = len + 1;
	buf[0] = reg;
	memcpy(buf + 1, buff, len);
	if (iicbus_transfer(dev, msg, 1) != 0) {
		device_printf(dev, "iicbus write failed\n");
		return (EIO);
	}

	return (0);

}

static int
fcu_read_1(device_t dev, uint32_t addr, uint8_t reg, uint8_t *data)
{
	uint8_t buf[4];

	struct iic_msg msg[2] = {
	    { addr, IIC_M_WR | IIC_M_NOSTOP, 1, &reg },
	    { addr, IIC_M_RD, 1, buf },
	};

	if (iicbus_transfer(dev, msg, 2) != 0) {
		device_printf(dev, "iicbus read failed\n");
		return (EIO);
	}

	*data = *((uint8_t*)buf);

	return (0);
}

static int
fcu_probe(device_t dev)
{
	const char  *name, *compatible;
	struct fcu_softc *sc;

	name = ofw_bus_get_name(dev);
	compatible = ofw_bus_get_compat(dev);

	if (!name)
		return (ENXIO);

	if (strcmp(name, "fan") != 0 || strcmp(compatible, "fcu") != 0)
		return (ENXIO);

	sc = device_get_softc(dev);
	sc->sc_dev = dev;
	sc->sc_addr = iicbus_get_addr(dev);

	device_set_desc(dev, "Apple Fan Control Unit");

	return (0);
}

static int
fcu_attach(device_t dev)
{
	struct fcu_softc *sc;

	sc = device_get_softc(dev);

	sc->enum_hook.ich_func = fcu_start;
	sc->enum_hook.ich_arg = dev;

	/* We have to wait until interrupts are enabled. I2C read and write
	 * only works if the interrupts are available.
	 * The unin/i2c is controlled by the htpic on unin. But this is not
	 * the master. The openpic on mac-io is controlling the htpic.
	 * This one gets attached after the mac-io probing and then the
	 * interrupts will be available.
	 */

	if (config_intrhook_establish(&sc->enum_hook) != 0)
		return (ENOMEM);

	return (0);
}

static void
fcu_start(void *xdev)
{
	unsigned char buf[1] = { 0xff };
	struct fcu_softc *sc;

	device_t dev = (device_t)xdev;

	sc = device_get_softc(dev);

	/* Start the fcu device. */
	fcu_write(sc->sc_dev, sc->sc_addr, 0xe, buf, 1);
	fcu_write(sc->sc_dev, sc->sc_addr, 0x2e, buf, 1);
	fcu_read_1(sc->sc_dev, sc->sc_addr, 0, buf);
	fcu_rpm_shift = (buf[0] == 1) ? 2 : 3;

	device_printf(dev, "FCU initialized, RPM shift: %d\n",
		      fcu_rpm_shift);

	/* Detect and attach child devices. */

	fcu_attach_fans(dev);

	config_intrhook_disestablish(&sc->enum_hook);

}

static int
fcu_fan_set_rpm(device_t dev, struct fcu_fan *fan, int rpm)
{
	uint8_t reg;
	struct fcu_softc *sc;
	unsigned char buf[2];

	sc = device_get_softc(dev);

	/* Clamp to allowed range */
	rpm = max(fan->min_rpm, rpm);
	rpm = min(fan->max_rpm, rpm);

	if (fan->type == FCU_FAN_RPM) {
		reg = FCU_RPM_SET(fan->id);
		fan->setpoint = rpm;
	} else if (fan->type == FCU_FAN_PWM) {
		reg = FCU_PWM_SET(fan->id);
		if (rpm > 3500)
			rpm = 3500;
		if (rpm < 500)
			rpm = 500;
		fan->setpoint = rpm;
		/* PWM 30: 550 rpm, PWM 255: 3400 rpm.  */
		rpm = (rpm * 255) / 3500;
	} else {
		device_printf(dev, "Unknown fan type: %d\n", fan->type);
		return (EIO);
	}

	if (fan->type == FCU_FAN_RPM) {
		buf[0] = rpm >> (8 - fcu_rpm_shift);
		buf[1] = rpm << fcu_rpm_shift;
		fcu_write(sc->sc_dev, sc->sc_addr, reg, buf, 2);
	} else {
		buf[0] = rpm;
		fcu_write(sc->sc_dev, sc->sc_addr, reg, buf, 1);
	}

	return (0);
}

static int
fcu_fan_get_rpm(device_t dev, struct fcu_fan *fan, int *rpm)
{
	uint8_t reg;
	struct fcu_softc *sc;
	uint8_t buff[2] = { 0, 0 };
	uint8_t active = 0, avail = 0, fail = 0;

	sc = device_get_softc(dev);

	if (fan->type == FCU_FAN_RPM) {
		/* Check if the fan is available. */
		reg = FCU_RPM_AVAILABLE;
		fcu_read_1(sc->sc_dev, sc->sc_addr, reg, &avail);
		if ((avail & (1 << fan->id)) == 0) {
			device_printf(dev, "RPM Fan not available ID: %d\n",
				      fan->id);
			return (EIO);
		}
		/* Check if we have a failed fan. */
		reg = FCU_RPM_FAIL;
		fcu_read_1(sc->sc_dev, sc->sc_addr, reg, &fail);
		if ((fail & (1 << fan->id)) != 0) {
			device_printf(dev, "RPM Fan failed ID: %d\n", fan->id);
			return (EIO);
		}
		/* Check if fan is active. */
		reg = FCU_RPM_ACTIVE;
		fcu_read_1(sc->sc_dev, sc->sc_addr, reg, &active);
		if ((active & (1 << fan->id)) == 0) {
			device_printf(dev, "RPM Fan not active ID: %d\n",
				      fan->id);
			return (ENXIO);
		}
		reg = FCU_RPM_READ(fan->id);
	} else if (fan->type == FCU_FAN_PWM) {
		/* Check if the fan is available. */
		reg = FCU_PWM_AVAILABLE;
		fcu_read_1(sc->sc_dev, sc->sc_addr, reg, &avail);
		if ((avail & (1 << fan->id)) == 0) {
			device_printf(dev, "PWM Fan not available ID: %d\n",
				      fan->id);
			return (EIO);
		}
		/* Check if we have a failed fan. */
		reg = FCU_PWM_FAIL;
		fcu_read_1(sc->sc_dev, sc->sc_addr, reg, &fail);
		if ((fail & (1 << fan->id)) != 0) {
			device_printf(dev, "PWM Fan failed ID: %d\n", fan->id);
			return (EIO);
		}
		/* Check if fan is active. */
		reg = FCU_PWM_ACTIVE;
		fcu_read_1(sc->sc_dev, sc->sc_addr, reg, &active);
		if ((active & (1 << fan->id)) == 0) {
			device_printf(dev, "PWM Fan not active ID: %d\n",
				      fan->id);
			return (ENXIO);
		}
		reg = FCU_PWM_READ(fan->id);
	} else {
		device_printf(dev, "Unknown fan type: %d\n", fan->type);
		return (EIO);
	}

	/* It seems that we can read the fans rpm. */
	fcu_read_1(sc->sc_dev, sc->sc_addr, reg, buff);

	*rpm = (buff[0] << (8 - fcu_rpm_shift)) | buff[1] >> fcu_rpm_shift;

	return (0);
}

/*
 * This function returns the number of fans. If we call it the second time
 * and we have allocated memory for sc->sc_fans, we fill in the properties.
 */
static int
fcu_fill_fan_prop(device_t dev)
{
	phandle_t child;
	struct fcu_softc *sc;
	u_int id[8];
	char location[96];
	char type[64];
	int i = 0, j, len = 0, prop_len, prev_len = 0;

	sc = device_get_softc(dev);

	child = ofw_bus_get_node(dev);

	/* Fill the fan location property. */
	prop_len = OF_getprop(child, "hwctrl-location", location,
			      sizeof(location));
	while (len < prop_len) {
		if (sc->sc_fans != NULL) {
			strcpy(sc->sc_fans[i].location, location + len);
		}
		prev_len = strlen(location + len) + 1;
		len += prev_len;
		i++;
	}
	if (sc->sc_fans == NULL)
		return (i);

	/* Fill the fan type property. */
	len = 0;
	i = 0;
	prev_len = 0;
	prop_len = OF_getprop(child, "hwctrl-type", type, sizeof(type));
	while (len < prop_len) {
		if (strcmp(type + len, "fan-rpm") == 0)
			sc->sc_fans[i].type = FCU_FAN_RPM;
		else
			sc->sc_fans[i].type = FCU_FAN_PWM;
		prev_len = strlen(type + len) + 1;
		len += prev_len;
		i++;
	}

	/* Fill the fan ID property. */
	prop_len = OF_getprop(child, "hwctrl-id", id, sizeof(id));
	for (j = 0; j < i; j++)
		sc->sc_fans[j].id = ((id[j] >> 8) & 0x0f) % 8;

	return (i);
}

static int
fcu_fanrpm_sysctl(SYSCTL_HANDLER_ARGS)
{
	device_t fcu;
	struct fcu_softc *sc;
	struct fcu_fan *fan;
	int rpm = 0, error;

	fcu = arg1;
	sc = device_get_softc(fcu);
	fan = &sc->sc_fans[arg2];
	fcu_fan_get_rpm(fcu, fan, &rpm);
	error = sysctl_handle_int(oidp, &rpm, 0, req);

	if (error || !req->newptr)
		return (error);

	return (fcu_fan_set_rpm(fcu, fan, rpm));
}

static void
fcu_attach_fans(device_t dev)
{
	struct fcu_softc *sc;
	struct sysctl_oid *oid, *fanroot_oid;
	struct sysctl_ctx_list *ctx;
	phandle_t child;
	char sysctl_name[32];
	int i, j;

	sc = device_get_softc(dev);

	sc->sc_nfans = 0;

	child = ofw_bus_get_node(dev);

	/* Count the actual number of fans. */
	sc->sc_nfans = fcu_fill_fan_prop(dev);

	device_printf(dev, "%d fans detected!\n", sc->sc_nfans);

	if (sc->sc_nfans == 0) {
		device_printf(dev, "WARNING: No fans detected!\n");
		return;
	}

	sc->sc_fans = malloc(sc->sc_nfans * sizeof(struct fcu_fan), M_FCU,
			     M_WAITOK | M_ZERO);

	ctx = device_get_sysctl_ctx(dev);
	fanroot_oid = SYSCTL_ADD_NODE(ctx,
	    SYSCTL_CHILDREN(device_get_sysctl_tree(dev)), OID_AUTO, "fans",
	    CTLFLAG_RD, 0, "FCU Fan Information");

	/* Now we can fill the properties into the allocated struct. */
	sc->sc_nfans = fcu_fill_fan_prop(dev);

	/* Add sysctls for the fans. */
	for (i = 0; i < sc->sc_nfans; i++) {
		for (j = 0; j < strlen(sc->sc_fans[i].location); j++) {
			sysctl_name[j] = tolower(sc->sc_fans[i].location[j]);
			if (isspace(sysctl_name[j]))
				sysctl_name[j] = '_';
		}
		sysctl_name[j] = 0;

		sc->sc_fans[i].min_rpm = 2400 >> fcu_rpm_shift;
		sc->sc_fans[i].max_rpm = 56000 >> fcu_rpm_shift;
		fcu_fan_get_rpm(dev, &sc->sc_fans[i], &sc->sc_fans[i].setpoint);

		oid = SYSCTL_ADD_NODE(ctx, SYSCTL_CHILDREN(fanroot_oid),
				      OID_AUTO, sysctl_name, CTLFLAG_RD, 0,
				      "Fan Information");
		SYSCTL_ADD_INT(ctx, SYSCTL_CHILDREN(oid), OID_AUTO, "minrpm",
			       CTLTYPE_INT | CTLFLAG_RD,
			       &(sc->sc_fans[i].min_rpm), sizeof(cell_t),
			       "Minimum allowed RPM");
		SYSCTL_ADD_INT(ctx, SYSCTL_CHILDREN(oid), OID_AUTO, "maxrpm",
			       CTLTYPE_INT | CTLFLAG_RD,
			       &(sc->sc_fans[i].max_rpm), sizeof(cell_t),
			       "Maximum allowed RPM");
		/* I use i to pass the fan id. */
		SYSCTL_ADD_PROC(ctx, SYSCTL_CHILDREN(oid), OID_AUTO, "rpm",
				CTLTYPE_INT | CTLFLAG_RW, dev, i,
				fcu_fanrpm_sysctl, "I", "Fan RPM");
	}

	/* Dump fan location, type & RPM. */
	if (bootverbose) {
		device_printf(dev, "Fans\n");
		for (i = 0; i < sc->sc_nfans; i++) {
			device_printf(dev, "Location: %s type: %d ID: %d RPM: %d\n",
				      sc->sc_fans[i].location,
				      sc->sc_fans[i].type, sc->sc_fans[i].id,
				      sc->sc_fans[i].setpoint);
	    }
	}
}
