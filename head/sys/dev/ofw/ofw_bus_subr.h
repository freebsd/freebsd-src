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
 *
 * $FreeBSD$
 */

#ifndef	_DEV_OFW_OFW_BUS_SUBR_H_
#define	_DEV_OFW_OFW_BUS_SUBR_H_

#include <sys/bus.h>

#include <dev/ofw/openfirm.h>

#include "ofw_bus_if.h"

#define	ORIP_NOINT	-1
#define	ORIR_NOTFOUND	0xffffffff

struct ofw_bus_iinfo {
	uint8_t			*opi_imap;
	uint8_t			*opi_imapmsk;
	int			opi_imapsz;
	pcell_t			opi_addrc;
};

struct ofw_compat_data {
	const char	*ocd_str;
	uintptr_t	 ocd_data;
};

/* Generic implementation of ofw_bus_if.m methods and helper routines */
int	ofw_bus_gen_setup_devinfo(struct ofw_bus_devinfo *, phandle_t);
void	ofw_bus_gen_destroy_devinfo(struct ofw_bus_devinfo *);

ofw_bus_get_compat_t	ofw_bus_gen_get_compat;
ofw_bus_get_model_t	ofw_bus_gen_get_model;
ofw_bus_get_name_t	ofw_bus_gen_get_name;
ofw_bus_get_node_t	ofw_bus_gen_get_node;
ofw_bus_get_type_t	ofw_bus_gen_get_type;

/* Helper method to report interesting OF properties in pnpinfo */
bus_child_pnpinfo_str_t	ofw_bus_gen_child_pnpinfo_str;

/* Routines for processing firmware interrupt maps */
void	ofw_bus_setup_iinfo(phandle_t, struct ofw_bus_iinfo *, int);
int	ofw_bus_lookup_imap(phandle_t, struct ofw_bus_iinfo *, void *, int,
	    void *, int, void *, int, phandle_t *);
int	ofw_bus_search_intrmap(void *, int, void *, int, void *, int, void *,
	    void *, void *, int, phandle_t *);

/* Routines for parsing device-tree data into resource lists. */
int ofw_bus_reg_to_rl(device_t, phandle_t, pcell_t, pcell_t,
    struct resource_list *);
int ofw_bus_intr_to_rl(device_t, phandle_t, struct resource_list *, int *);

/* Helper to get device status property */
const char *ofw_bus_get_status(device_t dev);
int ofw_bus_status_okay(device_t dev);

/* Helper to get node's interrupt parent */
void	ofw_bus_find_iparent(phandle_t);

/* Helper routine for checking compat prop */
int ofw_bus_is_compatible(device_t, const char *);
int ofw_bus_is_compatible_strict(device_t, const char *);

/* 
 * Helper routine to search a list of compat properties.  The table is
 * terminated by an entry with a NULL compat-string pointer; a pointer to that
 * table entry is returned if none of the compat strings match for the device,
 * giving you control over the not-found value.  Will not return NULL unless the
 * provided table pointer is NULL.
 */
const struct ofw_compat_data *
    ofw_bus_search_compatible(device_t, const struct ofw_compat_data *);

/* Helper routine for checking existence of a prop */
int ofw_bus_has_prop(device_t, const char *);

/* Helper to search for a child with a given compat prop */
phandle_t ofw_bus_find_compatible(phandle_t, const char *);

/* Helper to search for a child with a given name */
phandle_t ofw_bus_find_child(phandle_t, const char *);

/* Helper routine to find a device_t child matchig a given phandle_t */
device_t ofw_bus_find_child_device_by_phandle(device_t bus, phandle_t node);

#endif /* !_DEV_OFW_OFW_BUS_SUBR_H_ */
