/*-
 * Copyright (c) 2004 Philip Paeps <philip@FreeBSD.org>
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

/*
 * Driver for extra ACPI-controlled gadgets (hotkeys, leds, etc) found on
 * recent Asus (and Medion) laptops.  Inspired by the Acpi4Asus project which
 * implements these features in the Linux kernel.
 *
 *   <http://sourceforge.net/projects/acpi4asus/>
 *
 * Currently should support most features, but could use some more testing.
 * Particularly the display-switching stuff is a bit hairy.  If you have an
 * Asus laptop which doesn't appear to be supported, or strange things happen
 * when using this driver, please report to <acpi@FreeBSD.org>.
 *
 */

#include "opt_acpi.h"
#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/bus.h>
#include <sys/sbuf.h>

#include "acpi.h"
#include <dev/acpica/acpivar.h>
#include <dev/led/led.h>

#define _COMPONENT	ACPI_ASUS
ACPI_MODULE_NAME("ASUS")

struct acpi_asus_model {
	char	*name;

	char	*mled_set;
	char	*tled_set;
	char	*wled_set;

	char	*brn_get;
	char	*brn_set;
	char	*brn_up;
	char	*brn_dn;

	char	*lcd_get;
	char	*lcd_set;

	char	*disp_get;
	char	*disp_set;
};

struct acpi_asus_softc {
	device_t		dev;
	ACPI_HANDLE		handle;

	struct acpi_asus_model	*model;
	struct sysctl_ctx_list	sysctl_ctx;
	struct sysctl_oid	*sysctl_tree;

	struct cdev *s_mled;
	struct cdev *s_tled;
	struct cdev *s_wled;

	int			s_brn;
	int			s_disp;
	int			s_lcd;
};

/* Models we know about */
static struct acpi_asus_model acpi_asus_models[] = {
	{
		.name		= "L2D",
		.mled_set	= "MLED",
		.wled_set	= "WLED",
		.brn_up		= "\\Q0E",
		.brn_dn		= "\\Q0F",
		.lcd_get	= "\\SGP0",
		.lcd_set	= "\\Q10"
	},
	{
		.name		= "L3C",
		.mled_set	= "MLED",
		.wled_set	= "WLED",
		.brn_get	= "GPLV",
		.brn_set	= "SPLV",
		.lcd_get	= "\\GL32",
		.lcd_set	= "\\_SB.PCI0.PX40.ECD0._Q10"
	},
	{
		.name		= "L3D",
		.mled_set	= "MLED",
		.wled_set	= "WLED",
		.brn_get	= "GPLV",
		.brn_set	= "SPLV",
		.lcd_get	= "\\BKLG",
		.lcd_set	= "\\Q10"
	},
	{
		.name		= "L3H",
		.mled_set	= "MLED",
		.wled_set	= "WLED",
		.brn_get	= "GPLV",
		.brn_set	= "SPLV",
		.lcd_get	= "\\_SB.PCI0.PM.PBC",
		.lcd_set	= "EHK",
		.disp_get	= "\\_SB.INFB",
		.disp_set	= "SDSP"
	},
	{
		.name		= "L8L"
		/* Only has hotkeys, apparantly */
	},
	{
		.name		= "M1A",
		.mled_set	= "MLED",
		.brn_up		= "\\_SB.PCI0.PX40.EC0.Q0E",
		.brn_dn		= "\\_SB.PCI0.PX40.EC0.Q0F",
		.lcd_get	= "\\PNOF",
		.lcd_set	= "\\_SB.PCI0.PX40.EC0.Q10"
	},
	{
		.name		= "M2E",
		.mled_set	= "MLED",
		.wled_set	= "WLED",
		.brn_get	= "GPLV",
		.brn_set	= "SPLV",
		.lcd_get	= "\\GP06",
		.lcd_set	= "\\Q10"
	},
	{
		.name		= "P30",
		.wled_set	= "WLED",
		.brn_up		= "\\_SB.PCI0.LPCB.EC0._Q68",
		.brn_dn		= "\\_SB.PCI0.LPCB.EC0._Q69",
		.lcd_get	= "\\BKLT",
		.lcd_set	= "\\_SB.PCI0.LPCB.EC0._Q0E"
	},

