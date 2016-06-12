/*
 * Copyright (c) 2004-2008 Voltaire Inc.  All rights reserved.
 * Copyright (c) 2007 Xsigo Systems Inc.  All rights reserved.
 *
 * This software is available to you under a choice of one of two
 * licenses.  You may choose to be licensed under the terms of the GNU
 * General Public License (GPL) Version 2, available from the file
 * COPYING in the main directory of this source tree, or the
 * OpenIB.org BSD license below:
 *
 *     Redistribution and use in source and binary forms, with or
 *     without modification, are permitted provided that the following
 *     conditions are met:
 *
 *      - Redistributions of source code must retain the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer.
 *
 *      - Redistributions in binary form must reproduce the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer in the documentation and/or other materials
 *        provided with the distribution.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 */

#if HAVE_CONFIG_H
#  include <config.h>
#endif /* HAVE_CONFIG_H */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdarg.h>
#include <time.h>
#include <string.h>
#include <getopt.h>
#include <errno.h>
#include <inttypes.h>

#include <infiniband/common.h>
#include <infiniband/umad.h>
#include <infiniband/mad.h>
#include <complib/cl_nodenamemap.h>

#include "ibnetdiscover.h"
#include "grouping.h"
#include "ibdiag_common.h"

static char *node_type_str[] = {
	"???",
	"ca",
	"switch",
	"router",
	"iwarp rnic"
};

static char *linkwidth_str[] = {
	"??",
	"1x",
	"4x",
	"??",
	"8x",
	"??",
	"??",
	"??",
	"12x"
};

static char *linkspeed_str[] = {
	"???",
	"SDR",
	"DDR",
	"???",
	"QDR"
};

static int timeout = 2000;		/* ms */
static int dumplevel = 0;
static int verbose;
static FILE *f;

char *argv0 = "ibnetdiscover";

static char *node_name_map_file = NULL;
static nn_map_t *node_name_map = NULL;

Node *nodesdist[MAXHOPS+1];     /* last is Ca list */
Node *mynode;
int maxhops_discovered = 0;

struct ChassisList *chassis = NULL;

static char *
get_linkwidth_str(int linkwidth)
{
	if (linkwidth > 8)
		return linkwidth_str[0];
	else
		return linkwidth_str[linkwidth];
}

static char *
get_linkspeed_str(int linkspeed)
{
	if (linkspeed > 4)
		return linkspeed_str[0];
	else
		return linkspeed_str[linkspeed];
}

static inline const char*
node_type_str2(Node *node)
{
	switch(node->type) {
	case SWITCH_NODE: return "SW";
	case CA_NODE:     return "CA";
	case ROUTER_NODE: return "RT";
	}
	return "??";
}

void
decode_port_info(void *pi, Port *port)
{
	mad_decode_field(pi, IB_PORT_LID_F, &port->lid);
	mad_decode_field(pi, IB_PORT_LMC_F, &port->lmc);
	mad_decode_field(pi, IB_PORT_STATE_F, &port->state);
	mad_decode_field(pi, IB_PORT_PHYS_STATE_F, &port->physstate);
	mad_decode_field(pi, IB_PORT_LINK_WIDTH_ACTIVE_F, &port->linkwidth);
	mad_decode_field(pi, IB_PORT_LINK_SPEED_ACTIVE_F, &port->linkspeed);
}


int
get_port(Port *port, int portnum, ib_portid_t *portid)
{
	char portinfo[64];
	void *pi = portinfo;

	port->portnum = portnum;

	if (!smp_query(pi, portid, IB_ATTR_PORT_INFO, portnum, timeout))
		return -1;
	decode_port_info(pi, port);

	DEBUG("portid %s portnum %d: lid %d state %d physstate %d %s %s",
		portid2str(portid), portnum, port->lid, port->state, port->physstate, get_linkwidth_str(port->linkwidth), get_linkspeed_str(port->linkspeed));
	return 1;
}
/*
 * Returns 0 if non switch node is found, 1 if switch is found, -1 if error.
 */
