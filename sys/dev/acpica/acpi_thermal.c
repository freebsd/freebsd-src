/*-
 * Copyright (c) 2000, 2001 Michael Smith
 * Copyright (c) 2000 BSDi
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
__FBSDID("$FreeBSD$");

#include "opt_acpi.h"
#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/kthread.h>
#include <sys/bus.h>
#include <sys/proc.h>
#include <sys/reboot.h>
#include <sys/sysctl.h>
#include <sys/unistd.h>
#include <sys/power.h>

#include "acpi.h"
#include <dev/acpica/acpivar.h>

/* Hooks for the ACPI CA debugging infrastructure */
#define _COMPONENT	ACPI_THERMAL
ACPI_MODULE_NAME("THERMAL")

#define TZ_ZEROC	2732
#define TZ_KELVTOC(x)	(((x) - TZ_ZEROC) / 10), (((x) - TZ_ZEROC) % 10)

#define TZ_NOTIFY_TEMPERATURE	0x80
#define TZ_NOTIFY_DEVICES	0x81
#define TZ_NOTIFY_LEVELS	0x82

/* Check for temperature changes every 30 seconds by default */
#define TZ_POLLRATE	30

/* ACPI spec defines this */
#define TZ_NUMLEVELS	10
struct acpi_tz_zone {
    int		ac[TZ_NUMLEVELS];
    ACPI_BUFFER	al[TZ_NUMLEVELS];
    int		crt;
    int		hot;
    ACPI_BUFFER	psl;
    int		psv;
    int		tc1;
    int		tc2;
    int		tsp;
    int		tzp;
};


struct acpi_tz_softc {
    device_t			tz_dev;
    ACPI_HANDLE			tz_handle;	/*Thermal zone handle*/
    int				tz_temperature;	/*Current temperature*/
    int				tz_active;	/*Current active cooling*/
#define TZ_ACTIVE_NONE		-1
    int				tz_requested;	/*Minimum active cooling*/
    int				tz_thflags;	/*Current temp-related flags*/
#define TZ_THFLAG_NONE		0
#define TZ_THFLAG_PSV		(1<<0)
#define TZ_THFLAG_HOT		(1<<2)
#define TZ_THFLAG_CRT		(1<<3)    
    int				tz_flags;
#define TZ_FLAG_NO_SCP		(1<<0)		/*No _SCP method*/
#define TZ_FLAG_GETPROFILE	(1<<1)		/*Get power_profile in timeout*/
    struct timespec		tz_cooling_started;
					/*Current cooling starting time*/

    struct sysctl_ctx_list	tz_sysctl_ctx;
    struct sysctl_oid		*tz_sysctl_tree;
    
    struct acpi_tz_zone 	tz_zone;	/*Thermal zone parameters*/
    int				tz_tmp_updating;
};

static int	acpi_tz_probe(device_t dev);
static int	acpi_tz_attach(device_t dev);
static int	acpi_tz_establish(struct acpi_tz_softc *sc);
static void	acpi_tz_monitor(void *Context);
static void	acpi_tz_all_off(struct acpi_tz_softc *sc);
static void	acpi_tz_switch_cooler_off(ACPI_OBJECT *obj, void *arg);
static void	acpi_tz_switch_cooler_on(ACPI_OBJECT *obj, void *arg);
static void	acpi_tz_getparam(struct acpi_tz_softc *sc, char *node,
				 int *data);
static void	acpi_tz_sanity(struct acpi_tz_softc *sc, int *val, char *what);
static int	acpi_tz_active_sysctl(SYSCTL_HANDLER_ARGS);
static void	acpi_tz_notify_handler(ACPI_HANDLE h, UINT32 notify,
				       void *context);
static void	acpi_tz_timeout(struct acpi_tz_softc *sc);
static void	acpi_tz_power_profile(void *arg);
static void	acpi_tz_thread(void *arg);

static device_method_t acpi_tz_methods[] = {
    /* Device interface */
    DEVMETHOD(device_probe,	acpi_tz_probe),
    DEVMETHOD(device_attach,	acpi_tz_attach),

    {0, 0}
};

