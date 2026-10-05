/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2001 Mitsuru IWASAKI
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

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/fcntl.h>
#include <sys/kernel.h>
#include <sys/malloc.h>

#include <contrib/dev/acpica/include/acpi.h>

#include <dev/acpica/acpivar.h>
#include <dev/acpica/acpiio.h>

#include <machine/apm_bios.h>

/*
 * APM driver emulation 
 */

#define	APM_UNKNOWN	0xff

static int apm_active;

static MALLOC_DEFINE(M_APMDEV, "apmdev", "APM device emulation");

static d_ioctl_t	apmioctl;

static struct cdevsw apm_cdevsw = {
	.d_version =	D_VERSION,
	.d_ioctl =	apmioctl,
	.d_name =	"apm",
};

static int
acpi_capm_convert_battstate(struct  acpi_battinfo *battp)
{
	int	state;

	state = APM_UNKNOWN;

	if (battp->state & ACPI_BATT_STAT_DISCHARG) {
		if (battp->cap >= 50)
			state = 0;	/* high */
		else
			state = 1;	/* low */
	}
	if (battp->state & ACPI_BATT_STAT_CRITICAL)
		state = 2;		/* critical */
	if (battp->state & ACPI_BATT_STAT_CHARGING)
		state = 3;		/* charging */

	/* If still unknown, determine it based on the battery capacity. */
	if (state == APM_UNKNOWN) {
		if (battp->cap >= 50)
			state = 0;	/* high */
		else
			state = 1;	/* low */
	}

	return (state);
}

static int
acpi_capm_convert_battflags(struct  acpi_battinfo *battp)
{
	int	flags;

	flags = 0;

	if (battp->cap >= 50)
		flags |= APM_BATT_HIGH;
	else {
		if (battp->state & ACPI_BATT_STAT_CRITICAL)
			flags |= APM_BATT_CRITICAL;
		else
			flags |= APM_BATT_LOW;
	}
	if (battp->state & ACPI_BATT_STAT_CHARGING)
		flags |= APM_BATT_CHARGING;
	if (battp->state == ACPI_BATT_STAT_NOT_PRESENT)
		flags = APM_BATT_NOT_PRESENT;

	return (flags);
}

static int
acpi_capm_get_info(apm_info_t aip)
{
	int	acline;
	struct	acpi_battinfo batt;

	aip->ai_infoversion = 1;
	aip->ai_major       = 1;
	aip->ai_minor       = 2;
	aip->ai_status      = apm_active;
	aip->ai_capabilities= 0xff00;	/* unknown */

	if (acpi_acad_get_acline(&acline))
		aip->ai_acline = 1;		/* no info -- on-line best guess */
	else
		aip->ai_acline = acline;	/* on/off */

	if (acpi_battery_get_battinfo(NULL, &batt) != 0) {
		aip->ai_batt_stat = 0;		/* "high" old I/F has no unknown state */
		aip->ai_batt_life = 255;	/* N/A, not -1 */
		aip->ai_batt_time = -1;		/* unknown */
		aip->ai_batteries = ~0U;	/* unknown */
	} else {
		aip->ai_batt_stat = acpi_capm_convert_battstate(&batt);
		aip->ai_batt_life = (batt.cap == -1) ? 255 : batt.cap;
		aip->ai_batt_time = (batt.min == -1) ? -1 : batt.min * 60;
		aip->ai_batteries = acpi_battery_get_units();
	}

	return (0);
}