	{ .name = NULL }
};

/* Function prototypes */
static int	acpi_asus_probe(device_t dev);
static int	acpi_asus_attach(device_t dev);
static int	acpi_asus_detach(device_t dev);

static void	acpi_asus_mled(device_t dev, int state);
static void	acpi_asus_tled(device_t dev, int state);
static void	acpi_asus_wled(device_t dev, int state);

static int	acpi_asus_sysctl_brn(SYSCTL_HANDLER_ARGS);
static int	acpi_asus_sysctl_lcd(SYSCTL_HANDLER_ARGS);
static int	acpi_asus_sysctl_disp(SYSCTL_HANDLER_ARGS);

static void	acpi_asus_notify(ACPI_HANDLE h, UINT32 notify, void *context);

static device_method_t acpi_asus_methods[] = {
	DEVMETHOD(device_probe,	 acpi_asus_probe),
	DEVMETHOD(device_attach, acpi_asus_attach),
	DEVMETHOD(device_detach, acpi_asus_detach),

	{ 0, 0 }
};

static driver_t acpi_asus_driver = {
	"acpi_asus",
	acpi_asus_methods,
	sizeof(struct acpi_asus_softc)
};

static devclass_t acpi_asus_devclass;

DRIVER_MODULE(acpi_asus, acpi, acpi_asus_driver, acpi_asus_devclass, 0, 0);
MODULE_DEPEND(acpi_asus, acpi, 1, 1, 1);

static int
acpi_asus_probe(device_t dev)
{
	struct acpi_asus_model	*model;
	struct acpi_asus_softc	*sc;
	struct sbuf		*sb;
	ACPI_BUFFER		Buf;
	ACPI_OBJECT		Arg, *Obj;
	ACPI_OBJECT_LIST	Args;
	static char 		*asus_ids[] = { "ATK0100", NULL };

	ACPI_FUNCTION_TRACE((char *)(uintptr_t)__func__);

	if (!acpi_disabled("asus") &&
	    ACPI_ID_PROBE(device_get_parent(dev), dev, asus_ids)) {
		sc = device_get_softc(dev);
		sc->dev = dev;
		sc->handle = acpi_get_handle(dev);
	
		sb = sbuf_new(NULL, NULL, 0, SBUF_AUTOEXTEND);
				
		if (sb == NULL)
			return (ENOMEM);

		Arg.Type = ACPI_TYPE_INTEGER;
		Arg.Integer.Value = 0;

		Args.Count = 1;
		Args.Pointer = &Arg;

		Buf.Pointer = NULL;
		Buf.Length = ACPI_ALLOCATE_BUFFER;
	
		AcpiEvaluateObject(sc->handle, "INIT", &Args, &Buf);
		
		Obj = Buf.Pointer;

		for (model = acpi_asus_models; model->name != NULL; model++)
			if (strcmp(Obj->String.Pointer, model->name) == 0) {
				sbuf_printf(sb, "Asus %s Laptop Extras",
						Obj->String.Pointer);
				sbuf_finish(sb);
				
				sc->model = model;
				device_set_desc(dev, sbuf_data(sb));

				sbuf_delete(sb);
				AcpiOsFree(Buf.Pointer);
				return (0);
			}

		sbuf_printf(sb, "Unsupported Asus laptop detected: %s\n",
				Obj->String.Pointer);
		sbuf_finish(sb);

		device_printf(dev, sbuf_data(sb));

		sbuf_delete(sb);
		AcpiOsFree(Buf.Pointer);
	}

	return (ENXIO);
}