static driver_t acpi_tz_driver = {
    "acpi_tz",
    acpi_tz_methods,
    sizeof(struct acpi_tz_softc),
};

static devclass_t acpi_tz_devclass;
DRIVER_MODULE(acpi_tz, acpi, acpi_tz_driver, acpi_tz_devclass, 0, 0);

static struct sysctl_ctx_list	acpi_tz_sysctl_ctx;
static struct sysctl_oid	*acpi_tz_sysctl_tree;

/* Minimum cooling run time */
static int			acpi_tz_min_runtime = 0;
static int			acpi_tz_polling_rate = TZ_POLLRATE;

/* Timezone polling thread */
static struct proc		*acpi_tz_proc;

/*
 * Match an ACPI thermal zone.
 */
static int
acpi_tz_probe(device_t dev)
{
    int		result;
    ACPI_LOCK_DECL;
    
    ACPI_LOCK;
    
    /* No FUNCTION_TRACE - too noisy */

    if (acpi_get_type(dev) == ACPI_TYPE_THERMAL && !acpi_disabled("thermal")) {
	device_set_desc(dev, "thermal zone");
	result = -10;
    } else {
	result = ENXIO;
    }
    ACPI_UNLOCK;
    return (result);
}

/*
 * Attach to an ACPI thermal zone.
 */
