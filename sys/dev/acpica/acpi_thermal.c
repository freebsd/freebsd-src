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
#include <sys/sysctl.h>

#include "acpi.h"

#include <dev/acpica/acpivar.h>

/*
 * Hooks for the ACPI CA debugging infrastructure
 */
#define _COMPONENT	ACPI_THERMAL
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
    int				tz_temperature;
    int				tz_active;
#define TZ_ACTIVE_NONE	-1
    int				tz_flags;
#define TZ_FLAG_NONE	0
#define TZ_FLAG_PSV	(1<<0)
#define TZ_FLAG_HOT	(1<<2)
#define TZ_FLAG_CRT	(1<<3)    

    struct sysctl_ctx_list	tz_sysctl_ctx;
    struct sysctl_oid		*tz_sysctl_tree;
    
    struct acpi_tz_state 	tz_state;
};

static int	acpi_tz_probe(device_t dev);
static int	acpi_tz_attach(device_t dev);
static int	acpi_tz_establish(struct acpi_tz_softc *sc);
static void	acpi_tz_monitor(struct acpi_tz_softc *sc);
static void	acpi_tz_all_off(struct acpi_tz_softc *sc);
static void	acpi_tz_switch_cooler_off(ACPI_OBJECT *obj, void *arg);
static void	acpi_tz_switch_cooler_on(ACPI_OBJECT *obj, void *arg);
static void	acpi_tz_getparam(struct acpi_tz_softc *sc, char *node, int *data);
static void	acpi_tz_sanity(struct acpi_tz_softc *sc, int *val, char *what);
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

static struct sysctl_ctx_list	acpi_tz_sysctl_ctx;
static struct sysctl_oid	*acpi_tz_sysctl_tree;

/*
 * Match an ACPI thermal zone.
 */
