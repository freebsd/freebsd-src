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
 *	$Id: db_sym.h,v 1.13 1997/06/30 23:49:17 bde Exp $
 */

#ifndef _DDB_DB_SYM_H_
#define	_DDB_DB_SYM_H_

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

/*
 * Functions exported by the symtable module
 */
void		db_add_symbol_table __P((char *, char *, char *, char *));
					/* extend the list of symbol tables */

db_sym_t	db_search_symbol __P((db_addr_t, db_strategy_t, db_expr_t *));
					/* find symbol given value */

void		db_symbol_values __P((db_sym_t, char **, db_expr_t *));
					/* return name and value of symbol */

#define db_find_sym_and_offset(val,namep,offp)	\
	db_symbol_values(db_search_symbol(val,DB_STGY_ANY,offp),namep,0)
					/* find name&value given approx val */

#define db_find_xtrn_sym_and_offset(val,namep,offp)	\
	db_symbol_values(db_search_symbol(val,DB_STGY_XTRN,offp),namep,0)
					/* ditto, but no locals */

int		db_eqname __P((char *, char *, int));
					/* strcmp, modulo leading char */

void		db_printsym __P((db_expr_t, db_strategy_t));
					/* print closest symbol to a value */

int		db_sym_numargs __P((db_sym_t, int *, char **));

boolean_t	X_db_line_at_pc __P((db_symtab_t *symtab, db_sym_t cursym,
				     char **filename, int *linenum,
				     db_expr_t off));
db_sym_t	X_db_lookup __P((db_symtab_t *stab, char *symstr));
db_sym_t	X_db_search_symbol __P((db_symtab_t *symtab, db_addr_t off,
					db_strategy_t strategy,
					db_expr_t *diffp));
int		X_db_sym_numargs __P((db_symtab_t *, db_sym_t, int *,
				      char **));
void		X_db_symbol_values __P((db_sym_t sym, char **namep,
					db_expr_t *valuep));

#endif /* !_DDB_DB_SYM_H_ */
