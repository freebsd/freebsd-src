/*
 * Generate definitions needed by assembly language modules.
 * This code generates raw asm output which is post-processed to extract
 * and format the required data.
 */

#include <linux/config.h>
#include <linux/sched.h>

/* Use marker if you need to separate the values later */

#define DEFINE(sym, val, marker) \
	asm volatile("\n->" #sym " %0 " #val " " #marker : : "i" (val))

#define BLANK() asm volatile("\n->" : : )

int main(void)
{
	DEFINE(__THREAD_ar2, offsetof(struct task_struct, thread.ar2),);
	DEFINE(__THREAD_ar4, offsetof(struct task_struct, thread.ar4),);
	DEFINE(__THREAD_ksp, offsetof(struct task_struct, thread.ksp),);
	DEFINE(__THREAD_per, offsetof(struct task_struct, thread.per_info),);
	BLANK();
	DEFINE(__TASK_state, offsetof(struct task_struct, state),);
	DEFINE(__TASK_sigpending, offsetof(struct task_struct, sigpending),);
	DEFINE(__TASK_need_resched,
	       offsetof(struct task_struct, need_resched),);
	DEFINE(__TASK_ptrace, offsetof(struct task_struct, ptrace),);
	DEFINE(__TASK_processor, offsetof(struct task_struct, processor),);

	return 0;
}
