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

#include <sys/acpi.h>

#include <dev/acpi/acpi.h>

#include <dev/acpi/aml/aml_amlmem.h>
#include <dev/acpi/aml/aml_common.h>
#include <dev/acpi/aml/aml_env.h>
#include <dev/acpi/aml/aml_evalobj.h>
#include <dev/acpi/aml/aml_name.h>
#include <dev/acpi/aml/aml_parse.h>
#include <dev/acpi/aml/aml_memman.h>

static void acpi_powerres_init(acpi_softc_t *sc);
static int  acpi_powerres_register(struct aml_name *name, va_list ap);
static int  acpi_powerres_add_device(struct aml_name *name, va_list ap);

static void
acpi_powerres_init(acpi_softc_t *sc)
{
	struct	acpi_powerres_info *powerres;
	struct	acpi_powerres_device_ref *device_ref;
	struct	acpi_powerres_device *device;
	int	i;

	while ((powerres = LIST_FIRST(&sc->acpi_powerres_inflist))) {
#ifdef ACPI_DEBUG
		printf("acpi_powerres_init:");
		aml_print_curname(powerres->name);
		printf("[%d]\n", powerres->state);
#endif
		for (i = 0; i < 3; i++) {
#ifdef ACPI_DEBUG
			printf("\t_PR%d:", i);
#endif
			while ((device_ref = LIST_FIRST(&powerres->reflist[i]))) {
#ifdef ACPI_DEBUG
				device = device_ref->device;
				aml_print_curname(device->name);
				printf("[%d] ", device->state);
#endif
				LIST_REMOVE(device_ref, links);
				FREE(device_ref, M_TEMP);
			}
#ifdef ACPI_DEBUG
			printf("\n");
#endif
			LIST_INIT(&powerres->reflist[i]);
		}
		LIST_REMOVE(powerres, links);
		FREE(powerres, M_TEMP);
	}
	LIST_INIT(&sc->acpi_powerres_inflist);

	while ((device = LIST_FIRST(&sc->acpi_powerres_devlist))) {
		LIST_REMOVE(device, links);
		FREE(device, M_TEMP);
	}
	LIST_INIT(&sc->acpi_powerres_devlist);
}

static int
acpi_powerres_register(struct aml_name *name, va_list ap)
{
	int	i;
	acpi_softc_t *sc;
	struct	acpi_powerres_info *powerres;
	struct	aml_name *method;
	union	aml_object *ret;
	struct	aml_environ env;

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
	method = aml_find_from_namespace(name, "_STA");
	if (method != NULL) {
		bzero(&env, sizeof(env));
		aml_local_stack_push(aml_local_stack_create());
		ret = aml_eval_name(&env, method);
		aml_local_stack_delete(aml_local_stack_pop());
		powerres->state = ret->num.number;	/* OFF or ON */
	}

	/* XXX must be sorted by resource order of PowerResource */
	LIST_INSERT_HEAD(&sc->acpi_powerres_inflist, powerres, links);

	for (i = 0; i < 3; i++) {
		LIST_INIT(&powerres->reflist[i]);
	}

	return (0);
}

