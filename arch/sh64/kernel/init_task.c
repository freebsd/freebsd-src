/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * arch/sh64/kernel/init_task.c
 *
 * Copyright (C) 2000, 2001  Paolo Alberelli
 *
 */

#include <linux/mm.h>
#include <linux/sched.h>
#include <linux/init.h>

#include <asm/uaccess.h>
#include <asm/pgtable.h>

static struct fs_struct init_fs = INIT_FS;
static struct files_struct init_files = INIT_FILES;
static struct signal_struct init_signals = INIT_SIGNALS;
struct mm_struct init_mm = INIT_MM(init_mm);


struct pt_regs fake_swapper_regs;

/*
 * Initial task structure.
 *
 * We need to make sure that this is INIT_TASK_SIZE-byte aligned due
 * to the way process stacks are handled. This is done by having a
 * special "init_task" linker map entry..
 */
union task_union init_task_union 
	__attribute__((__section__(".data.init_task"))) =
		{ INIT_TASK(init_task_union.task) };
