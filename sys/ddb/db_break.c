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
 */
/*
 *	Author: David B. Golub, Carnegie Mellon University
 *	Date:	7/90
 */
/*
 * Breakpoints.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_comconsole.h"

#include <sys/param.h>

#include <vm/vm.h>
#include <vm/vm_kern.h>

#include <ddb/ddb.h>
#include <ddb/db_break.h>
#include <ddb/db_access.h>
#include <ddb/db_sym.h>

#define	NBREAKPOINTS	100
static struct db_breakpoint	db_break_table[NBREAKPOINTS];
static db_breakpoint_t		db_next_free_breakpoint = &db_break_table[0];
static db_breakpoint_t		db_free_breakpoints = 0;
static db_breakpoint_t		db_breakpoint_list = 0;

static db_breakpoint_t	db_breakpoint_alloc(void);
static void	db_breakpoint_free(db_breakpoint_t bkpt);
static void	db_delete_breakpoint(vm_map_t map, db_addr_t addr);
static db_breakpoint_t	db_find_breakpoint(vm_map_t map, db_addr_t addr);
static void	db_list_breakpoints(void);
static void	db_set_breakpoint(vm_map_t map, db_addr_t addr, int count);

static db_breakpoint_t
db_breakpoint_alloc()
{
	register db_breakpoint_t	bkpt;

	if ((bkpt = db_free_breakpoints) != 0) {
	    db_free_breakpoints = bkpt->link;
	    return (bkpt);
	}
	if (db_next_free_breakpoint == &db_break_table[NBREAKPOINTS]) {
	    db_printf("All breakpoints used.\n");
	    return (0);
	}
	bkpt = db_next_free_breakpoint;
	db_next_free_breakpoint++;

	return (bkpt);
}

static void
db_breakpoint_free(bkpt)
	register db_breakpoint_t	bkpt;
{
	bkpt->link = db_free_breakpoints;
	db_free_breakpoints = bkpt;
}

static void
db_set_breakpoint(map, addr, count)
	vm_map_t	map;
	db_addr_t	addr;
	int		count;
{
	register db_breakpoint_t	bkpt;

	if (db_find_breakpoint(map, addr)) {
	    db_printf("Already set.\n");
	    return;
	}

	bkpt = db_breakpoint_alloc();
	if (bkpt == 0) {
	    db_printf("Too many breakpoints.\n");
	    return;
	}

	bkpt->map = map;
	bkpt->address = addr;
	bkpt->flags = 0;
	bkpt->init_count = count;
	bkpt->count = count;

	bkpt->link = db_breakpoint_list;
	db_breakpoint_list = bkpt;
}

static void
db_delete_breakpoint(map, addr)
	vm_map_t	map;
	db_addr_t	addr;
{
	register db_breakpoint_t	bkpt;
	register db_breakpoint_t	*prev;

	for (prev = &db_breakpoint_list;
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

	db_breakpoint_free(bkpt);
}

static db_breakpoint_t
db_find_breakpoint(map, addr)
	vm_map_t	map;
	db_addr_t	addr;
{
	register db_breakpoint_t	bkpt;

	for (bkpt = db_breakpoint_list;
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
db_find_breakpoint_here(addr)
	db_addr_t	addr;
{
    return db_find_breakpoint(db_map_addr(addr), addr);
}

static boolean_t	db_breakpoints_inserted = TRUE;

#ifndef BKPT_WRITE
#define BKPT_WRITE(addr, storage)				\
do {								\
	*storage = db_get_value(addr, BKPT_SIZE, FALSE);	\
	db_put_value(addr, BKPT_SIZE, BKPT_SET(*storage));	\
} while (0)
#endif

#ifndef BKPT_CLEAR
#define BKPT_CLEAR(addr, storage) \
	db_put_value(addr, BKPT_SIZE, *storage)
#endif

void
db_set_breakpoints()
{
	register db_breakpoint_t	bkpt;

	if (!db_breakpoints_inserted) {

		for (bkpt = db_breakpoint_list;
		     bkpt != 0;
		     bkpt = bkpt->link)
			if (db_map_current(bkpt->map)) {
				BKPT_WRITE(bkpt->address, &bkpt->bkpt_inst);
			}
		db_breakpoints_inserted = TRUE;
	}
}

void
db_clear_breakpoints()
{
	register db_breakpoint_t	bkpt;

	if (db_breakpoints_inserted) {

		for (bkpt = db_breakpoint_list;
		     bkpt != 0;
		     bkpt = bkpt->link)
			if (db_map_current(bkpt->map)) {
				BKPT_CLEAR(bkpt->address, &bkpt->bkpt_inst);
			}
		db_breakpoints_inserted = FALSE;
	}
}

#ifdef SOFTWARE_SSTEP
/*
 * Set a temporary breakpoint.
 * The instruction is changed immediately,
 * so the breakpoint does not have to be on the breakpoint list.
 */
