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
 *	$Id: db_break.h,v 1.3 1995/05/30 07:56:51 rgrimes Exp $
 */

/*
 *	Author: David B. Golub, Carnegie Mellon University
 *	Date:	7/90
 */
#ifndef _DDB_DB_BREAK_H_
#define	_DDB_DB_BREAK_H_

#include <vm/vm_map.h>
#include <machine/db_machdep.h>

/*
 * Breakpoint.
 */

struct db_breakpoint {
	vm_map_t map;			/* in this map */
	db_addr_t address;		/* set here */
	int	init_count;		/* number of times to skip bkpt */
	int	count;			/* current count */
	int	flags;			/* flags: */
#define	BKPT_SINGLE_STEP	0x2	    /* to simulate single step */
#define	BKPT_TEMP		0x4	    /* temporary */
	int	bkpt_inst;		/* saved instruction at bkpt */
	struct db_breakpoint *link;	/* link in in-use or free chain */
};
typedef struct db_breakpoint *db_breakpoint_t;

db_breakpoint_t	db_breakpoint_alloc __P((void));
void		db_breakpoint_free __P((db_breakpoint_t bkpt));
void		db_clear_breakpoints __P((void));
void		db_delete_breakpoint __P((vm_map_t map, db_addr_t addr));
void		db_delete_temp_breakpoint __P((db_breakpoint_t bkpt));
db_breakpoint_t	db_find_breakpoint __P((vm_map_t map, db_addr_t addr));
db_breakpoint_t	db_find_breakpoint_here __P((db_addr_t addr));
void		db_list_breakpoints __P((void));
void		db_set_breakpoint __P((vm_map_t map, db_addr_t addr,
				       int count));
void		db_set_breakpoints __P((void));
db_breakpoint_t	db_set_temp_breakpoint __P((db_addr_t addr));

#endif /* !_DDB_DB_BREAK_H_ */
