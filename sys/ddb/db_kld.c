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
 *	$Id: db_kld.c,v 1.1 1998/06/28 00:57:28 dfr Exp $
 *	from db_aout.c,v 1.20 1998/06/07 17:09:36 dfr Exp
 */

/*
 *	Author: David B. Golub, Carnegie Mellon University
 *	Date:	7/90
 */
/*
 * Symbol table routines for a.out format files.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/linker.h>

#include <machine/bootinfo.h>

#include <ddb/ddb.h>
#include <ddb/db_sym.h>

extern linker_file_t linker_kernel_file;

#define KF linker_kernel_file

db_sym_t
X_db_lookup(stab, symstr)
	db_symtab_t	*stab;
	char *		symstr;
{
	linker_sym_t sym;

	if (!KF)
		return 0;

	if (KF->ops->lookup_symbol(KF, symstr, &sym) == 0)
	    return (db_sym_t) sym;
	else
	    return (db_sym_t) 0;
}

db_sym_t
X_db_search_symbol(symtab, off, strategy, diffp)
	db_symtab_t *	symtab;
	register
	db_addr_t	off;
	db_strategy_t	strategy;
	db_expr_t	*diffp;		/* in/out */
{
	linker_sym_t sym;
	long diff;

	if (!KF)
		return 0;

	if (KF->ops->search_symbol(KF, (caddr_t) off, &sym, &diff) == 0) {
		*diffp = (db_expr_t) diff;
		return (db_sym_t) sym;
	}

	return 0;
}

/*
 * Return the name and value for a symbol.
 */
void
X_db_symbol_values(symtab, dbsym, namep, valuep)
	db_symtab_t	*symtab;
	db_sym_t	dbsym;
	char		**namep;
	db_expr_t	*valuep;
{
	linker_sym_t sym = (linker_sym_t) dbsym;
	linker_symval_t symval;

	KF->ops->symbol_values(KF, sym, &symval);
	if (namep)
	    *namep = (char*) symval.name;
	if (valuep)
	    *valuep = (db_expr_t) symval.value;
}


boolean_t
X_db_line_at_pc(symtab, cursym, filename, linenum, off)
	db_symtab_t *	symtab;
	db_sym_t	cursym;
	char 		**filename;
	int 		*linenum;
	db_expr_t	off;
{
	return FALSE;
}

boolean_t
X_db_sym_numargs(symtab, cursym, nargp, argnamep)
	db_symtab_t *	symtab;
	db_sym_t	cursym;
	int		*nargp;
	char		**argnamep;
{
	return FALSE;
}

/*
 * Initialization routine for a.out files.
 */
void
kdb_init()
{
	db_add_symbol_table(0, 0, "kernel", 0);
}
