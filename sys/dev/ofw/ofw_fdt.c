/*-
 * Copyright (c) 2009-2010 The FreeBSD Foundation
 * All rights reserved.
 *
 * This software was developed by Semihalf under sponsorship from
 * the FreeBSD Foundation.
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
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
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

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/systm.h>

#include <contrib/libfdt/libfdt.h>

#include <machine/stdarg.h>

#include <dev/fdt/fdt_common.h>
#include <dev/ofw/ofwvar.h>
#include <dev/ofw/openfirm.h>

#include "ofw_if.h"

#ifdef DEBUG
#define debugf(fmt, args...) do { printf("%s(): ", __func__);	\
    printf(fmt,##args); } while (0)
#else
#define debugf(fmt, args...)
#endif

static int ofw_fdt_init(ofw_t, void *);
static phandle_t ofw_fdt_peer(ofw_t, phandle_t);
static phandle_t ofw_fdt_child(ofw_t, phandle_t);
static phandle_t ofw_fdt_parent(ofw_t, phandle_t);
static phandle_t ofw_fdt_instance_to_package(ofw_t, ihandle_t);
static ssize_t ofw_fdt_getproplen(ofw_t, phandle_t, const char *);
static ssize_t ofw_fdt_getprop(ofw_t, phandle_t, const char *, void *, size_t);
static int ofw_fdt_nextprop(ofw_t, phandle_t, const char *, char *, size_t);
static int ofw_fdt_setprop(ofw_t, phandle_t, const char *, const void *,
    size_t);
static ssize_t ofw_fdt_canon(ofw_t, const char *, char *, size_t);
static phandle_t ofw_fdt_finddevice(ofw_t, const char *);
static ssize_t ofw_fdt_instance_to_path(ofw_t, ihandle_t, char *, size_t);
static ssize_t ofw_fdt_package_to_path(ofw_t, phandle_t, char *, size_t);
static int ofw_fdt_interpret(ofw_t, const char *, int, cell_t *);

static ofw_method_t ofw_fdt_methods[] = {
	OFWMETHOD(ofw_init,			ofw_fdt_init),
	OFWMETHOD(ofw_peer,			ofw_fdt_peer),
	OFWMETHOD(ofw_child,			ofw_fdt_child),
	OFWMETHOD(ofw_parent,			ofw_fdt_parent),
	OFWMETHOD(ofw_instance_to_package,	ofw_fdt_instance_to_package),
	OFWMETHOD(ofw_getproplen,		ofw_fdt_getproplen),
	OFWMETHOD(ofw_getprop,			ofw_fdt_getprop),
	OFWMETHOD(ofw_nextprop,			ofw_fdt_nextprop),
	OFWMETHOD(ofw_setprop,			ofw_fdt_setprop),
	OFWMETHOD(ofw_canon,			ofw_fdt_canon),
	OFWMETHOD(ofw_finddevice,		ofw_fdt_finddevice),
	OFWMETHOD(ofw_instance_to_path,		ofw_fdt_instance_to_path),
	OFWMETHOD(ofw_package_to_path,		ofw_fdt_package_to_path),
	OFWMETHOD(ofw_interpret,		ofw_fdt_interpret),
	{ 0, 0 }
};

static ofw_def_t ofw_fdt = {
	OFW_FDT,
	ofw_fdt_methods,
	0
};
OFW_DEF(ofw_fdt);

static void *fdtp = NULL;

static int
ofw_fdt_init(ofw_t ofw, void *data)
{
	int err;

	/* Check FDT blob integrity */
	if ((err = fdt_check_header(data)) != 0)
		return (err);

	fdtp = data;
	return (0);
}

/*
 * Device tree functions
 */

static int
fdt_phandle_offset(phandle_t p)
{
	const char *dt_struct;
	int offset;

	dt_struct = (const char *)fdtp + fdt_off_dt_struct(fdtp);

	if (((const char *)p < dt_struct) ||
	    (const char *)p > (dt_struct + fdt_size_dt_struct(fdtp)))
		return (-1);

	offset = (const char *)p - dt_struct;
	if (offset < 0)
		return (-1);

	return (offset);
}

