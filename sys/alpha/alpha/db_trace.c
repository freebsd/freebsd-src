/* $NetBSD: db_trace.c,v 1.1 1997/09/06 02:00:50 thorpej Exp $ */

#include <sys/cdefs.h>			/* RCS ID & Copyright macro defns */

/* __KERNEL_RCSID(0, "$NetBSD: db_trace.c,v 1.1 1997/09/06 02:00:50 thorpej Exp $"); */

#include <sys/param.h>
#include <sys/proc.h>
#include <machine/db_machdep.h>

#include <ddb/ddb.h>
#include <ddb/db_sym.h> 
#include <ddb/db_access.h>
#include <ddb/db_variables.h>
#include <ddb/db_output.h>

void
db_stack_trace_cmd(addr, have_addr, count, modif)
	db_expr_t addr;
	boolean_t have_addr;
	db_expr_t count;
	char *modif;
{
	/* nothing, yet. */
}
