/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2023 Beckhoff Automation GmbH & Co. KG
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
#include "opt_platform.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/clock.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/module.h>
#include <sys/sysctl.h>

#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include <dev/iicbus/iiconf.h>
#include <dev/iicbus/iicbus.h>

#include "clock_if.h"
#include "iicbus_if.h"

/* Date registers */
#define	RV3032_SECS_100TH	0x00
#define	RV3032_SECS		0x01
#define	RV3032_MINS		0x02
#define	RV3032_HOURS		0x03
#define	RV3032_WEEKDAY		0x04
#define	RV3032_DATE		0x05
#define	RV3032_MONTH		0x06
#define	RV3032_YEAR		0x07

/* Alarm registers */
#define	RV3032_ALARM_MINUTES	0x08
#define	RV3032_ALARM_HOURS	0x09
#define	RV3032_ALARM_DATE	0x0A

/* Periodic countdown timer registers */
#define	RV3032_TIMER_VALUE0	0x0B
#define	RV3032_TIMER_VALUE1	0x0C

/* Status register */
#define	RV3032_STATUS		0x0D
#define	 RV3032_STATUS_VLF	(1 << 0)	/* Voltage Low Flag */
#define	 RV3032_STATUS_PORF	(1 << 1)	/* Power On Reset Flag */
#define	 RV3032_STATUS_EVF	(1 << 2)	/* External eVent Flag */
#define	 RV3032_STATUS_AF	(1 << 3)	/* Alarm Flag */
#define	 RV3032_STATUS_TF	(1 << 4)	/* periodic countdown Timer Flag */
#define	 RV3032_STATUS_UF	(1 << 5)	/* periodic time Update Flag */
#define	 RV3032_STATUS_TLF 	(1 << 6)	/* Temperature Low Flag */
#define	 RV3032_STATUS_THF	(1 << 7)	/* Temperature High Flag */

/* Temperature registers */
#define	RV3032_TEMP_LSB		0x0E
#define	 RV3032_TEMP_LSB_BSF	(1 << 0)
#define	 RV3032_TEMP_LSB_CLKF	(1 << 1)
#define	 RV3032_TEMP_LSB_EEBUSY	(1 << 2)
#define	 RV3032_TEMP_LSB_EEF	(1 << 3)
#define	 RV3032_TEMP_LSB_MASK	(0xF0)
#define	 RV3032_TEMP_LSB_SHIFT	4

#define	RV3032_TEMP_MSB		0x0F

#define	TEMP_DIV		16
#define	TEMP_C_TO_K		273

/* Control registers */
#define	RV3032_CTRL1		0x10
#define	 RV3032_CTRL1_TD_MASK	0x3	/* Timer clock frequency */
#define	 RV3032_CTRL1_TD_SHIFT	0
#define	 RV3032_CTRL1_TD_4096	0
#define	 RV3032_CTRL1_TD_64	1
#define	 RV3032_CTRL1_TD_1	2
#define	 RV3032_CTRL1_TD_1_60	3
#define	 RV3032_CTRL1_EERD	(1 << 2)	/* EEPROM memory refresh disable bit */
#define	 RV3032_CTRL1_TE	(1 << 3)	/* Periodic countdown timer enable bit */
#define	 RV3032_CTRL1_USEL	(1 << 4)	/* Update interrupt select bit */
#define	 RV3032_CTRL1_GP0	(1 << 5)	/* General Purpose bit 0 */

#define	RV3032_CTRL2		0x11
#define	 RV3032_CTRL2_STOP	(1 << 0)	/* Stop bit */
#define	 RV3032_CTRL2_GP1	(1 << 1)	/* General Purpose bit 1 */
#define	 RV3032_CTRL2_EIE	(1 << 2)	/* External event interrupt enable bit */
#define	 RV3032_CTRL2_AIE	(1 << 3)	/* Alarm interrupt enable bit */
#define	 RV3032_CTRL2_TIE	(1 << 4)	/* Periodic countdown timer interrupt enable bit */
#define	 RV3032_CTRL2_UIE	(1 << 5)	/* Periodic time update interrupt enable bit */
#define	 RV3032_CTRL2_CLKIE	(1 << 6)	/* Interrupt Controlled Clock Output Enable bit */
#define	RV3032_CTRL3		0x12
#define	 RV3032_CTRL3_TLIE	(1 << 0)	/* Temperature Low Interrupt Enable bit */
#define	 RV3032_CTRL3_THIE	(1 << 1)	/* Temperature High Interrupt Enable bit */
#define	 RV3032_CTRL3_TLE	(1 << 2)	/* Temperature Low Enable bit */
#define	 RV3032_CTRL3_THE	(1 << 3)	/* Temperature High Enable bit */
#define	 RV3032_CTRL3_BSIE	(1 << 4)	/* Backup Switchover Interrupt Enable bit */

