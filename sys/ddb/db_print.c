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
 *	$Id: db_print.c,v 1.14 1997/06/14 11:52:36 bde Exp $
 */

/*
 * 	Author: David B. Golub, Carnegie Mellon University
 *	Date:	7/90
 */

/*
 * Miscellaneous printing.
 */
#include <sys/param.h>
#include <sys/msgbuf.h>

#include <ddb/ddb.h>
#include <ddb/db_variables.h>
#include <ddb/db_sym.h>

void
db_show_regs(dummy1, dummy2, dummy3, dummy4)
	db_expr_t	dummy1;
	boolean_t	dummy2;
	db_expr_t	dummy3;
	char *		dummy4;
{
	register struct db_variable *regp;
	db_expr_t	value, offset;
	char *		name;

	for (regp = db_regs; regp < db_eregs; regp++) {
	    db_read_variable(regp, &value);
	    db_printf("%-12s%#10n", regp->name, value);
	    db_find_xtrn_sym_and_offset((db_addr_t)value, &name, &offset);
	    if (name != 0 && offset <= db_maxoff && offset != value) {
		db_printf("\t%s", name);
		if (offset != 0)
		    db_printf("+%+#n", offset);
	    }
	    db_printf("\n");
	}
	db_print_loc_and_inst(PC_REGS(DDB_REGS));
}


void
db_show_msgbuf(dummy1, dummy2, dummy3, dummy4)
	db_expr_t	dummy1;
	boolean_t	dummy2;
	db_expr_t	dummy3;
	char *		dummy4;
{
	int i,j;

	if (!msgbufmapped) {
		db_printf("msgbuf not mapped yet\n");
		return;
	}
	db_printf("msgbufp = %p\n", msgbufp);
	db_printf("magic = %x, size = %d, r= %d, w = %d, ptr = %p\n",
		msgbufp->msg_magic,
		msgbufp->msg_size,
		msgbufp->msg_bufr,
		msgbufp->msg_bufx,
		msgbufp->msg_ptr);
	for (i = 0; i < msgbufp->msg_size; i++) {
		j = msgbufp->msg_ptr[(i + msgbufp->msg_bufr) % msgbufp->msg_size];
		if (j)
			db_printf("%c", j);
	}
	db_printf("\n");
}

