/*-
 * Copyright (c) 2014 The FreeBSD Foundation
 *
 * This software was developed by Semihalf under
 * the sponsorship of the FreeBSD Foundation.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "opt_ddb.h"
#include "opt_gdb.h"

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/types.h>
#include <sys/kdb.h>
#include <sys/pcpu.h>
#include <sys/proc.h>
#include <sys/systm.h>
#include <sys/sysent.h>

#include <machine/armreg.h>
#include <machine/cpu.h>
#include <machine/debug_monitor.h>
#include <machine/kdb.h>

#ifdef DDB
#include <ddb/ddb.h>
#include <ddb/db_sym.h>
#endif

enum dbg_t {
	DBG_TYPE_BREAKPOINT = 0,
	DBG_TYPE_WATCHPOINT = 1,
};

static int dbg_watchpoint_num;
static int dbg_breakpoint_num;
static struct debug_monitor_state kernel_monitor = {
	.dbg_flags = DBGMON_KERNEL
};

/* Called from the exception handlers */
void dbg_monitor_enter(struct thread *);
void dbg_monitor_exit(struct thread *, struct trapframe *);

/* Watchpoints/breakpoints control register bitfields */
#define DBG_WATCH_CTRL_LEN_1		(0x1 << 5)
#define DBG_WATCH_CTRL_LEN_2		(0x3 << 5)
#define DBG_WATCH_CTRL_LEN_4		(0xf << 5)
#define DBG_WATCH_CTRL_LEN_8		(0xff << 5)
#define DBG_WATCH_CTRL_LEN_MASK(x)	((x) & (0xff << 5))
#define DBG_WATCH_CTRL_EXEC		(0x0 << 3)
#define DBG_WATCH_CTRL_LOAD		(0x1 << 3)
#define DBG_WATCH_CTRL_STORE		(0x2 << 3)
#define DBG_WATCH_CTRL_ACCESS_MASK(x)	((x) & (0x3 << 3))

/* Common for breakpoint and watchpoint */
#define DBG_WB_CTRL_EL1		(0x1 << 1)
#define DBG_WB_CTRL_EL0		(0x2 << 1)
#define DBG_WB_CTRL_ELX_MASK(x)	((x) & (0x3 << 1))
#define DBG_WB_CTRL_E		(0x1 << 0)

#define DBG_REG_BASE_BVR	0
#define DBG_REG_BASE_BCR	(DBG_REG_BASE_BVR + 16)
#define DBG_REG_BASE_WVR	(DBG_REG_BASE_BCR + 16)
#define DBG_REG_BASE_WCR	(DBG_REG_BASE_WVR + 16)

/* Watchpoint/breakpoint helpers */
#define DBG_WB_WVR	"wvr"
#define DBG_WB_WCR	"wcr"
#define DBG_WB_BVR	"bvr"
#define DBG_WB_BCR	"bcr"

#define DBG_WB_READ(reg, num, val) do {					\
	__asm __volatile("mrs %0, dbg" reg #num "_el1" : "=r" (val));	\
} while (0)

#define DBG_WB_WRITE(reg, num, val) do {				\
	__asm __volatile("msr dbg" reg #num "_el1, %0" :: "r" (val));	\
} while (0)

#define READ_WB_REG_CASE(reg, num, offset, val)		\
	case (num + offset):				\
		DBG_WB_READ(reg, num, val);		\
		break

#define WRITE_WB_REG_CASE(reg, num, offset, val)	\
	case (num + offset):				\
		DBG_WB_WRITE(reg, num, val);		\
		break

#define SWITCH_CASES_READ_WB_REG(reg, offset, val)	\
	READ_WB_REG_CASE(reg,  0, offset, val);		\
	READ_WB_REG_CASE(reg,  1, offset, val);		\
	READ_WB_REG_CASE(reg,  2, offset, val);		\
	READ_WB_REG_CASE(reg,  3, offset, val);		\
	READ_WB_REG_CASE(reg,  4, offset, val);		\
	READ_WB_REG_CASE(reg,  5, offset, val);		\
	READ_WB_REG_CASE(reg,  6, offset, val);		\
	READ_WB_REG_CASE(reg,  7, offset, val);		\
	READ_WB_REG_CASE(reg,  8, offset, val);		\
	READ_WB_REG_CASE(reg,  9, offset, val);		\
	READ_WB_REG_CASE(reg, 10, offset, val);		\
	READ_WB_REG_CASE(reg, 11, offset, val);		\
	READ_WB_REG_CASE(reg, 12, offset, val);		\
	READ_WB_REG_CASE(reg, 13, offset, val);		\
	READ_WB_REG_CASE(reg, 14, offset, val);		\
	READ_WB_REG_CASE(reg, 15, offset, val)

