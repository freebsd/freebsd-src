/*-
 * Copyright (c) 2001 Michael Smith
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

#include "opt_acpi.h"		/* XXX trim includes */
#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/ioccom.h>
#include <sys/reboot.h>
#include <sys/sysctl.h>
#include <sys/systm.h>
#include <sys/ctype.h>

#include <machine/clock.h>

#include <machine/resource.h>

#include "acpi.h"

#include <dev/acpica/acpivar.h>
#include <dev/acpica/acpiio.h>

/*
 * ACPI power resource management.
 *
 * Power resource behaviour is slightly complicated by the fact that
 * a single power resource may provide power for more than one device.
 * Thus, we must track the device(s) being powered by a given power
 * resource, and only deactivate it when there are no powered devices.
 *
 * Note that this only manages resources for known devices.  There is an
 * ugly case where we may turn of power to a device which is in use because
 * we don't know that it depends on a given resource.  We should perhaps
 * try to be smarter about this, but a more complete solution would involve
 * scanning all of the ACPI namespace to find devices we're not currently
 * aware of, and this raises questions about whether they should be left 
 * on, turned off, etc.
 *
 * XXX locking
 */

MALLOC_DEFINE(M_ACPIPWR, "acpipwr", "ACPI power resources");

/*
 * Hooks for the ACPI CA debugging infrastructure
 */
#define _COMPONENT	ACPI_POWER
MODULE_NAME("POWERRES")

/*
 * A relationship between a power resource and a consumer.
 */
struct acpi_powerreference {
    struct acpi_powerconsumer	*ar_consumer;
    struct acpi_powerresource	*ar_resource;
    TAILQ_ENTRY(acpi_powerreference) ar_rlink;	/* link on resource list */
    TAILQ_ENTRY(acpi_powerreference) ar_clink;	/* link on consumer */
};
    
/*
 * A power-managed device.
 */
struct acpi_powerconsumer {
    ACPI_HANDLE		ac_consumer;		/* device which is powered */
    int			ac_state;
    TAILQ_ENTRY(acpi_powerconsumer) ac_link;
    TAILQ_HEAD(,acpi_powerreference) ac_references;
};

/*
 * A power resource.
 */
struct acpi_powerresource {
    TAILQ_ENTRY(acpi_powerresource) ap_link;
    TAILQ_HEAD(,acpi_powerreference) ap_references;
    ACPI_HANDLE		ap_resource;		/* the resource's handle */
    ACPI_INTEGER	ap_systemlevel;
    ACPI_INTEGER	ap_order;
    int			ap_state;
#define ACPI_PWR_OFF	0
#define ACPI_PWR_ON	1
};

TAILQ_HEAD(acpi_powerresource_list, acpi_powerresource)	acpi_powerresources;
TAILQ_HEAD(acpi_powerconsumer_list, acpi_powerconsumer)	acpi_powerconsumers;

static ACPI_STATUS		acpi_pwr_register_consumer(ACPI_HANDLE consumer);
static ACPI_STATUS		acpi_pwr_deregister_consumer(ACPI_HANDLE consumer);
static ACPI_STATUS		acpi_pwr_register_resource(ACPI_HANDLE res);
static ACPI_STATUS		acpi_pwr_deregister_resource(ACPI_HANDLE res);
static void			acpi_pwr_reference_resource(ACPI_OBJECT *obj, void *arg);
static ACPI_STATUS		acpi_pwr_switch_power(void);
static struct acpi_powerresource *acpi_pwr_find_resource(ACPI_HANDLE res);
static struct acpi_powerconsumer *acpi_pwr_find_consumer(ACPI_HANDLE consumer);

/*
 * Initialise our lists.
 */    
static void
acpi_pwr_init(void *junk)
{
    TAILQ_INIT(&acpi_powerresources);
    TAILQ_INIT(&acpi_powerconsumers);
}
SYSINIT(acpi_powerresource, SI_SUB_TUNABLES, SI_ORDER_ANY, acpi_pwr_init, NULL);

