/*
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (c) 1992-1997,2001-2003 Silicon Graphics, Inc. All rights reserved.
 */

#ifndef _ASM_IA64_SN_SN2_SLOTNUM_H
#define _ASM_IA64_SN_SN2_SLOTNUM_H

#define SLOTNUM_MAXLENGTH	16

/*
 * This file defines IO widget to slot/device assignments.
 */


/* This determines module to pnode mapping. */

#define NODESLOTS_PER_MODULE		1
#define NODESLOTS_PER_MODULE_SHFT	1

#define SLOTNUM_NODE_CLASS	0x00	/* Node   */
#define SLOTNUM_ROUTER_CLASS	0x10	/* Router */
#define SLOTNUM_XTALK_CLASS	0x20	/* Xtalk  */
#define SLOTNUM_MIDPLANE_CLASS	0x30	/* Midplane */
#define SLOTNUM_XBOW_CLASS	0x40	/* Xbow  */
#define SLOTNUM_KNODE_CLASS	0x50	/* Kego node */
#define SLOTNUM_PCI_CLASS	0x60	/* PCI widgets on XBridge */
#define SLOTNUM_INVALID_CLASS	0xf0	/* Invalid */

#define SLOTNUM_CLASS_MASK	0xf0
#define SLOTNUM_SLOT_MASK	0x0f

#define SLOTNUM_GETCLASS(_sn)	((_sn) & SLOTNUM_CLASS_MASK)
#define SLOTNUM_GETSLOT(_sn)	((_sn) & SLOTNUM_SLOT_MASK)


#endif /* _ASM_IA64_SN_SN2_SLOTNUM_H */