int
get_node(Node *node, Port *port, ib_portid_t *portid)
{
	char portinfo[64];
	char switchinfo[64];
	void *pi = portinfo, *ni = node->nodeinfo, *nd = node->nodedesc;
	void *si = switchinfo;

	if (!smp_query(ni, portid, IB_ATTR_NODE_INFO, 0, timeout))
		return -1;

	mad_decode_field(ni, IB_NODE_GUID_F, &node->nodeguid);
	mad_decode_field(ni, IB_NODE_TYPE_F, &node->type);
	mad_decode_field(ni, IB_NODE_NPORTS_F, &node->numports);
	mad_decode_field(ni, IB_NODE_DEVID_F, &node->devid);
	mad_decode_field(ni, IB_NODE_VENDORID_F, &node->vendid);
	mad_decode_field(ni, IB_NODE_SYSTEM_GUID_F, &node->sysimgguid);
	mad_decode_field(ni, IB_NODE_PORT_GUID_F, &node->portguid);
	mad_decode_field(ni, IB_NODE_LOCAL_PORT_F, &node->localport);
	port->portnum = node->localport;
	port->portguid = node->portguid;

	if (!smp_query(nd, portid, IB_ATTR_NODE_DESC, 0, timeout))
		return -1;

	if (!smp_query(pi, portid, IB_ATTR_PORT_INFO, 0, timeout))
		return -1;
	decode_port_info(pi, port);

	if (node->type != SWITCH_NODE)
		return 0;

	node->smalid = port->lid;
	node->smalmc = port->lmc;

	/* after we have the sma information find out the real PortInfo for this port */
	if (!smp_query(pi, portid, IB_ATTR_PORT_INFO, node->localport, timeout))
	        return -1;
	decode_port_info(pi, port);

        if (!smp_query(si, portid, IB_ATTR_SWITCH_INFO, 0, timeout))
                node->smaenhsp0 = 0;	/* assume base SP0 */
	else
        	mad_decode_field(si, IB_SW_ENHANCED_PORT0_F, &node->smaenhsp0);

	DEBUG("portid %s: got switch node %" PRIx64 " '%s'",
	      portid2str(portid), node->nodeguid, node->nodedesc);
	return 1;
}

static int
extend_dpath(ib_dr_path_t *path, int nextport)
{
	if (path->cnt+2 >= sizeof(path->p))
		return -1;
	++path->cnt;
	if (path->cnt > maxhops_discovered)
		maxhops_discovered = path->cnt;
	path->p[path->cnt] = nextport;
	return path->cnt;
}

static void
dump_endnode(ib_portid_t *path, char *prompt, Node *node, Port *port)
{
	if (!dumplevel)
		return;

	fprintf(f, "%s -> %s %s {%016" PRIx64 "} portnum %d lid %d-%d\"%s\"\n",
		portid2str(path), prompt,
		(node->type <= IB_NODE_MAX ? node_type_str[node->type] : "???"),
		node->nodeguid, node->type == SWITCH_NODE ? 0 : port->portnum,
		port->lid, port->lid + (1 << port->lmc) - 1,
		clean_nodedesc(node->nodedesc));
}

#define HASHGUID(guid)		((uint32_t)(((uint32_t)(guid) * 101) ^ ((uint32_t)((guid) >> 32) * 103)))
#define HTSZ 137

static Node *nodestbl[HTSZ];

static Node *
find_node(Node *new)
{
	int hash = HASHGUID(new->nodeguid) % HTSZ;
	Node *node;

	for (node = nodestbl[hash]; node; node = node->htnext)
		if (node->nodeguid == new->nodeguid)
			return node;

	return NULL;
}

static Node *
create_node(Node *temp, ib_portid_t *path, int dist)
{
	Node *node;
	int hash = HASHGUID(temp->nodeguid) % HTSZ;

	node = malloc(sizeof(*node));
	if (!node)
		return NULL;

	memcpy(node, temp, sizeof(*node));
	node->dist = dist;
	node->path = *path;

	node->htnext = nodestbl[hash];
	nodestbl[hash] = node;

	if (node->type != SWITCH_NODE)
		dist = MAXHOPS; 	/* special Ca list */

	node->dnext = nodesdist[dist];
	nodesdist[dist] = node;

	return node;
}

static Port *
find_port(Node *node, Port *port)
{
	Port *old;

	for (old = node->ports; old; old = old->next)
		if (old->portnum == port->portnum)
			return old;

	return NULL;
}

static Port *
create_port(Node *node, Port *temp)
{
	Port *port;

	port = malloc(sizeof(*port));
	if (!port)
		return NULL;

	memcpy(port, temp, sizeof(*port));
	port->node = node;
	port->next = node->ports;
	node->ports = port;

	return port;
}

