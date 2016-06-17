/*
 * linux/kernel/context.c
 *
 * Mechanism for running arbitrary tasks in process context
 *
 * dwmw2@redhat.com:		Genesis
 *
 * andrewm@uow.edu.au:		2.4.0-test12
 *	- Child reaping
 *	- Support for tasks which re-add themselves
 *	- flush_scheduled_tasks.
 */

#define __KERNEL_SYSCALLS__

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/init.h>
#include <linux/unistd.h>
#include <linux/signal.h>
#include <linux/completion.h>

static DECLARE_TASK_QUEUE(tq_context);
static DECLARE_WAIT_QUEUE_HEAD(context_task_wq);
static DECLARE_WAIT_QUEUE_HEAD(context_task_done);
static int keventd_running;
static struct task_struct *keventd_task;

static int need_keventd(const char *who)
{
	if (keventd_running == 0)
		printk(KERN_ERR "%s(): keventd has not started\n", who);
	return keventd_running;
}
	
int current_is_keventd(void)
{
	int ret = 0;
	if (need_keventd(__FUNCTION__))
		ret = (current == keventd_task);
	return ret;
}

/**
 * schedule_task - schedule a function for subsequent execution in process context.
 * @task: pointer to a &tq_struct which defines the function to be scheduled.
 *
 * May be called from interrupt context.  The scheduled function is run at some
 * time in the near future by the keventd kernel thread.  If it can sleep, it
 * should be designed to do so for the minimum possible time, as it will be
 * stalling all other scheduled tasks.
 *
 * schedule_task() returns non-zero if the task was successfully scheduled.
 * If @task is already residing on a task queue then schedule_task() fails
 * to schedule your task and returns zero.
 */
int schedule_task(struct tq_struct *task)
{
	int ret;
	need_keventd(__FUNCTION__);
	ret = queue_task(task, &tq_context);
	wake_up(&context_task_wq);
	return ret;
}

static int context_thread(void *startup)
{
	struct task_struct *curtask = current;
	DECLARE_WAITQUEUE(wait, curtask);
	struct k_sigaction sa;

	daemonize();
	strcpy(curtask->comm, "keventd");
	keventd_running = 1;
	keventd_task = curtask;

	spin_lock_irq(&curtask->sigmask_lock);
	siginitsetinv(&curtask->blocked, sigmask(SIGCHLD));
	recalc_sigpending(curtask);
	spin_unlock_irq(&curtask->sigmask_lock);

	complete((struct completion *)startup);

	/* Install a handler so SIGCLD is delivered */
	sa.sa.sa_handler = SIG_IGN;
	sa.sa.sa_flags = 0;
	siginitset(&sa.sa.sa_mask, sigmask(SIGCHLD));
	do_sigaction(SIGCHLD, &sa, (struct k_sigaction *)0);

	/*
	 * If one of the functions on a task queue re-adds itself
	 * to the task queue we call schedule() in state TASK_RUNNING
	 */
	for (;;) {
		set_task_state(curtask, TASK_INTERRUPTIBLE);
		add_wait_queue(&context_task_wq, &wait);
		if (TQ_ACTIVE(tq_context))
			set_task_state(curtask, TASK_RUNNING);
		schedule();
		remove_wait_queue(&context_task_wq, &wait);
		run_task_queue(&tq_context);
		wake_up(&context_task_done);
		if (signal_pending(curtask)) {
			while (waitpid(-1, (unsigned int *)0, __WALL|WNOHANG) > 0)
				;
			spin_lock_irq(&curtask->sigmask_lock);
			flush_signals(curtask);
			recalc_sigpending(curtask);
			spin_unlock_irq(&curtask->sigmask_lock);
		}
	}
}

/**
 * flush_scheduled_tasks - ensure that any scheduled tasks have run to completion.
 *
 * Forces execution of the schedule_task() queue and blocks until its completion.
 *
 * If a kernel subsystem uses schedule_task() and wishes to flush any pending
 * tasks, it should use this function.  This is typically used in driver shutdown
 * handlers.
 *
 * The caller should hold no spinlocks and should hold no semaphores which could
 * cause the scheduled tasks to block.
 */
static struct tq_struct dummy_task;

void flush_scheduled_tasks(void)
{
	int count;
	DECLARE_WAITQUEUE(wait, current);

	/*
	 * Do it twice. It's possible, albeit highly unlikely, that
	 * the caller queued a task immediately before calling us,
	 * and that the eventd thread was already past the run_task_queue()
	 * but not yet into wake_up(), so it woke us up before completing
	 * the caller's queued task or our new dummy task.
	 */
	add_wait_queue(&context_task_done, &wait);
	for (count = 0; count < 2; count++) {
		set_current_state(TASK_UNINTERRUPTIBLE);

		/* Queue a dummy task to make sure we get kicked */
		schedule_task(&dummy_task);

		/* Wait for it to complete */
		schedule();
	}
	remove_wait_queue(&context_task_done, &wait);
}
	
int start_context_thread(void)
{
	static struct completion startup __initdata = COMPLETION_INITIALIZER(startup);

	kernel_thread(context_thread, &startup, CLONE_FS | CLONE_FILES);
	wait_for_completion(&startup);
	return 0;
}

EXPORT_SYMBOL(schedule_task);
EXPORT_SYMBOL(flush_scheduled_tasks);

