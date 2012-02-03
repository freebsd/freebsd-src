/*
 * Copyright (c) 2004-2007 Voltaire Inc.  All rights reserved.
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

/*========================================================*/
/*               FABRIC SCANNER SPECIFIC DATA             */
/*========================================================*/

#if HAVE_CONFIG_H
#  include <config.h>
#endif /* HAVE_CONFIG_H */

#include <stdint.h>
#include <stdlib.h>
#include <inttypes.h>

#include <infiniband/common.h>
#include <infiniband/mad.h>

#include "ibnetdiscover.h"
#include "grouping.h"

#define OUT_BUFFER_SIZE 16


extern Node *nodesdist[MAXHOPS+1];	/* last is CA list */
extern Node *mynode;
extern Port *myport;
extern int maxhops_discovered;

AllChassisList mylist;

char *ChassisTypeStr[5] = { "", "ISR9288", "ISR9096", "ISR2012", "ISR2004" };
char *ChassisSlotStr[4] = { "", "Line", "Spine", "SRBD" };


char *get_chassis_type(unsigned char chassistype)
{
	if (chassistype == UNRESOLVED_CT || chassistype > ISR2004_CT)
		return NULL;
	return ChassisTypeStr[chassistype];
}

char *get_chassis_slot(unsigned char chassisslot)
{
	if (chassisslot == UNRESOLVED_CS || chassisslot > SRBD_CS)
		return NULL;
	return ChassisSlotStr[chassisslot];
}

static struct ChassisList *find_chassisnum(unsigned char chassisnum)
{
	ChassisList *current;

	for (current = mylist.first; current; current = current->next) {
		if (current->chassisnum == chassisnum)
			return current;
	}

	return NULL;
}

static uint64_t topspin_chassisguid(uint64_t guid)
{
	/* Byte 3 in system image GUID is chassis type, and */
	/* Byte 4 is location ID (slot) so just mask off byte 4 */
	return guid & 0xffffffff00ffffffULL;
}

int is_xsigo_guid(uint64_t guid)
{
	if ((guid & 0xffffff0000000000ULL) == 0x0013970000000000ULL)
		return 1;
	else
		return 0;
}

static int is_xsigo_leafone(uint64_t guid)
{
	if ((guid & 0xffffffffff000000ULL) == 0x0013970102000000ULL)
		return 1;
	else
		return 0;
}

int is_xsigo_hca(uint64_t guid)
{
	/* NodeType 2 is HCA */
	if ((guid & 0xffffffff00000000ULL) == 0x0013970200000000ULL)
		return 1;
	else
		return 0;
}

int is_xsigo_tca(uint64_t guid)
{
	/* NodeType 3 is TCA */
	if ((guid & 0xffffffff00000000ULL) == 0x0013970300000000ULL)
		return 1;
	else
		return 0;
}

static int is_xsigo_ca(uint64_t guid)
{
	if (is_xsigo_hca(guid) || is_xsigo_tca(guid))
		return 1;
	else
		return 0;
}

static int is_xsigo_switch(uint64_t guid)
{
	if ((guid & 0xffffffff00000000ULL) == 0x0013970100000000ULL)
		return 1;
	else
		return 0;
}

static uint64_t xsigo_chassisguid(Node *node)
{
	if (!is_xsigo_ca(node->sysimgguid)) {
		/* Byte 3 is NodeType and byte 4 is PortType */
		/* If NodeType is 1 (switch), PortType is masked */
		if (is_xsigo_switch(node->sysimgguid))
			return node->sysimgguid & 0xffffffff00ffffffULL;
		else
			return node->sysimgguid;
	} else {
		/* Is there a peer port ? */
		if (!node->ports->remoteport)
			return node->sysimgguid;

		/* If peer port is Leaf 1, use its chassis GUID */
		if (is_xsigo_leafone(node->ports->remoteport->node->sysimgguid))
			return node->ports->remoteport->node->sysimgguid &
			       0xffffffff00ffffffULL;
		else
			return node->sysimgguid;
	}
}

static uint64_t get_chassisguid(Node *node)
{
	if (node->vendid == TS_VENDOR_ID || node->vendid == SS_VENDOR_ID)
		return topspin_chassisguid(node->sysimgguid);
	else if (node->vendid == XS_VENDOR_ID || is_xsigo_guid(node->sysimgguid))
		return xsigo_chassisguid(node);
	else
		return node->sysimgguid;
}