static int
acpi_tz_attach(device_t dev)
{
    struct acpi_tz_softc	*sc;
    struct acpi_softc		*acpi_sc;
    int				error;
    char			oidname[8];
    ACPI_LOCK_DECL;

    ACPI_FUNCTION_TRACE((char *)(uintptr_t)__func__);

    ACPI_LOCK;

    sc = device_get_softc(dev);
    sc->tz_dev = dev;
    sc->tz_handle = acpi_get_handle(dev);
    sc->tz_requested = TZ_ACTIVE_NONE;
    sc->tz_tmp_updating = 0;

    /*
     * Parse the current state of the thermal zone and build control
     * structures.
     */
    if ((error = acpi_tz_establish(sc)) != 0)
	goto out;
    
    /*
     * Register for any Notify events sent to this zone.
     */
    AcpiInstallNotifyHandler(sc->tz_handle, ACPI_DEVICE_NOTIFY, 
			     acpi_tz_notify_handler, sc);

    /*
     * Create our sysctl nodes.
     *
     * XXX we need a mechanism for adding nodes under ACPI.
     */
    if (device_get_unit(dev) == 0) {
	acpi_sc = acpi_device_get_parent_softc(dev);
	sysctl_ctx_init(&acpi_tz_sysctl_ctx);
	acpi_tz_sysctl_tree = SYSCTL_ADD_NODE(&acpi_tz_sysctl_ctx,
			      SYSCTL_CHILDREN(acpi_sc->acpi_sysctl_tree),
			      OID_AUTO, "thermal", CTLFLAG_RD, 0, "");
	SYSCTL_ADD_INT(&acpi_tz_sysctl_ctx,
		       SYSCTL_CHILDREN(acpi_tz_sysctl_tree),
		       OID_AUTO, "min_runtime", CTLFLAG_RD | CTLFLAG_RW,
		       &acpi_tz_min_runtime, 0,
		       "minimum cooling run time in sec");
	SYSCTL_ADD_INT(&acpi_tz_sysctl_ctx,
		       SYSCTL_CHILDREN(acpi_tz_sysctl_tree),
		       OID_AUTO, "polling_rate", CTLFLAG_RD | CTLFLAG_RW,
		       &acpi_tz_polling_rate, 0, "monitor polling rate");
    }
    sysctl_ctx_init(&sc->tz_sysctl_ctx);
    sprintf(oidname, "tz%d", device_get_unit(dev));
    sc->tz_sysctl_tree = SYSCTL_ADD_NODE(&sc->tz_sysctl_ctx,
					 SYSCTL_CHILDREN(acpi_tz_sysctl_tree),
					 OID_AUTO, oidname, CTLFLAG_RD, 0, "");
    SYSCTL_ADD_INT(&sc->tz_sysctl_ctx, SYSCTL_CHILDREN(sc->tz_sysctl_tree),
		   OID_AUTO, "temperature", CTLFLAG_RD,
		   &sc->tz_temperature, 0, "current thermal zone temperature");
    SYSCTL_ADD_PROC(&sc->tz_sysctl_ctx, SYSCTL_CHILDREN(sc->tz_sysctl_tree),
		    OID_AUTO, "active", CTLTYPE_INT | CTLFLAG_RW,
		    sc, 0, acpi_tz_active_sysctl, "I", "");
    
    SYSCTL_ADD_INT(&sc->tz_sysctl_ctx, SYSCTL_CHILDREN(sc->tz_sysctl_tree),
		   OID_AUTO, "thermal_flags", CTLFLAG_RD,
		   &sc->tz_thflags, 0, "thermal zone flags");
    SYSCTL_ADD_INT(&sc->tz_sysctl_ctx, SYSCTL_CHILDREN(sc->tz_sysctl_tree),
		   OID_AUTO, "_PSV", CTLFLAG_RD,
		   &sc->tz_zone.psv, 0, "");
    SYSCTL_ADD_INT(&sc->tz_sysctl_ctx, SYSCTL_CHILDREN(sc->tz_sysctl_tree),
		   OID_AUTO, "_HOT", CTLFLAG_RD,
		   &sc->tz_zone.hot, 0, "");
    SYSCTL_ADD_INT(&sc->tz_sysctl_ctx, SYSCTL_CHILDREN(sc->tz_sysctl_tree),
		   OID_AUTO, "_CRT", CTLFLAG_RD,
		   &sc->tz_zone.crt, 0, "");
    SYSCTL_ADD_OPAQUE(&sc->tz_sysctl_ctx, SYSCTL_CHILDREN(sc->tz_sysctl_tree),
		      OID_AUTO, "_ACx", CTLFLAG_RD, &sc->tz_zone.ac,
		      sizeof(sc->tz_zone.ac), "I", "");

    /*
     * Register our power profile event handler, and flag it for a manual
     * invocation by our timeout.  We defer it like this so that the rest
     * of the subsystem has time to come up.
     */
    EVENTHANDLER_REGISTER(power_profile_change, acpi_tz_power_profile, sc, 0);
    sc->tz_flags |= TZ_FLAG_GETPROFILE;

    /*
     * Don't bother evaluating/printing the temperature at this point;
     * on many systems it'll be bogus until the EC is running.
     */

    /*
     * Create our thread; we only need one, it will service all of the
     * thermal zones.
     */
    if (acpi_tz_proc == NULL) {
	    error = kthread_create(acpi_tz_thread, NULL, &acpi_tz_proc,
				   RFHIGHPID, 0, "acpi_thermal");
	    if (error != 0) {
		    device_printf(sc->tz_dev, "could not create thread - %d",
				  error);
		    goto out;
	    }
    }
	
 out:
    ACPI_UNLOCK;

    return_VALUE (error);
}

/*
 * Parse the current state of this thermal zone and set up to use it.
 *
 * Note that we may have previous state, which will have to be discarded.
 */
