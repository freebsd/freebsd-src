/*-
 * Copyright (c) 2000 Mitsuru IWASAKI <iwasaki@FreeBSD.org>
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
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/bus.h>

#include <sys/acpi.h>

#include <dev/acpi/acpi.h>

#include <dev/acpi/aml/aml_amlmem.h>
#include <dev/acpi/aml/aml_common.h>
#include <dev/acpi/aml/aml_env.h>
#include <dev/acpi/aml/aml_evalobj.h>
#include <dev/acpi/aml/aml_name.h>
#include <dev/acpi/aml/aml_parse.h>
#include <dev/acpi/aml/aml_memman.h>

static int	 acpi_powerres_register(struct aml_name *name, va_list ap);
static int	 acpi_powerres_add_device(struct aml_name *name, va_list ap);

static void	 acpi_set_device_next_state(acpi_softc_t *sc,
					    struct acpi_powerres_device *device,
					    u_int8_t sleeping_state,
					    u_int8_t def_dstate);

static char	*powerres_statestr[2] = {"_OFF", "_ON"};

/*
 * 7.3.3 Evaluates to the current device state.
 */
u_int8_t
acpi_get_current_device_state(struct aml_name *name)
{
	u_int8_t		dstate;
	struct aml_name		*method;
	union aml_object	*ret;
	struct aml_environ	env;

	dstate = ACPI_D_STATE_D0;
	method = aml_find_from_namespace(name, "_PSC");
	if (method == NULL) {
		return (dstate);
	}

	bzero(&env, sizeof(env));
	aml_local_stack_push(aml_local_stack_create());
	ret = aml_eval_name(&env, method);
	dstate = ret->num.number;
	aml_local_stack_delete(aml_local_stack_pop());
	return (dstate);
}

static __inline struct acpi_powerres_device *
acpi_powerres_get_powerres_device(acpi_softc_t *sc, struct aml_name *name)
{
	struct acpi_powerres_device	*device;

	LIST_FOREACH(device, &sc->acpi_powerres_devlist, links) {
		if (device->name == name) {
			return (device);
		}
	}

	return (NULL);
}

/*
 * 7.2.2-4: For the OS to put the device in the Dx device state.
 */
void
acpi_set_device_state(acpi_softc_t *sc, struct aml_name *name, u_int8_t dstate)
{
	char				psx[5];	/* "_PSx" */
	struct acpi_powerres_info	*powerres;
	struct acpi_powerres_device_ref	*device_ref;
	struct acpi_powerres_device	*device;
	struct aml_name			*method;
	struct aml_environ		env;

	if (dstate > ACPI_D_STATE_D3) {
		return;
	}

	device = acpi_powerres_get_powerres_device(sc, name);
	if (device == NULL) {
		return;
	}
	device->state = dstate;

	/*
	 * D3 state transition.  We don't need to check PowerResource,
	 * just execute _PS3 control method of the device.
	 */
	if (dstate == ACPI_D_STATE_D3) {
		goto method_execution;
	}

	/*
	 * D0 - D2 state transition.
	 * All Power Resources referenced by elements 1 through N 
	 * in _PRx of the device must be in the ON state.
	 * 
	 */
	LIST_FOREACH(powerres, &sc->acpi_powerres_inflist, links) {
		LIST_FOREACH(device_ref, &powerres->reflist[dstate], links) {
			if (device_ref->device->name != name) {
				continue;
			}
			if (powerres->state != ACPI_POWER_RESOURCE_ON) {
				acpi_set_powerres_state(sc, powerres->name,
				    ACPI_POWER_RESOURCE_ON);
			}
			break;	/* already found, goto next PowerResource */
		}
	}

method_execution:
	/*
	 * If present, the _PSx control method is executed to set the
	 * device into the Dx device state.
	 */
	snprintf(psx, sizeof(psx), "_PS%d", dstate);
	method = aml_find_from_namespace(name, psx);
	if (method == NULL) {
		return;
	}

	bzero(&env, sizeof(env));
	aml_local_stack_push(aml_local_stack_create());
	aml_eval_name(&env, method);
	aml_local_stack_delete(aml_local_stack_pop());
}

/*
 * 7.2.1: For the OS to have the defined wake capability properly enabled
 *        for the device.
 */