/*
 * Register a power resource.
 *
 * It's OK to call this if we already know about the resource.
 */
static ACPI_STATUS
acpi_pwr_register_resource(ACPI_HANDLE res)
{
    ACPI_STATUS			status;
    ACPI_BUFFER			buf;
    ACPI_OPERAND_OBJECT		*obj;
    struct acpi_powerresource	*rp, *srp;

    FUNCTION_TRACE(__func__);

    rp = NULL;
    obj = NULL;
    
    /* look to see if we know about this resource */
    if (acpi_pwr_find_resource(res) != NULL)
	return_ACPI_STATUS(AE_OK);		/* already know about it */

    /* allocate a new resource */
    if ((rp = malloc(sizeof(*rp), M_ACPIPWR, M_NOWAIT | M_ZERO)) == NULL) {
	status = AE_NO_MEMORY;
	goto out;
    }
    TAILQ_INIT(&rp->ap_references);
    rp->ap_resource = res;

    /* get the Power Resource object */
    if ((status = acpi_EvaluateIntoBuffer(res, NULL, NULL, &buf)) != AE_OK) {
	DEBUG_PRINT(TRACE_OBJECTS, ("no power resource object\n"));
	goto out;
    }
    obj = buf.Pointer;
    if (obj->Common.Type != ACPI_TYPE_POWER) {
	DEBUG_PRINT(TRACE_OBJECTS, ("bad power resource object\n"));
	status = AE_TYPE;
	goto out;
    }
    rp->ap_systemlevel = obj->PowerResource.SystemLevel;
    rp->ap_order = obj->PowerResource.ResourceOrder;
    
    /* get the current state of the resource */
    if ((status = acpi_EvaluateInteger(rp->ap_resource, "_STA", &rp->ap_state)) != AE_OK) {
	/* XXX is this an error? */
	DEBUG_PRINT(TRACE_OBJECTS, ("can't get current power resource state\n"));
	goto out;
    }

    /* sort the resource into the list */
    status = AE_OK;
    srp = TAILQ_FIRST(&acpi_powerresources);
    if ((srp == NULL) || (rp->ap_order < srp->ap_order)) {
	TAILQ_INSERT_HEAD(&acpi_powerresources, rp, ap_link);
	goto out;
    }
    TAILQ_FOREACH(srp, &acpi_powerresources, ap_link)
	if (rp->ap_order < srp->ap_order) {
	    TAILQ_INSERT_BEFORE(srp, rp, ap_link);
	    goto out;
	}
    TAILQ_INSERT_TAIL(&acpi_powerresources, rp, ap_link);
    DEBUG_PRINT(TRACE_OBJECTS, ("registered power resource %s\n", acpi_name(res)));

 out:
    if (obj != NULL)
	AcpiOsFree(obj);
    if ((status != AE_OK) && (rp != NULL))
	free(rp, M_ACPIPWR);
    return_ACPI_STATUS(status);
}

/*
 * Deregister a power resource.
 */
static ACPI_STATUS
acpi_pwr_deregister_resource(ACPI_HANDLE res)
{
    struct acpi_powerresource	*rp;

    FUNCTION_TRACE(__func__);

    rp = NULL;
    
    /* find the resource */
    if ((rp = acpi_pwr_find_resource(res)) == NULL)
	return_ACPI_STATUS(AE_BAD_PARAMETER);

    /* check that there are no consumers referencing this resource */
    if (TAILQ_FIRST(&rp->ap_references) != NULL)
	return_ACPI_STATUS(AE_BAD_PARAMETER);

    /* pull it off the list and free it */
    TAILQ_REMOVE(&acpi_powerresources, rp, ap_link);
    free(rp, M_ACPIPWR);

    DEBUG_PRINT(TRACE_OBJECTS, ("deregistered power resource %s\n", acpi_name(res)));

    return_ACPI_STATUS(AE_OK);
}