static int
acpi_powerres_add_device(struct aml_name *name, va_list ap)
{
	int	i;
	int	prnum;
	int	dev_found;
	acpi_softc_t *sc;
	struct	acpi_powerres_device *device;
	struct	acpi_powerres_device_ref *device_ref;
	struct	acpi_powerres_info *powerres;
	struct	aml_name *powerres_name;
	struct	aml_name *method;
	union	aml_object *ret;
	struct	aml_environ env;

	sc = va_arg(ap, acpi_softc_t *);

	/* should be _PR[0-2] */
	prnum =  name->name[3] - '0';
	if (!(prnum >= 0 && prnum <= 2)) {
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
		if (device->name == name) {
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

		/* this is a _PR[0-2] object, we need get a parent of this. */
		device->name = name->parent;
		device->state = 0;	/* assume D0 */

		/* get the current device state */
		method = aml_find_from_namespace(device->name, "_PSC");
		if (method != NULL) {
			bzero(&env, sizeof(env));
			aml_local_stack_push(aml_local_stack_create());
			ret = aml_eval_name(&env, method);
			aml_local_stack_delete(aml_local_stack_pop());
			device->state = ret->num.number;	/* D0 - D3 */
		}
		LIST_INSERT_HEAD(&sc->acpi_powerres_devlist, device, links);
	}

	/* find PowerResource which the device reference to */
	MALLOC(device_ref, struct acpi_powerres_device_ref *,
	   sizeof(*device_ref), M_TEMP, M_NOWAIT);
	if (device_ref == NULL) {
		return (1);
	}
	device_ref->device = device;
	env.curname = device->name;
	for (i = 0; i < name->property->package.elements; i++) {
		if (name->property->package.objects[i]->type != aml_t_namestr) {
			printf("acpi_powerres_add_device: not name string\n");
			continue;
		}
		powerres_name = aml_search_name(&env,
		    name->property->package.objects[i]->nstr.dp);
		if (powerres_name == NULL) {
			printf("acpi_powerres_add_device: not found\n");
			continue;
		}

		LIST_FOREACH(powerres, &sc->acpi_powerres_inflist, links) {
			if (powerres->name == powerres_name) {
				LIST_INSERT_HEAD(&powerres->reflist[prnum],
				    device_ref, links);
				break;
			}
		}
	}

	return (0);
}

void
acpi_powerres_set_sleeping_state(acpi_softc_t *sc, u_int8_t state)
{
	int	i;
	struct	acpi_powerres_info *powerres;
	struct	acpi_powerres_device *device;
	struct	acpi_powerres_device_ref *device_ref;
	struct	aml_name *method;
	union	aml_object *ret;
	struct	aml_environ env;

	if (!(state >= 1 && state <= 4)) {
		return;
	}

	acpi_powerres_init(sc);
	aml_apply_foreach_found_objects(aml_get_rootname(), ".",
	    acpi_powerres_register, sc);
	aml_apply_foreach_found_objects(aml_get_rootname(), "_PR",
	    acpi_powerres_add_device, sc);

	/*
	 * initialize with D0, then change to D3 later based on
	 * PowerResource state change.
	 */
	LIST_FOREACH(device, &sc->acpi_powerres_devlist, links) {
		device->next_state = 0;
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
			if (powerres->state == 1) {
				method = aml_find_from_namespace(powerres->name,
				    "_OFF");
				if (method == NULL) {
					continue;	/* just in case */
				}

				bzero(&env, sizeof(env));
				aml_local_stack_push(aml_local_stack_create());
				aml_eval_name(&env, method);
				aml_local_stack_delete(aml_local_stack_pop());
				powerres->state = 0;
			}
			/*
			 * Device states are compatible with the current
			 * Power Resource states.
			 */
			for (i = 0; i < 3; i++) {
				LIST_FOREACH(device_ref, &powerres->reflist[i], links) {
					/* D3 state */
					device_ref->device->next_state = 3;
				}
			}
		} else {
			/* if OFF state then put it in the ON state */
			if (powerres->state == 0) {
				method = aml_find_from_namespace(powerres->name,
				    "_ON");
				if (method == NULL) {
					continue;	/* just in case */
				}

				bzero(&env, sizeof(env));
				aml_local_stack_push(aml_local_stack_create());
				aml_eval_name(&env, method);
				aml_local_stack_delete(aml_local_stack_pop());
				powerres->state = 1;
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
		if (device->next_state == 3 && device->state != 3) {
			method = aml_find_from_namespace(device->name, "_PS3");
			if (method != NULL) {
				bzero(&env, sizeof(env));
				aml_local_stack_push(aml_local_stack_create());
				aml_eval_name(&env, method);
				aml_local_stack_delete(aml_local_stack_pop());
			}
		}
		if (device->next_state == 0 && device->state != 0) {
			method = aml_find_from_namespace(device->name, "_PS0");
			if (method != NULL) {
				bzero(&env, sizeof(env));
				aml_local_stack_push(aml_local_stack_create());
				aml_eval_name(&env, method);
				aml_local_stack_delete(aml_local_stack_pop());
			}
		}
		/* get the current device state */
		method = aml_find_from_namespace(device->name, "_PSC");
		if (method != NULL) {
			bzero(&env, sizeof(env));
			aml_local_stack_push(aml_local_stack_create());
			ret = aml_eval_name(&env, method);
			aml_local_stack_delete(aml_local_stack_pop());
			device->state = ret->num.number;	/* D0 - D3 */
		}
	}
#if 1
	acpi_powerres_init(sc);
#endif
}