db_breakpoint_t
db_set_temp_breakpoint(addr)
	db_addr_t	addr;
{
	register db_breakpoint_t	bkpt;

	bkpt = db_breakpoint_alloc();
	if (bkpt == 0) {
	    db_printf("Too many breakpoints.\n");
	    return 0;
	}

	bkpt->map = NULL;
	bkpt->address = addr;
	bkpt->flags = BKPT_TEMP;
	bkpt->init_count = 1;
	bkpt->count = 1;

	BKPT_WRITE(bkpt->address, &bkpt->bkpt_inst);
	return bkpt;
}

void
db_delete_temp_breakpoint(bkpt)
	db_breakpoint_t	bkpt;
{
	BKPT_CLEAR(bkpt->address, &bkpt->bkpt_inst);
	db_breakpoint_free(bkpt);
}
#endif /* SOFTWARE_SSTEP */

/*
 * List breakpoints.
 */
static void
db_list_breakpoints()
{
	register db_breakpoint_t	bkpt;

	if (db_breakpoint_list == 0) {
	    db_printf("No breakpoints set\n");
	    return;
	}

	db_printf(" Map      Count    Address\n");
	for (bkpt = db_breakpoint_list;
	     bkpt != 0;
	     bkpt = bkpt->link) {
	    db_printf("%s%8p %5d    ",
		      db_map_current(bkpt->map) ? "*" : " ",
		      (void *)bkpt->map, bkpt->init_count);
	    db_printsym(bkpt->address, DB_STGY_PROC);
	    db_printf("\n");
	}
}

/* Delete breakpoint */
/*ARGSUSED*/
void
db_delete_cmd(addr, have_addr, count, modif)
	db_expr_t	addr;
	boolean_t	have_addr;
	db_expr_t	count;
	char *		modif;
{
	db_delete_breakpoint(db_map_addr(addr), (db_addr_t)addr);
}

/* Set breakpoint with skip count */
/*ARGSUSED*/
void
db_breakpoint_cmd(addr, have_addr, count, modif)
	db_expr_t	addr;
	boolean_t	have_addr;
	db_expr_t	count;
	char *		modif;
{
	if (count == -1)
	    count = 1;

	db_set_breakpoint(db_map_addr(addr), (db_addr_t)addr, count);
}

/* list breakpoints */
void
db_listbreak_cmd(dummy1, dummy2, dummy3, dummy4)
	db_expr_t	dummy1;
	boolean_t	dummy2;
	db_expr_t	dummy3;
	char *		dummy4;
{
	db_list_breakpoints();
}

/*
 *	We want ddb to be usable before most of the kernel has been
 *	initialized.  In particular, current_thread() or kernel_map
 *	(or both) may be null.
 */

boolean_t
db_map_equal(map1, map2)
	vm_map_t	map1, map2;
{
	return ((map1 == map2) ||
		((map1 == NULL) && (map2 == kernel_map)) ||
		((map1 == kernel_map) && (map2 == NULL)));
}

boolean_t
db_map_current(map)
	vm_map_t	map;
{
#if 0
	thread_t	thread;

	return ((map == NULL) ||
		(map == kernel_map) ||
		(((thread = current_thread()) != NULL) &&
		 (map == thread->task->map)));
#else
	return (1);
#endif
}

vm_map_t
db_map_addr(addr)
	vm_offset_t addr;
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

#ifdef ALT_BREAK_TO_DEBUGGER
/*
 * Solaris implements a new BREAK which is initiated by a character sequence
 * CR ~ ^b which is similar to a familiar pattern used on Sun servers by the
 * Remote Console.
 *
 * Note that this function may be called from almost anywhere, with interrupts
 * disabled and with unknown locks held, so it must not access data other than
 * its arguments.  Its up to the caller to ensure that the state variable is
 * consistent.
 */

#define	KEY_CR		13	/* CR '\r' */
#define	KEY_TILDE	126	/* ~ */
#define	KEY_CRTLB	2	/* ^B */

int
db_alt_break(int data, int *state)
{
	int brk = 0;

	switch (data) {
	case KEY_CR:
		*state = KEY_TILDE;
		break;
	case KEY_TILDE:
		if (*state == KEY_TILDE)
			*state = KEY_CRTLB;
		else
			*state = 0;
		break;
	case KEY_CRTLB:
		if (*state == KEY_CRTLB)
			brk = 1;
		/* FALLTHROUGH */
	default:
		*state = 0;
		break;
	}
	return (brk);
}
#endif
