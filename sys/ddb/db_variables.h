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
 *	$Id: db_variables.h,v 1.4 1995/05/30 07:57:19 rgrimes Exp $
 */

/*
 * 	Author: David B. Golub, Carnegie Mellon University
 *	Date:	7/90
 */

#ifndef _DDB_DB_VARIABLES_H_
#define	_DDB_DB_VARIABLES_H_

/*
 * Debugger variables.
 */
struct db_variable;
typedef	int	db_varfcn_t __P((struct db_variable *vp, db_expr_t *valuep,
				 int op));
struct db_variable {
	char	*name;		/* Name of variable */
	int	*valuep;	/* value of variable */
				/* function to call when reading/writing */
	db_varfcn_t *fcn;
#define DB_VAR_GET	0
#define DB_VAR_SET	1
};
#define	FCN_NULL	((db_varfcn_t *)0)

extern struct db_variable	db_vars[];	/* debugger variables */
extern struct db_variable	*db_evars;
extern struct db_variable	db_regs[];	/* machine registers */
extern struct db_variable	*db_eregs;

void	db_read_variable __P((struct db_variable *, db_expr_t *));

#endif /* _!DDB_DB_VARIABLES_H_ */