#define SWITCH_CASES_WRITE_WB_REG(reg, offset, val)	\
	WRITE_WB_REG_CASE(reg,  0, offset, val);	\
	WRITE_WB_REG_CASE(reg,  1, offset, val);	\
	WRITE_WB_REG_CASE(reg,  2, offset, val);	\
	WRITE_WB_REG_CASE(reg,  3, offset, val);	\
	WRITE_WB_REG_CASE(reg,  4, offset, val);	\
	WRITE_WB_REG_CASE(reg,  5, offset, val);	\
	WRITE_WB_REG_CASE(reg,  6, offset, val);	\
	WRITE_WB_REG_CASE(reg,  7, offset, val);	\
	WRITE_WB_REG_CASE(reg,  8, offset, val);	\
	WRITE_WB_REG_CASE(reg,  9, offset, val);	\
	WRITE_WB_REG_CASE(reg, 10, offset, val);	\
	WRITE_WB_REG_CASE(reg, 11, offset, val);	\
	WRITE_WB_REG_CASE(reg, 12, offset, val);	\
	WRITE_WB_REG_CASE(reg, 13, offset, val);	\
	WRITE_WB_REG_CASE(reg, 14, offset, val);	\
	WRITE_WB_REG_CASE(reg, 15, offset, val)

#ifdef DDB
static uint64_t
dbg_wb_read_reg(int reg, int n)
{
	uint64_t val = 0;

	switch (reg + n) {
	SWITCH_CASES_READ_WB_REG(DBG_WB_WVR, DBG_REG_BASE_WVR, val);
	SWITCH_CASES_READ_WB_REG(DBG_WB_WCR, DBG_REG_BASE_WCR, val);
	SWITCH_CASES_READ_WB_REG(DBG_WB_BVR, DBG_REG_BASE_BVR, val);
	SWITCH_CASES_READ_WB_REG(DBG_WB_BCR, DBG_REG_BASE_BCR, val);
	default:
		printf("trying to read from wrong debug register %d\n", n);
	}

	return val;
}
#endif /* DDB */

static void
dbg_wb_write_reg(int reg, int n, uint64_t val)
{
	switch (reg + n) {
	SWITCH_CASES_WRITE_WB_REG(DBG_WB_WVR, DBG_REG_BASE_WVR, val);
	SWITCH_CASES_WRITE_WB_REG(DBG_WB_WCR, DBG_REG_BASE_WCR, val);
	SWITCH_CASES_WRITE_WB_REG(DBG_WB_BVR, DBG_REG_BASE_BVR, val);
	SWITCH_CASES_WRITE_WB_REG(DBG_WB_BCR, DBG_REG_BASE_BCR, val);
	default:
		printf("trying to write to wrong debug register %d\n", n);
		return;
	}
	isb();
}

#if defined(DDB) || defined(GDB)
void
kdb_cpu_set_singlestep(void)
{

	KASSERT((READ_SPECIALREG(daif) & PSR_D) == PSR_D,
	    ("%s: debug exceptions are not masked", __func__));

	kdb_frame->tf_spsr |= PSR_SS;
	WRITE_SPECIALREG(mdscr_el1, READ_SPECIALREG(mdscr_el1) |
	    MDSCR_SS | MDSCR_KDE);

	/*
	 * Disable breakpoints and watchpoints, e.g. stepping
	 * over watched instruction will trigger break exception instead of
	 * single-step exception and locks CPU on that instruction for ever.
	 */
	if ((kernel_monitor.dbg_flags & DBGMON_ENABLED) != 0) {
		WRITE_SPECIALREG(mdscr_el1,
		    READ_SPECIALREG(mdscr_el1) & ~MDSCR_MDE);
	}
}

void
kdb_cpu_clear_singlestep(void)
{

	KASSERT((READ_SPECIALREG(daif) & PSR_D) == PSR_D,
	    ("%s: debug exceptions are not masked", __func__));

	WRITE_SPECIALREG(mdscr_el1, READ_SPECIALREG(mdscr_el1) &
	    ~(MDSCR_SS | MDSCR_KDE));

	/* Restore breakpoints and watchpoints */
	if ((kernel_monitor.dbg_flags & DBGMON_ENABLED) != 0) {
		WRITE_SPECIALREG(mdscr_el1,
		    READ_SPECIALREG(mdscr_el1) | MDSCR_MDE);

		if ((kernel_monitor.dbg_flags & DBGMON_KERNEL) != 0) {
			WRITE_SPECIALREG(mdscr_el1,
			    READ_SPECIALREG(mdscr_el1) | MDSCR_KDE);
		}
	}
}

