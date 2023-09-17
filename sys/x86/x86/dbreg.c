/*-
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

#include "opt_ddb.h"

#include <sys/types.h>
#include <sys/kdb.h>
#include <sys/pcpu.h>
#include <sys/reg.h>
#include <sys/smp.h>
#include <sys/systm.h>

#include <machine/frame.h>
#include <machine/kdb.h>
#include <machine/md_var.h>

#include <ddb/ddb.h>
#include <ddb/db_sym.h>

#define NDBREGS		4
#ifdef __amd64__
#define	MAXWATCHSIZE	8
#else
#define	MAXWATCHSIZE	4
#endif

/*
 * Set a watchpoint in the debug register denoted by 'watchnum'.
 */
static void
dbreg_set_watchreg(int watchnum, vm_offset_t watchaddr, vm_size_t size,
    int access, struct dbreg *d)
{
	int len;

	MPASS(watchnum >= 0 && watchnum < NDBREGS);

	/* size must be 1 for an execution breakpoint */
	if (access == DBREG_DR7_EXEC)
		size = 1;

	/*
	 * we can watch a 1, 2, or 4 byte sized location
	 */
	switch (size) {
	case 1:
		len = DBREG_DR7_LEN_1;
		break;
	case 2:
		len = DBREG_DR7_LEN_2;
		break;
	case 4:
		len = DBREG_DR7_LEN_4;
		break;
#if MAXWATCHSIZE >= 8
	case 8:
		len = DBREG_DR7_LEN_8;
		break;
#endif
	default:
		return;
	}

	/* clear the bits we are about to affect */
	d->dr[7] &= ~DBREG_DR7_MASK(watchnum);

	/* set drN register to the address, N=watchnum */
	DBREG_DRX(d, watchnum) = watchaddr;

	/* enable the watchpoint */
	d->dr[7] |= DBREG_DR7_SET(watchnum, len, access,
	    DBREG_DR7_GLOBAL_ENABLE);
}

/*
 * Remove a watchpoint from the debug register denoted by 'watchnum'.
 */
static void
dbreg_clr_watchreg(int watchnum, struct dbreg *d)
{
	MPASS(watchnum >= 0 && watchnum < NDBREGS);

	d->dr[7] &= ~DBREG_DR7_MASK(watchnum);
	DBREG_DRX(d, watchnum) = 0;
}

/*
 * Sync the debug registers. Other cores will read these values from the PCPU
 * area when they resume. See amd64_db_resume_dbreg() below.
 */
static void
dbreg_sync(struct dbreg *dp)
{
#ifdef __amd64__
	struct pcpu *pc;
	int cpu, c;

	cpu = PCPU_GET(cpuid);
	CPU_FOREACH(c) {
		if (c == cpu)
			continue;
		pc = pcpu_find(c);
		memcpy(pc->pc_dbreg, dp, sizeof(*dp));
		pc->pc_dbreg_cmd = PC_DBREG_CMD_LOAD;
	}
#endif
}

int
dbreg_set_watchpoint(vm_offset_t addr, vm_size_t size, int access)
{
	struct dbreg *d;
	int avail, i, wsize;

#ifdef __amd64__
	d = (struct dbreg *)PCPU_PTR(dbreg);
#else
	/* debug registers aren't stored in PCPU on i386. */
	struct dbreg d_temp;
	d = &d_temp;
#endif

	/* Validate the access type */
	if (access != DBREG_DR7_EXEC && access != DBREG_DR7_WRONLY &&
	    access != DBREG_DR7_RDWR)
		return (EINVAL);

	fill_dbregs(NULL, d);

	/*
	 * Check if there are enough available registers to cover the desired
	 * area.
	 */
	avail = 0;
	for (i = 0; i < NDBREGS; i++) {
		if (!DBREG_DR7_ENABLED(d->dr[7], i))
			avail++;
	}

	if (avail * MAXWATCHSIZE < size)
		return (EBUSY);

	for (i = 0; i < NDBREGS && size > 0; i++) {
		if (!DBREG_DR7_ENABLED(d->dr[7], i)) {
			if ((size >= 8 || (avail == 1 && size > 4)) &&
			    MAXWATCHSIZE == 8)
				wsize = 8;
			else if (size > 2)
				wsize = 4;
			else
				wsize = size;
			dbreg_set_watchreg(i, addr, wsize, access, d);
			addr += wsize;
			size -= wsize;
			avail--;
		}
	}

	set_dbregs(NULL, d);
	dbreg_sync(d);

	return (0);
}

