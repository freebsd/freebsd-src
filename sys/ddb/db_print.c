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
 * Miscellaneous printing.
 */

#include <sys/param.h>
#include <sys/kdb.h>
#include <sys/proc.h>

#include <machine/pcb.h>

#include <ddb/ddb.h>
#include <ddb/db_variables.h>
#include <ddb/db_sym.h>

void
db_show_regs(db_expr_t _1, bool _2, db_expr_t _3, char *modif)
{
	struct trapframe *oldtf;
	struct db_variable *regp;
	db_expr_t value, offset;
	const char *name;

	/*
	 * The 'u' modifier instructs us to print the previous trapframe, most
	 * often containing state from userspace. This is done by temporarily
	 * switching out kdb_frame.
	 *
	 * NB: curthread is used instead of kdb_thread, so that behaviour is
	 * consistent with regular `show registers`, which always prints
	 * curthread's trapframe.
	 */
	oldtf = kdb_frame;
	if (modif[0] == 'u') {
		if (curthread->td_frame == NULL ||
		    curthread->td_frame == oldtf) {
			db_printf("previous trapframe unavailable");
			return;
		}
		kdb_frame = curthread->td_frame;
	}

	for (regp = db_regs; regp < db_eregs; regp++) {
		if (!db_read_variable(regp, &value))
			continue;
		db_printf("%-12s%#*lr", regp->name,
		    (int)(sizeof(unsigned long) * 2 + 2), (unsigned long)value);
		db_find_xtrn_sym_and_offset((db_addr_t)value, &name, &offset);
		if (name != NULL && offset <= (unsigned long)db_maxoff &&
		    offset != value) {
			db_printf("\t%s", name);
			if (offset != 0)
				db_printf("+%+#lr", (long)offset);
		}
		db_printf("\n");
	}
	db_print_loc_and_inst(PC_REGS());

	kdb_frame = oldtf;
}