static void
link_ports(Node *node, Port *port, Node *remotenode, Port *remoteport)
{
	DEBUG("linking: 0x%" PRIx64 " %p->%p:%u and 0x%" PRIx64 " %p->%p:%u",
		node->nodeguid, node, port, port->portnum,
		remotenode->nodeguid, remotenode, remoteport, remoteport->portnum);
	if (port->remoteport)
		port->remoteport->remoteport = NULL;
	if (remoteport->remoteport)
		remoteport->remoteport->remoteport = NULL;
	port->remoteport = remoteport;
	remoteport->remoteport = port;
}

static int
handle_port(Node *node, Port *port, ib_portid_t *path, int portnum, int dist)
{
	Node node_buf;
	Port port_buf;
	Node *remotenode, *oldnode;
	Port *remoteport, *oldport;

	memset(&node_buf, 0, sizeof(node_buf));
	memset(&port_buf, 0, sizeof(port_buf));

	DEBUG("handle node %p port %p:%d dist %d", node, port, portnum, dist);
	if (port->physstate != 5)	/* LinkUp */
		return -1;

	if (extend_dpath(&path->drpath, portnum) < 0)
		return -1;

	if (get_node(&node_buf, &port_buf, path) < 0) {
		IBWARN("NodeInfo on %s failed, skipping port",
			portid2str(path));
		path->drpath.cnt--;	/* restore path */
		return -1;
	}

	oldnode = find_node(&node_buf);
	if (oldnode)
		remotenode = oldnode;
	else if (!(remotenode = create_node(&node_buf, path, dist + 1)))
		IBERROR("no memory");

	oldport = find_port(remotenode, &port_buf);
	if (oldport) {
		remoteport = oldport;
		if (node != remotenode || port != remoteport)
			IBWARN("port moving...");
	} else if (!(remoteport = create_port(remotenode, &port_buf)))
		IBERROR("no memory");

	dump_endnode(path, oldnode ? "known remote" : "new remote",
		     remotenode, remoteport);

	link_ports(node, port, remotenode, remoteport);

	path->drpath.cnt--;	/* restore path */
	return 0;
}

/*
 * Return 1 if found, 0 if not, -1 on errors.
 */
static int
discover(ib_portid_t *from)
{
	Node node_buf;
	Port port_buf;
	Node *node;
	Port *port;
	int i;
	int dist = 0;
	ib_portid_t *path;

	DEBUG("from %s", portid2str(from));

	memset(&node_buf, 0, sizeof(node_buf));
	memset(&port_buf, 0, sizeof(port_buf));

	if (get_node(&node_buf, &port_buf, from) < 0) {
		IBWARN("can't reach node %s", portid2str(from));
		return -1;
	}

	node = create_node(&node_buf, from, 0);
	if (!node)
		IBERROR("out of memory");

	mynode = node;

	port = create_port(node, &port_buf);
	if (!port)
		IBERROR("out of memory");

	if (node->type != SWITCH_NODE &&
	    handle_port(node, port, from, node->localport, 0) < 0)
		return 0;

	for (dist = 0; dist < MAXHOPS; dist++) {

		for (node = nodesdist[dist]; node; node = node->dnext) {

			path = &node->path;

			DEBUG("dist %d node %p", dist, node);
			dump_endnode(path, "processing", node, port);

			for (i = 1; i <= node->numports; i++) {
				if (i == node->localport)
					continue;

				if (get_port(&port_buf, i, path) < 0) {
					IBWARN("can't reach node %s port %d", portid2str(path), i);
					continue;
				}

				port = find_port(node, &port_buf);
				if (port)
					continue;

				port = create_port(node, &port_buf);
				if (!port)
					IBERROR("out of memory");

				/* If switch, set port GUID to node GUID */
				if (node->type == SWITCH_NODE)
					port->portguid = node->portguid;

				handle_port(node, port, path, i, dist);
			}
		}
	}

	return 0;
}

char *
node_name(Node *node)
{
	static char buf[256];

	switch(node->type) {
	case SWITCH_NODE:
		sprintf(buf, "\"%s", "S");
		break;
	case CA_NODE:
		sprintf(buf, "\"%s", "H");
		break;
	case ROUTER_NODE:
		sprintf(buf, "\"%s", "R");
		break;
	default:
		sprintf(buf, "\"%s", "?");
		break;
	}
	sprintf(buf+2, "-%016" PRIx64 "\"", node->nodeguid);

	return buf;
}

