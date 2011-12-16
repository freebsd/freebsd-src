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

#ifndef _GROUPING_H_
#define _GROUPING_H_

/*========================================================*/
/*               FABRIC SCANNER SPECIFIC DATA             */
/*========================================================*/

#define SPINES_MAX_NUM 12
#define LINES_MAX_NUM 36

typedef struct ChassisList ChassisList;
typedef struct AllChassisList AllChassisList;

struct ChassisList {
	ChassisList *next;
	uint64_t chassisguid;
	int chassisnum;
	int chassistype;
	int nodecount;		/* used for grouping by SystemImageGUID */
	Node *spinenode[SPINES_MAX_NUM + 1];
	Node *linenode[LINES_MAX_NUM + 1];
};

struct AllChassisList {
	ChassisList *first;
	ChassisList *current;
	ChassisList *last;
};

/*========================================================*/
/*                CHASSIS RECOGNITION SPECIFIC DATA       */
/*========================================================*/

/* Device IDs */
#define VTR_DEVID_IB_FC_ROUTER		0x5a00
#define VTR_DEVID_IB_IP_ROUTER		0x5a01
#define VTR_DEVID_ISR9600_SPINE		0x5a02
#define VTR_DEVID_ISR9600_LEAF		0x5a03
#define VTR_DEVID_HCA1			0x5a04
#define VTR_DEVID_HCA2			0x5a44
#define VTR_DEVID_HCA3			0x6278
#define VTR_DEVID_SW_6IB4		0x5a05
#define VTR_DEVID_ISR9024		0x5a06
#define VTR_DEVID_ISR9288		0x5a07
#define VTR_DEVID_SLB24			0x5a09
#define VTR_DEVID_SFB12			0x5a08
#define VTR_DEVID_SFB4			0x5a0b
#define VTR_DEVID_ISR9024_12		0x5a0c
#define VTR_DEVID_SLB8			0x5a0d
#define VTR_DEVID_RLX_SWITCH_BLADE	0x5a20
#define VTR_DEVID_ISR9024_DDR		0x5a31
#define VTR_DEVID_SFB12_DDR		0x5a32
#define VTR_DEVID_SFB4_DDR		0x5a33
#define VTR_DEVID_SLB24_DDR		0x5a34
#define VTR_DEVID_SFB2012		0x5a37
#define VTR_DEVID_SLB2024		0x5a38
#define VTR_DEVID_ISR2012		0x5a39
#define VTR_DEVID_SFB2004		0x5a40
#define VTR_DEVID_ISR2004		0x5a41
#define VTR_DEVID_SRB2004		0x5a42

enum ChassisType { UNRESOLVED_CT, ISR9288_CT, ISR9096_CT, ISR2012_CT, ISR2004_CT };
enum ChassisSlot { UNRESOLVED_CS, LINE_CS, SPINE_CS, SRBD_CS };

/*========================================================*/
/*                External interface                      */
/*========================================================*/

ChassisList *group_nodes();
char *portmapstring(Port *port);
char *get_chassis_type(unsigned char chassistype);
char *get_chassis_slot(unsigned char chassisslot);
uint64_t get_chassis_guid(unsigned char chassisnum);

int is_xsigo_guid(uint64_t guid);
int is_xsigo_tca(uint64_t guid);
int is_xsigo_hca(uint64_t guid);

#endif	/* _GROUPING_H_ */