static int
acpi_tz_establish(struct acpi_tz_softc *sc)
{
    ACPI_OBJECT	*obj;
    int		i;
    char	nbuf[8];
    
    ACPI_FUNCTION_TRACE((char *)(uintptr_t)__func__);

    ACPI_ASSERTLOCK;

    /* Power everything off and erase any existing state. */
    acpi_tz_all_off(sc);
    for (i = 0; i < TZ_NUMLEVELS; i++)
	if (sc->tz_zone.al[i].Pointer != NULL)
	    AcpiOsFree(sc->tz_zone.al[i].Pointer);
    if (sc->tz_zone.psl.Pointer != NULL)
	AcpiOsFree(sc->tz_zone.psl.Pointer);
    bzero(&sc->tz_zone, sizeof(sc->tz_zone));

    /* Evaluate thermal zone parameters. */
    for (i = 0; i < TZ_NUMLEVELS; i++) {
	sprintf(nbuf, "_AC%d", i);
	acpi_tz_getparam(sc, nbuf, &sc->tz_zone.ac[i]);
	sprintf(nbuf, "_AL%d", i);
	sc->tz_zone.al[i].Length = ACPI_ALLOCATE_BUFFER;
	sc->tz_zone.al[i].Pointer = NULL;
	AcpiEvaluateObject(sc->tz_handle, nbuf, NULL, &sc->tz_zone.al[i]);
	obj = (ACPI_OBJECT *)sc->tz_zone.al[i].Pointer;
	if (obj != NULL) {
	    /* Should be a package containing a list of power objects */
	    if (obj->Type != ACPI_TYPE_PACKAGE) {
		device_printf(sc->tz_dev, "%s has unknown type %d, rejecting\n",
			      nbuf, obj->Type);
		return_VALUE (ENXIO);
	    }
	}
    }
    acpi_tz_getparam(sc, "_CRT", &sc->tz_zone.crt);
    acpi_tz_getparam(sc, "_HOT", &sc->tz_zone.hot);
    sc->tz_zone.psl.Length = ACPI_ALLOCATE_BUFFER;
    sc->tz_zone.psl.Pointer = NULL;
    AcpiEvaluateObject(sc->tz_handle, "_PSL", NULL, &sc->tz_zone.psl);
    acpi_tz_getparam(sc, "_PSV", &sc->tz_zone.psv);
    acpi_tz_getparam(sc, "_TC1", &sc->tz_zone.tc1);
    acpi_tz_getparam(sc, "_TC2", &sc->tz_zone.tc2);
    acpi_tz_getparam(sc, "_TSP", &sc->tz_zone.tsp);
    acpi_tz_getparam(sc, "_TZP", &sc->tz_zone.tzp);

    /*
     * Sanity-check the values we've been given.
     *
     * XXX what do we do about systems that give us the same value for
     *     more than one of these setpoints?
     */
    acpi_tz_sanity(sc, &sc->tz_zone.crt, "_CRT");
    acpi_tz_sanity(sc, &sc->tz_zone.hot, "_HOT");
    acpi_tz_sanity(sc, &sc->tz_zone.psv, "_PSV");
    for (i = 0; i < TZ_NUMLEVELS; i++)
	acpi_tz_sanity(sc, &sc->tz_zone.ac[i], "_ACx");

    /*
     * Power off everything that we've just been given.
     */
    acpi_tz_all_off(sc);

    return_VALUE (0);
}

static char	*aclevel_string[] =	{
	"NONE", "_AC0", "_AC1", "_AC2", "_AC3", "_AC4",
	"_AC5", "_AC6", "_AC7", "_AC8", "_AC9" };

static __inline const char *
acpi_tz_aclevel_string(int active)
{
	if (active < -1 || active >= TZ_NUMLEVELS)
		return (aclevel_string[0]);

	return (aclevel_string[active+1]);
}

/*
 * Evaluate the condition of a thermal zone, take appropriate actions.
 */