void
acpi_set_device_wakecap(acpi_softc_t *sc, struct aml_name *name, u_int8_t cap)
{
	struct acpi_powerres_info	*powerres;
	struct acpi_powerres_device_ref	*device_ref;
	struct acpi_powerres_device	*device;
	struct aml_name			*method;
	union aml_object		argv[1];

	if (cap != ACPI_D_WAKECAP_ENABLE && cap != ACPI_D_WAKECAP_DISABLE ) {
		return;
	}

	device = acpi_powerres_get_powerres_device(sc, name);
	if (device == NULL) {
		return;
	}
	device->wake_cap = cap;

	/*
	 * Disable wake capability.  We don't need to check PowerResource,
	 * just execute _PSW control method of the device.
	 */
	if (cap == ACPI_D_WAKECAP_DISABLE ) {
		goto method_execution;
	}

	/*
	 * Enable wake capability.
	 * All Power Resources referenced by elements 2 through N 
	 * are put into the ON state.
	 * 
	 */
	LIST_FOREACH(powerres, &sc->acpi_powerres_inflist, links) {
		LIST_FOREACH(device_ref, &powerres->prwlist, links) {
			if (device_ref->device->name != name) {
				continue;
			}
			if (powerres->state != ACPI_POWER_RESOURCE_ON) {
				acpi_set_powerres_state(sc, powerres->name,
				    ACPI_POWER_RESOURCE_ON);
			}
			break;	/* already found, goto next PowerResource */
		}
	}

method_execution:
	/*
	 * If present, the _PSW control method is executed to set the
	 * device-specific registers to enable the wake functionality
	 * of the device.
	 */
	method = aml_find_from_namespace(name, "_PSW");
	if (method == NULL) {
		return;
	}

	argv[0].type = aml_t_num;
	argv[0].num.number = cap;
	aml_invoke_method(method, 1, argv);	/* no result code */
}

/*
 * 7.4.1 Returns the current ON or OFF status for the power resource.
 */
u_int8_t
acpi_get_current_powerres_state(struct aml_name *name)
{
	u_int8_t		pstate;
	struct aml_name		*method;
	union aml_object	*ret;
	struct aml_environ	env;

	pstate = ACPI_POWER_RESOURCE_ON;
	method = aml_find_from_namespace(name, "_STA");
	if (method == NULL) {
		return (pstate);	/* just in case */
	}

	bzero(&env, sizeof(env));
	aml_local_stack_push(aml_local_stack_create());
	ret = aml_eval_name(&env, method);
	pstate = ret->num.number;	/* OFF or ON */
	aml_local_stack_delete(aml_local_stack_pop());
	return (pstate);
}

static __inline struct acpi_powerres_info *
acpi_powerres_get_powerres(acpi_softc_t *sc, struct aml_name *name)
{
	struct acpi_powerres_info	*powerres;

	LIST_FOREACH(powerres, &sc->acpi_powerres_inflist, links) {
		if (powerres->name == name) {
			return (powerres);
		}
	}

	return (NULL);
}

/*
 * 7.4.2,3 Puts the power resource into the ON/OFF state.
 */
void
acpi_set_powerres_state(acpi_softc_t *sc, struct aml_name *name,
			u_int8_t pstate)
{
	struct acpi_powerres_info	*powerres;
	struct aml_name			*method;
	struct aml_environ		env;

	if (pstate != ACPI_POWER_RESOURCE_ON &&
	    pstate != ACPI_POWER_RESOURCE_OFF) {
		return;
	}

	powerres = acpi_powerres_get_powerres(sc, name);
	if (powerres == NULL) {
		return;
	}
	powerres->state = pstate;

	method = aml_find_from_namespace(name, powerres_statestr[pstate]);
	if (method == NULL) {
		return;			/* just in case */
	}

	bzero(&env, sizeof(env));
	aml_local_stack_push(aml_local_stack_create());
	aml_eval_name(&env, method);
	aml_local_stack_delete(aml_local_stack_pop());
}

/*
 * 7.1,2 Initialize the relationship of PowerResources and devices.
 */
void
acpi_powerres_init(acpi_softc_t *sc)
{
	struct acpi_powerres_info	*powerres;
	struct acpi_powerres_device_ref	*device_ref;
	struct acpi_powerres_device	*device;
	int				i;

	while ((powerres = LIST_FIRST(&sc->acpi_powerres_inflist))) {
		for (i = 0; i < ACPI_PR_MAX; i++) {
			while ((device_ref = LIST_FIRST(&powerres->reflist[i]))) {
				LIST_REMOVE(device_ref, links);
				FREE(device_ref, M_TEMP);
			}
			LIST_INIT(&powerres->reflist[i]);
		}
		LIST_INIT(&powerres->prwlist);
		LIST_REMOVE(powerres, links);
		FREE(powerres, M_TEMP);
	}
	LIST_INIT(&sc->acpi_powerres_inflist);

	while ((device = LIST_FIRST(&sc->acpi_powerres_devlist))) {
		LIST_REMOVE(device, links);
		FREE(device, M_TEMP);
	}
	LIST_INIT(&sc->acpi_powerres_devlist);

	aml_apply_foreach_found_objects(NULL, ".",
	    acpi_powerres_register, sc);
	aml_apply_foreach_found_objects(NULL, "_PR",
	    acpi_powerres_add_device, sc);
}