/* Return the next sibling of this node or 0. */
static phandle_t
ofw_fdt_peer(ofw_t ofw, phandle_t node)
{
	phandle_t p;
	int depth, offset;

	if (node == 0) {
		/* Find root node */
		offset = fdt_path_offset(fdtp, "/");
		p = (phandle_t)fdt_offset_ptr(fdtp, offset, sizeof(p));

		return (p);
	}

	offset = fdt_phandle_offset(node);
	if (offset < 0)
		return (0);

	for (depth = 1, offset = fdt_next_node(fdtp, offset, &depth);
	    offset >= 0;
	    offset = fdt_next_node(fdtp, offset, &depth)) {
		if (depth < 0)
			return (0);
		if (depth == 1) {
			p = (phandle_t)fdt_offset_ptr(fdtp, offset, sizeof(p));
			return (p);
		}
	}

	return (0);
}

/* Return the first child of this node or 0. */
static phandle_t
ofw_fdt_child(ofw_t ofw, phandle_t node)
{
	phandle_t p;
	int depth, offset;

	offset = fdt_phandle_offset(node);
	if (offset < 0)
		return (0);

	for (depth = 0, offset = fdt_next_node(fdtp, offset, &depth);
	    (offset >= 0) && (depth > 0);
	    offset = fdt_next_node(fdtp, offset, &depth)) {
		if (depth < 0)
			return (0);
		if (depth == 1) {
			p = (phandle_t)fdt_offset_ptr(fdtp, offset, sizeof(p));
			return (p);
		}
	}

	return (0);
}

/* Return the parent of this node or 0. */
static phandle_t
ofw_fdt_parent(ofw_t ofw, phandle_t node)
{
	phandle_t p;
	int offset, paroffset;

	offset = fdt_phandle_offset(node);
	if (offset < 0)
		return (0);

	paroffset = fdt_parent_offset(fdtp, offset);
	p = (phandle_t)fdt_offset_ptr(fdtp, paroffset, sizeof(phandle_t));
	return (p);
}

/* Return the package handle that corresponds to an instance handle. */
static phandle_t
ofw_fdt_instance_to_package(ofw_t ofw, ihandle_t instance)
{
	phandle_t p;
	int offset;

	/*
	 * Note: FDT does not have the notion of instances, but we somewhat
	 * abuse the semantics and let treat as 'instance' the internal
	 * 'phandle' prop, so that ofw I/F consumers have a uniform way of
	 * translation between internal representation (which appear in some
	 * contexts as property values) and effective phandles.
	 */
	offset = fdt_node_offset_by_phandle(fdtp, instance);
	if (offset < 0)
		return (-1);

	p = (phandle_t)fdt_offset_ptr(fdtp, offset, sizeof(phandle_t));
	return (p);
}

/* Get the length of a property of a package. */
static ssize_t
ofw_fdt_getproplen(ofw_t ofw, phandle_t package, const char *propname)
{
	const struct fdt_property *prop;
	int offset, len;

	offset = fdt_phandle_offset(package);
	if (offset < 0)
		return (-1);

	if (strcmp(propname, "name") == 0) {
		/* Emulate the 'name' property */
		fdt_get_name(fdtp, offset, &len);
		return (len + 1);
	}

	len = -1;
	prop = fdt_get_property(fdtp, offset, propname, &len);

	return (len);
}

/* Get the value of a property of a package. */
static ssize_t
ofw_fdt_getprop(ofw_t ofw, phandle_t package, const char *propname, void *buf,
    size_t buflen)
{
	const void *prop;
	const char *name;
	int len, offset;

	offset = fdt_phandle_offset(package);
	if (offset < 0)
		return (-1);

	if (strcmp(propname, "name") == 0) {
		/* Emulate the 'name' property */
		name = fdt_get_name(fdtp, offset, &len);
		strncpy(buf, name, buflen);
		if (len + 1 > buflen)
			len = buflen;
		return (len + 1);
	}

	prop = fdt_getprop(fdtp, offset, propname, &len);
	if (prop == NULL)
		return (-1);

	if (len > buflen)
		len = buflen;
	bcopy(prop, buf, len);
	return (len);
}