static int
acpi_asus_attach(device_t dev)
{
	struct acpi_asus_softc	*sc;
	struct acpi_softc	*acpi_sc;

	ACPI_FUNCTION_TRACE((char *)(uintptr_t)__func__);

	sc = device_get_softc(dev);
	acpi_sc = acpi_device_get_parent_softc(dev);

	/* Build sysctl tree */
	sysctl_ctx_init(&sc->sysctl_ctx);
	sc->sysctl_tree = SYSCTL_ADD_NODE(&sc->sysctl_ctx,
	    SYSCTL_CHILDREN(acpi_sc->acpi_sysctl_tree),
	    OID_AUTO, "asus", CTLFLAG_RD, 0, "");

	/* Attach leds */
	if (sc->model->mled_set)
		sc->s_mled = led_create((led_t *)acpi_asus_mled, dev, "mled");

	if (sc->model->tled_set)
		sc->s_tled = led_create((led_t *)acpi_asus_tled, dev, "tled");

	if (sc->model->wled_set)
		sc->s_wled = led_create((led_t *)acpi_asus_wled, dev, "wled");

	/* Attach brightness for GPLV/SPLV models */
	if (sc->model->brn_get && ACPI_SUCCESS(acpi_GetInteger(sc->handle,
	    sc->model->brn_get, &sc->s_brn)))
		SYSCTL_ADD_PROC(&sc->sysctl_ctx,
		    SYSCTL_CHILDREN(sc->sysctl_tree), OID_AUTO,
		    "lcd_brightness", CTLTYPE_INT | CTLFLAG_RW, sc, 0,
		    acpi_asus_sysctl_brn, "I", "brightness of the lcd panel");

	/* Attach brightness for other models */
	if (sc->model->brn_up &&
	    ACPI_SUCCESS(AcpiEvaluateObject(sc->handle, sc->model->brn_up,
	    NULL, NULL)) &&
	    ACPI_SUCCESS(AcpiEvaluateObject(sc->handle, sc->model->brn_dn,
	    NULL, NULL)))
		SYSCTL_ADD_PROC(&sc->sysctl_ctx,
		    SYSCTL_CHILDREN(sc->sysctl_tree), OID_AUTO,
		    "lcd_brightness", CTLTYPE_INT | CTLFLAG_RW, sc, 0,
		    acpi_asus_sysctl_brn, "I", "brightness of the lcd panel");

	/* Attach display switching */
	if (sc->model->disp_get && ACPI_SUCCESS(acpi_GetInteger(sc->handle,
	    sc->model->disp_get, &sc->s_disp)))
		SYSCTL_ADD_PROC(&sc->sysctl_ctx,
		    SYSCTL_CHILDREN(sc->sysctl_tree), OID_AUTO,
		    "video_output", CTLTYPE_INT | CTLFLAG_RW, sc, 0,
		    acpi_asus_sysctl_disp, "I", "display output state");

	/* Attach LCD state, easy for most models... */
	if (sc->model->lcd_get && strncmp(sc->model->name, "L3H", 3) != 0 &&
	    ACPI_SUCCESS(acpi_GetInteger(sc->handle, sc->model->lcd_get,
	    &sc->s_lcd)))
		SYSCTL_ADD_PROC(&sc->sysctl_ctx,
		    SYSCTL_CHILDREN(sc->sysctl_tree), OID_AUTO,
		    "lcd_backlight", CTLTYPE_INT | CTLFLAG_RW, sc, 0,
		    acpi_asus_sysctl_lcd, "I", "state of the lcd backlight");

	/* ...a nightmare for the L3H */
	else if (sc->model->lcd_get) {
		ACPI_BUFFER		Buf;
		ACPI_OBJECT		Arg[2], Obj;
		ACPI_OBJECT_LIST	Args;

		Arg[0].Type = ACPI_TYPE_INTEGER;
		Arg[0].Integer.Value = 0x02;
		Arg[1].Type = ACPI_TYPE_INTEGER;
		Arg[1].Integer.Value = 0x03;

		Args.Count = 2;
		Args.Pointer = Arg;

		Buf.Length = sizeof(Obj);
		Buf.Pointer = &Obj;

		if (ACPI_SUCCESS(AcpiEvaluateObject(sc->handle,
		    sc->model->lcd_get, &Args, &Buf)) &&
		    Obj.Type == ACPI_TYPE_INTEGER) {
			sc->s_lcd = Obj.Integer.Value >> 8;

			SYSCTL_ADD_PROC(&sc->sysctl_ctx,
			    SYSCTL_CHILDREN(sc->sysctl_tree), OID_AUTO,
			    "lcd_backlight", CTLTYPE_INT | CTLFLAG_RW, sc, 0,
			    acpi_asus_sysctl_lcd, "I",
			    "state of the lcd backlight");
		}
	}

	/* Activate hotkeys */
	AcpiEvaluateObject(sc->handle, "BSTS", NULL, NULL);

	/* Handle notifies */
	AcpiInstallNotifyHandler(sc->handle, ACPI_SYSTEM_NOTIFY,
	    acpi_asus_notify, dev);
	
	return (0);
}

