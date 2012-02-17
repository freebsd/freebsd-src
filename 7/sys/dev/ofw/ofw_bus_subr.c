/*-
 * Copyright (c) 2005 Marius Strobl <marius@FreeBSD.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions, and the following disclaimer,
 *    without modification, immediately at the beginning of the file.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/errno.h>

#include <dev/ofw/ofw_bus_subr.h>
#include <dev/ofw/openfirm.h>

#include "ofw_bus_if.h"

int
ofw_bus_gen_setup_devinfo(struct ofw_bus_devinfo *obd, phandle_t node)
{

	if (obd == NULL)
		return (ENOMEM);
	/* The 'name' property is considered mandatory. */
	if ((OF_getprop_alloc(node, "name", 1, (void **)&obd->obd_name)) == -1)
		return (EINVAL);
	OF_getprop_alloc(node, "compatible", 1, (void **)&obd->obd_compat);
	OF_getprop_alloc(node, "device_type", 1, (void **)&obd->obd_type);
	OF_getprop_alloc(node, "model", 1, (void **)&obd->obd_model);
	obd->obd_node = node;
	return (0);
}

void
ofw_bus_gen_destroy_devinfo(struct ofw_bus_devinfo *obd)
{

	if (obd == NULL)
		return;
	if (obd->obd_compat != NULL)
		free(obd->obd_compat, M_OFWPROP);
	if (obd->obd_model != NULL)
		free(obd->obd_model, M_OFWPROP);
	if (obd->obd_name != NULL)
		free(obd->obd_name, M_OFWPROP);
	if (obd->obd_type != NULL)
		free(obd->obd_type, M_OFWPROP);
}


const char *
ofw_bus_gen_get_compat(device_t bus, device_t dev)
{
	const struct ofw_bus_devinfo *obd;
 
 	obd = OFW_BUS_GET_DEVINFO(bus, dev);
	if (obd == NULL)
		return (NULL);
	return (obd->obd_compat);
}
 
const char *
ofw_bus_gen_get_model(device_t bus, device_t dev)
{
	const struct ofw_bus_devinfo *obd;

 	obd = OFW_BUS_GET_DEVINFO(bus, dev);
	if (obd == NULL)
		return (NULL);
	return (obd->obd_model);
}

const char *
ofw_bus_gen_get_name(device_t bus, device_t dev)
{
	const struct ofw_bus_devinfo *obd;

 	obd = OFW_BUS_GET_DEVINFO(bus, dev);
	if (obd == NULL)
		return (NULL);
	return (obd->obd_name);
}

phandle_t
ofw_bus_gen_get_node(device_t bus, device_t dev)
{
	const struct ofw_bus_devinfo *obd;

 	obd = OFW_BUS_GET_DEVINFO(bus, dev);
	if (obd == NULL)
		return (0);
	return (obd->obd_node);
}

const char *
ofw_bus_gen_get_type(device_t bus, device_t dev)
{
	const struct ofw_bus_devinfo *obd;

 	obd = OFW_BUS_GET_DEVINFO(bus, dev);
	if (obd == NULL)
		return (NULL);
	return (obd->obd_type);
}