int
kdb_cpu_set_watchpoint(vm_offset_t addr, vm_size_t size, int access)
{
	enum dbg_access_t dbg_access;

	switch (access) {
	case KDB_DBG_ACCESS_R:
		dbg_access = HW_BREAKPOINT_R;
		break;
	case KDB_DBG_ACCESS_W:
		dbg_access = HW_BREAKPOINT_W;
		break;
	case KDB_DBG_ACCESS_RW:
		dbg_access = HW_BREAKPOINT_RW;
		break;
	default:
		return (EINVAL);
	}

	return (dbg_setup_watchpoint(NULL, addr, size, dbg_access));
}

int
kdb_cpu_clr_watchpoint(vm_offset_t addr, vm_size_t size)
{

	return (dbg_remove_watchpoint(NULL, addr, size));
}
#endif /* DDB || GDB */

#ifdef DDB
static const char *
dbg_watchtype_str(uint32_t type)
{
	switch (type) {
		case DBG_WATCH_CTRL_EXEC:
			return ("execute");
		case DBG_WATCH_CTRL_STORE:
			return ("write");
		case DBG_WATCH_CTRL_LOAD:
			return ("read");
		case DBG_WATCH_CTRL_LOAD | DBG_WATCH_CTRL_STORE:
			return ("read/write");
		default:
			return ("invalid");
	}
}

static int
dbg_watchtype_len(uint32_t len)
{
	switch (len) {
	case DBG_WATCH_CTRL_LEN_1:
		return (1);
	case DBG_WATCH_CTRL_LEN_2:
		return (2);
	case DBG_WATCH_CTRL_LEN_4:
		return (4);
	case DBG_WATCH_CTRL_LEN_8:
		return (8);
	default:
		return (0);
	}
}

void
dbg_show_watchpoint(void)
{
	uint32_t wcr, len, type;
	uint64_t addr;
	int i;

	db_printf("\nhardware watchpoints:\n");
	db_printf("  watch    status        type  len             address              symbol\n");
	db_printf("  -----  --------  ----------  ---  ------------------  ------------------\n");
	for (i = 0; i < dbg_watchpoint_num; i++) {
		wcr = dbg_wb_read_reg(DBG_REG_BASE_WCR, i);
		if ((wcr & DBG_WB_CTRL_E) != 0) {
			type = DBG_WATCH_CTRL_ACCESS_MASK(wcr);
			len = DBG_WATCH_CTRL_LEN_MASK(wcr);
			addr = dbg_wb_read_reg(DBG_REG_BASE_WVR, i);
			db_printf("  %-5d  %-8s  %10s  %3d  0x%16lx  ",
			    i, "enabled", dbg_watchtype_str(type),
			    dbg_watchtype_len(len), addr);
			db_printsym((db_addr_t)addr, DB_STGY_ANY);
			db_printf("\n");
		} else {
			db_printf("  %-5d  disabled\n", i);
		}
	}
}
#endif /* DDB */

static int
dbg_find_free_slot(struct debug_monitor_state *monitor, enum dbg_t type)
{
	uint64_t *reg;
	u_int max, i;

	switch(type) {
	case DBG_TYPE_BREAKPOINT:
		max = dbg_breakpoint_num;
		reg = monitor->dbg_bcr;
		break;
	case DBG_TYPE_WATCHPOINT:
		max = dbg_watchpoint_num;
		reg = monitor->dbg_wcr;
		break;
	default:
		printf("Unsupported debug type\n");
		return (i);
	}

	for (i = 0; i < max; i++) {
		if ((reg[i] & DBG_WB_CTRL_E) == 0)
			return (i);
	}

	return (-1);
}

static int
dbg_find_slot(struct debug_monitor_state *monitor, enum dbg_t type,
    vm_offset_t addr)
{
	uint64_t *reg_addr, *reg_ctrl;
	u_int max, i;

	switch(type) {
	case DBG_TYPE_BREAKPOINT:
		max = dbg_breakpoint_num;
		reg_addr = monitor->dbg_bvr;
		reg_ctrl = monitor->dbg_bcr;
		break;
	case DBG_TYPE_WATCHPOINT:
		max = dbg_watchpoint_num;
		reg_addr = monitor->dbg_wvr;
		reg_ctrl = monitor->dbg_wcr;
		break;
	default:
		printf("Unsupported debug type\n");
		return (i);
	}

	for (i = 0; i < max; i++) {
		if (reg_addr[i] == addr &&
		    (reg_ctrl[i] & DBG_WB_CTRL_E) != 0)
			return (i);
	}

	return (-1);
}

