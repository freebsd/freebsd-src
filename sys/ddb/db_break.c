/*-
 * SPDX-License-Identifier: MIT-CMU
 *
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
 */
/*
 *	Author: David B. Golub, Carnegie Mellon University
 *	Date:	7/90
 */
/*
 * Breakpoints.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kdb.h>

#include <vm/vm.h>
#include <vm/vm_kern.h>

#include <machine/kdb.h>

#include <ddb/ddb.h>
#include <ddb/db_break.h>
#include <ddb/db_access.h>
#include <ddb/db_sym.h>

struct db_breakpoint_type {
	db_breakpoint_t		db_next_free_breakpoint;
	db_breakpoint_t		db_breakpoint_limit;
	db_breakpoint_t		db_free_breakpoints;
	db_breakpoint_t		db_breakpoint_list;
};

#define	NBREAKPOINTS	100
static struct db_breakpoint	db_break_table[NBREAKPOINTS];

static struct db_breakpoint_type db_breakpoint = {
	.db_next_free_breakpoint = &db_break_table[0],
	.db_breakpoint_limit = &db_break_table[NBREAKPOINTS],
	.db_free_breakpoints = NULL,
	.db_breakpoint_list = NULL,
};

#ifdef HAS_HW_BREAKPOINT
static struct db_breakpoint	db_hbreak_table[NHBREAKPOINTS];

static struct db_breakpoint_type db_hbreakpoint = {
	.db_next_free_breakpoint = &db_hbreak_table[0],
	.db_breakpoint_limit = &db_hbreak_table[NHBREAKPOINTS],
	.db_free_breakpoints = NULL,
	.db_breakpoint_list = NULL,
};
#endif

static db_breakpoint_t	db_breakpoint_alloc(
	struct db_breakpoint_type *bkpt_type);
static void	db_breakpoint_free(struct db_breakpoint_type *bkpt_typ,
	db_breakpoint_t bkpt);
static void	db_delete_breakpoint(struct db_breakpoint_type *bkpt_type,
	vm_map_t map, db_addr_t addr);
static db_breakpoint_t	db_find_breakpoint(struct db_breakpoint_type *bkpt_type,
	vm_map_t map, db_addr_t addr);
static void	db_list_breakpoints(void);
static bool	db_set_breakpoint(struct db_breakpoint_type *bkpt_type,
	vm_map_t map, db_addr_t addr, int count);

static db_breakpoint_t
db_breakpoint_alloc(struct db_breakpoint_type *bkpt_type)
{
	register db_breakpoint_t	bkpt;

	if ((bkpt = bkpt_type->db_free_breakpoints) != 0) {
	    bkpt_type->db_free_breakpoints = bkpt->link;
	    return (bkpt);
	}
	if (bkpt_type->db_next_free_breakpoint ==
	    bkpt_type->db_breakpoint_limit) {
	    db_printf("All breakpoints used.\n");
	    return (0);
	}
	bkpt = bkpt_type->db_next_free_breakpoint;
	bkpt_type->db_next_free_breakpoint++;

	return (bkpt);
}

static void
db_breakpoint_free(struct db_breakpoint_type *bkpt_type, db_breakpoint_t bkpt)
{
	bkpt->link = bkpt_type->db_free_breakpoints;
	bkpt_type->db_free_breakpoints = bkpt;
}

static bool
db_set_breakpoint(struct db_breakpoint_type *bkpt_type, vm_map_t map,
    db_addr_t addr, int count)
{
	register db_breakpoint_t	bkpt;

	if (db_find_breakpoint(bkpt_type, map, addr)) {
	    db_printf("Already set.\n");
	    return (false);
	}

	bkpt = db_breakpoint_alloc(bkpt_type);
	if (bkpt == 0) {
	    db_printf("Too many breakpoints.\n");
	    return (false);
	}

	bkpt->map = map;
	bkpt->address = addr;
	bkpt->flags = 0;
	bkpt->init_count = count;
	bkpt->count = count;

	bkpt->link = bkpt_type->db_breakpoint_list;
	bkpt_type->db_breakpoint_list = bkpt;

	return (true);
}

static void
db_delete_breakpoint(struct db_breakpoint_type *bkpt_type, vm_map_t map,
    db_addr_t addr)
{
	register db_breakpoint_t	bkpt;
	register db_breakpoint_t	*prev;

	for (prev = &bkpt_type->db_breakpoint_list;
	     (bkpt = *prev) != 0;
	     prev = &bkpt->link) {
	    if (db_map_equal(bkpt->map, map) &&
		(bkpt->address == addr)) {
		*prev = bkpt->link;
		break;
	    }
	}
	if (bkpt == 0) {
	    db_printf("Not set.\n");
	    return;
	}

	db_breakpoint_free(bkpt_type, bkpt);
}

static db_breakpoint_t
db_find_breakpoint(struct db_breakpoint_type *bkpt_type, vm_map_t map,
    db_addr_t addr)
{
	register db_breakpoint_t	bkpt;

	for (bkpt = bkpt_type->db_breakpoint_list;
	     bkpt != 0;
	     bkpt = bkpt->link)
	{
	    if (db_map_equal(bkpt->map, map) &&
		(bkpt->address == addr))
		return (bkpt);
	}
	return (0);
}

db_breakpoint_t
db_find_breakpoint_here(db_addr_t addr)
{
	db_breakpoint_t bkpt;

	bkpt = db_find_breakpoint(&db_breakpoint, db_map_addr(addr), addr);
#ifdef HAS_HW_BREAKPOINT
	if (bkpt == NULL)
		bkpt = db_find_breakpoint(&db_hbreakpoint, db_map_addr(addr),
		    addr);
#endif

	return (bkpt);
}

static bool	db_breakpoints_inserted = true;

#ifndef BKPT_WRITE
#define	BKPT_WRITE(addr, storage)				\
do {								\
	*storage = db_get_value(addr, BKPT_SIZE, false);	\
	db_put_value(addr, BKPT_SIZE, BKPT_SET(*storage));	\
} while (0)
#endif

#ifndef BKPT_CLEAR
#define	BKPT_CLEAR(addr, storage) \
	db_put_value(addr, BKPT_SIZE, *storage)
#endif

/*
 * Set software breakpoints.
 */