static struct ChassisList *find_chassisguid(Node *node)
{
	ChassisList *current;
	uint64_t chguid;

	chguid = get_chassisguid(node);
	for (current = mylist.first; current; current = current->next) {
		if (current->chassisguid == chguid)
			return current;
	}

	return NULL;
}

uint64_t get_chassis_guid(unsigned char chassisnum)
{
	ChassisList *chassis;

	chassis = find_chassisnum(chassisnum);
	if (chassis)
		return chassis->chassisguid;
	else
		return 0;
}

static int is_router(Node *node)
{
	return (node->devid == VTR_DEVID_IB_FC_ROUTER ||
		node->devid == VTR_DEVID_IB_IP_ROUTER);
}

static int is_spine_9096(Node *node)
{
	return (node->devid == VTR_DEVID_SFB4 ||
		node->devid == VTR_DEVID_SFB4_DDR);
}

static int is_spine_9288(Node *node)
{
	return (node->devid == VTR_DEVID_SFB12 ||
		node->devid == VTR_DEVID_SFB12_DDR);
}

static int is_spine_2004(Node *node)
{
	return (node->devid == VTR_DEVID_SFB2004);
}

static int is_spine_2012(Node *node)
{
	return (node->devid == VTR_DEVID_SFB2012);
}

static int is_spine(Node *node)
{
	return (is_spine_9096(node) || is_spine_9288(node) ||
		is_spine_2004(node) || is_spine_2012(node));
}

static int is_line_24(Node *node)
{
	return (node->devid == VTR_DEVID_SLB24 ||
		node->devid == VTR_DEVID_SLB24_DDR ||
		node->devid == VTR_DEVID_SRB2004);
}

static int is_line_8(Node *node)
{
	return (node->devid == VTR_DEVID_SLB8);
}

static int is_line_2024(Node *node)
{
	return (node->devid == VTR_DEVID_SLB2024);
}

static int is_line(Node *node)
{
	return (is_line_24(node) || is_line_8(node) || is_line_2024(node));
}

int is_chassis_switch(Node *node)
{
    return (is_spine(node) || is_line(node));
}

/* these structs help find Line (Anafa) slot number while using spine portnum */
int line_slot_2_sfb4[25]        = { 0, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4 };
int anafa_line_slot_2_sfb4[25]  = { 0, 1, 1, 1, 2, 2, 2, 1, 1, 1, 2, 2, 2, 1, 1, 1, 2, 2, 2, 1, 1, 1, 2, 2, 2 };
int line_slot_2_sfb12[25]       = { 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9,10, 10, 11, 11, 12, 12 };
int anafa_line_slot_2_sfb12[25] = { 0, 1, 2, 1, 2, 1, 2, 1, 2, 1, 2, 1, 2, 1, 2, 1, 2, 1, 2, 1, 2, 1, 2, 1, 2 };

/* IPR FCR modules connectivity while using sFB4 port as reference */
int ipr_slot_2_sfb4_port[25]    = { 0, 3, 2, 1, 3, 2, 1, 3, 2, 1, 3, 2, 1, 3, 2, 1, 3, 2, 1, 3, 2, 1, 3, 2, 1 };

