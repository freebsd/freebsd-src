/*-
 * Copyright (c) 2002 Sean Bullington <seanATstalker.org>
 *               2003-2005 Anish Mistry <amistry@am-productions.biz>
 *               2004 Mark Santcroos <marks@ripe.net>
 * All Rights Reserved.
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_acpi.h"
#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/bus.h>
#include <sys/module.h>
#include <sys/sysctl.h>

#include "acpi.h"
#include <dev/acpica/acpivar.h>

/* Hooks for the ACPI CA debugging infrastructure */
#define _COMPONENT	ACPI_OEM
ACPI_MODULE_NAME("Fujitsu")

/* Change and update bits for the buttons */
#define MOUSE_UPDATED_BIT	0x80000000
#define VOLUME_MUTE_BIT		0x40000000
#define VOLUME_CHANGE_BIT	0x80000000
#define BRIGHTNESS_CHANGE_BIT	0x80000000

/* Values of settings */
#define GENERAL_SETTING_BITS	0x0fffffff
#define MOUSE_SETTING_BITS	GENERAL_SETTING_BITS
#define VOLUME_SETTING_BITS	GENERAL_SETTING_BITS
#define BRIGHTNESS_SETTING_BITS	GENERAL_SETTING_BITS

/* Possible state changes */
#define VOLUME_CHANGED	1
#define BRIGHT_CHANGED	2
#define MOUSE_CHANGED	3

/* sysctl values */
#define FN_MUTE			0
#define FN_POINTER_ENABLE	1
#define FN_LCD_BRIGHTNESS	2
#define FN_VOLUME		3

/* Methods */
#define METHOD_GBLL	1
#define METHOD_GMOU	2
#define METHOD_GVOL	3
#define METHOD_MUTE	4

/* Notify event */
#define	ACPI_NOTIFY_STATUS_CHANGED	0x80

/*
 * Holds a control method name and its associated integer value.
 * Only used for no-argument control methods which return a value.
 */
struct int_nameval {
	char	*name;
	int	value;
};

/*
 * Driver extension for the FUJITSU ACPI driver.
 */
struct acpi_fujitsu_softc {
	device_t	dev;
	ACPI_HANDLE	handle;

	/* Control methods */
	struct int_nameval	_sta,	/* unused */
				gbll,	/* brightness */
				ghks,	/* unused */
				gmou,	/* mouse */
				gsif,	/* unused */
				gvol,	/* volume */
				rbll,	/* unused */
				rvol;	/* unused */

	/* State variables */
	uint8_t		bIsMuted;	/* Is volume muted */
	uint8_t		bIntPtrEnabled;	/* Is internal ptr enabled */
	uint32_t	lastValChanged;	/* The last value updated */

	/* sysctl tree */
	struct sysctl_ctx_list	sysctl_ctx;
	struct sysctl_oid	*sysctl_tree;
};

/* Driver entry point forward declarations. */
static int	acpi_fujitsu_probe(device_t dev);
static int	acpi_fujitsu_attach(device_t dev);
static int	acpi_fujitsu_detach(device_t dev);
static int	acpi_fujitsu_suspend(device_t dev);
static int	acpi_fujitsu_resume(device_t dev);

static void	acpi_fujitsu_notify_status_changed(void *arg);
static void	acpi_fujitsu_notify_handler(ACPI_HANDLE h, uint32_t notify, void *context);
static int	acpi_fujitsu_sysctl(SYSCTL_HANDLER_ARGS);

/* Utility function declarations */
static uint8_t acpi_fujitsu_update(struct acpi_fujitsu_softc *sc);
static uint8_t acpi_fujitsu_init(struct acpi_fujitsu_softc *sc);

/* Driver/Module specific structure definitions. */
static device_method_t acpi_fujitsu_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		acpi_fujitsu_probe),
	DEVMETHOD(device_attach,	acpi_fujitsu_attach),
	DEVMETHOD(device_detach,	acpi_fujitsu_detach),
	DEVMETHOD(device_suspend,	acpi_fujitsu_suspend),
	DEVMETHOD(device_resume,	acpi_fujitsu_resume),
	{0, 0}
};

static driver_t acpi_fujitsu_driver = {
	"acpi_fujitsu",
	acpi_fujitsu_methods,
	sizeof(struct acpi_fujitsu_softc),
};