int
dbg_setup_watchpoint(struct debug_monitor_state *monitor, vm_offset_t addr,
    vm_size_t size, enum dbg_access_t access)
{
	uint64_t wcr_size, wcr_priv, wcr_access;
	u_int i;

	if (monitor == NULL)
		monitor = &kernel_monitor;

	i = dbg_find_free_slot(monitor, DBG_TYPE_WATCHPOINT);
	if (i == -1) {
		printf("Can not find slot for watchpoint, max %d"
		    " watchpoints supported\n", dbg_watchpoint_num);
		return (EBUSY);
	}

	switch(size) {
	case 1:
		wcr_size = DBG_WATCH_CTRL_LEN_1;
		break;
	case 2:
		wcr_size = DBG_WATCH_CTRL_LEN_2;
		break;
	case 4:
		wcr_size = DBG_WATCH_CTRL_LEN_4;
		break;
	case 8:
		wcr_size = DBG_WATCH_CTRL_LEN_8;
		break;
	default:
		printf("Unsupported address size for watchpoint: %zu\n", size);
		return (EINVAL);
	}

	if ((monitor->dbg_flags & DBGMON_KERNEL) == 0)
		wcr_priv = DBG_WB_CTRL_EL0;
	else
		wcr_priv = DBG_WB_CTRL_EL1;

	switch(access) {
	case HW_BREAKPOINT_X:
		wcr_access = DBG_WATCH_CTRL_EXEC;
		break;
	case HW_BREAKPOINT_R:
		wcr_access = DBG_WATCH_CTRL_LOAD;
		break;
	case HW_BREAKPOINT_W:
		wcr_access = DBG_WATCH_CTRL_STORE;
		break;
	case HW_BREAKPOINT_RW:
		wcr_access = DBG_WATCH_CTRL_LOAD | DBG_WATCH_CTRL_STORE;
		break;
	default:
		printf("Unsupported access type for watchpoint: %d\n", access);
		return (EINVAL);
	}

	monitor->dbg_wvr[i] = addr;
	monitor->dbg_wcr[i] = wcr_size | wcr_access | wcr_priv | DBG_WB_CTRL_E;
	monitor->dbg_enable_count++;
	monitor->dbg_flags |= DBGMON_ENABLED;

	dbg_register_sync(monitor);
	return (0);
}

int
dbg_remove_watchpoint(struct debug_monitor_state *monitor, vm_offset_t addr,
    vm_size_t size)
{
	u_int i;

	if (monitor == NULL)
		monitor = &kernel_monitor;

	i = dbg_find_slot(monitor, DBG_TYPE_WATCHPOINT, addr);
	if (i == -1) {
		printf("Can not find watchpoint for address 0%lx\n", addr);
		return (EINVAL);
	}

	monitor->dbg_wvr[i] = 0;
	monitor->dbg_wcr[i] = 0;
	monitor->dbg_enable_count--;
	if (monitor->dbg_enable_count == 0)
		monitor->dbg_flags &= ~DBGMON_ENABLED;

	dbg_register_sync(monitor);
	return (0);
}

void
dbg_register_sync(struct debug_monitor_state *monitor)
{
	uint64_t mdscr;
	int i;

	if (monitor == NULL)
		monitor = &kernel_monitor;

	mdscr = READ_SPECIALREG(mdscr_el1);
	if ((monitor->dbg_flags & DBGMON_ENABLED) == 0) {
		mdscr &= ~(MDSCR_MDE | MDSCR_KDE);
	} else {
		for (i = 0; i < dbg_breakpoint_num; i++) {
			dbg_wb_write_reg(DBG_REG_BASE_BCR, i,
			    monitor->dbg_bcr[i]);
			dbg_wb_write_reg(DBG_REG_BASE_BVR, i,
			    monitor->dbg_bvr[i]);
		}

		for (i = 0; i < dbg_watchpoint_num; i++) {
			dbg_wb_write_reg(DBG_REG_BASE_WCR, i,
			    monitor->dbg_wcr[i]);
			dbg_wb_write_reg(DBG_REG_BASE_WVR, i,
			    monitor->dbg_wvr[i]);
		}
		mdscr |= MDSCR_MDE;
		if ((monitor->dbg_flags & DBGMON_KERNEL) == DBGMON_KERNEL)
			mdscr |= MDSCR_KDE;
	}
	WRITE_SPECIALREG(mdscr_el1, mdscr);
	isb();
}

