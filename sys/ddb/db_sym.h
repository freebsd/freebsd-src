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
 * $Log: db_sym.h,v $
 * Revision 1.1  1992/03/25  21:45:29  pace
 * Initial revision
 *
 * Revision 2.3  91/02/05  17:07:12  mrt
 * 	Changed to new Mach copyright
 * 	[91/01/31  16:19:27  mrt]
 * 
 * Revision 2.2  90/08/27  21:52:39  dbg
 * 	Changed type of db_sym_t to char * - it's a better type for an
 * 	opaque pointer.
 * 	[90/08/22            dbg]
 * 
 * 	Created.
 * 	[90/08/19            af]
 * 
 */
/*
 * 	Author: Alessandro Forin, Carnegie Mellon University
 *	Date:	8/90
 */

/*
 * This module can handle multiple symbol tables
 */
typedef struct {
	char		*name;		/* symtab name */
	char		*start;		/* symtab location */
	char		*end;
	char		*private;	/* optional machdep pointer */
} db_symtab_t;

extern db_symtab_t	*db_last_symtab; /* where last symbol was found */

/*
 * Symbol representation is specific to the symtab style:
 * BSD compilers use dbx' nlist, other compilers might use
 * a different one
 */
typedef	char *		db_sym_t;	/* opaque handle on symbols */
#define	DB_SYM_NULL	((db_sym_t)0)

/*
 * Non-stripped symbol tables will have duplicates, for instance
 * the same string could match a parameter name, a local var, a
 * global var, etc.
 * We are most concern with the following matches.
 */
typedef int		db_strategy_t;	/* search strategy */

#define	DB_STGY_ANY	0			/* anything goes */
#define DB_STGY_XTRN	1			/* only external symbols */
#define DB_STGY_PROC	2			/* only procedures */

extern boolean_t	db_qualify_ambiguous_names;
					/* if TRUE, check across symbol tables
					 * for multiple occurrences of a name.
					 * Might slow down quite a bit */

/*
 * Functions exported by the symtable module
 */
extern void	db_add_symbol_table();
					/* extend the list of symbol tables */

extern int	db_value_of_name(/* char*, db_expr_t* */);
					/* find symbol value given name */

extern db_sym_t	db_search_symbol(/* db_expr_t, db_strategy_t, int* */);
					/* find symbol given value */

extern void	db_symbol_values(/* db_sym_t, char**, db_expr_t* */);
					/* return name and value of symbol */

#define db_find_sym_and_offset(val,namep,offp)	\
	db_symbol_values(db_search_symbol(val,DB_STGY_ANY,offp),namep,0)
					/* find name&value given approx val */

#define db_find_xtrn_sym_and_offset(val,namep,offp)	\
	db_symbol_values(db_search_symbol(val,DB_STGY_XTRN,offp),namep,0)
					/* ditto, but no locals */

extern int	db_eqname(/* char*, char*, char */);
					/* strcmp, modulo leading char */

extern void	db_printsym(/* db_expr_t, db_strategy_t */);
					/* print closest symbol to a value */
