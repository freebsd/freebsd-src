/* $Id$
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 1992-1997,2000-2003 Silicon Graphics, Inc. All rights reserved.
 */
#ifndef _ASM_IA64_SN_CDL_H
#define _ASM_IA64_SN_CDL_H

#ifdef __KERNEL__
#include <asm/sn/sgi.h>
#endif

struct cdl {
	int part_num;			/* Part part number */
	int mfg_num;			/* Part MFG number */
	int (*attach)(vertex_hdl_t);	/* Attach routine */
};


/*
 *	cdl: connection/driver list
 *
 *	support code for bus infrastructure for busses
 *	that have self-identifying devices; initially
 *	constructed for xtalk, pciio and gioio modules.
 */
typedef struct cdl     *cdl_p;

/*
 *	cdl_add_connpt: add a connection point
 *
 *	Calls the attach routines of all the drivers on
 *	the list that match this connection point, in
 *	the order that they were added to the list.
 */
extern int		cdl_add_connpt(int key1,
				       int key2,
				       vertex_hdl_t conn,
				       int drv_flags);
#endif /* _ASM_IA64_SN_CDL_H */