static int
acpi_capm_get_pwstatus(apm_pwstatus_t app)
{
	device_t dev;
	int	acline, unit, error;
	struct	acpi_battinfo batt;

	if (app->ap_device != PMDV_ALLDEV &&
	    (app->ap_device < PMDV_BATT0 || app->ap_device > PMDV_BATT_ALL))
		return (1);

	if (app->ap_device == PMDV_ALLDEV)
		error = acpi_battery_get_battinfo(NULL, &batt);
	else {
		unit = app->ap_device - PMDV_BATT0;
		dev = devclass_get_device(devclass_find("battery"), unit);
		if (dev != NULL)
			error = acpi_battery_get_battinfo(dev, &batt);
		else
			error = ENXIO;
	}
	if (error)
		return (1);

	app->ap_batt_stat = acpi_capm_convert_battstate(&batt);
	app->ap_batt_flag = acpi_capm_convert_battflags(&batt);
	app->ap_batt_life = (batt.cap == -1) ? 255 : batt.cap;
	app->ap_batt_time = (batt.min == -1) ? -1 : batt.min * 60;

	if (acpi_acad_get_acline(&acline))
		app->ap_acline = 1;		/* no info -- on-line best guess */
	else
		app->ap_acline = acline;	/* on/off */

	return (0);
}

static int
apmioctl(struct cdev *dev, u_long cmd, caddr_t addr, int flag, struct thread *td)
{
	int	error;
	struct	acpi_softc *acpi_sc = dev->si_drv1;
	struct	apm_info info;
	apm_info_old_t aiop;

	error = 0;

	switch (cmd) {
	case APMIO_SUSPEND:
		if ((flag & FWRITE) == 0)
			return (EPERM);
		if (acpi_sc->acpi_next_stype == POWER_STYPE_AWAKE) {
			if (power_suspend_stype != POWER_STYPE_POWEROFF) {
				error = acpi_ReqSleepState(acpi_sc,
				    power_suspend_stype);
			} else {
				printf(
			"power off via apm suspend not supported\n");
				error = ENXIO;
			}
		}
		break;
	case APMIO_STANDBY:
		if ((flag & FWRITE) == 0)
			return (EPERM);
		if (acpi_sc->acpi_next_stype == POWER_STYPE_AWAKE) {
			if (power_standby_stype != POWER_STYPE_POWEROFF) {
				error = acpi_ReqSleepState(acpi_sc,
				    power_standby_stype);
			} else {
				printf(
			"power off via apm standby not supported\n");
				error = ENXIO;
			}
		}
		break;
	case APMIO_GETINFO_OLD:
		if (acpi_capm_get_info(&info))
			error = ENXIO;
		aiop = (apm_info_old_t)addr;
		aiop->ai_major = info.ai_major;
		aiop->ai_minor = info.ai_minor;
		aiop->ai_acline = info.ai_acline;
		aiop->ai_batt_stat = info.ai_batt_stat;
		aiop->ai_batt_life = info.ai_batt_life;
		aiop->ai_status = info.ai_status;
		break;
	case APMIO_GETINFO:
		if (acpi_capm_get_info((apm_info_t)addr))
			error = ENXIO;
		break;
	case APMIO_GETPWSTATUS:
		if (acpi_capm_get_pwstatus((apm_pwstatus_t)addr))
			error = ENXIO;
		break;
	case APMIO_ENABLE:
		if ((flag & FWRITE) == 0)
			return (EPERM);
		apm_active = 1;
		break;
	case APMIO_DISABLE:
		if ((flag & FWRITE) == 0)
			return (EPERM);
		apm_active = 0;
		break;
	case APMIO_HALTCPU:
		break;
	case APMIO_NOTHALTCPU:
		break;
	case APMIO_DISPLAY:
		if ((flag & FWRITE) == 0)
			return (EPERM);
		break;
	case APMIO_BIOS:
		if ((flag & FWRITE) == 0)
			return (EPERM);
		return (EOPNOTSUPP);
	default:
		error = EINVAL;
		break;
	}

	return (error);
}

void
acpi_apm_init(struct acpi_softc *sc)
{
	struct make_dev_args args;
	struct cdev *dev;

	make_dev_args_init(&args);
	args.mda_devsw = &apm_cdevsw;
	args.mda_uid = UID_ROOT;
	args.mda_gid = GID_OPERATOR;
	args.mda_mode = 0664;
	args.mda_si_drv1 = sc;
	args.mda_flags = MAKEDEV_ETERNAL;
	make_dev_s(&args, &dev, "apm");
}