static int
acpi_tz_probe(device_t dev)
{
    int		result;
    
    ACPI_LOCK;
    
    /* no FUNCTION_TRACE - too noisy */

    if ((acpi_get_type(dev) == ACPI_TYPE_THERMAL) &&
	!acpi_disabled("thermal")) {
	device_set_desc(dev, "thermal zone");
	result = -10;
    } else {
	result = ENXIO;
    }
    ACPI_UNLOCK;
    return(result);
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
    int				i;

    FUNCTION_TRACE(__func__);

    ACPI_LOCK;

    sc = device_get_softc(dev);
    sc->tz_dev = dev;
    sc->tz_handle = acpi_get_handle(dev);

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
    }
    sysctl_ctx_init(&sc->tz_sysctl_ctx);
    sprintf(oidname, "tz%d", device_get_unit(dev));
    sc->tz_sysctl_tree = SYSCTL_ADD_NODE(&sc->tz_sysctl_ctx,
					 SYSCTL_CHILDREN(acpi_tz_sysctl_tree), OID_AUTO,
					 oidname, CTLFLAG_RD, 0, "");
    SYSCTL_ADD_INT(&sc->tz_sysctl_ctx, SYSCTL_CHILDREN(sc->tz_sysctl_tree),
		   OID_AUTO, "temperature", CTLFLAG_RD,
		   &sc->tz_temperature, 0, "current thermal zone temperature");
    SYSCTL_ADD_INT(&sc->tz_sysctl_ctx, SYSCTL_CHILDREN(sc->tz_sysctl_tree),
		   OID_AUTO, "active", CTLFLAG_RD,
		   &sc->tz_active, 0, "active cooling mode");
    SYSCTL_ADD_INT(&sc->tz_sysctl_ctx, SYSCTL_CHILDREN(sc->tz_sysctl_tree),
		   OID_AUTO, "flags", CTLFLAG_RD,
		   &sc->tz_flags, 0, "thermal zone flags");

    SYSCTL_ADD_INT(&sc->tz_sysctl_ctx, SYSCTL_CHILDREN(sc->tz_sysctl_tree),
		   OID_AUTO, "_PSV", CTLFLAG_RD,
		   &sc->tz_state.psv, 0, "");
    SYSCTL_ADD_INT(&sc->tz_sysctl_ctx, SYSCTL_CHILDREN(sc->tz_sysctl_tree),
		   OID_AUTO, "_HOT", CTLFLAG_RD,
		   &sc->tz_state.hot, 0, "");
    SYSCTL_ADD_INT(&sc->tz_sysctl_ctx, SYSCTL_CHILDREN(sc->tz_sysctl_tree),
		   OID_AUTO, "_CRT", CTLFLAG_RD,
		   &sc->tz_state.crt, 0, "");
    for (i = 0; i < TZ_NUMLEVELS; i++) {
	sprintf(oidname, "_AC%d", i);
	SYSCTL_ADD_INT(&sc->tz_sysctl_ctx, SYSCTL_CHILDREN(sc->tz_sysctl_tree),
		       OID_AUTO, oidname, CTLFLAG_RD,
		       &sc->tz_state.ac[i], 0, "");
    }

    /*
     * Don't bother evaluating/printing the temperature at this point;
     * on many systems it'll be bogus until the EC is running.
     */

 out:
    ACPI_UNLOCK;
    return_VALUE(error);
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

    ACPI_ASSERTLOCK;

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
     * Sanity-check the values we've been given.
     *
     * XXX what do we do about systems that give us the same value for
     *     more than one of these setpoints?
     */
    acpi_tz_sanity(sc, &sc->tz_state.crt, "_CRT");
    acpi_tz_sanity(sc, &sc->tz_state.hot, "_HOT");
    acpi_tz_sanity(sc, &sc->tz_state.psv, "_PSV");
    for (i = 0; i < TZ_NUMLEVELS; i++)
	acpi_tz_sanity(sc, &sc->tz_state.ac[i], "_ACx");

    /*
     * Power off everything that we've just been given.
     */
    acpi_tz_all_off(sc);

    /*
     * The timeout routine always needs to run, since it may be involved
     * in passive cooling.
     */
    sc->tz_timeout = timeout(acpi_tz_timeout, sc, 0);
	

    return_VALUE(0);
}

/*
 * Evaluate the condition of a thermal zone, take appropriate actions.
 */