void
list_node(Node *node)
{
	char *node_type;
	char *nodename = remap_node_name(node_name_map, node->nodeguid,
					      node->nodedesc);

	switch(node->type) {
	case SWITCH_NODE:
		node_type = "Switch";
		break;
	case CA_NODE:
		node_type = "Ca";
		break;
	case ROUTER_NODE:
		node_type = "Router";
		break;
	default:
		node_type = "???";
		break;
	}
	fprintf(f, "%s\t : 0x%016" PRIx64 " ports %d devid 0x%x vendid 0x%x \"%s\"\n",
		node_type,
		node->nodeguid, node->numports, node->devid, node->vendid,
		nodename);

	free(nodename);
}

void
out_ids(Node *node, int group, char *chname)
{
	fprintf(f, "\nvendid=0x%x\ndevid=0x%x\n", node->vendid, node->devid);
	if (node->sysimgguid)
		fprintf(f, "sysimgguid=0x%" PRIx64, node->sysimgguid);
	if (group
	    && node->chrecord && node->chrecord->chassisnum) {
		fprintf(f, "\t\t# Chassis %d", node->chrecord->chassisnum);
		if (chname)
			fprintf(f, " (%s)", chname);
		if (is_xsigo_tca(node->nodeguid) && node->ports->remoteport)
			fprintf(f, " slot %d", node->ports->remoteport->portnum);
	}
	fprintf(f, "\n");
}

uint64_t
out_chassis(int chassisnum)
{
	uint64_t guid;

	fprintf(f, "\nChassis %d", chassisnum);
	guid = get_chassis_guid(chassisnum);
	if (guid)
		fprintf(f, " (guid 0x%" PRIx64 ")", guid);
	fprintf(f, "\n");
	return guid;
}

void
out_switch(Node *node, int group, char *chname)
{
	char *str;
	char *nodename = NULL;

	out_ids(node, group, chname);
	fprintf(f, "switchguid=0x%" PRIx64, node->nodeguid);
	fprintf(f, "(%" PRIx64 ")", node->portguid);
	/* Currently, only if Voltaire chassis */
	if (group
	    && node->chrecord && node->chrecord->chassisnum
	    && node->vendid == VTR_VENDOR_ID) {
		str = get_chassis_type(node->chrecord->chassistype);
		if (str)
			fprintf(f, "%s ", str);
		str = get_chassis_slot(node->chrecord->chassisslot);
		if (str)
			fprintf(f, "%s ", str);
		fprintf(f, "%d Chip %d", node->chrecord->slotnum, node->chrecord->anafanum);
	}

	nodename = remap_node_name(node_name_map, node->nodeguid,
				node->nodedesc);

	fprintf(f, "\nSwitch\t%d %s\t\t# \"%s\" %s port 0 lid %d lmc %d\n",
		node->numports, node_name(node),
		nodename,
		node->smaenhsp0 ? "enhanced" : "base",
		node->smalid, node->smalmc);

	free(nodename);
}

void
out_ca(Node *node, int group, char *chname)
{
	char *node_type;
	char *node_type2;
	char *nodename = remap_node_name(node_name_map, node->nodeguid,
					      node->nodedesc);

	out_ids(node, group, chname);
	switch(node->type) {
	case CA_NODE:
		node_type = "ca";
		node_type2 = "Ca";
		break;
	case ROUTER_NODE:
		node_type = "rt";
		node_type2 = "Rt";
		break;
	default:
		node_type = "???";
		node_type2 = "???";
		break;
	}

	fprintf(f, "%sguid=0x%" PRIx64 "\n", node_type, node->nodeguid);
	fprintf(f, "%s\t%d %s\t\t# \"%s\"",
		node_type2, node->numports, node_name(node),
		nodename);
	if (group && is_xsigo_hca(node->nodeguid))
		fprintf(f, " (scp)");
	fprintf(f, "\n");

	free(nodename);
}

static char *
out_ext_port(Port *port, int group)
{
	char *str = NULL;

	/* Currently, only if Voltaire chassis */
	if (group
	    && port->node->chrecord && port->node->vendid == VTR_VENDOR_ID)
		str = portmapstring(port);

	return (str);
}