/* Prototype for function buttons for getting/setting a value. */
static int acpi_fujitsu_method_get(struct acpi_fujitsu_softc *sc, int method);
static int acpi_fujitsu_method_set(struct acpi_fujitsu_softc *sc, int method, int value);

static char *fujitsu_ids[] = { "FUJ02B1", NULL };

ACPI_SERIAL_DECL(fujitsu, "Fujitsu Function Buttons");

/* sysctl names and function calls */
static struct {
	char		*name;
	int		method;
	char		*description;
} sysctl_table[] = {
	{
		.name		= "mute",
		.method		= METHOD_MUTE,
		.description	= "Speakers/headphones mute status"
	},
	{
		.name		= "pointer_enable",
		.method		= METHOD_GMOU,
		.description	= "Enable and disable the internal pointer"
	},
	{
		.name		= "lcd_brightness",
		.method		= METHOD_GBLL,
		.description	= "Brightness level of the LCD panel"
	},
	{
		.name		= "volume",
		.method		= METHOD_GVOL,
		.description	= "Speakers/headphones volume level"
	},

	{ NULL, 0, NULL }
};

static devclass_t acpi_fujitsu_devclass;
DRIVER_MODULE(acpi_fujitsu, acpi, acpi_fujitsu_driver,
    acpi_fujitsu_devclass, 0, 0);
MODULE_DEPEND(acpi_fujitsu, acpi, 1, 1, 1);
MODULE_VERSION(acpi_fujitsu, 1);

static int
acpi_fujitsu_probe(device_t dev)
{

	if (acpi_disabled("fujitsu") ||
	    ACPI_ID_PROBE(device_get_parent(dev), dev, fujitsu_ids) == NULL ||
	    device_get_unit(dev) != 0)
		return (ENXIO);

	device_set_desc(dev, "Fujitsu Function Buttons");

	return (0);
}

static int
acpi_fujitsu_attach(device_t dev)
{
	struct acpi_fujitsu_softc *sc;

	ACPI_FUNCTION_TRACE((char *)(uintptr_t)__func__);

	sc = device_get_softc(dev);
	sc->dev = dev;
	sc->handle = acpi_get_handle(dev);

	/* Install notification handler */
	AcpiInstallNotifyHandler(sc->handle, ACPI_DEVICE_NOTIFY,
	    acpi_fujitsu_notify_handler, sc);

	/* Snag our default values for the buttons / button states. */
	ACPI_SERIAL_BEGIN(fujitsu);
	if (!acpi_fujitsu_init(sc))
		device_printf(dev, "Couldn't initialize button states!\n");
	ACPI_SERIAL_END(fujitsu);

	return (0);
}

/*
 * Called when the system is being suspended, simply
 * set an event to be signalled when we wake up.
 */
static int
acpi_fujitsu_suspend(device_t dev)
{

	return (0);
}

static int
acpi_fujitsu_resume(device_t dev)
{
	struct acpi_fujitsu_softc   *sc;
	ACPI_STATUS		    status;

	sc = device_get_softc(dev);

	/*
	 * The pointer needs to be re-enabled for
	 * some revisions of the P series (2120).
	 */
	ACPI_SERIAL_BEGIN(fujitsu);

	status = acpi_SetInteger(sc->handle, "SMOU", 1);
	if (ACPI_FAILURE(status))
		device_printf(sc->dev, "Couldn't enable pointer\n");

	ACPI_SERIAL_END(fujitsu);

	return (0);
}

static void
acpi_fujitsu_notify_status_changed(void *arg)
{
	struct acpi_fujitsu_softc *sc;

	ACPI_FUNCTION_TRACE((char *)(uintptr_t)__func__);

	sc = (struct acpi_fujitsu_softc *)arg;

	/*
	 * Since our notify function is called, we know something has
	 * happened.  So the only reason for acpi_fujitsu_update to fail
	 * is if we can't find what has changed or an error occurs.
	 */
	ACPI_SERIAL_BEGIN(fujitsu);
	acpi_fujitsu_update(sc);
	ACPI_SERIAL_END(fujitsu);
}


static void
acpi_fujitsu_notify_handler(ACPI_HANDLE h, uint32_t notify, void *context)
{
	struct acpi_fujitsu_softc *sc;

	ACPI_FUNCTION_TRACE_U32((char *)(uintptr_t)__func__, notify);

	sc = (struct acpi_fujitsu_softc *)context;

	switch (notify) {
	case ACPI_NOTIFY_STATUS_CHANGED:
		AcpiOsQueueForExecution(OSD_PRIORITY_LO,
		    acpi_fujitsu_notify_status_changed, sc);
		break;
	default:
		/* unknown notification value */
		break;
	}
}