static int
acpi_asus_detach(device_t dev)
{
	struct acpi_asus_softc	*sc;
	
	ACPI_FUNCTION_TRACE((char *)(uintptr_t)__func__);

	sc = device_get_softc(dev);

	/* Turn the lights off */
	if (sc->model->mled_set)
		led_destroy(sc->s_mled);

	if (sc->model->tled_set)
		led_destroy(sc->s_tled);

	if (sc->model->wled_set)
		led_destroy(sc->s_wled);

	/* Remove notify handler */
	AcpiRemoveNotifyHandler(sc->handle, ACPI_SYSTEM_NOTIFY,
	    acpi_asus_notify);

	/* Free sysctl tree */
	sysctl_ctx_free(&sc->sysctl_ctx);

	return (0);
}

static void
acpi_asus_mled(device_t dev, int state)
{
	struct acpi_asus_softc	*sc;
	ACPI_OBJECT		Arg;
	ACPI_OBJECT_LIST	Args;

	ACPI_FUNCTION_TRACE((char *)(uintptr_t)__func__);

	sc = device_get_softc(dev);

	Arg.Type = ACPI_TYPE_INTEGER;
	Arg.Integer.Value = !state;	/* Inverted, yes! */

	Args.Count = 1;
	Args.Pointer = &Arg;

	AcpiEvaluateObject(sc->handle, sc->model->mled_set, &Args, NULL);
}

static void
acpi_asus_tled(device_t dev, int state)
{
	struct acpi_asus_softc	*sc;
	ACPI_OBJECT		Arg;
	ACPI_OBJECT_LIST	Args;

	ACPI_FUNCTION_TRACE((char *)(uintptr_t)__func__);

	sc = device_get_softc(dev);

	Arg.Type = ACPI_TYPE_INTEGER;
	Arg.Integer.Value = state;

	Args.Count = 1;
	Args.Pointer = &Arg;

	AcpiEvaluateObject(sc->handle, sc->model->tled_set, &Args, NULL);
}

static void
acpi_asus_wled(device_t dev, int state)
{
	struct acpi_asus_softc	*sc;
	ACPI_OBJECT		Arg;
	ACPI_OBJECT_LIST	Args;

	ACPI_FUNCTION_TRACE((char *)(uintptr_t)__func__);

	sc = device_get_softc(dev);

	Arg.Type = ACPI_TYPE_INTEGER;
	Arg.Integer.Value = state;

	Args.Count = 1;
	Args.Pointer = &Arg;

	AcpiEvaluateObject(sc->handle, sc->model->wled_set, &Args, NULL);
}

static int
acpi_asus_sysctl_brn(SYSCTL_HANDLER_ARGS)
{
	struct acpi_asus_softc	*sc;
	ACPI_OBJECT		Arg;
	ACPI_OBJECT_LIST	Args;
	int			brn, err;

	ACPI_FUNCTION_TRACE((char *)(uintptr_t)__func__);

	sc = (struct acpi_asus_softc *)oidp->oid_arg1;

	/* Sanity check */
	brn = sc->s_brn;
	err = sysctl_handle_int(oidp, &brn, 0, req);

	if (err != 0 || req->newptr == NULL)
		return (err);

	if (brn < 0 || brn > 15)
		return (EINVAL);

	/* Keep track and update */
	sc->s_brn = brn;

	Arg.Type = ACPI_TYPE_INTEGER;
	Arg.Integer.Value = brn;

	Args.Count = 1;
	Args.Pointer = &Arg;

	if (sc->model->brn_set)
		AcpiEvaluateObject(sc->handle, sc->model->brn_set, &Args, NULL);
	else {
		brn -= sc->s_brn;

		while (brn != 0) {
			AcpiEvaluateObject(sc->handle, (brn > 0) ?
			    sc->model->brn_up : sc->model->brn_dn,
			    NULL, NULL);

			(brn > 0) ? brn-- : brn++;
		}
	}

	return (0);
}