static __inline void
acpi_powerres_device_prw_print(struct acpi_powerres_device *device)
{

	printf("[PRW:%d:", device->wake_cap);
	switch (device->prw_val[0]->type) {
	case aml_t_num:
		/* bit index in GPEx_EN of the enable bit */
		printf("0x%x", device->prw_val[0]->num.number);
		break;
	default:
		/* XXX in ACPI 2.0, we can have additional GPE blocks */
		printf("GPE block");
		break;
	}
	/* the lowest sleeping state */
	printf(":%d] ", device->prw_val[1]->num.number);
}

void
acpi_powerres_debug(acpi_softc_t *sc)
{
	struct acpi_powerres_info	*powerres;
	struct acpi_powerres_device_ref	*device_ref;
	struct acpi_powerres_device	*device;
	int				i;

	LIST_FOREACH(powerres, &sc->acpi_powerres_inflist, links) {
		printf("acpi_powerres_debug[powerres]:");
		aml_print_curname(powerres->name);
		printf("[%d:%d:%s]\n", powerres->name->property->pres.level, 
		    powerres->name->property->pres.order, 
		    powerres_statestr[powerres->state]);

		/* for _PR[0-2] */
		for (i = 0; i < ACPI_PR_MAX; i++) {
			if (LIST_EMPTY(&powerres->reflist[i])) {
				continue;
			}
			printf("\t_PR%d:", i);
			LIST_FOREACH(device_ref, &powerres->reflist[i], links) {
				device = device_ref->device;
				aml_print_curname(device->name);
				printf("[D%d] ", device->state);
			}
			printf("\n");
		}

		/* for _PRW */
		if (LIST_EMPTY(&powerres->prwlist)) {
			continue;
		}
		printf("\t_PRW:");
		LIST_FOREACH(device_ref, &powerres->prwlist, links) {
			device = device_ref->device;
			aml_print_curname(device->name);
			acpi_powerres_device_prw_print(device);
		}
		printf("\n");
	}

	LIST_FOREACH(device, &sc->acpi_powerres_devlist, links) {
		printf("acpi_powerres_debug[device]:");
		aml_print_curname(device->name);
		if (device->state != ACPI_D_STATE_UNKNOWN) {
			printf("[D%d] ", device->state);
		}
		if (device->wake_cap != ACPI_D_WAKECAP_UNKNOWN) {
			acpi_powerres_device_prw_print(device);
		}
		printf("\n");
	}
}

static int
acpi_powerres_register(struct aml_name *name, va_list ap)
{
	int				i, order;
	acpi_softc_t			*sc;
	struct acpi_powerres_info	*powerres, *other_pr, *last_pr;

	sc = va_arg(ap, acpi_softc_t *);

	if (name->property == NULL ||
	    name->property->type != aml_t_powerres) {
		return (0);
	}

	MALLOC(powerres, struct acpi_powerres_info *,
	    sizeof(*powerres), M_TEMP, M_NOWAIT);
	if (powerres == NULL) {
		return (1);
	}

	powerres->name = name;

	/* get the current ON or OFF status for the power resource */
	powerres->state = acpi_get_current_powerres_state(name);

	/* must be sorted by resource order of PowerResource */
	order = powerres->name->property->pres.order;
	other_pr = last_pr = NULL;
	if (LIST_EMPTY(&sc->acpi_powerres_inflist)) {
		LIST_INSERT_HEAD(&sc->acpi_powerres_inflist, powerres, links);
	} else {
		LIST_FOREACH(other_pr, &sc->acpi_powerres_inflist, links) {
			if (other_pr->name->property->pres.order >= order) {
				break;		/* found */
			}
			last_pr = other_pr;
		}
		if (other_pr != NULL) {
			LIST_INSERT_BEFORE(other_pr, powerres, links);
		} else {
			LIST_INSERT_AFTER(last_pr, powerres, links);
		}
	}

	for (i = 0; i < ACPI_PR_MAX; i++) {
		LIST_INIT(&powerres->reflist[i]);
	}
	LIST_INIT(&powerres->prwlist);

	return (0);
}

