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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/sys/i386/acpica/acpi_machdep.c,v 1.37.6.1 2008/11/25 02:59:29 kensmith Exp $");

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/condvar.h>
#include <sys/conf.h>
#include <sys/fcntl.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/poll.h>
#include <sys/sysctl.h>
#include <sys/uio.h>
#include <vm/vm.h>
#include <vm/pmap.h>

#include <contrib/dev/acpica/acpi.h>
#include <dev/acpica/acpivar.h>
#include <dev/acpica/acpiio.h>

/*
 * APM driver emulation 
 */

#include <machine/apm_bios.h>
#include <machine/pc/bios.h>

#include <i386/bios/apm.h>

SYSCTL_DECL(_debug_acpi);

uint32_t acpi_resume_beep;
TUNABLE_INT("debug.acpi.resume_beep", &acpi_resume_beep);
SYSCTL_UINT(_debug_acpi, OID_AUTO, resume_beep, CTLFLAG_RW, &acpi_resume_beep,
    0, "Beep the PC speaker when resuming");
uint32_t acpi_reset_video;
TUNABLE_INT("hw.acpi.reset_video", &acpi_reset_video);

static int intr_model = ACPI_INTR_PIC;
static int apm_active;
static struct clonedevs *apm_clones;

MALLOC_DEFINE(M_APMDEV, "apmdev", "APM device emulation");

static d_open_t		apmopen;
static d_close_t	apmclose;
static d_write_t	apmwrite;
static d_ioctl_t	apmioctl;
static d_poll_t		apmpoll;
static d_kqfilter_t	apmkqfilter;
static void		apmreadfiltdetach(struct knote *kn);
static int		apmreadfilt(struct knote *kn, long hint);
static struct filterops	apm_readfiltops =
	{ 1, NULL, apmreadfiltdetach, apmreadfilt };

static struct cdevsw apm_cdevsw = {
	.d_version =	D_VERSION,
	.d_flags =	D_TRACKCLOSE,
	.d_open =	apmopen,
	.d_close =	apmclose,
	.d_write =	apmwrite,
	.d_ioctl =	apmioctl,
	.d_poll =	apmpoll,
	.d_name =	"apm",
	.d_kqfilter =	apmkqfilter
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
		aip->ai_acline = APM_UNKNOWN;	/* unknown */
	else
		aip->ai_acline = acline;	/* on/off */

	if (acpi_battery_get_battinfo(NULL, &batt) != 0) {
		aip->ai_batt_stat = APM_UNKNOWN;
		aip->ai_batt_life = APM_UNKNOWN;
		aip->ai_batt_time = -1;		 /* unknown */
		aip->ai_batteries = ~0U;	 /* unknown */
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
	app->ap_batt_life = batt.cap;
	app->ap_batt_time = (batt.min == -1) ? -1 : batt.min * 60;

	if (acpi_acad_get_acline(&acline))
		app->ap_acline = APM_UNKNOWN;
	else
		app->ap_acline = acline;	/* on/off */

	return (0);
}

/* Create single-use devices for /dev/apm and /dev/apmctl. */
static void
apm_clone(void *arg, struct ucred *cred, char *name, int namelen,
    struct cdev **dev)
{
	int ctl_dev, unit;

	if (*dev != NULL)
		return;
	if (strcmp(name, "apmctl") == 0)
		ctl_dev = TRUE;
	else if (strcmp(name, "apm") == 0)
		ctl_dev = FALSE;
	else
		return;

