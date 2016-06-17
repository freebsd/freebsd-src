/* $Id$
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 1992-1997,2000-2003 Silicon Graphics, Inc.  All Rights Reserved.
 */

#ifndef _ASM_SN_KSYS_L1_H
#define _ASM_SN_KSYS_L1_H

#include <linux/config.h>
#include <asm/sn/vector.h>
#include <asm/sn/addrs.h>
#include <asm/atomic.h>
#include <asm/sn/sv.h>

/* L1 Target Addresses */
/*
 * L1 commands and responses use source/target addresses that are
 * 32 bits long.  These are broken up into multiple bitfields that
 * specify the type of the target controller (could actually be L2
 * L3, not just L1), the rack and bay of the target, and the task
 * id (L1 functionality is divided into several independent "tasks"
 * that can each receive command requests and transmit responses)
 */
#define L1_ADDR_TYPE_L1		0x00	/* L1 system controller */
#define L1_ADDR_TYPE_L2		0x01	/* L2 system controller */
#define L1_ADDR_TYPE_L3		0x02	/* L3 system controller */
#define L1_ADDR_TYPE_CBRICK	0x03	/* attached C brick	*/
#define L1_ADDR_TYPE_IOBRICK	0x04	/* attached I/O brick	*/
#define L1_ADDR_TASK_SHFT	0
#define L1_ADDR_TASK_MASK	0x0000001F
#define L1_ADDR_TASK_INVALID	0x00	/* invalid task 	*/
#define	L1_ADDR_TASK_IROUTER	0x01	/* iRouter		*/
#define L1_ADDR_TASK_SYS_MGMT	0x02	/* system management port */
#define L1_ADDR_TASK_CMD	0x03	/* command interpreter	*/
#define L1_ADDR_TASK_ENV	0x04	/* environmental monitor */
#define L1_ADDR_TASK_BEDROCK	0x05	/* bedrock		*/
#define L1_ADDR_TASK_GENERAL	0x06	/* general requests	*/

#define L1_ADDR_LOCAL				\
    (L1_ADDR_TYPE_L1 << L1_ADDR_TYPE_SHFT) |	\
    (L1_ADDR_RACK_LOCAL << L1_ADDR_RACK_SHFT) |	\
    (L1_ADDR_BAY_LOCAL << L1_ADDR_BAY_SHFT)

#define L1_ADDR_LOCALIO					\
    (L1_ADDR_TYPE_IOBRICK << L1_ADDR_TYPE_SHFT) |	\
    (L1_ADDR_RACK_LOCAL << L1_ADDR_RACK_SHFT) |		\
    (L1_ADDR_BAY_LOCAL << L1_ADDR_BAY_SHFT)

#define L1_ADDR_LOCAL_SHFT	L1_ADDR_BAY_SHFT

/* response argument types */
#define L1_ARG_INT		0x00	/* 4-byte integer (big-endian)	*/
#define L1_ARG_ASCII		0x01	/* null-terminated ASCII string */
#define L1_ARG_UNKNOWN		0x80	/* unknown data type.  The low
					 * 7 bits will contain the data
					 * length.			*/

/* response codes */
#define L1_RESP_OK	    0	/* no problems encountered      */
#define L1_RESP_IROUTER	(-  1)	/* iRouter error	        */
#define L1_RESP_ARGC	(-100)	/* arg count mismatch	        */
#define L1_RESP_REQC	(-101)	/* bad request code	        */
#define L1_RESP_NAVAIL	(-104)	/* requested data not available */
#define L1_RESP_ARGVAL	(-105)  /* arg value out of range       */
#define L1_RESP_INVAL   (-107)  /* requested data invalid       */

/* L1 general requests */

/* request codes */
#define	L1_REQ_RDBG		0x0001	/* read debug switches	*/
#define L1_REQ_RRACK		0x0002	/* read brick rack & bay */
#define L1_REQ_RRBT		0x0003  /* read brick rack, bay & type */
#define L1_REQ_SER_NUM		0x0004  /* read brick serial number */
#define L1_REQ_FW_REV		0x0005  /* read L1 firmware revision */
#define L1_REQ_EEPROM		0x0006  /* read EEPROM info */
#define L1_REQ_EEPROM_FMT	0x0007  /* get EEPROM data format & size */
#define L1_REQ_SYS_SERIAL	0x0008	/* read system serial number */
#define L1_REQ_PARTITION_GET	0x0009	/* read partition id */
#define L1_REQ_PORTSPEED	0x000a	/* get ioport speed */

