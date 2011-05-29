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

#ifndef _IBNETDISCOVER_H_
#define _IBNETDISCOVER_H_

#define MAXHOPS		63

#define CA_NODE		1
#define SWITCH_NODE	2
#define ROUTER_NODE	3

#define LIST_CA_NODE	 (1 << CA_NODE)
#define LIST_SWITCH_NODE (1 << SWITCH_NODE)
#define LIST_ROUTER_NODE (1 << ROUTER_NODE)

/* Vendor IDs (for chassis based systems) */
#define VTR_VENDOR_ID			0x8f1	/* Voltaire */
#define TS_VENDOR_ID			0x5ad	/* Cisco */
#define SS_VENDOR_ID			0x66a	/* InfiniCon */
#define XS_VENDOR_ID			0x1397	/* Xsigo */


typedef struct Port Port;
typedef struct Node Node;
typedef struct ChassisRecord ChassisRecord;

struct ChassisRecord {
	ChassisRecord *next;

	unsigned char chassisnum;
	unsigned char anafanum;
	unsigned char slotnum;
	unsigned char chassistype;
	unsigned char chassisslot;
};

struct Port {
	Port *next;
	uint64_t portguid;
	int portnum;
	int lid;
	int lmc;
	int state;
	int physstate;
	int linkwidth;
	int linkspeed;

	Node *node;
	Port *remoteport;		/* null if SMA */
};

struct Node {
	Node *htnext;
	Node *dnext;
	Port *ports;
	ib_portid_t path;
	int type;
	int dist;
	int numports;
	int localport;
	int smalid;
	int smalmc;
	int smaenhsp0;
	uint32_t devid;
	uint32_t vendid;
	uint64_t sysimgguid;
	uint64_t nodeguid;
	uint64_t portguid;
	char nodedesc[64];
	uint8_t nodeinfo[64];

	ChassisRecord *chrecord;
};

#endif	/* _IBNETDISCOVER_H_ */