/* EEPROM registers */
#define	RV3032_EEPROM_ADDRESS		0x3D
#define	RV3032_EEPROM_DATA		0x3E
#define	RV3032_EEPROM_COMMAND		0x3F
#define RV3032_EEPROM_CMD_UPDATE	0x11
#define RV3032_EEPROM_CMD_REFRESH	0x12
#define RV3032_EEPROM_CMD_WRITE_ONE	0x21
#define RV3032_EEPROM_CMD_READ_ONE	0x22

/* PMU register */
#define	RV3032_EEPROM_PMU	0xC0
#define	 RV3032_PMU_TCM_MASK	0x3
#define	 RV3032_PMU_TCM_SHIFT	0
#define	 RV3032_PMU_TCM_OFF	0
#define	 RV3032_PMU_TCM_175V	1
#define	 RV3032_PMU_TCM_30V	2
#define	 RV3032_PMU_TCM_45V	3
#define	 RV3032_PMU_TCR_MASK	0x3
#define	 RV3032_PMU_TCR_SHIFT	2
#define	 RV3032_PMU_TCR_06K	0
#define	 RV3032_PMU_TCR_2K	1
#define	 RV3032_PMU_TCR_7K	2
#define	 RV3032_PMU_TCR_12K	3
#define	 RV3032_PMU_BSM_MASK	0x3
#define	 RV3032_PMU_BSM_SHIFT	4
#define	 RV3032_PMU_BSM_OFF	0
#define	 RV3032_PMU_BSM_DSM	1
#define	 RV3032_PMU_BSM_LSM	2
#define	 RV3032_PMU_BSM_OFF2	3
#define	 RV3032_PMU_NCLKE	(1 << 6)

struct rv3032_softc {
	device_t		dev;
	device_t		busdev;
	struct intr_config_hook	init_hook;
};

struct rv3032_timeregs {
	uint8_t	secs;
	uint8_t	mins;
	uint8_t	hours;
	uint8_t	weekday;
	uint8_t	date;
	uint8_t	month;
	uint8_t	year;
};

static struct ofw_compat_data compat_data[] = {
	{"microcrystal,rv3032",	1},
	{NULL,			0},
};

static int
rv3032_update_register(struct rv3032_softc *sc, uint8_t reg, uint8_t value, uint8_t mask)
{
	int rv;
	uint8_t data;

	if ((rv = iicdev_readfrom(sc->dev, reg, &data, 1, IIC_WAIT)) != 0)
		return (rv);
	data &= mask;
	data |= value;
	if ((rv = iicdev_writeto(sc->dev, reg, &data, 1, IIC_WAIT)) != 0)
		return (rv);
	return (0);
}

static int
rv3032_eeprom_wait(struct rv3032_softc *sc)
{
	int rv, timeout;
	uint8_t data;

	for (timeout = 1000; timeout > 0; timeout--) {
		if ((rv = iicdev_readfrom(sc->dev, RV3032_TEMP_LSB, &data, sizeof(data), IIC_WAIT)) != 0)
		return (rv);
		if ((data & RV3032_TEMP_LSB_EEBUSY) == 0) {
			break;
		}
	}
	if (timeout == 0) {
		device_printf(sc->dev, "Timeout updating the eeprom\n");
		return (ETIMEDOUT);
	}
	/* Wait 1ms before allowing another eeprom access */
	DELAY(1000);

	return (0);
}

static int
rv3032_eeprom_disable(struct rv3032_softc *sc)
{
	int rv;

	if ((rv = rv3032_update_register(sc, RV3032_CTRL1, RV3032_CTRL1_EERD, ~RV3032_CTRL1_EERD)) != 0)
		return (rv);
	/* Wait 1ms before checking EBUSY */
	DELAY(1000);
	return (rv3032_eeprom_wait(sc));
}