/*
 * Register a power consumer.  
 *
 * It's OK to call this if we already know about the consumer.
 */
static ACPI_STATUS
acpi_pwr_register_consumer(ACPI_HANDLE consumer)
{
    struct acpi_powerconsumer	*pc;
    
    FUNCTION_TRACE(__func__);

    /* check to see whether we know about this consumer already */
    if ((pc = acpi_pwr_find_consumer(consumer)) != NULL)
	return_ACPI_STATUS(AE_OK);
    
    /* allocate a new power consumer */
    if ((pc = malloc(sizeof(*pc), M_ACPIPWR, M_NOWAIT)) == NULL)
	return_ACPI_STATUS(AE_NO_MEMORY);
    TAILQ_INSERT_HEAD(&acpi_powerconsumers, pc, ac_link);
    TAILQ_INIT(&pc->ac_references);
    pc->ac_consumer = consumer;

    pc->ac_state = ACPI_STATE_UNKNOWN;	/* XXX we should try to find its current state */

    DEBUG_PRINT(TRACE_OBJECTS, ("registered power consumer %s\n", acpi_name(consumer)));
    
    return_ACPI_STATUS(AE_OK);
}

/*
 * Deregister a power consumer.
 *
 * This should only be done once the consumer has been powered off.
 * (XXX is this correct?  Check once implemented)
 */
static ACPI_STATUS
acpi_pwr_deregister_consumer(ACPI_HANDLE consumer)
{
    struct acpi_powerconsumer	*pc;
    
    FUNCTION_TRACE(__func__);

    /* find the consumer */
    if ((pc = acpi_pwr_find_consumer(consumer)) == NULL)
	return_ACPI_STATUS(AE_BAD_PARAMETER);
    
    /* make sure the consumer's not referencing anything right now */
    if (TAILQ_FIRST(&pc->ac_references) != NULL)
	return_ACPI_STATUS(AE_BAD_PARAMETER);

    /* pull the consumer off the list and free it */
    TAILQ_REMOVE(&acpi_powerconsumers, pc, ac_link);

    DEBUG_PRINT(TRACE_OBJECTS, ("deregistered power consumer %s\n", acpi_name(consumer)));

    return_ACPI_STATUS(AE_OK);
}

/*
 * Set a power consumer to a particular power state.
 */
