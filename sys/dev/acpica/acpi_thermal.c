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
 *
 *	$FreeBSD$
 */

#include "opt_acpi.h"
#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/bus.h>
#include <sys/reboot.h>

#include "acpi.h"

#include <dev/acpica/acpivar.h>

/*
 * Hooks for the ACPI CA debugging infrastructure
 */
#define _COMPONENT	ACPI_THERMAL_ZONE
MODULE_NAME("THERMAL")

#define TZ_ZEROC	2732
#define TZ_KELVTOC(x)	(((x) - TZ_ZEROC) / 10), (((x) - TZ_ZEROC) % 10)

#define TZ_NOTIFY_TEMPERATURE	0x80
#define TZ_NOTIFY_DEVICES	0x81
#define TZ_NOTIFY_LEVELS	0x82

#define TZ_POLLRATE	(hz * 10)	/* every ten seconds */

#define TZ_NUMLEVELS	10		/* defined by ACPI spec */
struct acpi_tz_state {
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
    ACPI_HANDLE			tz_handle;
    struct callout_handle	tz_timeout;
    int				tz_current;
#define TZ_STATE_NONE		0
#define TZ_STATE_PSV		1
#define TZ_STATE_AC0		2
#define TZ_STATE_HOT		(TZ_STATE_AC0 + TZ_NUMLEVELS)
#define TZ_STATE_CRT		(TZ_STATE_AC0 + TZ_NUMLEVELS + 1)
    
    struct acpi_tz_state 	tz_state;
};

static int	acpi_tz_probe(device_t dev);
static int	acpi_tz_attach(device_t dev);
static int	acpi_tz_establish(struct acpi_tz_softc *sc);
static void	acpi_tz_all_off(struct acpi_tz_softc *sc);
static void	acpi_tz_switch_cooler_off(ACPI_OBJECT *obj, void *arg);
static void	acpi_tz_switch_cooler_on(ACPI_OBJECT *obj, void *arg);
static void	acpi_tz_getparam(struct acpi_tz_softc *sc, char *node, int *data);
static void	acpi_tz_notify_handler(ACPI_HANDLE h, UINT32 notify, void *context);
static void	acpi_tz_timeout(void *arg);

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

devclass_t acpi_tz_devclass;
DRIVER_MODULE(acpi_tz, acpi, acpi_tz_driver, acpi_tz_devclass, 0, 0);

/*
 * Match an ACPI thermal zone.
 */
static int
acpi_tz_probe(device_t dev)
{

    /* no FUNCTION_TRACE - too noisy */

    if ((acpi_get_type(dev) == ACPI_TYPE_THERMAL) &&
	!acpi_disabled("thermal")) {
	device_set_desc(dev, "thermal zone");
	return(-10);
    }
    return(ENXIO);
}

/*
 * Attach to an ACPI thermal zone.
 */