static int
rv3032_eeprom_update(struct rv3032_softc *sc)
{
	int rv;
	uint8_t data;

	data = RV3032_EEPROM_CMD_UPDATE;
	if ((rv = iicdev_writeto(sc->dev, RV3032_EEPROM_COMMAND, &data, sizeof(data), IIC_WAIT)) != 0)
		return (rv);
	/* Wait 1ms before checking EBUSY */
	DELAY(1000);
	return (rv3032_eeprom_wait(sc));
}

static int
rv3032_eeprom_enable(struct rv3032_softc *sc)
{
	int rv;

	/* Restore eeprom refresh */
	if ((rv = rv3032_update_register(sc, RV3032_CTRL1, 0, ~RV3032_CTRL1_EERD)) != 0)
		return (rv);
	DELAY(1000);

	return (0);
}

static int
rv3032_update_cfg(struct rv3032_softc *sc)
{
	int rv;

	if ((rv = rv3032_eeprom_disable(sc)) != 0)
		return (rv);

	/* Save configuration in eeprom and re-enable it */
	if ((rv = rv3032_eeprom_update(sc)) != 0)
		return (rv);
	return (rv3032_eeprom_enable(sc));
}

static int
rv3032_temp_read(struct rv3032_softc *sc, int *temp)
{
	int rv, temp2;
	uint8_t data[2];

	if ((rv = iicdev_readfrom(sc->dev, RV3032_TEMP_LSB, &data, sizeof(data), IIC_WAIT)) != 0)
		return (rv);

	/* Wait for temp to be stable */
	*temp = (((data[0] & RV3032_TEMP_LSB_MASK) >> RV3032_TEMP_LSB_SHIFT) |
	    (data[1] << RV3032_TEMP_LSB_SHIFT));
	do {
		temp2 = *temp;
		*temp = (((data[0] & RV3032_TEMP_LSB_MASK) >> RV3032_TEMP_LSB_SHIFT) |
		    (data[1] << RV3032_TEMP_LSB_SHIFT));
	} while (temp2 != *temp);
	*temp = (*temp / TEMP_DIV) + TEMP_C_TO_K;
	return (0);
}

static int
rv3032_temp_sysctl(SYSCTL_HANDLER_ARGS)
{
	int error, temp;
	struct rv3032_softc *sc;

	sc = (struct rv3032_softc *)arg1;
	if (rv3032_temp_read(sc, &temp) != 0)
		return (EIO);
	error = sysctl_handle_int(oidp, &temp, 0, req);

	return (error);
}

static void
rv3032_init(void *arg)
{
	struct rv3032_softc	*sc;
	struct sysctl_ctx_list *ctx;
	struct sysctl_oid *tree_node;
	struct sysctl_oid_list *tree;
	int rv;

	sc = (struct rv3032_softc*)arg;
	config_intrhook_disestablish(&sc->init_hook);

	/* Set direct switching mode */
	rv3032_update_register(sc,
	    RV3032_EEPROM_PMU,
	    RV3032_PMU_BSM_DSM << RV3032_PMU_BSM_SHIFT,
	    RV3032_PMU_BSM_MASK);
	if ((rv = rv3032_update_cfg(sc)) != 0) {
		device_printf(sc->dev, "Cannot set to DSM mode (%d)\n", rv);
		return;
	}

	/* Register as clock source */
	clock_register_flags(sc->dev, 1000000, CLOCKF_SETTIME_NO_ADJ);
	clock_schedule(sc->dev, 1);

	ctx = device_get_sysctl_ctx(sc->dev);
	tree_node = device_get_sysctl_tree(sc->dev);
	tree = SYSCTL_CHILDREN(tree_node);
	SYSCTL_ADD_PROC(ctx, tree, OID_AUTO, "temperature",
	    CTLTYPE_INT | CTLFLAG_RD | CTLFLAG_MPSAFE, sc, 0,
	    rv3032_temp_sysctl, "IK0", "Current temperature");
	return;
}

