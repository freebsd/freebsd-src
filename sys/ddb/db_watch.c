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
 *	$Id: db_watch.c,v 1.3 1993/11/25 01:30:15 wollman Exp $
 */

/*
 * 	Author: Richard P. Draves, Carnegie Mellon University
 *	Date:	10/90
 */

#include "param.h"
#include "systm.h"
#include "proc.h"
#include "ddb/ddb.h"

#include <vm/vm_map.h>
#include <ddb/db_lex.h>
#include <ddb/db_watch.h>
#include <ddb/db_access.h>
#include <ddb/db_sym.h>

/*
 * Watchpoints.
 */

extern boolean_t db_map_equal();
extern boolean_t db_map_current();
extern vm_map_t db_map_addr();

boolean_t	db_watchpoints_inserted = TRUE;

#define	NWATCHPOINTS	100
struct db_watchpoint	db_watch_table[NWATCHPOINTS];
db_watchpoint_t		db_next_free_watchpoint = &db_watch_table[0];
db_watchpoint_t		db_free_watchpoints = 0;
db_watchpoint_t		db_watchpoint_list = 0;

db_watchpoint_t
db_watchpoint_alloc()
{
	register db_watchpoint_t	watch;

	if ((watch = db_free_watchpoints) != 0) {
	    db_free_watchpoints = watch->link;
	    return (watch);
	}
	if (db_next_free_watchpoint == &db_watch_table[NWATCHPOINTS]) {
	    db_printf("All watchpoints used.\n");
	    return (0);
	}
	watch = db_next_free_watchpoint;
	db_next_free_watchpoint++;

	return (watch);
}

void
db_watchpoint_free(watch)
	register db_watchpoint_t	watch;
{
	watch->link = db_free_watchpoints;
	db_free_watchpoints = watch;
}

void
db_set_watchpoint(map, addr, size)
	vm_map_t	map;
	db_addr_t	addr;
	vm_size_t	size;
{
	register db_watchpoint_t	watch;

	if (map == NULL) {
	    db_printf("No map.\n");
	    return;
	}

	/*
	 *	Should we do anything fancy with overlapping regions?
	 */

	for (watch = db_watchpoint_list;
	     watch != 0;
	     watch = watch->link)
	    if (db_map_equal(watch->map, map) &&
		(watch->loaddr == addr) &&
		(watch->hiaddr == addr+size)) {
		db_printf("Already set.\n");
		return;
	    }

	watch = db_watchpoint_alloc();
	if (watch == 0) {
	    db_printf("Too many watchpoints.\n");
	    return;
	}

	watch->map = map;
	watch->loaddr = addr;
	watch->hiaddr = addr+size;

	watch->link = db_watchpoint_list;
	db_watchpoint_list = watch;

	db_watchpoints_inserted = FALSE;
}

void
db_delete_watchpoint(map, addr)
	vm_map_t	map;
	db_addr_t	addr;
{
	register db_watchpoint_t	watch;
	register db_watchpoint_t	*prev;

	for (prev = &db_watchpoint_list;
	     (watch = *prev) != 0;
	     prev = &watch->link)
	    if (db_map_equal(watch->map, map) &&
		(watch->loaddr <= addr) &&
		(addr < watch->hiaddr)) {
		*prev = watch->link;
		db_watchpoint_free(watch);
		return;
	    }

	db_printf("Not set.\n");
}

void
db_list_watchpoints()
{
	register db_watchpoint_t	watch;

	if (db_watchpoint_list == 0) {
	    db_printf("No watchpoints set\n");
	    return;
	}

	db_printf(" Map        Address  Size\n");
	for (watch = db_watchpoint_list;
	     watch != 0;
	     watch = watch->link)
	    db_printf("%s%8x  %8x  %x\n",
		      db_map_current(watch->map) ? "*" : " ",
		      watch->map, watch->loaddr,
		      watch->hiaddr - watch->loaddr);
}

/* Delete watchpoint */
/*ARGSUSED*/
void
db_deletewatch_cmd(addr, have_addr, count, modif)
	db_expr_t	addr;
	int		have_addr;
	db_expr_t	count;
	char *		modif;
{
	db_delete_watchpoint(db_map_addr(addr), addr);
}

/* Set watchpoint */
/*ARGSUSED*/
void
db_watchpoint_cmd(addr, have_addr, count, modif)
	db_expr_t	addr;
	int		have_addr;
	db_expr_t	count;
	char *		modif;
{
	vm_size_t	size;
	db_expr_t	value;

	if (db_expression(&value))
	    size = (vm_size_t) value;
	else
	    size = 4;
	db_skip_to_eol();

	db_set_watchpoint(db_map_addr(addr), addr, size);
}

/* list watchpoints */
void
db_listwatch_cmd(db_expr_t dummy1, int dummy2, db_expr_t dummy3, char *dummmy4)
{
	db_list_watchpoints();
}

void
db_set_watchpoints()
{
	register db_watchpoint_t	watch;

	if (!db_watchpoints_inserted) {
	    for (watch = db_watchpoint_list;
	         watch != 0;
	         watch = watch->link)
		pmap_protect(watch->map->pmap,
			     trunc_page(watch->loaddr),
			     round_page(watch->hiaddr),
			     VM_PROT_READ);

	    db_watchpoints_inserted = TRUE;
	}
}

void
db_clear_watchpoints()
{
	db_watchpoints_inserted = FALSE;
}

boolean_t
db_find_watchpoint(map, addr, regs)
	vm_map_t	map;
	db_addr_t	addr;
	db_regs_t	*regs;
{
	register db_watchpoint_t watch;
	db_watchpoint_t found = 0;

	for (watch = db_watchpoint_list;
	     watch != 0;
	     watch = watch->link)
	    if (db_map_equal(watch->map, map)) {
		if ((watch->loaddr <= addr) &&
		    (addr < watch->hiaddr))
		    return (TRUE);
		else if ((trunc_page(watch->loaddr) <= addr) &&
			 (addr < round_page(watch->hiaddr)))
		    found = watch;
	    }

	/*
	 *	We didn't hit exactly on a watchpoint, but we are
	 *	in a protected region.  We want to single-step
	 *	and then re-protect.
	 */

	if (found) {
	    db_watchpoints_inserted = FALSE;
	    db_single_step(regs);
	}

	return (FALSE);
}