static int
acpi_powerres_add_device(struct aml_name *name, va_list ap)
{
	int				i, offset, objtype;
	int				prnum;
	int				dev_found;
	acpi_softc_t			*sc;
	struct acpi_powerres_device	*device;
	struct acpi_powerres_device_ref	*device_ref;
	struct acpi_powerres_info	*powerres;
	struct aml_name			*powerres_name;
	struct aml_environ		env;
	union aml_object		**objects;

	sc = va_arg(ap, acpi_softc_t *);
	objtype = prnum = 0;

	/* should be _PR[0-2] or _PRW */
	switch (name->name[3]) {
	case '0' ... '2':
		objtype = ACPI_D_PM_TYPE_PRX;
		prnum =  name->name[3] - '0';
		offset = 0;
		break;
	case 'W':
		objtype = ACPI_D_PM_TYPE_PRW;
		/* for _PRW, PowerResource reference starts from elements 2 */ 
		offset = 2;
		break;
	default:
		return (0);
	}

	if (name->property == NULL ||
	    name->property->type != aml_t_package) {
		return (0);
	}

	if (name->property->package.elements == 0) {
		return (0);
	}

	/* make the list of devices */
	dev_found = 0;
	LIST_FOREACH(device, &sc->acpi_powerres_devlist, links) {
		if (device->name == name->parent) {
			dev_found = 1;
			break;
		}
	}
	if (!dev_found) {
		MALLOC(device, struct acpi_powerres_device *,
		   sizeof(*device), M_TEMP, M_NOWAIT);
		if (device == NULL) {
			return (1);
		}

		/* set default values */
		device->state = ACPI_D_STATE_UNKNOWN;
		device->wake_cap = ACPI_D_WAKECAP_UNKNOWN;

		/* this is a _PR[0-2|W] object, we need the parent of this. */
		device->name = name->parent;


		LIST_INSERT_HEAD(&sc->acpi_powerres_devlist, device, links);
	}

	objects = name->property->package.objects;

	switch (objtype) {
	case ACPI_D_PM_TYPE_PRX:
		/* get the current device state.  */
		if (device->state == ACPI_D_STATE_UNKNOWN) {
			device->state = acpi_get_current_device_state(device->name);
		}
		break;
	case ACPI_D_PM_TYPE_PRW:
		device->wake_cap = ACPI_D_WAKECAP_DEFAULT;
		for (i = 0; i < 2; i++) {
			device->prw_val[i] = objects[i];
		}
		break;
	default:
		break;
	}

	/* find PowerResource which the device reference to */
	MALLOC(device_ref, struct acpi_powerres_device_ref *,
	   sizeof(*device_ref), M_TEMP, M_NOWAIT);
	if (device_ref == NULL) {
		return (1);
	}

	device_ref->device = device;
	env.curname = device->name;
	for (i = offset; i < name->property->package.elements; i++) {
		if (objects[i]->type != aml_t_namestr) {
			printf("acpi_powerres_add_device: not name string\n");
			continue;
		}
		powerres_name = aml_search_name(&env, objects[i]->nstr.dp);
		if (powerres_name == NULL) {
			printf("acpi_powerres_add_device: not found\n");
			continue;
		}

		LIST_FOREACH(powerres, &sc->acpi_powerres_inflist, links) {
			if (powerres->name != powerres_name) {
				continue;
			}

			switch (objtype) {
			case ACPI_D_PM_TYPE_PRX:
				LIST_INSERT_HEAD(&powerres->reflist[prnum],
				    device_ref, links);
				break;
			case ACPI_D_PM_TYPE_PRW:
				LIST_INSERT_HEAD(&powerres->prwlist,
				    device_ref, links);
				break;
			default:
				break;
			}
			/* already found, go to next element... */
			break;
		}
	}

	switch (objtype) {
	case ACPI_D_PM_TYPE_PRX:
		/* XXX
		 * force to set the current device state to make
		 * PowerResource compatible with the device state.
		 */
		acpi_set_device_state(sc, device->name, device->state);
		break;
	case ACPI_D_PM_TYPE_PRW:
		acpi_set_device_wakecap(sc, device->name, device->wake_cap);
		break;
	default:
		break;
	}

	return (0);
}