ACPI_STATUS
acpi_pwr_switch_consumer(ACPI_HANDLE consumer, int state)
{
    struct acpi_powerconsumer	*pc;
    struct acpi_powerreference	*pr;
    ACPI_HANDLE			method_handle, reslist_handle;
    ACPI_BUFFER			reslist_buffer;
    ACPI_OBJECT			*reslist_object;
    ACPI_STATUS			status;
    char			*method_name, *reslist_name;
    int				res_changed;

    FUNCTION_TRACE(__func__);

    /* find the consumer */
    if ((pc = acpi_pwr_find_consumer(consumer)) == NULL) {
	if ((status = acpi_pwr_register_consumer(consumer)) != AE_OK)
	    return_ACPI_STATUS(status);
	if ((pc = acpi_pwr_find_consumer(consumer)) == NULL) {
	    return_ACPI_STATUS(AE_ERROR);	/* something very wrong */
	}
    }

    /* check for valid transitions */
    if ((pc->ac_state == ACPI_STATE_D3) && (state != ACPI_STATE_D0))
	return_ACPI_STATUS(AE_BAD_PARAMETER);	/* can only go to D0 from D3 */

    /* find transition mechanism(s) */
    switch(state) {
    case ACPI_STATE_D0:
	method_name = "_PS0";
	reslist_name = "_PR0";
	break;
    case ACPI_STATE_D1:
	method_name = "_PS1";
	reslist_name = "_PR1";
	break;
    case ACPI_STATE_D2:
	method_name = "_PS2";
	reslist_name = "_PR2";
	break;
    case ACPI_STATE_D3:
	method_name = "_PS3";
	reslist_name = "_PR3";
	break;
    default:
	return_ACPI_STATUS(AE_BAD_PARAMETER);
    }
    DEBUG_PRINT(TRACE_OBJECTS, ("setup to switch %s D%d -> D%d\n",
				acpi_name(consumer), pc->ac_state, state));

    /*
     * Verify that this state is supported, ie. one of method or
     * reslist must be present.  We need to do this before we go 
     * dereferencing resources (since we might be trying to go to
     * a state we don't support).
     *
     * Note that if any states are supported, the device has to
     * support D0 and D3.  It's never an error to try to go to
     * D0.
     */
    if (AcpiGetHandle(consumer, method_name, &method_handle) != AE_OK)
	method_handle = NULL;
    if (AcpiGetHandle(consumer, reslist_name, &reslist_handle) != AE_OK)
	reslist_handle = NULL;
    if ((reslist_handle == NULL) && (method_handle == NULL)) {
	if (state == ACPI_STATE_D0) {
	    pc->ac_state = ACPI_STATE_D0;
	    return_ACPI_STATUS(AE_OK);
	}
	DEBUG_PRINT(TRACE_OBJECTS, ("attempt to set unsupported state D%d\n", 
				    state));
	return_ACPI_STATUS(AE_BAD_PARAMETER);
    }

    /*
     * Check that we can actually fetch the list of power resources
     */
    if (reslist_handle != NULL) {
	if ((status = acpi_EvaluateIntoBuffer(reslist_handle, NULL, NULL, &reslist_buffer)) != AE_OK) {
	    DEBUG_PRINT(TRACE_OBJECTS, ("can't evaluate resource list %s\n",
					acpi_name(reslist_handle)));
	    return_ACPI_STATUS(status);
	}
	reslist_object = (ACPI_OBJECT *)reslist_buffer.Pointer;
	if (reslist_object->Type != ACPI_TYPE_PACKAGE) {
	    DEBUG_PRINT(TRACE_OBJECTS, ("resource list is not ACPI_TYPE_PACKAGE (%d)\n",
					reslist_object->Type));
	    return_ACPI_STATUS(AE_TYPE);
	}
    } else {
	reslist_object = NULL;
    }

    /*
     * Now we are ready to switch, so  kill off any current power resource references.
     */
    res_changed = 0;
    TAILQ_FOREACH(pr, &pc->ac_references, ar_clink) {
	TAILQ_REMOVE(&pr->ar_resource->ap_references, pr, ar_rlink);
	res_changed = 1;
    }

    /*
     * Add new power resource references, if we have any.  Traverse the
     * package that we got from evaluating reslist_handle, and look up each
     * of the resources that are referenced.
     */
    if (reslist_object != NULL) {
	DEBUG_PRINT(TRACE_OBJECTS, ("referencing %d new resources\n", 
				    reslist_object->Package.Count));
	acpi_ForeachPackageObject(reslist_object, acpi_pwr_reference_resource, pc);
	res_changed = 1;
    }

    /*
     * If we changed anything in the resource list, we need to run a switch
     * pass now.
     */
    if ((status = acpi_pwr_switch_power()) != AE_OK) {
	DEBUG_PRINT(TRACE_OBJECTS, ("failed to correctly switch resources to move %s to D%d\n",
				    acpi_name(consumer), state));
	return_ACPI_STATUS(status);	/* XXX is this appropriate?  Should we return to previous state? */
    }

    /* invoke power state switch method (if present) */
    if (method_handle != NULL) {
	DEBUG_PRINT(TRACE_OBJECTS, ("invoking state transition method %s\n",
				    acpi_name(method_handle)));
	if ((status = AcpiEvaluateObject(method_handle, NULL, NULL, NULL)) != AE_OK)
	    pc->ac_state = ACPI_STATE_UNKNOWN;
	    return_ACPI_STATUS(status);	/* XXX is this appropriate?  Should we return to previous state? */
    }

    /* transition was successful */
    pc->ac_state = state;
    return_ACPI_STATUS(AE_OK);
}