static int
acpi_asus_sysctl_lcd(SYSCTL_HANDLER_ARGS)
{
	struct acpi_asus_softc	*sc;
	int			lcd, err;

	ACPI_FUNCTION_TRACE((char *)(uintptr_t)__func__);

	sc = (struct acpi_asus_softc *)oidp->oid_arg1;

	/* Sanity check */
	lcd = sc->s_lcd;
	err = sysctl_handle_int(oidp, &lcd, 0, req);

	if (err != 0 || req->newptr == NULL)
		return (err);

	if (lcd < 0 || lcd > 1)
		return (EINVAL);

	/* Keep track and update */
	sc->s_lcd = lcd;

	/* Most models just need a lcd_set evaluated, the L3H is trickier */
	if (strncmp(sc->model->name, "L3H", 3) != 0)
		AcpiEvaluateObject(sc->handle, sc->model->lcd_set, NULL, NULL);
	else {
		ACPI_OBJECT		Arg;
		ACPI_OBJECT_LIST	Args;

		Arg.Type = ACPI_TYPE_INTEGER;
		Arg.Integer.Value = 0x07;

		Args.Count = 1;
		Args.Pointer = &Arg;

		AcpiEvaluateObject(sc->handle, sc->model->lcd_set, &Args, NULL);
	}

	return (0);
}

static int
acpi_asus_sysctl_disp(SYSCTL_HANDLER_ARGS)
{
	struct acpi_asus_softc	*sc;
	ACPI_OBJECT		Arg;
	ACPI_OBJECT_LIST	Args;
	int			disp, err;

	ACPI_FUNCTION_TRACE((char *)(uintptr_t)__func__);

	sc = (struct acpi_asus_softc *)oidp->oid_arg1;

	/* Sanity check */
	disp = sc->s_disp;
	err = sysctl_handle_int(oidp, &disp, 0, req);

	if (err != 0 || req->newptr == NULL)
		return (err);

	if (disp < 0 || disp > 7)
		return (EINVAL);

	/* Keep track and update */
	sc->s_disp = disp;

	Arg.Type = ACPI_TYPE_INTEGER;
	Arg.Integer.Value = disp;

	Args.Count = 1;
	Args.Pointer = &Arg;

	AcpiEvaluateObject(sc->handle, sc->model->disp_set, &Args, NULL);

	return (0);
}

static void
acpi_asus_notify(ACPI_HANDLE h, UINT32 notify, void *context)
{
	struct acpi_asus_softc	*sc;
	struct acpi_softc	*acpi_sc;

	ACPI_FUNCTION_TRACE((char *)(uintptr_t)__func__);

	sc = device_get_softc((device_t)context);
	acpi_sc = acpi_device_get_parent_softc(sc->dev);

	if ((notify & ~0x10) <= 15) {
		sc->s_brn = notify & ~0x10;
		ACPI_VPRINT(sc->dev, acpi_sc, "Brightness increased\n");
	} else if ((notify & ~0x20) <= 15) {
		sc->s_brn = notify & ~0x20;
		ACPI_VPRINT(sc->dev, acpi_sc, "Brightness decreased\n");
	} else if (notify == 0x33) {
		sc->s_lcd = 1;
		ACPI_VPRINT(sc->dev, acpi_sc, "LCD turned on\n");
	} else if (notify == 0x34) {
		sc->s_lcd = 0;
		ACPI_VPRINT(sc->dev, acpi_sc, "LCD turned off\n");
	} else {
		/* Notify devd(8) */
		acpi_UserNotify("ASUS", h, notify);
	}
}