int
dbreg_clr_watchpoint(vm_offset_t addr, vm_size_t size)
{
	struct dbreg *d;
	int i;

#ifdef __amd64__
	d = (struct dbreg *)PCPU_PTR(dbreg);
#else
	/* debug registers aren't stored in PCPU on i386. */
	struct dbreg d_temp;
	d = &d_temp;
#endif
	fill_dbregs(NULL, d);

	for (i = 0; i < NDBREGS; i++) {
		if (DBREG_DR7_ENABLED(d->dr[7], i)) {
			if (DBREG_DRX((d), i) >= addr &&
			    DBREG_DRX((d), i) < addr + size)
				dbreg_clr_watchreg(i, d);
		}
	}

	set_dbregs(NULL, d);
	dbreg_sync(d);

	return (0);
}

#ifdef DDB
static const char *
watchtype_str(int type)
{

	switch (type) {
	case DBREG_DR7_EXEC:
		return ("execute");
	case DBREG_DR7_RDWR:
		return ("read/write");
	case DBREG_DR7_WRONLY:
		return ("write");
	default:
		return ("invalid");
	}
}

void
dbreg_list_watchpoints(void)
{
	struct dbreg d;
	int i, len, type;

	fill_dbregs(NULL, &d);

	db_printf("\nhardware watchpoints:\n");
	db_printf("  watch    status        type  len     address\n");
	db_printf("  -----  --------  ----------  ---  ----------\n");
	for (i = 0; i < NDBREGS; i++) {
		if (DBREG_DR7_ENABLED(d.dr[7], i)) {
			type = DBREG_DR7_ACCESS(d.dr[7], i);
			len = DBREG_DR7_LEN(d.dr[7], i);
			db_printf("  %-5d  %-8s  %10s  %3d  ",
			    i, "enabled", watchtype_str(type), len + 1);
			db_printsym((db_addr_t)DBREG_DRX(&d, i), DB_STGY_ANY);
			db_printf("\n");
		} else {
			db_printf("  %-5d  disabled\n", i);
		}
	}
}
#endif

#ifdef __amd64__
/* Sync debug registers when resuming from debugger. */
void
amd64_db_resume_dbreg(void)
{
	struct dbreg *d;

	switch (PCPU_GET(dbreg_cmd)) {
	case PC_DBREG_CMD_LOAD:
		d = (struct dbreg *)PCPU_PTR(dbreg);
		set_dbregs(NULL, d);
		PCPU_SET(dbreg_cmd, PC_DBREG_CMD_NONE);
		break;
	}
}
#endif

int
kdb_cpu_set_watchpoint(vm_offset_t addr, vm_size_t size, int access)
{

	/* Convert the KDB access type */
	switch (access) {
	case KDB_DBG_ACCESS_W:
		access = DBREG_DR7_WRONLY;
		break;
	case KDB_DBG_ACCESS_RW:
		access = DBREG_DR7_RDWR;
		break;
	case KDB_DBG_ACCESS_R:
		/* FALLTHROUGH: read-only not supported */
	default:
		return (EINVAL);
	}

	return (dbreg_set_watchpoint(addr, size, access));
}

int
kdb_cpu_clr_watchpoint(vm_offset_t addr, vm_size_t size)
{

	return (dbreg_clr_watchpoint(addr, size));
}
