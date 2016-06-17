#include <linux/mm.h>
#include <linux/sched.h>

#include <asm/pgtable.h>
#include <asm/uaccess.h>
#include <asm/processor.h>

static struct fs_struct init_fs = INIT_FS;
static struct files_struct init_files = INIT_FILES;
static struct signal_struct init_signals = INIT_SIGNALS;
struct mm_struct init_mm = INIT_MM(init_mm);

/* .text section in head.S is aligned at 2 page boundry and this gets linked
 * right after that so that the init_task_union is aligned properly as well.
 * We really don't need this special alignment like the Intel does, but
 * I do it anyways for completeness.
 */
__asm__ (".text");
union task_union init_task_union = { INIT_TASK(init_task_union.task) };

/*
 * This is to make the init_task+stack of the right size for >8k pagesize.
 * The definition of task_union in sched.h makes it 16k wide.
 */
#if PAGE_SHIFT != 13
char init_task_stack[THREAD_SIZE - INIT_TASK_SIZE] = { 0 };
#endif