	/* Always create a new device and unit number. */
	unit = -1;
	if (clone_create(&apm_clones, &apm_cdevsw, &unit, dev, 0)) {
		if (ctl_dev) {
			*dev = make_dev(&apm_cdevsw, unit2minor(unit),
			    UID_ROOT, GID_OPERATOR, 0660, "apmctl%d", unit);
		} else {
			*dev = make_dev(&apm_cdevsw, unit2minor(unit),
			    UID_ROOT, GID_OPERATOR, 0664, "apm%d", unit);
		}
		if (*dev != NULL) {
			dev_ref(*dev);
			(*dev)->si_flags |= SI_CHEAPCLONE;
		}
	}
}

/* Create a struct for tracking per-device suspend notification. */
static struct apm_clone_data *
apm_create_clone(struct cdev *dev, struct acpi_softc *acpi_sc)
{
	struct apm_clone_data *clone;

	clone = malloc(sizeof(*clone), M_APMDEV, M_WAITOK);
	clone->cdev = dev;
	clone->acpi_sc = acpi_sc;
	clone->notify_status = APM_EV_NONE;
	bzero(&clone->sel_read, sizeof(clone->sel_read));
	knlist_init(&clone->sel_read.si_note, &acpi_mutex, NULL, NULL, NULL);

	/*
	 * The acpi device is always managed by devd(8) and is considered
	 * writable (i.e., ack is required to allow suspend to proceed.)
	 */
	if (strcmp("acpi", devtoname(dev)) == 0)
		clone->flags = ACPI_EVF_DEVD | ACPI_EVF_WRITE;
	else
		clone->flags = ACPI_EVF_NONE;

	ACPI_LOCK(acpi);
	STAILQ_INSERT_TAIL(&acpi_sc->apm_cdevs, clone, entries);
	ACPI_UNLOCK(acpi);
	return (clone);
}

static int
apmopen(struct cdev *dev, int flag, int fmt, d_thread_t *td)
{
	struct	acpi_softc *acpi_sc;
	struct 	apm_clone_data *clone;

	acpi_sc = devclass_get_softc(devclass_find("acpi"), 0);
	clone = apm_create_clone(dev, acpi_sc);
	dev->si_drv1 = clone;

	/* If the device is opened for write, record that. */
	if ((flag & FWRITE) != 0)
		clone->flags |= ACPI_EVF_WRITE;

	return (0);
}

static int
apmclose(struct cdev *dev, int flag, int fmt, d_thread_t *td)
{
	struct	apm_clone_data *clone;
	struct	acpi_softc *acpi_sc;

	clone = dev->si_drv1;
	acpi_sc = clone->acpi_sc;

	/* We are about to lose a reference so check if suspend should occur */
	if (acpi_sc->acpi_next_sstate != 0 &&
	    clone->notify_status != APM_EV_ACKED)
		acpi_AckSleepState(clone, 0);

	/* Remove this clone's data from the list and free it. */
	ACPI_LOCK(acpi);
	STAILQ_REMOVE(&acpi_sc->apm_cdevs, clone, apm_clone_data, entries);
	knlist_destroy(&clone->sel_read.si_note);
	ACPI_UNLOCK(acpi);
	free(clone, M_APMDEV);
	destroy_dev_sched(dev);
	return (0);
}

static int
apmioctl(struct cdev *dev, u_long cmd, caddr_t addr, int flag, d_thread_t *td)
{
	int	error;
	struct	apm_clone_data *clone;
	struct	acpi_softc *acpi_sc;
	struct	apm_info info;
	struct 	apm_event_info *ev_info;
	apm_info_old_t aiop;

	error = 0;
	clone = dev->si_drv1;
	acpi_sc = clone->acpi_sc;

	switch (cmd) {
	case APMIO_SUSPEND:
		if ((flag & FWRITE) == 0)
			return (EPERM);
		if (acpi_sc->acpi_next_sstate == 0) {
			if (acpi_sc->acpi_suspend_sx != ACPI_STATE_S5) {
				error = acpi_ReqSleepState(acpi_sc,
				    acpi_sc->acpi_suspend_sx);
			} else {
				printf(
			"power off via apm suspend not supported\n");
				error = ENXIO;
			}
		} else
			error = acpi_AckSleepState(clone, 0);
		break;
	case APMIO_STANDBY:
		if ((flag & FWRITE) == 0)
			return (EPERM);
		if (acpi_sc->acpi_next_sstate == 0) {
			if (acpi_sc->acpi_standby_sx != ACPI_STATE_S5) {
				error = acpi_ReqSleepState(acpi_sc,
				    acpi_sc->acpi_standby_sx);
			} else {
				printf(
			"power off via apm standby not supported\n");
				error = ENXIO;
			}
		} else
			error = acpi_AckSleepState(clone, 0);
		break;
	case APMIO_NEXTEVENT:
		printf("apm nextevent start\n");
		ACPI_LOCK(acpi);
		if (acpi_sc->acpi_next_sstate != 0 && clone->notify_status ==
		    APM_EV_NONE) {
			ev_info = (struct apm_event_info *)addr;
			if (acpi_sc->acpi_next_sstate <= ACPI_STATE_S3)
				ev_info->type = PMEV_STANDBYREQ;
			else
				ev_info->type = PMEV_SUSPENDREQ;
			ev_info->index = 0;
			clone->notify_status = APM_EV_NOTIFIED;
			printf("apm event returning %d\n", ev_info->type);
		} else
			error = EAGAIN;
		ACPI_UNLOCK(acpi);
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
		bzero(addr, sizeof(struct apm_bios_arg));
		break;
	default:
		error = EINVAL;
		break;
	}

	return (error);
}

static int
apmwrite(struct cdev *dev, struct uio *uio, int ioflag)
{
	return (uio->uio_resid);
}

static int
apmpoll(struct cdev *dev, int events, d_thread_t *td)
{
	struct	apm_clone_data *clone;
	int revents;

	revents = 0;
	ACPI_LOCK(acpi);
	clone = dev->si_drv1;
	if (clone->acpi_sc->acpi_next_sstate)
		revents |= events & (POLLIN | POLLRDNORM);
	else
		selrecord(td, &clone->sel_read);
	ACPI_UNLOCK(acpi);
	return (revents);
}

static int
apmkqfilter(struct cdev *dev, struct knote *kn)
{
	struct	apm_clone_data *clone;

	ACPI_LOCK(acpi);
	clone = dev->si_drv1;
	kn->kn_hook = clone;
	kn->kn_fop = &apm_readfiltops;
	knlist_add(&clone->sel_read.si_note, kn, 0);
	ACPI_UNLOCK(acpi);
	return (0);
}

static void
apmreadfiltdetach(struct knote *kn)
{
	struct	apm_clone_data *clone;

	ACPI_LOCK(acpi);
	clone = kn->kn_hook;
	knlist_remove(&clone->sel_read.si_note, kn, 0);
	ACPI_UNLOCK(acpi);
}

static int
apmreadfilt(struct knote *kn, long hint)
{
	struct	apm_clone_data *clone;
	int	sleeping;

	ACPI_LOCK(acpi);
	clone = kn->kn_hook;
	sleeping = clone->acpi_sc->acpi_next_sstate ? 1 : 0;
	ACPI_UNLOCK(acpi);
	return (sleeping);
}

int
acpi_machdep_init(device_t dev)
{
	struct	acpi_softc *acpi_sc;

	acpi_sc = devclass_get_softc(devclass_find("acpi"), 0);

	/* Create a clone for /dev/acpi also. */
	STAILQ_INIT(&acpi_sc->apm_cdevs);
	acpi_sc->acpi_clone = apm_create_clone(acpi_sc->acpi_dev_t, acpi_sc);
	clone_setup(&apm_clones);
	EVENTHANDLER_REGISTER(dev_clone, apm_clone, 0, 1000);
	acpi_install_wakeup_handler(acpi_sc);

	if (intr_model == ACPI_INTR_PIC)
		BUS_CONFIG_INTR(dev, AcpiGbl_FADT.SciInterrupt,
		    INTR_TRIGGER_LEVEL, INTR_POLARITY_LOW);
	else
		acpi_SetIntrModel(intr_model);

	SYSCTL_ADD_UINT(&acpi_sc->acpi_sysctl_ctx,
	    SYSCTL_CHILDREN(acpi_sc->acpi_sysctl_tree), OID_AUTO,
	    "reset_video", CTLFLAG_RW, &acpi_reset_video, 0,
	    "Call the VESA reset BIOS vector on the resume path");

	return (0);
}

void
acpi_SetDefaultIntrModel(int model)
{

	intr_model = model;
}

/* Check BIOS date.  If 1998 or older, disable ACPI. */
int
acpi_machdep_quirks(int *quirks)
{
	char *va;
	int year;

	/* BIOS address 0xffff5 contains the date in the format mm/dd/yy. */
	va = pmap_mapbios(0xffff0, 16);
	sscanf(va + 11, "%2d", &year);
	pmap_unmapbios((vm_offset_t)va, 16);

	/* 
	 * Date must be >= 1/1/1999 or we don't trust ACPI.  Note that this
	 * check must be changed by my 114th birthday.
	 */
	if (year > 90 && year < 99)
		*quirks = ACPI_Q_BROKEN;

	return (0);
}

void
acpi_cpu_c1()
{
	__asm __volatile("sti; hlt");
}