static void
acpi_tz_monitor(void *Context)
{
    struct acpi_tz_softc *sc;
    struct	timespec curtime;
    int		temp;
    int		i;
    int		newactive, newflags;
    ACPI_STATUS	status;

    ACPI_FUNCTION_TRACE((char *)(uintptr_t)__func__);

    ACPI_ASSERTLOCK;

    sc = (struct acpi_tz_softc *)Context;
    if (sc->tz_tmp_updating)
	goto out;
    sc->tz_tmp_updating = 1;

    /* Get the current temperature. */
    status = acpi_EvaluateInteger(sc->tz_handle, "_TMP", &temp);
    if (ACPI_FAILURE(status)) {
	ACPI_VPRINT(sc->tz_dev, acpi_device_get_parent_softc(sc->tz_dev),
	    "error fetching current temperature -- %s\n",
	     AcpiFormatException(status));
	/* XXX disable zone? go to max cooling? */
	goto out;
    }

    ACPI_DEBUG_PRINT((ACPI_DB_VALUES, "got %d.%dC\n", TZ_KELVTOC(temp)));
    sc->tz_temperature = temp;

    /*
     * Work out what we ought to be doing right now.
     *
     * Note that the _ACx levels sort from hot to cold.
     */
    newactive = TZ_ACTIVE_NONE;
    for (i = TZ_NUMLEVELS - 1; i >= 0; i--) {
	if ((sc->tz_zone.ac[i] != -1) && (temp >= sc->tz_zone.ac[i])) {
	    newactive = i;
	    if (sc->tz_active != newactive) {
		ACPI_VPRINT(sc->tz_dev,
			    acpi_device_get_parent_softc(sc->tz_dev),
			    "_AC%d: temperature %d.%d >= setpoint %d.%d\n", i,
			    TZ_KELVTOC(temp), TZ_KELVTOC(sc->tz_zone.ac[i]));
		getnanotime(&sc->tz_cooling_started);
	    }
	}
    }

    /*
     * We are going to get _ACx level down (colder side), but give a guaranteed
     * minimum cooling run time if requested.
     */
    if (acpi_tz_min_runtime > 0 && sc->tz_active != TZ_ACTIVE_NONE &&
	(newactive == TZ_ACTIVE_NONE || newactive > sc->tz_active)) {

	getnanotime(&curtime);
	timespecsub(&curtime, &sc->tz_cooling_started);
	if (curtime.tv_sec < acpi_tz_min_runtime)
	    newactive = sc->tz_active;
    }

    /* Handle user override of active mode */
    if (sc->tz_requested > newactive)
	newactive = sc->tz_requested;

    /* update temperature-related flags */
    newflags = TZ_THFLAG_NONE;
    if ((sc->tz_zone.psv != -1) && (temp >= sc->tz_zone.psv))
	newflags |= TZ_THFLAG_PSV;
    if ((sc->tz_zone.hot != -1) && (temp >= sc->tz_zone.hot))
	newflags |= TZ_THFLAG_HOT;
    if ((sc->tz_zone.crt != -1) && (temp >= sc->tz_zone.crt))
	newflags |= TZ_THFLAG_CRT;

    /* If the active cooling state has changed, we have to switch things. */
    if (newactive != sc->tz_active) {
	/* Turn off the cooling devices that are on, if any are */
	if (sc->tz_active != TZ_ACTIVE_NONE)
	    acpi_ForeachPackageObject(
		(ACPI_OBJECT *)sc->tz_zone.al[sc->tz_active].Pointer,
		acpi_tz_switch_cooler_off, sc);

	/* Turn on cooling devices that are required, if any are */
	if (newactive != TZ_ACTIVE_NONE) {
	    acpi_ForeachPackageObject(
		(ACPI_OBJECT *)sc->tz_zone.al[newactive].Pointer,
		acpi_tz_switch_cooler_on, sc);
	}
	ACPI_VPRINT(sc->tz_dev, acpi_device_get_parent_softc(sc->tz_dev),
		    "switched from %s to %s: %d.%dC\n",
		    acpi_tz_aclevel_string(sc->tz_active),
		    acpi_tz_aclevel_string(newactive), TZ_KELVTOC(temp));
	sc->tz_active = newactive;
    }

    /* XXX (de)activate any passive cooling that may be required. */

    /*
     * If we have just become _HOT or _CRT, warn the user.
     *
     * We should actually shut down at this point, but it's not clear
     * that some systems don't actually map _CRT to the same value as _AC0.
     */
    if ((newflags & (TZ_THFLAG_HOT | TZ_THFLAG_CRT)) != 0 && 
	(sc->tz_thflags & (TZ_THFLAG_HOT | TZ_THFLAG_CRT)) == 0) {

	device_printf(sc->tz_dev,
	    "WARNING - current temperature (%d.%dC) exceeds system limits\n",
		      TZ_KELVTOC(sc->tz_temperature));
	/* shutdown_nice(RB_POWEROFF);*/
    }
    sc->tz_thflags = newflags;

out:
    sc->tz_tmp_updating = 0;
    return_VOID;
}

