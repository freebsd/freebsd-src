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
 * $Log: db_variables.h,v $
 * Revision 1.1  1992/03/25  21:45:35  pace
 * Initial revision
 *
 * Revision 2.3  91/02/05  17:07:23  mrt
 * 	Changed to new Mach copyright
 * 	[91/01/31  16:19:54  mrt]
 * 
 * Revision 2.2  90/08/27  21:53:40  dbg
 * 	Modularized typedef name.  Documented the calling sequence of
 * 	the (optional) access function of a variable.  Now the valuep
 * 	field can be made opaque, eg be an offset that fcn() resolves.
 * 	[90/08/20            af]
 * 
 * 	Created.
 * 	[90/07/25            dbg]
 * 
 */
/*
 * 	Author: David B. Golub, Carnegie Mellon University
 *	Date:	7/90
 */

#ifndef	_DB_VARIABLES_H_
#define	_DB_VARIABLES_H_

/*
 * Debugger variables.
 */
struct db_variable {
	char	*name;		/* Name of variable */
	int	*valuep;	/* value of variable */
				/* function to call when reading/writing */
	int	(*fcn)(/* db_variable *vp, db_expr_t *valuep, int op */);
#define DB_VAR_GET	0
#define DB_VAR_SET	1
};
#define	FCN_NULL	((int (*)())0)

extern struct db_variable	db_vars[];	/* debugger variables */
extern struct db_variable	*db_evars;
extern struct db_variable	db_regs[];	/* machine registers */
extern struct db_variable	*db_eregs;

#endif	/* _DB_VARIABLES_H_ */