static int
acpi_tz_attach(device_t dev)
{
    struct acpi_tz_softc	*sc;
    int				error;

    FUNCTION_TRACE(__func__);

    sc = device_get_softc(dev);
    sc->tz_dev = dev;
    sc->tz_handle = acpi_get_handle(dev);

    /*
     * Parse the current state of the thermal zone and build control
     * structures.
     */
    if ((error = acpi_tz_establish(sc)) != 0)
	return_VALUE(error);
    
    /*
     * Register for any Notify events sent to this zone.
     */
    AcpiInstallNotifyHandler(sc->tz_handle, ACPI_DEVICE_NOTIFY, 
			     acpi_tz_notify_handler, dev);

    /*
     * Don't bother evaluating/printing the temperature at this point;
     * on many systems it'll be bogus until the EC is running.
     */
    return_VALUE(0);
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
    
    FUNCTION_TRACE(__func__);

    /*
     * Power everything off and erase any existing state.
     */
    acpi_tz_all_off(sc);
    for (i = 0; i < TZ_NUMLEVELS; i++)
	if (sc->tz_state.al[i].Pointer != NULL)
	    AcpiOsFree(sc->tz_state.al[i].Pointer);
    if (sc->tz_state.psl.Pointer != NULL)
	AcpiOsFree(sc->tz_state.psl.Pointer);
    bzero(&sc->tz_state, sizeof(sc->tz_state));

    /* kill the timeout (harmless if not running */
    untimeout(acpi_tz_timeout, sc, sc->tz_timeout);
    
    /*
     * Evaluate thermal zone parameters.
     */
    for (i = 0; i < TZ_NUMLEVELS; i++) {
	sprintf(nbuf, "_AC%d", i);
	acpi_tz_getparam(sc, nbuf, &sc->tz_state.ac[i]);
	sprintf(nbuf, "_AL%d", i);
	acpi_EvaluateIntoBuffer(sc->tz_handle, nbuf, NULL, &sc->tz_state.al[i]);
	obj = (ACPI_OBJECT *)sc->tz_state.al[i].Pointer;
	if (obj != NULL) {
	    /* should be a package containing a list of power objects */
	    if (obj->Type != ACPI_TYPE_PACKAGE) {
		device_printf(sc->tz_dev, "%s has unknown object type %d, rejecting\n",
			      nbuf, obj->Type);
		return_VALUE(ENXIO);
	    }
	}
    }
    acpi_tz_getparam(sc, "_CRT", &sc->tz_state.crt);
    acpi_tz_getparam(sc, "_HOT", &sc->tz_state.hot);
    acpi_EvaluateIntoBuffer(sc->tz_handle, "_PSL", NULL, &sc->tz_state.psl);
    acpi_tz_getparam(sc, "_PSV", &sc->tz_state.psv);
    acpi_tz_getparam(sc, "_TC1", &sc->tz_state.tc1);
    acpi_tz_getparam(sc, "_TC2", &sc->tz_state.tc2);
    acpi_tz_getparam(sc, "_TSP", &sc->tz_state.tsp);
    acpi_tz_getparam(sc, "_TZP", &sc->tz_state.tzp);

    /*
     * Power off everything that we've just been given.
     */
    acpi_tz_all_off(sc);

    /*
     * Do we need to poll the thermal zone?  Ignore the suggested
     * rate.
     */
    if (sc->tz_state.tzp != 0)
	sc->tz_timeout = timeout(acpi_tz_timeout, sc, TZ_POLLRATE);
	

    return_VALUE(0);
}

/*
 * Evaluate the condition of a thermal zone, take appropriate actions.
 */
static void
acpi_tz_monitor(struct acpi_tz_softc *sc)
{
    int		temp, new;
    int		i;

    FUNCTION_TRACE(__func__);

    /*
     * Get the current temperature.
     */
    if ((acpi_EvaluateInteger(sc->tz_handle, "_TMP", &temp)) != AE_OK) {
	device_printf(sc->tz_dev, "error fetching current temperature\n");
	/* XXX disable zone? go to max cooling? */
	return_VOID;
    }

    /*
     * Work out what we ought to be doing right now.
     */
    new = TZ_STATE_NONE;
    if ((sc->tz_state.psv != -1) && (temp > sc->tz_state.psv))
	new = TZ_STATE_PSV;
    for (i = 0; i < TZ_NUMLEVELS; i++)
	if ((sc->tz_state.ac[i] != -1) && (temp > sc->tz_state.ac[i]))
	    new = TZ_STATE_AC0 + i;
    if ((sc->tz_state.hot != -1) && (temp > sc->tz_state.hot))
	new = TZ_STATE_HOT;
    if ((sc->tz_state.crt != -1) && (temp > sc->tz_state.crt))
	new = TZ_STATE_CRT;

    /*
     * If our state has not changed, do nothing.
     */
    if (new == sc->tz_current)
	return_VOID;
    
    /*
     * XXX if we're in a passive-cooling mode, revert to full-speed operation.
     */
    if (sc->tz_current == TZ_STATE_PSV) {
	/* XXX implement */
    }

    /*
     * If we're in an active-cooling mode, turn off the current cooler(s).
     */
    if ((sc->tz_current >= TZ_STATE_AC0) && (sc->tz_current < (TZ_STATE_AC0 + TZ_NUMLEVELS)))
	acpi_ForeachPackageObject((ACPI_OBJECT *)sc->tz_state.al[sc->tz_current - TZ_STATE_AC0].Pointer,
				  acpi_tz_switch_cooler_off, sc);

    /*
     * XXX If the new mode is passive-cooling, make appropriate adjustments.
     */

    /*
     * If the new mode is an active-cooling mode, turn on the new cooler(s).
     */
    if ((new >= TZ_STATE_AC0) && (new < (TZ_STATE_AC0 + TZ_NUMLEVELS)))
	acpi_ForeachPackageObject((ACPI_OBJECT *)sc->tz_state.al[new - TZ_STATE_AC0].Pointer,
				  acpi_tz_switch_cooler_on, sc);

    /*
     * If we're _HOT or _CRT, shut down now!
     */
    if ((new == TZ_STATE_HOT) || (new == TZ_STATE_CRT)) {
	device_printf(sc->tz_dev, "WARNING - emergency thermal shutdown in progress.\n");
	shutdown_nice(RB_POWEROFF);
    }

    /* gone to new state */
    sc->tz_current = new;

    return_VOID;
}