/*
 * Called to create a reference between a power consumer and a power resource
 * identified in the object.
 */
static void
acpi_pwr_reference_resource(ACPI_OBJECT *obj, void *arg)
{
    struct acpi_powerconsumer	*pc = (struct acpi_powerconsumer *)arg;

    FUNCTION_TRACE(__func__);

    DEBUG_PRINT(TRACE_OBJECTS, ("called to create a reference using object type %d\n",
				obj->Type));

    return_VOID;
}


/*
 * Switch power resources to conform to the desired state.
 *
 * Consumers may have modified the power resource list in an arbitrary
 * fashion; we sweep it in sequence order.
 */
static ACPI_STATUS
acpi_pwr_switch_power(void)
{
    struct acpi_powerresource	*rp;
    ACPI_STATUS			status;
    int				cur;

    FUNCTION_TRACE(__func__);

    /*
     * Sweep the list forwards turning things on.
     */
    TAILQ_FOREACH(rp, &acpi_powerresources, ap_link) {
	if (rp->ap_state != ACPI_PWR_ON)
	    continue;	/* not turning this one on */

	/* we could cache this if we trusted it not to change under us */
	if ((status = acpi_EvaluateInteger(rp->ap_resource, "_STA", &cur)) != AE_OK) {
	    DEBUG_PRINT(TRACE_OBJECTS, ("can't get status of %s - %d\n",
					acpi_name(rp->ap_resource), status));
	    continue;	/* XXX is this correct?  Always switch if in doubt? */
	}

	/*
	 * Switch if required.  Note that we ignore the result of the switch
	 * effort; we don't know what to do if it fails, so checking wouldn't
	 * help much.
	 */
	if (cur != ACPI_PWR_ON)
	    AcpiEvaluateObject(rp->ap_resource, "_ON", NULL, NULL);
    }
    
    /*
     * Sweep the list backwards turning things off.
     */
    TAILQ_FOREACH_REVERSE(rp, &acpi_powerresources, acpi_powerresource_list, ap_link) {
	if (rp->ap_state != ACPI_PWR_OFF)
	    continue;	/* not turning this one off */

	/* we could cache this if we trusted it not to change under us */
	if ((status = acpi_EvaluateInteger(rp->ap_resource, "_STA", &cur)) != AE_OK) {
	    DEBUG_PRINT(TRACE_OBJECTS, ("can't get status of %s - %d\n",
					acpi_name(rp->ap_resource), status));
	    continue;	/* XXX is this correct?  Always switch if in doubt? */
	}

	/*
	 * Switch if required.  Note that we ignore the result of the switch
	 * effort; we don't know what to do if it fails, so checking wouldn't
	 * help much.
	 */
	if (cur != ACPI_PWR_OFF)
	    AcpiEvaluateObject(rp->ap_resource, "_OFF", NULL, NULL);
    }
    return_ACPI_STATUS(AE_OK);
}

/*
 * Find a power resource's control structure.
 */
static struct acpi_powerresource *
acpi_pwr_find_resource(ACPI_HANDLE res)
{
    struct acpi_powerresource	*rp;
    
    FUNCTION_TRACE(__func__);

    TAILQ_FOREACH(rp, &acpi_powerresources, ap_link)
	if (rp->ap_resource == res)
	    break;
    return_VALUE(rp);
}

/*
 * Find a power consumer's control structure.
 */
static struct acpi_powerconsumer *
acpi_pwr_find_consumer(ACPI_HANDLE consumer)
{
    struct acpi_powerconsumer	*pc;
    
    FUNCTION_TRACE(__func__);

    TAILQ_FOREACH(pc, &acpi_powerconsumers, ac_link)
	if (pc->ac_consumer == consumer)
	    break;
    return_VALUE(pc);
}