static void
acpi_tz_monitor(struct acpi_tz_softc *sc)
{
    int		temp;
    int		i;
    int		newactive, newflags;

    FUNCTION_TRACE(__func__);

    ACPI_ASSERTLOCK;

    /*
     * Get the current temperature.
     */
    if ((acpi_EvaluateInteger(sc->tz_handle, "_TMP", &temp)) != AE_OK) {
	device_printf(sc->tz_dev, "error fetching current temperature\n");
	/* XXX disable zone? go to max cooling? */
	return_VOID;
    }
    DEBUG_PRINT(TRACE_VALUES, ("got %d.%dC\n", TZ_KELVTOC(temp)));
    sc->tz_temperature = temp;

    /*
     * Work out what we ought to be doing right now.
     *
     * Note that the _ACx levels sort from hot to cold.
     */
    newactive = TZ_ACTIVE_NONE;
    for (i = TZ_NUMLEVELS - 1; i >= 0; i--)
	if ((sc->tz_state.ac[i] != -1) && (temp > sc->tz_state.ac[i]))
	    newactive = i;

    newflags = TZ_FLAG_NONE;
    if ((sc->tz_state.psv != -1) && (temp > sc->tz_state.psv))
	newflags |= TZ_FLAG_PSV;
    if ((sc->tz_state.hot != -1) && (temp > sc->tz_state.hot))
	newflags |= TZ_FLAG_HOT;
    if ((sc->tz_state.crt != -1) && (temp > sc->tz_state.crt))
	newflags |= TZ_FLAG_CRT;

    /*
     * If the active cooling state has changed, we have to switch things.
     */
    if (newactive != sc->tz_active) {

	/* turn off the cooling devices that are on, if any are */
	if (sc->tz_active != TZ_ACTIVE_NONE)
	    acpi_ForeachPackageObject((ACPI_OBJECT *)sc->tz_state.al[sc->tz_active].Pointer,
				      acpi_tz_switch_cooler_off, sc);

	/* turn on cooling devices that are required, if any are */
	if (newactive != TZ_ACTIVE_NONE)
	    acpi_ForeachPackageObject((ACPI_OBJECT *)sc->tz_state.al[newactive].Pointer,
				      acpi_tz_switch_cooler_on, sc);
	sc->tz_active = newactive;
    }

    /*
     * XXX (de)activate any passive cooling that may be required.
     */

    /*
     * If we have just become _HOT or _CRT, warn the user.
     *
     * We should actually shut down at this point, but it's not clear
     * that some systems don't actually map _CRT to the same value as _AC0.
     */
    if ((newflags & (TZ_FLAG_HOT | TZ_FLAG_CRT)) && 
	!(sc->tz_flags & (TZ_FLAG_HOT | TZ_FLAG_CRT))) {
	device_printf(sc->tz_dev, "WARNING - current temperature (%d.%dC) exceeds system limits\n",
		      TZ_KELVTOC(sc->tz_temperature), sc->tz_temperature);
	/* shutdown_nice(RB_POWEROFF);*/
    }
    sc->tz_flags = newflags;

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

    ACPI_ASSERTLOCK;
    
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

    sc->tz_active = TZ_ACTIVE_NONE;
    sc->tz_flags = TZ_FLAG_NONE;
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

    ACPI_ASSERTLOCK;

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
    ACPI_HANDLE			cooler;

    FUNCTION_TRACE(__func__);

    ACPI_ASSERTLOCK;

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

    ACPI_ASSERTLOCK;

    if (acpi_EvaluateInteger(sc->tz_handle, node, data) != AE_OK) {
	*data = -1;
    } else {
	DEBUG_PRINT(TRACE_VALUES, ("%s.%s = %d\n", acpi_name(sc->tz_handle),
				   node, *data));
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
    if ((*val != -1) && ((*val < TZ_ZEROC) || (*val > (TZ_ZEROC + 1500)))) {
	device_printf(sc->tz_dev, "%s value is absurd, ignored (%d.%dC)\n",
		      what, TZ_KELVTOC(*val));
	*val = -1;
    }
}
    
/*
 * Respond to a Notify event sent to the zone.
 */
static void
acpi_tz_notify_handler(ACPI_HANDLE h, UINT32 notify, void *context)
{
    struct acpi_tz_softc	*sc = (struct acpi_tz_softc *)context;

    FUNCTION_TRACE(__func__);

    ACPI_ASSERTLOCK;

    switch(notify) {
    case TZ_NOTIFY_TEMPERATURE:
	/* temperature change occurred */
	AcpiOsQueueForExecution(OSD_PRIORITY_HIGH, (OSD_EXECUTION_CALLBACK)acpi_tz_monitor, sc);
	break;
    case TZ_NOTIFY_DEVICES:
    case TZ_NOTIFY_LEVELS:
	/* zone devices/setpoints changed */
	AcpiOsQueueForExecution(OSD_PRIORITY_HIGH, (OSD_EXECUTION_CALLBACK)acpi_tz_establish, sc);
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

    ACPI_LOCK;
    
    /* check temperature, take action */
    AcpiOsQueueForExecution(OSD_PRIORITY_HIGH, (OSD_EXECUTION_CALLBACK)acpi_tz_monitor, sc);

    /* XXX passive cooling actions? */

    /* re-register ourself */
    sc->tz_timeout = timeout(acpi_tz_timeout, sc, TZ_POLLRATE);

    ACPI_UNLOCK;
}
