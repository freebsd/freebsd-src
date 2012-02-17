#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/systm.h>

#include <machine/db_machdep.h>

#include "opt_ddb.h"
#ifdef DDB
#include <ddb/ddb.h>
#endif

#include <support/kdb.h>

#ifdef DDB
DB_FUNC(xfs, xfs_ddb_cmd, db_cmd_set, CS_MORE, NULL)
{
	db_error("No commands registered.\n");
}
#endif

int
kdb_register(char *cmd, kdb_func_t func, char *usage, char *help, short minlen)
{
	return 0;
}

int
kdb_unregister(char *cmd)
{
	return 0;
}

int
kdbgetaddrarg(int argc, const char **argv, int *nextarg,
    kdb_machreg_t *value,  long *offset, char **name, struct pt_regs *regs)
{
	return 0;
}

int
kdbnearsym(unsigned long addr, kdb_symtab_t *symtab)

{
	return 0;
}

void
kdb_printf(const char *fmt, ...)
{
}

int
kdb_getarea_size(void *res, unsigned long addr, size_t size)
{
	return 0;
}

int
db_putarea_size(unsigned long addr, void *res, size_t size)
{
	return 0;
}
