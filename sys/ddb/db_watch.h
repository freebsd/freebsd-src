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
 *
 *	$Id: db_watch.h,v 1.2 1993/10/16 16:47:33 rgrimes Exp $
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