void
db_set_breakpoints(void)
{
	register db_breakpoint_t	bkpt;

	if (!db_breakpoints_inserted) {
		for (bkpt = db_breakpoint.db_breakpoint_list;
		     bkpt != 0;
		     bkpt = bkpt->link)
			if (db_map_current(bkpt->map)) {
				BKPT_WRITE(bkpt->address, &bkpt->bkpt_inst);
			}
		db_breakpoints_inserted = true;
	}
}

/*
 * Clean software breakpoints.
 */
void
db_clear_breakpoints(void)
{
	register db_breakpoint_t	bkpt;

	if (db_breakpoints_inserted) {
		for (bkpt = db_breakpoint.db_breakpoint_list;
		     bkpt != 0;
		     bkpt = bkpt->link)
			if (db_map_current(bkpt->map)) {
				BKPT_CLEAR(bkpt->address, &bkpt->bkpt_inst);
			}
		db_breakpoints_inserted = false;
	}
}

/*
 * List software breakpoints.
 */
static void
db_list_breakpoints(void)
{
	register db_breakpoint_t	bkpt;

	if (db_breakpoint.db_breakpoint_list == 0) {
	    db_printf("No breakpoints set\n");
	    return;
	}

	db_printf(" Map      Count    Address\n");
	for (bkpt = db_breakpoint.db_breakpoint_list;
	     bkpt != 0;
	     bkpt = bkpt->link) {
	    db_printf("%s%8p %5d    ",
		      db_map_current(bkpt->map) ? "*" : " ",
		      (void *)bkpt->map, bkpt->init_count);
	    db_printsym(bkpt->address, DB_STGY_PROC);
	    db_printf("\n");
	}
}

/*
 * Delete software breakpoint
 */
/*ARGSUSED*/
void
db_delete_cmd(db_expr_t addr, bool have_addr, db_expr_t count, char *modif)
{
	db_delete_breakpoint(&db_breakpoint, db_map_addr(addr),
	    (db_addr_t)addr);
}

/*
 * Set software breakpoint with skip count
 */
/*ARGSUSED*/
void
db_breakpoint_cmd(db_expr_t addr, bool have_addr, db_expr_t count, char *modif)
{
	if (count == -1)
	    count = 1;

	db_set_breakpoint(&db_breakpoint, db_map_addr(addr), (db_addr_t)addr,
	    count);
}

#ifdef HAS_HW_BREAKPOINT
/*
 * Delete hardware breakpoint
 */
void
db_deletehbreak_cmd(db_expr_t addr, bool have_addr, db_expr_t count,
    char *modif)
{
	if (count == -1)
	    count = 1;

	if (kdb_cpu_clr_breakpoint(addr) != 0) {
		db_printf("hardware breakpoint could not be delete\n");
		return;
	}

	db_delete_breakpoint(&db_hbreakpoint, db_map_addr(addr),
	    (db_addr_t)addr);
}

/*
 * Set hardware breakpoint
 */
void
db_hbreakpoint_cmd(db_expr_t addr, bool have_addr, db_expr_t count, char *modif)
{
	if (count == -1)
	    count = 1;

	if (!db_set_breakpoint(&db_hbreakpoint, db_map_addr(addr),
	    (db_addr_t)addr, count))
		return;

	if (kdb_cpu_set_breakpoint(addr) != 0) {
		db_printf("hardware breakpoint could not be set\n");
		db_delete_breakpoint(&db_hbreakpoint, db_map_addr(addr),
		    (db_addr_t)addr);
	}
}
#endif

/* list breakpoints */
void
db_listbreak_cmd(db_expr_t dummy1, bool dummy2, db_expr_t dummy3, char *dummy4)
{
	db_list_breakpoints();
#ifdef HAS_HW_BREAKPOINT
	db_md_list_breakpoints();
#endif
}

/*
 *	We want ddb to be usable before most of the kernel has been
 *	initialized.  In particular, current_thread() or kernel_map
 *	(or both) may be null.
 */

bool
db_map_equal(vm_map_t map1, vm_map_t map2)
{
	return ((map1 == map2) ||
		((map1 == NULL) && (map2 == kernel_map)) ||
		((map1 == kernel_map) && (map2 == NULL)));
}

bool
db_map_current(vm_map_t map)
{
#if 0
	thread_t	thread;

	return ((map == NULL) ||
		(map == kernel_map) ||
		(((thread = current_thread()) != NULL) &&
		 (map == thread->task->map)));
#else
	return (true);
#endif
}

vm_map_t
db_map_addr(vm_offset_t addr)
{
#if 0
	thread_t	thread;

	/*
	 *	We want to return kernel_map for all
	 *	non-user addresses, even when debugging
	 *	kernel tasks with their own maps.
	 */

	if ((VM_MIN_ADDRESS <= addr) &&
	    (addr < VM_MAX_ADDRESS) &&
	    ((thread = current_thread()) != NULL))
	    return thread->task->map;
	else
#endif
	    return kernel_map;
}
