/*-
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
 *
 *      $FreeBSD$
 */

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/fcntl.h>
#include <sys/uio.h>

#include "acpi.h"

#include <dev/acpica/acpivar.h>
#include <dev/acpica/acpiio.h>

static device_t	acpi_dev;

/*
 * APM driver emulation 
 */

#if __FreeBSD_version < 500000
#include <sys/select.h>
#else
#include <sys/selinfo.h>
#endif

#include <machine/apm_bios.h>
#include <machine/pc/bios.h>

#include <i386/apm/apm.h>

static struct apm_softc	apm_softc;

static d_open_t apmopen;
static d_close_t apmclose;
static d_write_t apmwrite;
static d_ioctl_t apmioctl;
static d_poll_t apmpoll;

#define CDEV_MAJOR 39
static struct cdevsw apm_cdevsw = {
	/* open */	apmopen,
	/* close */	apmclose,
	/* read */	noread,
	/* write */	apmwrite,
	/* ioctl */	apmioctl,
	/* poll */	apmpoll,
	/* mmap */	nommap,
	/* strategy */	nostrategy,
	/* name */	"apm",
	/* maj */	CDEV_MAJOR,
	/* dump */	nodump,
	/* psize */	nopsize,
	/* flags */	0,
};

static int
acpi_capm_convert_battstate(struct  acpi_battinfo *battp)
{
	int	state;

	state = 0xff;	/* XXX unknown */

	if (battp->state & ACPI_BATT_STAT_DISCHARG) {
		if (battp->cap >= 50) {
			state = 0;	/* high */
		} else {
			state = 1;	/* low */
		}
	}
	if (battp->state & ACPI_BATT_STAT_CRITICAL) {
		state = 2;		/* critical */
	}
	if (battp->state & ACPI_BATT_STAT_CHARGING) {
		state = 3;		/* charging */
	}
	return (state);
}

static int
acpi_capm_convert_battflags(struct  acpi_battinfo *battp)
{
	int	flags;

	flags = 0;

	if (battp->cap >= 50) {
		flags |= APM_BATT_HIGH;
	} else {
		if (battp->state & ACPI_BATT_STAT_CRITICAL) {
			flags |= APM_BATT_CRITICAL;
		} else {
			flags |= APM_BATT_LOW;
		}
	}
	if (battp->state & ACPI_BATT_STAT_CHARGING) {
		flags |= APM_BATT_CHARGING;
	}
	if (battp->state == ACPI_BATT_STAT_NOT_PRESENT) {
		flags = APM_BATT_NOT_PRESENT;
	}

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
	aip->ai_status      = apm_softc.active;
	aip->ai_capabilities= 0xff00;	/* XXX unknown */

	if (acpi_acad_get_acline(&acline)) {
		aip->ai_acline = 0xff;		/* unknown */
	} else {
		aip->ai_acline = acline;	/* on/off */
	}

	if (acpi_battery_get_battinfo(-1, &batt)) {
		aip->ai_batt_stat = 0xff;	/* unknown */
		aip->ai_batt_life = 0xff;	/* unknown */
		aip->ai_batt_time = -1;		/* unknown */
		aip->ai_batteries = 0;
	} else {
		aip->ai_batt_stat = acpi_capm_convert_battstate(&batt);
		aip->ai_batt_life = batt.cap;
		aip->ai_batt_time = (batt.min == -1) ? -1 : batt.min * 60;
		aip->ai_batteries = acpi_battery_get_units();
	}

	return (0);
}

static int
acpi_capm_get_pwstatus(apm_pwstatus_t app)
{
	int	batt_unit;
	int	acline;
	struct	acpi_battinfo batt;

	if (app->ap_device != PMDV_ALLDEV &&
	    (app->ap_device < PMDV_BATT0 || app->ap_device > PMDV_BATT_ALL)) {
		return (1);
	}

	if (app->ap_device == PMDV_ALLDEV) {
		batt_unit = -1;			/* all units */
	} else {
		batt_unit = app->ap_device - PMDV_BATT0;
	}

	if (acpi_battery_get_battinfo(batt_unit, &batt)) {
		return (1);
	}

	app->ap_batt_stat = acpi_capm_convert_battstate(&batt);
	app->ap_batt_flag = acpi_capm_convert_battflags(&batt);
	app->ap_batt_life = batt.cap;
	app->ap_batt_time = (batt.min == -1) ? -1 : batt.min * 60;

	if (acpi_acad_get_acline(&acline)) {
		app->ap_acline = 0xff;		/* unknown */
	} else {
		app->ap_acline = acline;	/* on/off */
	}

	return (0);
}

static int
apmopen(dev_t dev, int flag, int fmt, d_thread_t *td)
{
	return (0);
}

static int
apmclose(dev_t dev, int flag, int fmt, d_thread_t *td)
{
	return (0);
}

static int
apmioctl(dev_t dev, u_long cmd, caddr_t addr, int flag, d_thread_t *td)
{
	int	error = 0;
	struct	acpi_softc *acpi_sc;
	struct apm_info info;
	apm_info_old_t aiop;

	if ((acpi_sc = device_get_softc(acpi_dev)) == NULL) {
		return (ENXIO);
	}

	switch (cmd) {
	case APMIO_SUSPEND:
		if (!(flag & FWRITE))
			return (EPERM);
		if (apm_softc.active)
			acpi_SetSleepState(acpi_sc, acpi_sc->acpi_suspend_sx);
		else
			error = EINVAL;
		break;

	case APMIO_STANDBY:
		if (!(flag & FWRITE))
			return (EPERM);
		if (apm_softc.active)
			acpi_SetSleepState(acpi_sc, acpi_sc->acpi_standby_sx);
		else
			error = EINVAL;
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
		if (!(flag & FWRITE))
			return (EPERM);
		apm_softc.active = 1;
		break;

	case APMIO_DISABLE:
		if (!(flag & FWRITE))
			return (EPERM);
		apm_softc.active = 0;
		break;

	case APMIO_HALTCPU:
		break;

	case APMIO_NOTHALTCPU:
		break;

	case APMIO_DISPLAY:
		if (!(flag & FWRITE))
			return (EPERM);
		break;

	case APMIO_BIOS:
		if (!(flag & FWRITE))
			return (EPERM);
		bzero(addr, sizeof(struct apm_bios_arg));
		break;

	default:
		error = EINVAL;
		break;
	}

	return (error);
}

static int
apmwrite(dev_t dev, struct uio *uio, int ioflag)
{

	return (uio->uio_resid);
}

static int
apmpoll(dev_t dev, int events, d_thread_t *td)
{
	return (0);
}

static void
acpi_capm_init(struct acpi_softc *sc)
{

        make_dev(&apm_cdevsw, 0, 0, 5, 0664, "apm");
}

int
acpi_machdep_init(device_t dev)
{
	struct	acpi_softc *sc;

	acpi_dev = dev;
	if ((sc = device_get_softc(acpi_dev)) == NULL) {
		return (ENXIO);
	}

	/*
	 * XXX: Prevent the PnP BIOS code from interfering with
	 * our own scan of ISA devices.
	 */
	PnPBIOStable = NULL;

	acpi_capm_init(sc);

	acpi_install_wakeup_handler(sc);

#ifdef SMP
	acpi_SetIntrModel(ACPI_INTR_APIC);
#endif
	return (0);
}