/*
 * Turn off all the cooling devices.
 */
static void
acpi_tz_all_off(struct acpi_tz_softc *sc)
{
    int		i;

    ACPI_FUNCTION_TRACE((char *)(uintptr_t)__func__);

    ACPI_ASSERTLOCK;
    
    /* Scan all the _ALx objects and turn them all off. */
    for (i = 0; i < TZ_NUMLEVELS; i++) {
	if (sc->tz_zone.al[i].Pointer == NULL)
	    continue;
	acpi_ForeachPackageObject((ACPI_OBJECT *)sc->tz_zone.al[i].Pointer,
				  acpi_tz_switch_cooler_off, sc);
    }

    /*
     * XXX revert any passive-cooling options.
     */

    sc->tz_active = TZ_ACTIVE_NONE;
    sc->tz_thflags = TZ_THFLAG_NONE;

    return_VOID;
}

/*
 * Given an object, verify that it's a reference to a device of some sort, 
 * and try to switch it off.
 */
static void
acpi_tz_switch_cooler_off(ACPI_OBJECT *obj, void *arg)
{
    ACPI_HANDLE		cooler;

    ACPI_FUNCTION_TRACE((char *)(uintptr_t)__func__);

    ACPI_ASSERTLOCK;

    switch(obj->Type) {
    case ACPI_TYPE_ANY:
	ACPI_DEBUG_PRINT((ACPI_DB_OBJECTS, "called to turn %s off\n",
			 acpi_name(obj->Reference.Handle)));

	acpi_pwr_switch_consumer(obj->Reference.Handle, ACPI_STATE_D3);
	break;
    case ACPI_TYPE_STRING:
	ACPI_DEBUG_PRINT((ACPI_DB_OBJECTS, "called to turn %s off\n",
			 obj->String.Pointer));

	/*
	 * Find the handle for the device and turn it off.
	 * The String object here seems to contain a fully-qualified path, so we
	 * don't have to search for it in our parents.
	 *
	 * XXX This may not always be the case.
	 */
	if (ACPI_SUCCESS(AcpiGetHandle(NULL, obj->String.Pointer, &cooler)))
	    acpi_pwr_switch_consumer(cooler, ACPI_STATE_D3);
	break;
    default:
	ACPI_DEBUG_PRINT((ACPI_DB_OBJECTS,
			 "called to handle unsupported object type %d\n",
			 obj->Type));
	break;
    }

    return_VOID;
}

/*
 * Given an object, verify that it's a reference to a device of some sort, 
 * and try to switch it on.
 *
 * XXX replication of off/on function code is bad, mmmkay?
 */
