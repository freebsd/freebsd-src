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
 * $Log: db_access.h,v $
 * Revision 1.1  1992/03/25  21:44:53  pace
 * Initial revision
 *
 * Revision 2.3  91/02/05  17:05:49  mrt
 * 	Changed to new Mach copyright
 * 	[91/01/31  16:16:37  mrt]
 * 
 * Revision 2.2  90/08/27  21:48:27  dbg
 * 	Created.
 * 	[90/08/07            dbg]
 * 
 */
/*
 *	Author: David B. Golub, Carnegie Mellon University
 *	Date:	7/90
 */
/*
 * Data access functions for debugger.
 */
#include <machine/db_machdep.h>		/* expression types */

extern db_expr_t db_get_value(/* db_addr_t addr,
				 int size,
				 boolean_t is_signed */);
extern void	 db_put_value(/* db_addr_t addr,
				 int size,
				 db_expr_t value */);