/* these structs help find Spine (Anafa) slot number while using spine portnum */
int spine12_slot_2_slb[25]      = { 0, 1, 1, 1, 2, 2, 2, 3, 3, 3, 4, 4, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
int anafa_spine12_slot_2_slb[25]= { 0, 1, 2, 3, 1, 2, 3, 1, 2, 3, 1, 2, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
int spine4_slot_2_slb[25]       = { 0, 1, 1, 1, 2, 2, 2, 3, 3, 3, 4, 4, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
int anafa_spine4_slot_2_slb[25] = { 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
/*	reference                     { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24 }; */

static void get_sfb_slot(Node *node, Port *lineport)
{
	ChassisRecord *ch = node->chrecord;

	ch->chassisslot = SPINE_CS;
	if (is_spine_9096(node)) {
		ch->chassistype = ISR9096_CT;
		ch->slotnum = spine4_slot_2_slb[lineport->portnum];
		ch->anafanum = anafa_spine4_slot_2_slb[lineport->portnum];
	} else if (is_spine_9288(node)) {
		ch->chassistype = ISR9288_CT;
		ch->slotnum = spine12_slot_2_slb[lineport->portnum];
		ch->anafanum = anafa_spine12_slot_2_slb[lineport->portnum];
	} else if (is_spine_2012(node)) {
		ch->chassistype = ISR2012_CT;
		ch->slotnum = spine12_slot_2_slb[lineport->portnum];
		ch->anafanum = anafa_spine12_slot_2_slb[lineport->portnum];
	} else if (is_spine_2004(node)) {
		ch->chassistype = ISR2004_CT;
		ch->slotnum = spine4_slot_2_slb[lineport->portnum];
		ch->anafanum = anafa_spine4_slot_2_slb[lineport->portnum];
	} else {
		IBPANIC("Unexpected node found: guid 0x%016" PRIx64, node->nodeguid);
	}
}

static void get_router_slot(Node *node, Port *spineport)
{
	ChassisRecord *ch = node->chrecord;
	int guessnum = 0;

	if (!ch) {
		if (!(node->chrecord = calloc(1, sizeof(ChassisRecord))))
			IBPANIC("out of mem");
		ch = node->chrecord;
	}

	ch->chassisslot = SRBD_CS;
	if (is_spine_9096(spineport->node)) {
		ch->chassistype = ISR9096_CT;
		ch->slotnum = line_slot_2_sfb4[spineport->portnum];
		ch->anafanum = ipr_slot_2_sfb4_port[spineport->portnum];
	} else if (is_spine_9288(spineport->node)) {
		ch->chassistype = ISR9288_CT;
		ch->slotnum = line_slot_2_sfb12[spineport->portnum];
		/* this is a smart guess based on nodeguids order on sFB-12 module */
		guessnum = spineport->node->nodeguid % 4;
		/* module 1 <--> remote anafa 3 */
		/* module 2 <--> remote anafa 2 */
		/* module 3 <--> remote anafa 1 */
		ch->anafanum = (guessnum == 3 ? 1 : (guessnum == 1 ? 3 : 2));
	} else if (is_spine_2012(spineport->node)) {
		ch->chassistype = ISR2012_CT;
		ch->slotnum = line_slot_2_sfb12[spineport->portnum];
		/* this is a smart guess based on nodeguids order on sFB-12 module */
		guessnum = spineport->node->nodeguid % 4;
		// module 1 <--> remote anafa 3
		// module 2 <--> remote anafa 2
		// module 3 <--> remote anafa 1
		ch->anafanum = (guessnum == 3? 1 : (guessnum == 1 ? 3 : 2));
	} else if (is_spine_2004(spineport->node)) {
		ch->chassistype = ISR2004_CT;
		ch->slotnum = line_slot_2_sfb4[spineport->portnum];
		ch->anafanum = ipr_slot_2_sfb4_port[spineport->portnum];
	} else {
		IBPANIC("Unexpected node found: guid 0x%016" PRIx64, spineport->node->nodeguid);
	}
}

static void get_slb_slot(ChassisRecord *ch, Port *spineport)
{
	ch->chassisslot = LINE_CS;
	if (is_spine_9096(spineport->node)) {
		ch->chassistype = ISR9096_CT;
		ch->slotnum = line_slot_2_sfb4[spineport->portnum];
		ch->anafanum = anafa_line_slot_2_sfb4[spineport->portnum];
	} else if (is_spine_9288(spineport->node)) {
		ch->chassistype = ISR9288_CT;
		ch->slotnum = line_slot_2_sfb12[spineport->portnum];
		ch->anafanum = anafa_line_slot_2_sfb12[spineport->portnum];
	} else if (is_spine_2012(spineport->node)) {
		ch->chassistype = ISR2012_CT;
		ch->slotnum = line_slot_2_sfb12[spineport->portnum];
		ch->anafanum = anafa_line_slot_2_sfb12[spineport->portnum];
	} else if (is_spine_2004(spineport->node)) {
		ch->chassistype = ISR2004_CT;
		ch->slotnum = line_slot_2_sfb4[spineport->portnum];
		ch->anafanum = anafa_line_slot_2_sfb4[spineport->portnum];
	} else {
		IBPANIC("Unexpected node found: guid 0x%016" PRIx64, spineport->node->nodeguid);
	}
}

/*
	This function called for every Voltaire node in fabric
	It could be optimized so, but time overhead is very small
	and its only diag.util
*/
static void fill_chassis_record(Node *node)
{
	Port *port;
	Node *remnode = 0;
	ChassisRecord *ch = 0;

	if (node->chrecord) /* somehow this node has already been passed */
		return;

	if (!(node->chrecord = calloc(1, sizeof(ChassisRecord))))
		IBPANIC("out of mem");

	ch = node->chrecord;

	/* node is router only in case of using unique lid */
	/* (which is lid of chassis router port) */
	/* in such case node->ports is actually a requested port... */
	if (is_router(node) && is_spine(node->ports->remoteport->node))
		get_router_slot(node, node->ports->remoteport);
	else if (is_spine(node)) {
		for (port = node->ports; port; port = port->next) {
			if (!port->remoteport)
				continue;
			remnode = port->remoteport->node;
			if (remnode->type != SWITCH_NODE) {
				if (!remnode->chrecord)
					get_router_slot(remnode, port);
				continue;
			}
			if (!ch->chassistype)
				/* we assume here that remoteport belongs to line */
				get_sfb_slot(node, port->remoteport);

				/* we could break here, but need to find if more routers connected */
		}

	} else if (is_line(node)) {
		for (port = node->ports; port; port = port->next) {
			if (port->portnum > 12)
				continue;
			if (!port->remoteport)
				continue;
			/* we assume here that remoteport belongs to spine */
			get_slb_slot(ch, port->remoteport);
			break;
		}
	}

	return;
}

static int get_line_index(Node *node)
{
	int retval = 3 * (node->chrecord->slotnum - 1) + node->chrecord->anafanum;

	if (retval > LINES_MAX_NUM || retval < 1)
		IBPANIC("Internal error");
	return retval;
}

static int get_spine_index(Node *node)
{
	int retval;

	if (is_spine_9288(node) || is_spine_2012(node))
		retval = 3 * (node->chrecord->slotnum - 1) + node->chrecord->anafanum;
	else
		retval = node->chrecord->slotnum;

	if (retval > SPINES_MAX_NUM || retval < 1)
		IBPANIC("Internal error");
	return retval;
}

static void insert_line_router(Node *node, ChassisList *chassislist)
{
	int i = get_line_index(node);

	if (chassislist->linenode[i])
		return;		/* already filled slot */

	chassislist->linenode[i] = node;
	node->chrecord->chassisnum = chassislist->chassisnum;
}

static void insert_spine(Node *node, ChassisList *chassislist)
{
	int i = get_spine_index(node);

	if (chassislist->spinenode[i])
		return;		/* already filled slot */

	chassislist->spinenode[i] = node;
	node->chrecord->chassisnum = chassislist->chassisnum;
}

static void pass_on_lines_catch_spines(ChassisList *chassislist)
{
	Node *node, *remnode;
	Port *port;
	int i;

	for (i = 1; i <= LINES_MAX_NUM; i++) {
		node = chassislist->linenode[i];

		if (!(node && is_line(node)))
			continue;	/* empty slot or router */

		for (port = node->ports; port; port = port->next) {
			if (port->portnum > 12)
				continue;

			if (!port->remoteport)
				continue;
			remnode = port->remoteport->node;

			if (!remnode->chrecord)
				continue;	/* some error - spine not initialized ? FIXME */
			insert_spine(remnode, chassislist);
		}
	}
}

static void pass_on_spines_catch_lines(ChassisList *chassislist)
{
	Node *node, *remnode;
	Port *port;
	int i;

	for (i = 1; i <= SPINES_MAX_NUM; i++) {
		node = chassislist->spinenode[i];
		if (!node)
			continue;	/* empty slot */
		for (port = node->ports; port; port = port->next) {
			if (!port->remoteport)
				continue;
			remnode = port->remoteport->node;

			if (!remnode->chrecord)
				continue;	/* some error - line/router not initialized ? FIXME */
			insert_line_router(remnode, chassislist);
		}
	}
}

/*
	Stupid interpolation algorithm...
	But nothing to do - have to be compliant with VoltaireSM/NMS
*/
static void pass_on_spines_interpolate_chguid(ChassisList *chassislist)
{
	Node *node;
	int i;

	for (i = 1; i <= SPINES_MAX_NUM; i++) {
		node = chassislist->spinenode[i];
		if (!node)
			continue;	/* skip the empty slots */

		/* take first guid minus one to be consistent with SM */
		chassislist->chassisguid = node->nodeguid - 1;
		break;
	}
}

/*
	This function fills chassislist structure with all nodes
	in that chassis
	chassislist structure = structure of one standalone chassis
*/
static void build_chassis(Node *node, ChassisList *chassislist)
{
	Node *remnode = 0;
	Port *port = 0;

	/* we get here with node = chassis_spine */
	chassislist->chassistype = node->chrecord->chassistype;
	insert_spine(node, chassislist);

	/* loop: pass on all ports of node */
	for (port = node->ports; port; port = port->next) {
		if (!port->remoteport)
			continue;
		remnode = port->remoteport->node;

		if (!remnode->chrecord)
			continue; /* some error - line or router not initialized ? FIXME */

		insert_line_router(remnode, chassislist);
	}

	pass_on_lines_catch_spines(chassislist);
	/* this pass needed for to catch routers, since routers connected only */
	/* to spines in slot 1 or 4 and we could miss them first time */
	pass_on_spines_catch_lines(chassislist);

	/* additional 2 passes needed for to overcome a problem of pure "in-chassis" */
	/* connectivity - extra pass to ensure that all related chips/modules */
	/* inserted into the chassislist */
	pass_on_lines_catch_spines(chassislist);
	pass_on_spines_catch_lines(chassislist);
	pass_on_spines_interpolate_chguid(chassislist);
}

/*========================================================*/
/*                INTERNAL TO EXTERNAL PORT MAPPING       */
/*========================================================*/

/*
Description : On ISR9288/9096 external ports indexing
              is not matching the internal ( anafa ) port
              indexes. Use this MAP to translate the data you get from
              the OpenIB diagnostics (smpquery, ibroute, ibtracert, etc.)


Module : sLB-24
                anafa 1             anafa 2
ext port | 13 14 15 16 17 18 | 19 20 21 22 23 24
int port | 22 23 24 18 17 16 | 22 23 24 18 17 16
ext port | 1  2  3  4  5  6  | 7  8  9  10 11 12
int port | 19 20 21 15 14 13 | 19 20 21 15 14 13
------------------------------------------------

Module : sLB-8
                anafa 1             anafa 2
ext port | 13 14 15 16 17 18 | 19 20 21 22 23 24
int port | 24 23 22 18 17 16 | 24 23 22 18 17 16
ext port | 1  2  3  4  5  6  | 7  8  9  10 11 12
int port | 21 20 19 15 14 13 | 21 20 19 15 14 13

----------->
                anafa 1             anafa 2
ext port | -  -  5  -  -  6  | -  -  7  -  -  8
int port | 24 23 22 18 17 16 | 24 23 22 18 17 16
ext port | -  -  1  -  -  2  | -  -  3  -  -  4
int port | 21 20 19 15 14 13 | 21 20 19 15 14 13
------------------------------------------------

Module : sLB-2024

ext port | 13 14 15 16 17 18 19 20 21 22 23 24
A1 int port| 13 14 15 16 17 18 19 20 21 22 23 24
ext port | 1 2 3 4 5 6 7 8 9 10 11 12
A2 int port| 13 14 15 16 17 18 19 20 21 22 23 24
---------------------------------------------------

*/

int int2ext_map_slb24[2][25] = {
					{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 6, 5, 4, 18, 17, 16, 1, 2, 3, 13, 14, 15 },
					{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 12, 11, 10, 24, 23, 22, 7, 8, 9, 19, 20, 21 }
				};
int int2ext_map_slb8[2][25] = {
					{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 2, 2, 6, 6, 6, 1, 1, 1, 5, 5, 5 },
					{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4, 4, 4, 8, 8, 8, 3, 3, 3, 7, 7, 7 }
				};
int int2ext_map_slb2024[2][25] = {
					{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24 },
					{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12 }
				};
/*	reference			{ 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24 }; */

/*
	This function relevant only for line modules/chips
	Returns string with external port index
*/
char *portmapstring(Port *port)
{
	static char mapping[OUT_BUFFER_SIZE];
	ChassisRecord *ch = port->node->chrecord;
	int portnum = port->portnum;
	int chipnum = 0;
	int pindex = 0;
	Node *node = port->node;

	if (!ch || !is_line(node) || (portnum < 13 || portnum > 24))
		return NULL;

	if (ch->anafanum < 1 || ch->anafanum > 2)
		return NULL;

	memset(mapping, 0, sizeof(mapping));

	chipnum = ch->anafanum - 1;

	if (is_line_24(node))
		pindex = int2ext_map_slb24[chipnum][portnum];
	else if (is_line_2024(node))
		pindex = int2ext_map_slb2024[chipnum][portnum];
	else
		pindex = int2ext_map_slb8[chipnum][portnum];

	sprintf(mapping, "[ext %d]", pindex);

	return mapping;
}

static void add_chassislist()
{
	if (!(mylist.current = calloc(1, sizeof(ChassisList))))
		IBPANIC("out of mem");

	if (mylist.first == NULL) {
		mylist.first = mylist.current;
		mylist.last = mylist.current;
	} else {
		mylist.last->next = mylist.current;
		mylist.current->next = NULL;
		mylist.last = mylist.current;
	}
}

/*
	Main grouping function
	Algorithm:
	1. pass on every Voltaire node
	2. catch spine chip for every Voltaire node
		2.1 build/interpolate chassis around this chip
		2.2 go to 1.
	3. pass on non Voltaire nodes (SystemImageGUID based grouping)
	4. now group non Voltaire nodes by SystemImageGUID
*/
ChassisList *group_nodes()
{
	Node *node;
	int dist;
	int chassisnum = 0;
	struct ChassisList *chassis;

	mylist.first = NULL;
	mylist.current = NULL;
	mylist.last = NULL;

	/* first pass on switches and build for every Voltaire node */
	/* an appropriate chassis record (slotnum and position) */
	/* according to internal connectivity */
	/* not very efficient but clear code so... */
	for (dist = 0; dist <= maxhops_discovered; dist++) {
		for (node = nodesdist[dist]; node; node = node->dnext) {
			if (node->vendid == VTR_VENDOR_ID)
				fill_chassis_record(node);
		}
	}

	/* separate every Voltaire chassis from each other and build linked list of them */
	/* algorithm: catch spine and find all surrounding nodes */
	for (dist = 0; dist <= maxhops_discovered; dist++) {
		for (node = nodesdist[dist]; node; node = node->dnext) {
			if (node->vendid != VTR_VENDOR_ID)
				continue;
			if (!node->chrecord || node->chrecord->chassisnum || !is_spine(node))
				continue;
			add_chassislist();
			mylist.current->chassisnum = ++chassisnum;
			build_chassis(node, mylist.current);
		}
	}

	/* now make pass on nodes for chassis which are not Voltaire */
	/* grouped by common SystemImageGUID */
	for (dist = 0; dist <= maxhops_discovered; dist++) {
		for (node = nodesdist[dist]; node; node = node->dnext) {
			if (node->vendid == VTR_VENDOR_ID)
				continue;
			if (node->sysimgguid) {
				chassis = find_chassisguid(node);
				if (chassis)
					chassis->nodecount++;
				else {
					/* Possible new chassis */
					add_chassislist();
					mylist.current->chassisguid = get_chassisguid(node);
					mylist.current->nodecount = 1;
				}
			}
		}
	}

	/* now, make another pass to see which nodes are part of chassis */
	/* (defined as chassis->nodecount > 1) */
	for (dist = 0; dist <= MAXHOPS; ) {
		for (node = nodesdist[dist]; node; node = node->dnext) {
			if (node->vendid == VTR_VENDOR_ID)
				continue;
			if (node->sysimgguid) {
				chassis = find_chassisguid(node);
				if (chassis && chassis->nodecount > 1) {
					if (!chassis->chassisnum)
						chassis->chassisnum = ++chassisnum;
					if (!node->chrecord) {
						if (!(node->chrecord = calloc(1, sizeof(ChassisRecord))))
							IBPANIC("out of mem");
						node->chrecord->chassisnum = chassis->chassisnum;
					}
				}
			}
		}
		if (dist == maxhops_discovered)
			dist = MAXHOPS;	/* skip to CAs */
		else
			dist++;
	}

	return (mylist.first);
}