static void
acpi_tz_switch_cooler_on(ACPI_OBJECT *obj, void *arg)
{
    struct acpi_tz_softc	*sc = (struct acpi_tz_softc *)arg;
    ACPI_HANDLE			cooler;
    ACPI_STATUS			status;
    
    ACPI_FUNCTION_TRACE((char *)(uintptr_t)__func__);

    ACPI_ASSERTLOCK;

    switch(obj->Type) {
    case ACPI_TYPE_ANY:
	ACPI_DEBUG_PRINT((ACPI_DB_OBJECTS, "called to turn %s on\n",
			 acpi_name(obj->Reference.Handle)));

	status = acpi_pwr_switch_consumer(obj->Reference.Handle, ACPI_STATE_D0);
	if (ACPI_FAILURE(status)) {
	    ACPI_VPRINT(sc->tz_dev, acpi_device_get_parent_softc(sc->tz_dev),
			"failed to activate %s - %s\n",
			acpi_name(obj->Reference.Handle),
			AcpiFormatException(status));
	}
	break;
    case ACPI_TYPE_STRING:
	ACPI_DEBUG_PRINT((ACPI_DB_OBJECTS, "called to turn %s on\n",
			 obj->String.Pointer));

	/*
	 * Find the handle for the device and turn it off.
	 * The String object here seems to contain a fully-qualified path, so we
	 * don't have to search for it in our parents.
	 *
	 * XXX This may not always be the case.
	 */
	if (ACPI_SUCCESS(AcpiGetHandle(NULL, obj->String.Pointer, &cooler))) {
	    status = acpi_pwr_switch_consumer(cooler, ACPI_STATE_D0);
	    if (ACPI_FAILURE(status)) {
		ACPI_VPRINT(sc->tz_dev,
			    acpi_device_get_parent_softc(sc->tz_dev),
			    "failed to activate %s - %s\n",
			    obj->String.Pointer, AcpiFormatException(status));
	    }
	} else {
	    ACPI_VPRINT(sc->tz_dev, acpi_device_get_parent_softc(sc->tz_dev),
			"couldn't find %s\n", obj->String.Pointer);
	}
	break;
    default:
	ACPI_DEBUG_PRINT((ACPI_DB_OBJECTS, "unsupported object type %d\n",
			  obj->Type));
	break;
    }

    return_VOID;
}

/*
 * Read/debug-print a parameter, default it to -1.
 */
static void
acpi_tz_getparam(struct acpi_tz_softc *sc, char *node, int *data)
{

    ACPI_FUNCTION_TRACE((char *)(uintptr_t)__func__);

    ACPI_ASSERTLOCK;

    if (ACPI_FAILURE(acpi_EvaluateInteger(sc->tz_handle, node, data))) {
	*data = -1;
    } else {
	ACPI_DEBUG_PRINT((ACPI_DB_VALUES, "%s.%s = %d\n",
			 acpi_name(sc->tz_handle), node, *data));
    }

    return_VOID;    
}

/*
 * Sanity-check a temperature value.  Assume that setpoints
 * should be between 0C and 150C.
 */
static void
acpi_tz_sanity(struct acpi_tz_softc *sc, int *val, char *what)
{
    if (*val != -1 && (*val < TZ_ZEROC || *val > TZ_ZEROC + 1500)) {
	device_printf(sc->tz_dev, "%s value is absurd, ignored (%d.%dC)\n",
		      what, TZ_KELVTOC(*val));
	*val = -1;
    }
}

/*
 * Respond to a sysctl on the active state node.
 */    
static int
acpi_tz_active_sysctl(SYSCTL_HANDLER_ARGS)
{
    struct acpi_tz_softc	*sc;
    int				active;
    int		 		error;
    ACPI_LOCK_DECL;

    ACPI_LOCK;

    sc = (struct acpi_tz_softc *)oidp->oid_arg1;
    active = sc->tz_active;
    error = sysctl_handle_int(oidp, &active, 0, req);

    /* Error or no new value */
    if (error != 0 || req->newptr == NULL)
	goto out;
    if (active < -1 || active >= TZ_NUMLEVELS) {
	error = EINVAL;
	goto out;
    }

    /* Set new preferred level and re-switch */
    sc->tz_requested = active;
    acpi_tz_monitor(sc);

 out:
    ACPI_UNLOCK;
    return (error);
}

/*
 * Respond to a Notify event sent to the zone.
 */
static void
acpi_tz_notify_handler(ACPI_HANDLE h, UINT32 notify, void *context)
{
    struct acpi_tz_softc	*sc = (struct acpi_tz_softc *)context;

    ACPI_FUNCTION_TRACE((char *)(uintptr_t)__func__);

    ACPI_ASSERTLOCK;

    switch(notify) {
    case TZ_NOTIFY_TEMPERATURE:
	/* Temperature change occurred */
	AcpiOsQueueForExecution(OSD_PRIORITY_HIGH, acpi_tz_monitor, sc);
	break;
    case TZ_NOTIFY_DEVICES:
    case TZ_NOTIFY_LEVELS:
	/* Zone devices/setpoints changed */
	AcpiOsQueueForExecution(OSD_PRIORITY_HIGH,
				(OSD_EXECUTION_CALLBACK)acpi_tz_establish, sc);
	break;
    default:
	ACPI_VPRINT(sc->tz_dev, acpi_device_get_parent_softc(sc->tz_dev),
		    "unknown Notify event 0x%x\n", notify);
	break;
    }

    return_VOID;
}