static int
fdt_nextprop(int offset, char *buf, size_t size)
{
	const struct fdt_property *prop;
	const char *name;
	uint32_t tag;
	int nextoffset, depth;

	depth = 0;
	tag = fdt_next_tag(fdtp, offset, &nextoffset);

	/* Find the next prop */
	do {
		offset = nextoffset;
		tag = fdt_next_tag(fdtp, offset, &nextoffset);

		if (tag == FDT_BEGIN_NODE)
			depth++;
		else if (tag == FDT_END_NODE)
			depth--;
		else if ((tag == FDT_PROP) && (depth == 0)) {
			prop =
			    (const struct fdt_property *)fdt_offset_ptr(fdtp,
			    offset, sizeof(*prop));
			name = fdt_string(fdtp,
			    fdt32_to_cpu(prop->nameoff));
			strncpy(buf, name, size);
			return (strlen(name));
		} else
			depth = -1;
	} while (depth >= 0);

	return (-1);
}

/*
 * Get the next property of a package. Return the actual len of retrieved
 * prop name.
 */
static int
ofw_fdt_nextprop(ofw_t ofw, phandle_t package, const char *previous, char *buf,
    size_t size)
{
	const struct fdt_property *prop;
	int offset, rv;

	offset = fdt_phandle_offset(package);
	if (offset < 0)
		return (-1);

	if (previous == NULL)
		/* Find the first prop in the node */
		return (fdt_nextprop(offset, buf, size));

	/*
	 * Advance to the previous prop
	 */
	prop = fdt_get_property(fdtp, offset, previous, NULL);
	if (prop == NULL)
		return (-1);

	offset = fdt_phandle_offset((phandle_t)prop);
	rv = fdt_nextprop(offset, buf, size);
	return (rv);
}

/* Set the value of a property of a package. */
static int
ofw_fdt_setprop(ofw_t ofw, phandle_t package, const char *propname,
    const void *buf, size_t len)
{
	int offset;

	offset = fdt_phandle_offset(package);
	if (offset < 0)
		return (-1);

	return (fdt_setprop_inplace(fdtp, offset, propname, buf, len));
}

/* Convert a device specifier to a fully qualified pathname. */
static ssize_t
ofw_fdt_canon(ofw_t ofw, const char *device, char *buf, size_t len)
{

	return (-1);
}

/* Return a package handle for the specified device. */
static phandle_t
ofw_fdt_finddevice(ofw_t ofw, const char *device)
{
	phandle_t p;
	int offset;

	offset = fdt_path_offset(fdtp, device);

	p = (phandle_t)fdt_offset_ptr(fdtp, offset, sizeof(p));

	return (p);
}

/* Return the fully qualified pathname corresponding to an instance. */
static ssize_t
ofw_fdt_instance_to_path(ofw_t ofw, ihandle_t instance, char *buf, size_t len)
{

	return (-1);
}

/* Return the fully qualified pathname corresponding to a package. */
static ssize_t
ofw_fdt_package_to_path(ofw_t ofw, phandle_t package, char *buf, size_t len)
{

	return (-1);
}

static int
ofw_fdt_fixup(ofw_t ofw)
{
#define FDT_MODEL_LEN	80
	char model[FDT_MODEL_LEN];
	phandle_t root;
	ssize_t len;
	int i;

	if ((root = ofw_fdt_finddevice(ofw, "/")) == 0)
		return (ENODEV);

	if ((len = ofw_fdt_getproplen(ofw, root, "model")) <= 0)
		return (0);

	bzero(model, FDT_MODEL_LEN);
	if (ofw_fdt_getprop(ofw, root, "model", model, FDT_MODEL_LEN) <= 0)
		return (0);

	/*
	 * Search fixup table and call handler if appropriate.
	 */
	for (i = 0; fdt_fixup_table[i].model != NULL; i++) {
		if (strncmp(model, fdt_fixup_table[i].model,
		    FDT_MODEL_LEN) != 0)
			continue;

		if (fdt_fixup_table[i].handler != NULL)
			(*fdt_fixup_table[i].handler)(root);
	}

	return (0);
}

static int
ofw_fdt_interpret(ofw_t ofw, const char *cmd, int nret, cell_t *retvals)
{
	int rv;

	/*
	 * Note: FDT does not have the possibility to 'interpret' commands,
	 * but we abuse the interface a bit to use it for doing non-standard
	 * operations on the device tree blob.
	 *
	 * Currently the only supported 'command' is to trigger performing
	 * fixups.
	 */
	if (strncmp("perform-fixup", cmd, 13) != 0)
		return (0);

	rv = ofw_fdt_fixup(ofw);
	if (nret > 0)
		retvals[0] = rv;

	return (rv);
}