static int
acpi_fujitsu_detach(device_t dev)
{
	struct acpi_fujitsu_softc *sc;

	sc = device_get_softc(dev);
	AcpiRemoveNotifyHandler(sc->handle, ACPI_DEVICE_NOTIFY,
	   acpi_fujitsu_notify_handler);

	sysctl_ctx_free(&sc->sysctl_ctx);

	return (0);
}

/*
 * Initializes the names of the ACPI control methods and grabs
 * the current state of all of the ACPI buttons into the softc.
 */
static uint8_t
acpi_fujitsu_init(struct acpi_fujitsu_softc *sc)
{
	struct acpi_softc *acpi_sc;
	int i;

	ACPI_SERIAL_ASSERT(fujitsu);

	/* Setup all of the names for each control method */
	sc->_sta.name = "_STA";
	sc->gbll.name = "GBLL";
	sc->ghks.name = "GHKS";
	sc->gmou.name = "GMOU";
	sc->gsif.name = "GSIF";
	sc->gvol.name = "GVOL";
	sc->rbll.name = "RBLL";
	sc->rvol.name = "RVOL";

	/* Build the sysctl tree */
	acpi_sc = acpi_device_get_parent_softc(sc->dev);
	sysctl_ctx_init(&sc->sysctl_ctx);
	sc->sysctl_tree = SYSCTL_ADD_NODE(&sc->sysctl_ctx,
	    SYSCTL_CHILDREN(acpi_sc->acpi_sysctl_tree),
	    OID_AUTO, "fujitsu", CTLFLAG_RD, 0, "");

	for (i = 0; sysctl_table[i].name != NULL; i++) {
		SYSCTL_ADD_PROC(&sc->sysctl_ctx,
		    SYSCTL_CHILDREN(sc->sysctl_tree), OID_AUTO,
		    sysctl_table[i].name,
		    CTLTYPE_INT | CTLFLAG_RW | CTLFLAG_ANYBODY,
		    sc, i, acpi_fujitsu_sysctl, "I",
		    sysctl_table[i].description);
	}

	/* Set the buttons to their initial states */
	if (!acpi_fujitsu_update(sc)) {
		device_printf(sc->dev, "Couldn't init button states\n");
		return (FALSE);
	}

	return (TRUE);
}

static int
acpi_fujitsu_sysctl(SYSCTL_HANDLER_ARGS)
{
	struct acpi_fujitsu_softc	*sc;
	int				method;
	int				arg;
	int				function_num, error = 0;

	sc = (struct acpi_fujitsu_softc *)oidp->oid_arg1;
	function_num = oidp->oid_arg2;
	method = sysctl_table[function_num].method;

	ACPI_SERIAL_BEGIN(fujitsu);

	/* Get the current value */
	arg = acpi_fujitsu_method_get(sc, method);
	error = sysctl_handle_int(oidp, &arg, 0, req);

	if (error != 0 || req->newptr == NULL)
		goto out;

	/* Update the value */
	error = acpi_fujitsu_method_set(sc, method, arg);

out:
	ACPI_SERIAL_END(fujitsu);
	return (error);
}

static int
acpi_fujitsu_method_get(struct acpi_fujitsu_softc *sc, int method)
{
	struct int_nameval	nv;
	ACPI_STATUS		status;

	ACPI_SERIAL_ASSERT(fujitsu);

	switch (method) {
		case METHOD_GBLL:
			nv = sc->gbll;
			break;
		case METHOD_GMOU:
			nv = sc->gmou;
			break;
		case METHOD_GVOL:
		case METHOD_MUTE:
			nv = sc->gvol;
			break;
		default:
			return (FALSE);
	}

	status = acpi_GetInteger(sc->handle, nv.name, &nv.value);
	if (ACPI_FAILURE(status)) {
		device_printf(sc->dev, "Couldn't query method\n");
		return (FALSE);
	}

	if (method == METHOD_MUTE) {
		sc->bIsMuted = (uint8_t)((nv.value & VOLUME_MUTE_BIT) != 0);
		return (sc->bIsMuted);
	}

	nv.value &= GENERAL_SETTING_BITS;
	return (nv.value);
}

