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
 * $Log: db_break.h,v $
 * Revision 1.1  1992/03/25  21:44:59  pace
 * Initial revision
 *
 * Revision 2.4  91/02/05  17:06:06  mrt
 * 	Changed to new Mach copyright
 * 	[91/01/31  16:17:10  mrt]
 * 
 * Revision 2.3  90/10/25  14:43:40  rwd
 * 	Added map field to breakpoints.
 * 	[90/10/18            rpd]
 * 
 * Revision 2.2  90/08/27  21:50:00  dbg
 * 	Modularized typedef names.
 * 	[90/08/20            af]
 * 	Add external defintions.
 * 	[90/08/07            dbg]
 * 	Created.
 * 	[90/07/25            dbg]
 * 
 */
/*
 *	Author: David B. Golub, Carnegie Mellon University
 *	Date:	7/90
 */
#ifndef	_DDB_DB_BREAK_H_
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

extern db_breakpoint_t	db_find_breakpoint();
extern db_breakpoint_t	db_find_breakpoint_here();
extern void		db_set_breakpoints();
extern void		db_clear_breakpoints();

extern db_breakpoint_t	db_set_temp_breakpoint(/* db_addr_t addr */);
extern void		db_delete_temp_breakpoint(/* db_breakpoint_t bkpt */);

#endif	_DDB_DB_BREAK_H_