static __inline void
acpi_set_device_prw_gpe(acpi_softc_t *sc, struct acpi_powerres_device *device,
			boolean_t on_off)
{
	u_long		ef;

	device->gpe_enabled = on_off;

	/* The proper general-purpose register bits are enabled. */
	switch (device->prw_val[0]->type) {
	case aml_t_num:
		/* bit index in GPEx_EN of the enable bit */
		ef = read_eflags();	/* XXX should MI */
		acpi_gpe_enable_bit(sc, device->prw_val[0]->num.number, on_off);
		write_eflags(ef);
		break;
	default:
		/* XXX in ACPI 2.0, we can have additional GPE blocks */
		printf("ACPI 2.0 style _PRW/GPE handling is not supported\n");
		break;
	}
}

static void
acpi_set_device_next_state(acpi_softc_t *sc, struct acpi_powerres_device *device,
			   u_int8_t sleeping_state, u_int8_t def_dstate)
{

	/* set given default device state */
	device->next_state = def_dstate;

	if (device->wake_cap != ACPI_D_WAKECAP_ENABLE) {
		goto out;
	}

	/*
	 * 7.2.1 _PRW
	 * The sleeping state being enterted must be greater or equal to the
	 * power state declared in element 1 of the _PRW object.
	 */
	if (sleeping_state < device->prw_val[1]->num.number) {
		goto out;
	}

	device->next_state = ACPI_D_STATE_D0;	/* XXX need to refer _SxD ? */

	if (sleeping_state > ACPI_S_STATE_S0 && device->gpe_enabled == 0) {
		acpi_set_device_prw_gpe(sc, device, 1);
	}

out:
	return;
}

/*
 * 7.1-5 PowerResource manipulation on the sleeping state transision.
 */
void
acpi_powerres_set_sleeping_state(acpi_softc_t *sc, u_int8_t state)
{
	int				i;
	struct acpi_powerres_info	*powerres;
	struct acpi_powerres_device	*device;
	struct acpi_powerres_device_ref	*device_ref;

	if (state > ACPI_S_STATE_S4) {
		return;
	}

	/*
	 * initialize the next device state to D0, then change to D3 later
	 *  based on PowerResource state change.
	 */
	LIST_FOREACH(device, &sc->acpi_powerres_devlist, links) {
		acpi_set_device_next_state(sc, device, state, ACPI_D_STATE_D0);
	}

	/*
	 * 7.5.2 System \_Sx state
	 * Power Resources are in a state compatible with the system Sx
	 * state.  All power Resources that supply a System Level reference
	 * of Sn (where n < x) are in the OFF state.
	 */
	LIST_FOREACH(powerres, &sc->acpi_powerres_inflist, links) {
		if (powerres->name->property->pres.level < state) {
			/* if ON state then put it in the OFF state */
			if (powerres->state == ACPI_POWER_RESOURCE_ON) {
				acpi_set_powerres_state(sc, powerres->name,
				    ACPI_POWER_RESOURCE_OFF);
			}
			/*
			 * Device states are compatible with the current
			 * Power Resource states.
			 */
			for (i = 0; i < ACPI_PR_MAX; i++) {
				LIST_FOREACH(device_ref, &powerres->reflist[i], links) {
					device = device_ref->device;
					acpi_set_device_next_state(sc, device,
					    state, ACPI_D_STATE_D3);
				}
			}
		} else {
			/* if OFF state then put it in the ON state */
			if (powerres->state == ACPI_POWER_RESOURCE_OFF) {
				acpi_set_powerres_state(sc, powerres->name,
				    ACPI_POWER_RESOURCE_ON);
			}
		}
	}

	/*
	 * Devices states are compatible with the current Power Resource
	 * states. only devices which solely reference Power Resources which
	 * are in the ON state for a given device state can be in that device
	 * state. In all other cases, the device is in the D3 (off) state.
	 * Note:
	 * Or is at least assumed to be in the D3 state by its device driver.
	 * For example, if the device doesn't explicitly describe how it can
	 * stay in some state non-off state while the system is in a sleeping
	 * state, the operating software must assume that the device can lose
	 * its power and state.
	 */

	LIST_FOREACH(device, &sc->acpi_powerres_devlist, links) {
		if (device->next_state == ACPI_D_STATE_D3 &&
		    device->state != ACPI_D_STATE_D3) {
			acpi_set_device_state(sc, device->name, ACPI_D_STATE_D3);
		}
		if (device->next_state == ACPI_D_STATE_D0 &&
		    device->state != ACPI_D_STATE_D0) {
			acpi_set_device_state(sc, device->name, ACPI_D_STATE_D0);
		}

		/* XXX reset GEPx_EN enabled bit on S0 state */
		if (state == ACPI_S_STATE_S0 && device->gpe_enabled &&
		    device->wake_cap == ACPI_D_WAKECAP_ENABLE) {
			acpi_set_device_prw_gpe(sc, device, 0);
		}
	}
}