static int
acpi_fujitsu_method_set(struct acpi_fujitsu_softc *sc, int method, int value)
{
	struct int_nameval	nv;
	ACPI_STATUS		status;
	char			*control;
	int			changed;

	ACPI_SERIAL_ASSERT(fujitsu);

	switch (method) {
		case METHOD_GBLL:
			changed = BRIGHT_CHANGED;
			control = "SBLL";
			nv = sc->gbll;
			break;
		case METHOD_GMOU:
			changed = MOUSE_CHANGED;
			control = "SMOU";
			nv = sc->gmou;
			break;
		case METHOD_GVOL:
		case METHOD_MUTE:
			changed = VOLUME_CHANGED;
			control = "SVOL";
			nv = sc->gvol;
			break;
		default:
			return (EINVAL);
	}

	if (method == METHOD_MUTE) {
		if (value == 1)
			value = nv.value | VOLUME_MUTE_BIT;
		else if (value == 0)
			value = nv.value & ~VOLUME_MUTE_BIT;
		else
			return (EINVAL);
	}

	status = acpi_SetInteger(sc->handle, control, value);
	if (ACPI_FAILURE(status)) {
		device_printf(sc->dev, "Couldn't update %s\n", control);
		return (EINVAL);
	}

	sc->lastValChanged = changed;
	return (0);
}

/*
 * Query each of the ACPI control methods that contain information we're
 * interested in. We check the return values from the control methods and
 * adjust any state variables if they should be adjusted.
 */
static uint8_t
acpi_fujitsu_update(struct acpi_fujitsu_softc *sc)
{
	struct acpi_softc *acpi_sc;

	acpi_sc = acpi_device_get_parent_softc(sc->dev);

	ACPI_SERIAL_ASSERT(fujitsu);

	/* System Volume Level */
	if (ACPI_FAILURE(acpi_GetInteger(sc->handle,
	    sc->gvol.name, &(sc->gvol.value)))) {
		device_printf(sc->dev, "Couldn't query volume level\n");
		return (FALSE);
	}

	if (sc->gvol.value & VOLUME_CHANGE_BIT) {
		sc->bIsMuted =
		    (uint8_t)((sc->gvol.value & VOLUME_MUTE_BIT) != 0);

		/* Clear the modification bit */
		sc->gvol.value &= VOLUME_SETTING_BITS;

		if (sc->bIsMuted) {
			acpi_UserNotify("FUJITSU", sc->handle, FN_MUTE);
			ACPI_VPRINT(sc->dev, acpi_sc, "Volume is now mute\n");
		} else
			ACPI_VPRINT(sc->dev, acpi_sc, "Volume is now %d\n",
			    sc->gvol.value);

		acpi_UserNotify("FUJITSU", sc->handle, FN_VOLUME);

		sc->lastValChanged = VOLUME_CHANGED;
	}

	/* Internal mouse pointer (eraserhead) */
	if (ACPI_FAILURE(acpi_GetInteger(sc->handle,
	    sc->gmou.name, &(sc->gmou.value)))) {
		device_printf(sc->dev, "Couldn't query pointer state\n");
		return (FALSE);
	}

	if (sc->gmou.value & MOUSE_UPDATED_BIT) {
		sc->bIntPtrEnabled = (uint8_t)(sc->gmou.value & 0x1);

		/* Clear the modification bit */
		sc->gmou.value &= MOUSE_SETTING_BITS;
		
		acpi_UserNotify("FUJITSU", sc->handle, FN_POINTER_ENABLE);

		ACPI_VPRINT(sc->dev, acpi_sc, "Internal pointer is now %s\n",
		    (sc->bIntPtrEnabled) ? "enabled" : "disabled");

		sc->lastValChanged = MOUSE_CHANGED;
	}

	/* Screen Brightness Level */
	if (ACPI_FAILURE(acpi_GetInteger(sc->handle,
	    sc->gbll.name, &(sc->gbll.value)))) {
		device_printf(sc->dev, "Couldn't query brightness level\n");
		return (FALSE);
	}

	if (sc->gbll.value & BRIGHTNESS_CHANGE_BIT) {
		/* No state to record here. */

		/* Clear the modification bit */
		sc->gbll.value &= BRIGHTNESS_SETTING_BITS;

		acpi_UserNotify("FUJITSU", sc->handle, FN_LCD_BRIGHTNESS);

		ACPI_VPRINT(sc->dev, acpi_sc, "Brightness level is now %d\n",
		    sc->gbll.value);

		sc->lastValChanged = BRIGHT_CHANGED;
	}

	return (TRUE);
}
