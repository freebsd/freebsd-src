/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2019 Emmanuel Vadot <manu@FreeBSD.org>
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_platform.h"
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/errno.h>

#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>
#include <dev/ofw/ofw_graph.h>
#include <dev/ofw/openfirm.h>

#include "ofw_bus_if.h"

#define	PORT_MAX_NAME	8

phandle_t
ofw_graph_get_port_by_idx(phandle_t node, uint32_t idx)
{
	phandle_t ports, child;
	uint32_t reg;
	char portnode[PORT_MAX_NAME];

	/* First try to find a port@<idx> node */
	snprintf(portnode, sizeof(portnode), "port@%d", idx);
	child = ofw_bus_find_child(node, portnode);
	if (child != 0)
		return (child);

	/* Now check for 'port' without explicit index. */
	if (idx == 0) {
		snprintf(portnode, sizeof(portnode), "port");
		child = ofw_bus_find_child(node, portnode);
		if (child != 0)
			return (child);
	}

	/* Next try to look under ports */
	ports = ofw_bus_find_child(node, "ports");
	if (ports == 0)
		return (0);

	for (child = OF_child(ports); child != 0; child = OF_peer(child)) {
		if (OF_getencprop(child, "reg", &reg, sizeof(uint32_t)) <= 0 ||
		    reg != idx)
			continue;

		return (child);
	}

	return (0);
}

size_t
ofw_graph_port_get_num_endpoints(phandle_t port)
{
	phandle_t child;
	char *name;
	size_t num = 0;
	int ret;

	for (num = 0, child = OF_child(port); child != 0;
	     child = OF_peer(child)) {
		ret = OF_getprop_alloc(child, "name", (void **)&name);
		if (ret == -1)
			continue;
		if (strcmp(name, "endpoint") == 0)
			num++;
		else if (strncmp(name, "endpoint@", 9) == 0)
			num++;
		free(name, M_OFWPROP);
	}

	return (num);
}

phandle_t
ofw_graph_get_endpoint_by_idx(phandle_t port, uint32_t idx)
{
	phandle_t endpoint, child;
	uint32_t reg;

	/* First test if we have only one endpoint */
	endpoint = ofw_bus_find_child(port, "endpoint");
	if (endpoint != 0)
		return (endpoint);

	/* Then test all childs based on the reg property */
	for (child = OF_child(port); child != 0; child = OF_peer(child)) {
		if (OF_getencprop(child, "reg", &reg, sizeof(uint32_t)) <= 0 ||
		    reg != idx)
			continue;

		return (child);
	}

	return (0);
}

phandle_t
ofw_graph_get_remote_endpoint(phandle_t endpoint)
{
	phandle_t remote;

	if (OF_getencprop(endpoint, "remote-endpoint", &remote,
	      sizeof(phandle_t)) <= 0)
		return (0);

	return (remote);
}

phandle_t
ofw_graph_get_remote_parent(phandle_t remote)
{
	phandle_t node;
	char *name;
	int ret;

	/* get the endpoint node */
	node = OF_node_from_xref(remote);

	/* go to the port@X node */
	node = OF_parent(node);
	/* go to the ports node or parent */
	node = OF_parent(node);

	/* if the node name is 'ports' we need to go up one last time */
	ret = OF_getprop_alloc(node, "name", (void **)&name);
	if (ret == -1) {
		printf("%s: Node %x don't have a name, abort\n", __func__, node);
		node = 0;
		goto end;
	}
	if (strcmp("ports", name) == 0)
		node = OF_parent(node);

end:
	free(name, M_OFWPROP);
	return (node);
}

device_t
ofw_graph_get_device_by_port_ep(phandle_t node, uint32_t port_id, uint32_t ep_id)
{
	phandle_t outport, port, endpoint, remote;

	port = ofw_graph_get_port_by_idx(node, port_id);
	if (port == 0)
		return (NULL);
	endpoint = ofw_graph_get_endpoint_by_idx(port, ep_id);
	if (endpoint == 0)
		return NULL;
	remote = ofw_graph_get_remote_endpoint(endpoint);
	if (remote == 0)
		return (NULL);
	outport = ofw_graph_get_remote_parent(remote);
	if (outport == 0)
		return (NULL);

	return (OF_device_from_xref(OF_xref_from_node(outport)));
}