void
out_switch_port(Port *port, int group)
{
	char *ext_port_str = NULL;
	char *rem_nodename = NULL;

	DEBUG("port %p:%d remoteport %p", port, port->portnum, port->remoteport);
	fprintf(f, "[%d]", port->portnum);

	ext_port_str = out_ext_port(port, group);
	if (ext_port_str)
		fprintf(f, "%s", ext_port_str);

	rem_nodename = remap_node_name(node_name_map,
				port->remoteport->node->nodeguid,
				port->remoteport->node->nodedesc);

	ext_port_str = out_ext_port(port->remoteport, group);
	fprintf(f, "\t%s[%d]%s",
		node_name(port->remoteport->node),
		port->remoteport->portnum,
		ext_port_str ? ext_port_str : "");
	if (port->remoteport->node->type != SWITCH_NODE)
		fprintf(f, "(%" PRIx64 ") ", port->remoteport->portguid);
	fprintf(f, "\t\t# \"%s\" lid %d %s%s",
		rem_nodename,
		port->remoteport->node->type == SWITCH_NODE ? port->remoteport->node->smalid : port->remoteport->lid,
		get_linkwidth_str(port->linkwidth),
		get_linkspeed_str(port->linkspeed));

	if (is_xsigo_tca(port->remoteport->portguid))
		fprintf(f, " slot %d", port->portnum);
	else if (is_xsigo_hca(port->remoteport->portguid))
		fprintf(f, " (scp)");
	fprintf(f, "\n");

	free(rem_nodename);
}

void
out_ca_port(Port *port, int group)
{
	char *str = NULL;
	char *rem_nodename = NULL;

	fprintf(f, "[%d]", port->portnum);
	if (port->node->type != SWITCH_NODE)
		fprintf(f, "(%" PRIx64 ") ", port->portguid);
	fprintf(f, "\t%s[%d]",
		node_name(port->remoteport->node),
		port->remoteport->portnum);
	str = out_ext_port(port->remoteport, group);
	if (str)
		fprintf(f, "%s", str);
	if (port->remoteport->node->type != SWITCH_NODE)
		fprintf(f, " (%" PRIx64 ") ", port->remoteport->portguid);

	rem_nodename = remap_node_name(node_name_map,
				port->remoteport->node->nodeguid,
				port->remoteport->node->nodedesc);

	fprintf(f, "\t\t# lid %d lmc %d \"%s\" lid %d %s%s\n",
		port->lid, port->lmc, rem_nodename,
		port->remoteport->node->type == SWITCH_NODE ? port->remoteport->node->smalid : port->remoteport->lid,
		get_linkwidth_str(port->linkwidth),
		get_linkspeed_str(port->linkspeed));

	free(rem_nodename);
}