/*
 * Turn off all the cooling devices.
 */
static void
acpi_tz_all_off(struct acpi_tz_softc *sc)
{
    int		i;

    FUNCTION_TRACE(__func__);
    
    /*
     * Scan all the _AL objects, and turn them all off.
     */
    for (i = 0; i < TZ_NUMLEVELS; i++) {
	if (sc->tz_state.al[i].Pointer == NULL)
	    continue;
	acpi_ForeachPackageObject((ACPI_OBJECT *)sc->tz_state.al[i].Pointer,
				  acpi_tz_switch_cooler_off, sc);
    }

    /*
     * XXX revert any passive-cooling options.
     */

    sc->tz_current = TZ_STATE_NONE;
    return_VOID;
}

/*
 * Given an object, verify that it's a reference to a device of some sort, 
 * and try to switch it off.
 */
static void
acpi_tz_switch_cooler_off(ACPI_OBJECT *obj, void *arg)
{
    struct acpi_tz_softc	*sc = (struct acpi_tz_softc *)arg;
    ACPI_HANDLE			cooler;

    FUNCTION_TRACE(__func__);

    switch(obj->Type) {
    case ACPI_TYPE_STRING:
	DEBUG_PRINT(TRACE_OBJECTS, ("called to turn %s off\n", obj->String.Pointer));

	/*
	 * Find the handle for the device and turn it off.
	 * The String object here seems to contain a fully-qualified path, so we
	 * don't have to search for it in our parents.
	 *
	 * XXX This may not always be the case.
	 */
	if (AcpiGetHandle(obj->String.Pointer, NULL, &cooler) == AE_OK)
	    acpi_pwr_switch_consumer(cooler, ACPI_STATE_D3);
	break;
	
    default:
	DEBUG_PRINT(TRACE_OBJECTS, ("called to handle unsupported object type %d\n",
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
    ACPI_HANDLE			cooler, parent;

    FUNCTION_TRACE(__func__);

    switch(obj->Type) {
    case ACPI_TYPE_STRING:
	DEBUG_PRINT(TRACE_OBJECTS, ("called to turn %s off\n", obj->String.Pointer));

	/* find the handle for the device and turn it off */
	if (acpi_GetHandleInScope(sc->tz_handle, obj->String.Pointer, &cooler) == AE_OK)
	    acpi_pwr_switch_consumer(cooler, ACPI_STATE_D0);
	break;
	
    default:
	DEBUG_PRINT(TRACE_OBJECTS, ("called to handle unsupported object type %d\n",
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

    FUNCTION_TRACE(__func__);

    if (acpi_EvaluateInteger(sc->tz_handle, node, data) != AE_OK) {
	*data = -1;
    } else {
	DEBUG_PRINT(TRACE_VALUES, ("%s.%s = %d\n", acpi_name(sc->tz_handle),
				   node, *data));
    }
    return_VOID;    
}
    
/*
 * Respond to a Notify event sent to the zone.
 */
static void
acpi_tz_notify_handler(ACPI_HANDLE h, UINT32 notify, void *context)
{
    struct acpi_tz_softc	*sc = (struct acpi_tz_softc *)context;

    FUNCTION_TRACE(__func__);

    switch(notify) {
    case TZ_NOTIFY_TEMPERATURE:
	acpi_tz_monitor(sc);	/* temperature change occurred */
	break;
    case TZ_NOTIFY_DEVICES:
    case TZ_NOTIFY_LEVELS:
	acpi_tz_establish(sc);	/* zone devices/setpoints changed */
	break;
    default:
	device_printf(sc->tz_dev, "unknown Notify event 0x%x\n", notify);
	break;
    }
    return_VOID;
}

/*
 * Poll the thermal zone.
 */
static void
acpi_tz_timeout(void *arg)
{
    struct acpi_tz_softc	*sc = (struct acpi_tz_softc *)arg;

    /* check temperature, take action */
    acpi_tz_monitor(sc);

    /* XXX passive cooling actions? */

    /* re-register ourself */
    sc->tz_timeout = timeout(acpi_tz_timeout, sc, TZ_POLLRATE);
}