static int
rv3032_probe(device_t dev)
{

	if (!ofw_bus_status_okay(dev))
		return (ENXIO);
	if (ofw_bus_search_compatible(dev, compat_data)->ocd_data != 0) {
		device_set_desc(dev, "Microcrystal RV3032");
		return (BUS_PROBE_DEFAULT);
	}
	return (ENXIO);
}

static int
rv3032_attach(device_t dev)
{
	struct rv3032_softc	*sc;
	
	sc = device_get_softc(dev);
	sc->dev = dev;
	sc->busdev = device_get_parent(sc->dev);

	sc->init_hook.ich_func = rv3032_init;
	sc->init_hook.ich_arg = sc;
	if (config_intrhook_establish(&sc->init_hook) != 0)
		return (ENOMEM);

	return (0);
}

static int
rv3032_detach(device_t dev)
{

	clock_unregister(dev);
	return (0);
}

static int
rv3032_gettime(device_t dev, struct timespec *ts)
{
	struct rv3032_softc *sc;
	struct rv3032_timeregs time_regs;
	struct clocktime ct;
	uint8_t status;
	int rv;

	sc = device_get_softc(dev);

	if ((rv = iicdev_readfrom(sc->dev, RV3032_STATUS, &status, sizeof(status), IIC_WAIT)) != 0)
		return (rv);
	if (status & (RV3032_STATUS_PORF | RV3032_STATUS_VLF))
		return (EINVAL);
	if ((rv = iicdev_readfrom(sc->dev, RV3032_SECS, &time_regs, sizeof(time_regs), IIC_WAIT)) != 0)
		return (rv);

	bzero(&ct, sizeof(ct));
	ct.sec = FROMBCD(time_regs.secs & 0x7f);
	ct.min = FROMBCD(time_regs.mins & 0x7f);
	ct.hour = FROMBCD(time_regs.hours & 0x3f);
	ct.day = FROMBCD(time_regs.date & 0x3f);
	ct.mon = FROMBCD(time_regs.month & 0x1f) - 1;
	ct.year = FROMBCD(time_regs.year) + 2000;

	return (clock_ct_to_ts(&ct, ts));
}

static int
rv3032_settime(device_t dev, struct timespec *ts)
{
	struct rv3032_softc *sc;
	struct rv3032_timeregs time_regs;
	struct clocktime ct;
	uint8_t status;
	int rv;

	sc = device_get_softc(dev);
	if ((rv = iicdev_readfrom(sc->dev, RV3032_STATUS, &status, sizeof(status), IIC_WAIT)) != 0)
		return (rv);

	clock_ts_to_ct(ts, &ct);

	time_regs.secs = TOBCD(ct.sec);
	time_regs.mins = TOBCD(ct.min);
	time_regs.hours = TOBCD(ct.hour);
	time_regs.date = TOBCD(ct.day);
	time_regs.month = TOBCD(ct.mon + 1);
	time_regs.year = TOBCD(ct.year - 2000);

	if ((rv = iicdev_writeto(sc->dev, RV3032_SECS, &time_regs, sizeof(time_regs), IIC_WAIT)) != 0)
		return (rv);

	/* Force a power on reset event so rv3032 reload the registers */
	status &= ~(RV3032_STATUS_PORF | RV3032_STATUS_VLF);
	if ((rv = iicdev_writeto(sc->dev, RV3032_STATUS, &status, sizeof(status), IIC_WAIT)) != 0)
		return (rv);
	return (0);
}

static device_method_t rv3032_methods[] = {
        /* device_if methods */
	DEVMETHOD(device_probe,		rv3032_probe),
	DEVMETHOD(device_attach,	rv3032_attach),
	DEVMETHOD(device_detach,	rv3032_detach),

        /* clock_if methods */
	DEVMETHOD(clock_gettime,	rv3032_gettime),
	DEVMETHOD(clock_settime,	rv3032_settime),

	DEVMETHOD_END,
};

static driver_t rv3032_driver = {
	"rv3032",
	rv3032_methods,
	sizeof(struct rv3032_softc),
};

DRIVER_MODULE(rv3032, iicbus, rv3032_driver, NULL, NULL);
MODULE_VERSION(rv3032, 1);
MODULE_DEPEND(rv3032, iicbus, IICBUS_MINVER, IICBUS_PREFVER, IICBUS_MAXVER);
IICBUS_FDT_PNP_INFO(compat_data);
