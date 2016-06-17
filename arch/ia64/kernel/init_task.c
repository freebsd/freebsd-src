/*
 * This is where we statically allocate and initialize the initial
 * task.
 *
 * Copyright (C) 1999 Hewlett-Packard Co
 * Copyright (C) 1999 David Mosberger-Tang <davidm@hpl.hp.com>
 */

#include <linux/init.h>
#include <linux/mm.h>
#include <linux/sched.h>

#include <asm/uaccess.h>
#include <asm/pgtable.h>

static struct fs_struct init_fs = INIT_FS;
static struct files_struct init_files = INIT_FILES;
static struct signal_struct init_signals = INIT_SIGNALS;
struct mm_struct init_mm = INIT_MM(init_mm);

/*
 * Initial task structure.
 *
 * We need to make sure that this is page aligned due to the way
 * process stacks are handled. This is done by having a special
 * "init_task" linker map entry..
 */
union task_union init_task_union
	__attribute__((section("init_task"))) =
		{ INIT_TASK(init_task_union.task) };
