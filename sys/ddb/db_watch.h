/* 
 * Mach Operating System
 * Copyright (c) 1991,1990 Carnegie Mellon University
 * All Rights Reserved.
 * 
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 * 
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS 
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND FOR
 * ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 * 
 * Carnegie Mellon requests users of this software to return to
 * 
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 * 
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 */
/*
 * HISTORY
 * $Log: db_watch.h,v $
 * Revision 1.1  1992/03/25  21:45:40  pace
 * Initial revision
 *
 * Revision 2.3  91/02/05  17:07:31  mrt
 * 	Changed to new Mach copyright
 * 	[91/01/31  16:20:09  mrt]
 * 
 * Revision 2.2  90/10/25  14:44:21  rwd
 * 	Generalized the watchpoint support.
 * 	[90/10/16            rwd]
 * 	Created.
 * 	[90/10/16            rpd]
 * 
 */
/*
 * 	Author: David B. Golub, Carnegie Mellon University
 *	Date:	10/90
 */

#ifndef	_DDB_DB_WATCH_
#define	_DDB_DB_WATCH_

#include <vm/vm_map.h>
#include <machine/db_machdep.h>

/*
 * Watchpoint.
 */

typedef struct db_watchpoint {
	vm_map_t map;			/* in this map */
	db_addr_t loaddr;		/* from this address */
	db_addr_t hiaddr;		/* to this address */
	struct db_watchpoint *link;	/* link in in-use or free chain */
} *db_watchpoint_t;

extern boolean_t db_find_watchpoint(/* vm_map_t map, db_addr_t addr,
				     db_regs_t *regs */);
extern void db_set_watchpoints();
extern void db_clear_watchpoints();

extern void db_set_watchpoint(/* vm_map_t map, db_addr_t addr, vm_size_t size */);
extern void db_delete_watchpoint(/* vm_map_t map, db_addr_t addr */);
extern void db_list_watchpoints();

#endif	_DDB_DB_WATCH_