/*
 * Poll the thermal zone.
 */
static void
acpi_tz_timeout(struct acpi_tz_softc *sc)
{
    /* Do we need to get the power profile settings? */
    if (sc->tz_flags & TZ_FLAG_GETPROFILE) {
	acpi_tz_power_profile((void *)sc);
	sc->tz_flags &= ~TZ_FLAG_GETPROFILE;
    }

    ACPI_ASSERTLOCK;

    /* Check the current temperature and take action based on it */
    acpi_tz_monitor(sc);

    /* XXX passive cooling actions? */
}

/*
 * System power profile may have changed; fetch and notify the
 * thermal zone accordingly.
 *
 * Since this can be called from an arbitrary eventhandler, it needs
 * to get the ACPI lock itself.
 */
static void
acpi_tz_power_profile(void *arg)
{
    ACPI_OBJECT_LIST		args;
    ACPI_OBJECT			obj;
    ACPI_STATUS			status;
    struct acpi_tz_softc	*sc = (struct acpi_tz_softc *)arg;
    int				state;
    ACPI_LOCK_DECL;

    state = power_profile_get_state();
    if (state != POWER_PROFILE_PERFORMANCE && state != POWER_PROFILE_ECONOMY)
	return;

    ACPI_LOCK;

    /* check that we haven't decided there's no _SCP method */
    if ((sc->tz_flags & TZ_FLAG_NO_SCP) == 0) {

	/* Call _SCP to set the new profile */
	obj.Type = ACPI_TYPE_INTEGER;
	obj.Integer.Value = (state == POWER_PROFILE_PERFORMANCE) ? 0 : 1;
	args.Count = 1;
	args.Pointer = &obj;
	status = AcpiEvaluateObject(sc->tz_handle, "_SCP", &args, NULL);
	if (ACPI_FAILURE(status)) {
	    if (status != AE_NOT_FOUND)
		ACPI_VPRINT(sc->tz_dev,
			    acpi_device_get_parent_softc(sc->tz_dev),
			    "can't evaluate %s._SCP - %s\n",
			    acpi_name(sc->tz_handle),
			    AcpiFormatException(status));
	    sc->tz_flags |= TZ_FLAG_NO_SCP;
	} else {
	    /* We have to re-evaluate the entire zone now */
	    AcpiOsQueueForExecution(OSD_PRIORITY_HIGH,
				    (OSD_EXECUTION_CALLBACK)acpi_tz_establish,
				    sc);
	}
    }

    ACPI_UNLOCK;
}

/*
 * Thermal zone monitor thread.
 */
static void
acpi_tz_thread(void *arg)
{
    device_t	*devs;
    int		devcount, i;
    ACPI_LOCK_DECL;

    ACPI_FUNCTION_TRACE((char *)(uintptr_t)__func__);

    devs = NULL;
    devcount = 0;

    for (;;) {
	tsleep(&acpi_tz_proc, PZERO, "tzpoll", hz * acpi_tz_polling_rate);

#if __FreeBSD_version >= 500000
	mtx_lock(&Giant);
#endif

	if (devcount == 0)
	    devclass_get_devices(acpi_tz_devclass, &devs, &devcount);

	ACPI_LOCK;
	for (i = 0; i < devcount; i++)
	    acpi_tz_timeout(device_get_softc(devs[i]));
	ACPI_UNLOCK;

#if __FreeBSD_version >= 500000
	mtx_unlock(&Giant);
#endif
    }
}