int
dump_topology(int listtype, int group)
{
	Node *node;
	Port *port;
	int i = 0, dist = 0;
	time_t t = time(0);
	uint64_t chguid;
	char *chname = NULL;

	if (!listtype) {
		fprintf(f, "#\n# Topology file: generated on %s#\n", ctime(&t));
		fprintf(f, "# Max of %d hops discovered\n", maxhops_discovered);
		fprintf(f, "# Initiated from node %016" PRIx64 " port %016" PRIx64 "\n", mynode->nodeguid, mynode->portguid);
	}

	/* Make pass on switches */
	if (group && !listtype) {
		ChassisList *ch = NULL;

		/* Chassis based switches first */
		for (ch = chassis; ch; ch = ch->next) {
			int n = 0;

			if (!ch->chassisnum)
				continue;
			chguid = out_chassis(ch->chassisnum);
			if (chname)
				free(chname);
			chname = NULL;
			if (is_xsigo_guid(chguid)) {
				for (node = nodesdist[MAXHOPS]; node; node = node->dnext) {
					if (!node->chrecord ||
					    !node->chrecord->chassisnum)
						continue;

					if (node->chrecord->chassisnum != ch->chassisnum)
						continue;

					if (is_xsigo_hca(node->nodeguid)) {
						chname = remap_node_name(node_name_map,
								node->nodeguid,
								node->nodedesc);
						fprintf(f, "Hostname: %s\n", chname);
					}
				}
			}

			fprintf(f, "\n# Spine Nodes");
			for (n = 1; n <= (SPINES_MAX_NUM); n++) {
				if (ch->spinenode[n]) {
					out_switch(ch->spinenode[n], group, chname);
					for (port = ch->spinenode[n]->ports; port; port = port->next, i++)
						if (port->remoteport)
							out_switch_port(port, group);
				}
			}
			fprintf(f, "\n# Line Nodes");
			for (n = 1; n <= (LINES_MAX_NUM); n++) {
				if (ch->linenode[n]) {
					out_switch(ch->linenode[n], group, chname);
					for (port = ch->linenode[n]->ports; port; port = port->next, i++)
						if (port->remoteport)
							out_switch_port(port, group);
				}
			}

			fprintf(f, "\n# Chassis Switches");
			for (dist = 0; dist <= maxhops_discovered; dist++) {

				for (node = nodesdist[dist]; node; node = node->dnext) {

					/* Non Voltaire chassis */
					if (node->vendid == VTR_VENDOR_ID)
						continue;
					if (!node->chrecord ||
					    !node->chrecord->chassisnum)
						continue;

					if (node->chrecord->chassisnum != ch->chassisnum)
						continue;

					out_switch(node, group, chname);
					for (port = node->ports; port; port = port->next, i++)
						if (port->remoteport)
							out_switch_port(port, group);

				}

			}

			fprintf(f, "\n# Chassis CAs");
			for (node = nodesdist[MAXHOPS]; node; node = node->dnext) {
				if (!node->chrecord ||
				    !node->chrecord->chassisnum)
					continue;

				if (node->chrecord->chassisnum != ch->chassisnum)
					continue;

				out_ca(node, group, chname);
				for (port = node->ports; port; port = port->next, i++)
					if (port->remoteport)
						out_ca_port(port, group);

			}

		}

	} else {
		for (dist = 0; dist <= maxhops_discovered; dist++) {

			for (node = nodesdist[dist]; node; node = node->dnext) {

				DEBUG("SWITCH: dist %d node %p", dist, node);
				if (!listtype)
					out_switch(node, group, chname);
				else {
					if (listtype & LIST_SWITCH_NODE)
						list_node(node);
					continue;
				}

				for (port = node->ports; port; port = port->next, i++)
					if (port->remoteport)
						out_switch_port(port, group);
			}
		}
	}

	if (chname)
		free(chname);
	chname = NULL;
	if (group && !listtype) {

		fprintf(f, "\nNon-Chassis Nodes\n");

		for (dist = 0; dist <= maxhops_discovered; dist++) {

			for (node = nodesdist[dist]; node; node = node->dnext) {

				DEBUG("SWITCH: dist %d node %p", dist, node);
				/* Now, skip chassis based switches */
				if (node->chrecord &&
				    node->chrecord->chassisnum)
					continue;
				out_switch(node, group, chname);

				for (port = node->ports; port; port = port->next, i++)
					if (port->remoteport)
						out_switch_port(port, group);
			}

		}

	}

	/* Make pass on CAs */
	for (node = nodesdist[MAXHOPS]; node; node = node->dnext) {

		DEBUG("CA: dist %d node %p", dist, node);
		if (!listtype) {
			/* Now, skip chassis based CAs */
			if (group && node->chrecord &&
			    node->chrecord->chassisnum)
				continue;
			out_ca(node, group, chname);
		} else {
			if (((listtype & LIST_CA_NODE) && (node->type == CA_NODE)) ||
			    ((listtype & LIST_ROUTER_NODE) && (node->type == ROUTER_NODE)))
				list_node(node);
			continue;
		}

		for (port = node->ports; port; port = port->next, i++)
			if (port->remoteport)
				out_ca_port(port, group);
	}

	if (chname)
		free(chname);

	return i;
}

void dump_ports_report ()
{
	int b, n = 0, p;
	Node *node;
	Port *port;

	// If switch and LID == 0, search of other switch ports with
	// valid LID and assign it to all ports of that switch
	for (b = 0; b <= MAXHOPS; b++)
		for (node = nodesdist[b]; node; node = node->dnext)
			if (node->type == SWITCH_NODE) {
				int swlid = 0;
				for (p = 0, port = node->ports;
				     p < node->numports && port && !swlid;
				     port = port->next)
					if (port->lid != 0)
						swlid = port->lid;
				for (p = 0, port = node->ports;
				     p < node->numports && port;
				     port = port->next)
					port->lid = swlid;
			}

	for (b = 0; b <= MAXHOPS; b++)
		for (node = nodesdist[b]; node; node = node->dnext) {
			for (p = 0, port = node->ports;
			     p < node->numports && port;
			     p++, port = port->next) {
				fprintf(stdout,
					"%2s %5d %2d 0x%016" PRIx64 " %s %s",
					node_type_str2(port->node), port->lid,
					port->portnum,
					port->portguid,
					get_linkwidth_str(port->linkwidth),
					get_linkspeed_str(port->linkspeed));
				if (port->remoteport)
					fprintf(stdout,
						" - %2s %5d %2d 0x%016" PRIx64
						" ( '%s' - '%s' )\n",
						node_type_str2(port->remoteport->node),
						port->remoteport->lid,
						port->remoteport->portnum,
						port->remoteport->portguid,
						port->node->nodedesc,
						port->remoteport->node->nodedesc);
				else
					fprintf(stdout, "%36s'%s'\n", "",
						port->node->nodedesc);
			}
			n++;
		}
}