#define L1_REQ_CONS_SUBCH	0x1002  /* select this node's console 
					   subchannel */
#define L1_REQ_CONS_NODE	0x1003  /* volunteer to be the master 
					   (console-hosting) node */
#define L1_REQ_DISP1		0x1004  /* write line 1 of L1 display */
#define L1_REQ_DISP2		0x1005  /* write line 2 of L1 display */
#define L1_REQ_PARTITION_SET	0x1006	/* set partition id */
#define L1_REQ_EVENT_SUBCH	0x1007	/* set the subchannel for system
					   controller event transmission */

#define L1_REQ_RESET		0x2000	/* request a full system reset */
#define L1_REQ_PCI_UP		0x2001  /* power up pci slot or bus */
#define L1_REQ_PCI_DOWN		0x2002  /* power down pci slot or bus */
#define L1_REQ_PCI_RESET	0x2003  /* reset pci bus or slot */

/* L1 command interpreter requests */

/* request codes */
#define L1_REQ_EXEC_CMD		0x0000	/* interpret and execute an ASCII
					   command string */

/* brick type response codes */
#define L1_BRICKTYPE_PX         0x23            /* # */
#define L1_BRICKTYPE_PE         0x25            /* % */
#define L1_BRICKTYPE_N_p0       0x26            /* & */
#define L1_BRICKTYPE_IP45       0x34            /* 4 */
#define L1_BRICKTYPE_IP41       0x35            /* 5 */
#define L1_BRICKTYPE_TWISTER    0x36            /* 6 */ /* IP53 & ROUTER */
#define L1_BRICKTYPE_IX         0x3d            /* = */
#define L1_BRICKTYPE_IP34       0x61            /* a */
#define L1_BRICKTYPE_C          0x63            /* c */
#define L1_BRICKTYPE_I          0x69            /* i */
#define L1_BRICKTYPE_N          0x6e            /* n */
#define L1_BRICKTYPE_OPUS       0x6f		/* o */
#define L1_BRICKTYPE_P          0x70            /* p */
#define L1_BRICKTYPE_R          0x72            /* r */
#define L1_BRICKTYPE_CHI_CG     0x76            /* v */
#define L1_BRICKTYPE_X          0x78            /* x */
#define L1_BRICKTYPE_X2         0x79            /* y */

/* EEPROM codes (for the "read EEPROM" request) */
/* c brick */
#define L1_EEP_NODE		0x00	/* node board */
#define L1_EEP_PIMM0		0x01
#define L1_EEP_PIMM(x)		(L1_EEP_PIMM0+(x))
#define L1_EEP_DIMM0		0x03
#define L1_EEP_DIMM(x)		(L1_EEP_DIMM0+(x))

/* other brick types */
#define L1_EEP_POWER		0x00	/* power board */
#define L1_EEP_LOGIC		0x01	/* logic board */

/* info area types */
#define L1_EEP_CHASSIS		1	/* chassis info area */
#define L1_EEP_BOARD		2	/* board info area */
#define L1_EEP_IUSE		3	/* internal use area */
#define L1_EEP_SPD		4	/* serial presence detect record */

typedef uint32_t l1addr_t;

#define L1_BUILD_ADDR(addr,at,r,s,t)					\
    (*(l1addr_t *)(addr) = ((l1addr_t)(at) << L1_ADDR_TYPE_SHFT) |	\
			     ((l1addr_t)(r)  << L1_ADDR_RACK_SHFT) |	\
			     ((l1addr_t)(s)  << L1_ADDR_BAY_SHFT) |	\
			     ((l1addr_t)(t)  << L1_ADDR_TASK_SHFT))

#define L1_ADDRESS_TO_TASK(addr,trb,tsk)				\
    (*(l1addr_t *)(addr) = (l1addr_t)(trb) |				\
    			     ((l1addr_t)(tsk) << L1_ADDR_TASK_SHFT))

#define L1_DISPLAY_LINE_LENGTH	12	/* L1 display characters/line */

#ifdef L1_DISP_2LINES
#define L1_DISPLAY_LINES	2	/* number of L1 display lines */
#else
#define L1_DISPLAY_LINES	1	/* number of L1 display lines available
					 * to system software */
#endif

#define bzero(d, n)	memset((d), 0, (n))

int	elsc_display_line(nasid_t nasid, char *line, int lnum);
int	iobrick_rack_bay_type_get( nasid_t nasid, uint *rack,
				   uint *bay, uint *brick_type );
int	iobrick_module_get( nasid_t nasid );


#endif /* _ASM_SN_KSYS_L1_H */