void
dbg_monitor_init(void)
{
	uint64_t aa64dfr0;
	u_int i;

	/* Find out many breakpoints and watchpoints we can use */
	aa64dfr0 = READ_SPECIALREG(id_aa64dfr0_el1);
	dbg_watchpoint_num = ID_AA64DFR0_WRPs_VAL(aa64dfr0);
	dbg_breakpoint_num = ID_AA64DFR0_BRPs_VAL(aa64dfr0);

	if (bootverbose && PCPU_GET(cpuid) == 0) {
		printf("%d watchpoints and %d breakpoints supported\n",
		    dbg_watchpoint_num, dbg_breakpoint_num);
	}

	/*
	 * We have limited number of {watch,break}points, each consists of
	 * two registers:
	 * - wcr/bcr regsiter configurates corresponding {watch,break}point
	 *   behaviour
	 * - wvr/bvr register keeps address we are hunting for
	 *
	 * Reset all breakpoints and watchpoints.
	 */
	for (i = 0; i < dbg_watchpoint_num; i++) {
		dbg_wb_write_reg(DBG_REG_BASE_WCR, i, 0);
		dbg_wb_write_reg(DBG_REG_BASE_WVR, i, 0);
	}

	for (i = 0; i < dbg_breakpoint_num; i++) {
		dbg_wb_write_reg(DBG_REG_BASE_BCR, i, 0);
		dbg_wb_write_reg(DBG_REG_BASE_BVR, i, 0);
	}

	dbg_enable();
}

void
dbg_monitor_enter(struct thread *thread)
{
	int i;

	if ((kernel_monitor.dbg_flags & DBGMON_ENABLED) != 0) {
		/* Install the kernel version of the registers */
		dbg_register_sync(&kernel_monitor);
	} else if ((thread->td_pcb->pcb_dbg_regs.dbg_flags & DBGMON_ENABLED) != 0) {
		/* Disable the user breakpoints until we return to userspace */
		for (i = 0; i < dbg_watchpoint_num; i++) {
			dbg_wb_write_reg(DBG_REG_BASE_WCR, i, 0);
			dbg_wb_write_reg(DBG_REG_BASE_WVR, i, 0);
		}

		for (i = 0; i < dbg_breakpoint_num; ++i) {
			dbg_wb_write_reg(DBG_REG_BASE_BCR, i, 0);
			dbg_wb_write_reg(DBG_REG_BASE_BVR, i, 0);
		}
		WRITE_SPECIALREG(mdscr_el1,
		    READ_SPECIALREG(mdscr_el1) & ~(MDSCR_MDE | MDSCR_KDE));
		isb();
	}
}

void
dbg_monitor_exit(struct thread *thread, struct trapframe *frame)
{
	int i;

	/*
	 * PSR_D is an aarch64-only flag. On aarch32, it switches
	 * the processor to big-endian, so avoid setting it for
	 * 32bits binaries.
	 */
	if (!(SV_PROC_FLAG(thread->td_proc, SV_ILP32)))
		frame->tf_spsr |= PSR_D;
	if ((thread->td_pcb->pcb_dbg_regs.dbg_flags & DBGMON_ENABLED) != 0) {
		/* Install the thread's version of the registers */
		dbg_register_sync(&thread->td_pcb->pcb_dbg_regs);
		frame->tf_spsr &= ~PSR_D;
	} else if ((kernel_monitor.dbg_flags & DBGMON_ENABLED) != 0) {
		/* Disable the kernel breakpoints until we re-enter */
		for (i = 0; i < dbg_watchpoint_num; i++) {
			dbg_wb_write_reg(DBG_REG_BASE_WCR, i, 0);
			dbg_wb_write_reg(DBG_REG_BASE_WVR, i, 0);
		}

		for (i = 0; i < dbg_breakpoint_num; ++i) {
			dbg_wb_write_reg(DBG_REG_BASE_BCR, i, 0);
			dbg_wb_write_reg(DBG_REG_BASE_BVR, i, 0);
		}
		WRITE_SPECIALREG(mdscr_el1,
		    READ_SPECIALREG(mdscr_el1) & ~(MDSCR_MDE | MDSCR_KDE));
		isb();
	}
}