void
usage(void)
{
	fprintf(stderr, "Usage: %s [-d(ebug)] -e(rr_show) -v(erbose) -s(how) -l(ist) -g(rouping) -H(ca_list) -S(witch_list) -R(outer_list) -V(ersion) -C ca_name -P ca_port "
			"-t(imeout) timeout_ms --node-name-map node-name-map] -p(orts) [<topology-file>]\n",
			argv0);
	fprintf(stderr, "       --node-name-map <node-name-map> specify a node name map file\n");
	exit(-1);
}

int
main(int argc, char **argv)
{
	int mgmt_classes[2] = {IB_SMI_CLASS, IB_SMI_DIRECT_CLASS};
	ib_portid_t my_portid = {0};
	int udebug = 0, list = 0;
	char *ca = 0;
	int ca_port = 0;
	int group = 0;
	int ports_report = 0;

	static char const str_opts[] = "C:P:t:devslgHSRpVhu";
	static const struct option long_opts[] = {
		{ "C", 1, 0, 'C'},
		{ "P", 1, 0, 'P'},
		{ "debug", 0, 0, 'd'},
		{ "err_show", 0, 0, 'e'},
		{ "verbose", 0, 0, 'v'},
		{ "show", 0, 0, 's'},
		{ "list", 0, 0, 'l'},
		{ "grouping", 0, 0, 'g'},
		{ "Hca_list", 0, 0, 'H'},
		{ "Switch_list", 0, 0, 'S'},
		{ "Router_list", 0, 0, 'R'},
		{ "timeout", 1, 0, 't'},
		{ "node-name-map", 1, 0, 1},
		{ "ports", 0, 0, 'p'},
		{ "Version", 0, 0, 'V'},
		{ "help", 0, 0, 'h'},
		{ "usage", 0, 0, 'u'},
		{ }
	};

	f = stdout;

	argv0 = argv[0];

	while (1) {
		int ch = getopt_long(argc, argv, str_opts, long_opts, NULL);
		if ( ch == -1 )
			break;
		switch(ch) {
		case 1:
			node_name_map_file = strdup(optarg);
			break;
		case 'C':
			ca = optarg;
			break;
		case 'P':
			ca_port = strtoul(optarg, 0, 0);
			break;
		case 'd':
			ibdebug++;
			madrpc_show_errors(1);
			umad_debug(udebug);
			udebug++;
			break;
		case 't':
			timeout = strtoul(optarg, 0, 0);
			break;
		case 'v':
			verbose++;
			dumplevel++;
			break;
		case 's':
			dumplevel = 1;
			break;
		case 'e':
			madrpc_show_errors(1);
			break;
		case 'l':
			list = LIST_CA_NODE | LIST_SWITCH_NODE | LIST_ROUTER_NODE;
			break;
		case 'g':
			group = 1;
			break;
		case 'S':
			list = LIST_SWITCH_NODE;
			break;
		case 'H':
			list = LIST_CA_NODE;
			break;
		case 'R':
			list = LIST_ROUTER_NODE;
			break;
		case 'V':
			fprintf(stderr, "%s %s\n", argv0, get_build_version() );
			exit(-1);
		case 'p':
			ports_report = 1;
			break;
		default:
			usage();
			break;
		}
	}
	argc -= optind;
	argv += optind;

	if (argc && !(f = fopen(argv[0], "w")))
		IBERROR("can't open file %s for writing", argv[0]);

	madrpc_init(ca, ca_port, mgmt_classes, 2);
	node_name_map = open_node_name_map(node_name_map_file);

	if (discover(&my_portid) < 0)
		IBERROR("discover");

	if (group)
		chassis = group_nodes();

	if (ports_report)
		dump_ports_report();
	else
		dump_topology(list, group);

	close_node_name_map(node_name_map);
	exit(0);
}
