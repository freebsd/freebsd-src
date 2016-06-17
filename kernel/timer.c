/*
 *  linux/kernel/timer.c
 *
 *  Kernel internal timers, kernel timekeeping, basic process system calls
 *
 *  Copyright (C) 1991, 1992  Linus Torvalds
 *
 *  1997-01-28  Modified by Finn Arne Gangstad to make timers scale better.
 *
 *  1997-09-10  Updated NTP code according to technical memorandum Jan '96
 *              "A Kernel Model for Precision Timekeeping" by Dave Mills
 *  1998-12-24  Fixed a xtime SMP race (we need the xtime_lock rw spinlock to
 *              serialize accesses to xtime/lost_ticks).
 *                              Copyright (C) 1998  Andrea Arcangeli
 *  1999-03-10  Improved NTP compatibility by Ulrich Windl
 */

#include <linux/config.h>
#include <linux/mm.h>
#include <linux/timex.h>
#include <linux/delay.h>
#include <linux/smp_lock.h>
#include <linux/interrupt.h>
#include <linux/kernel_stat.h>

#include <asm/uaccess.h>

/*
 * Timekeeping variables
 */

long tick = (1000000 + HZ/2) / HZ;	/* timer interrupt period */

/* The current time */
struct timeval xtime __attribute__ ((aligned (16)));

/* Don't completely fail for HZ > 500.  */
int tickadj = 500/HZ ? : 1;		/* microsecs */

DECLARE_TASK_QUEUE(tq_timer);
DECLARE_TASK_QUEUE(tq_immediate);

/*
 * phase-lock loop variables
 */
/* TIME_ERROR prevents overwriting the CMOS clock */
int time_state = TIME_OK;		/* clock synchronization status	*/
int time_status = STA_UNSYNC;		/* clock status bits		*/
long time_offset;			/* time adjustment (us)		*/
long time_constant = 2;			/* pll time constant		*/
long time_tolerance = MAXFREQ;		/* frequency tolerance (ppm)	*/
long time_precision = 1;		/* clock precision (us)		*/
long time_maxerror = NTP_PHASE_LIMIT;	/* maximum error (us)		*/
long time_esterror = NTP_PHASE_LIMIT;	/* estimated error (us)		*/
long time_phase;			/* phase offset (scaled us)	*/
long time_freq = ((1000000 + HZ/2) % HZ - HZ/2) << SHIFT_USEC;
					/* frequency offset (scaled ppm)*/
long time_adj;				/* tick adjust (scaled 1 / HZ)	*/
long time_reftime;			/* time at last adjustment (s)	*/

long time_adjust;
long time_adjust_step;

unsigned long event;

extern int do_setitimer(int, struct itimerval *, struct itimerval *);

unsigned long volatile jiffies;

unsigned int * prof_buffer;
unsigned long prof_len;
unsigned long prof_shift;

/*
 * Event timer code
 */
#define TVN_BITS 6
#define TVR_BITS 8
#define TVN_SIZE (1 << TVN_BITS)
#define TVR_SIZE (1 << TVR_BITS)
#define TVN_MASK (TVN_SIZE - 1)
#define TVR_MASK (TVR_SIZE - 1)

struct timer_vec {
	int index;
	struct list_head vec[TVN_SIZE];
};

struct timer_vec_root {
	int index;
	struct list_head vec[TVR_SIZE];
};

static struct timer_vec tv5;
static struct timer_vec tv4;
static struct timer_vec tv3;
static struct timer_vec tv2;
static struct timer_vec_root tv1;

static struct timer_vec * const tvecs[] = {
	(struct timer_vec *)&tv1, &tv2, &tv3, &tv4, &tv5
};

static struct list_head * run_timer_list_running;

#define NOOF_TVECS (sizeof(tvecs) / sizeof(tvecs[0]))

void init_timervecs (void)
{
	int i;

	for (i = 0; i < TVN_SIZE; i++) {
		INIT_LIST_HEAD(tv5.vec + i);
		INIT_LIST_HEAD(tv4.vec + i);
		INIT_LIST_HEAD(tv3.vec + i);
		INIT_LIST_HEAD(tv2.vec + i);
	}
	for (i = 0; i < TVR_SIZE; i++)
		INIT_LIST_HEAD(tv1.vec + i);
}

static unsigned long timer_jiffies;

static inline void internal_add_timer(struct timer_list *timer)
{
	/*
	 * must be cli-ed when calling this
	 */
	unsigned long expires = timer->expires;
	unsigned long idx = expires - timer_jiffies;
	struct list_head * vec;

	if (run_timer_list_running)
		vec = run_timer_list_running;
	else if (idx < TVR_SIZE) {
		int i = expires & TVR_MASK;
		vec = tv1.vec + i;
	} else if (idx < 1 << (TVR_BITS + TVN_BITS)) {
		int i = (expires >> TVR_BITS) & TVN_MASK;
		vec = tv2.vec + i;
	} else if (idx < 1 << (TVR_BITS + 2 * TVN_BITS)) {
		int i = (expires >> (TVR_BITS + TVN_BITS)) & TVN_MASK;
		vec =  tv3.vec + i;
	} else if (idx < 1 << (TVR_BITS + 3 * TVN_BITS)) {
		int i = (expires >> (TVR_BITS + 2 * TVN_BITS)) & TVN_MASK;
		vec = tv4.vec + i;
	} else if ((signed long) idx < 0) {
		/* can happen if you add a timer with expires == jiffies,
		 * or you set a timer to go off in the past
		 */
		vec = tv1.vec + tv1.index;
	} else if (idx <= 0xffffffffUL) {
		int i = (expires >> (TVR_BITS + 3 * TVN_BITS)) & TVN_MASK;
		vec = tv5.vec + i;
	} else {
		/* Can only get here on architectures with 64-bit jiffies */
		INIT_LIST_HEAD(&timer->list);
		return;
	}
	/*
	 * Timers are FIFO!
	 */
	list_add(&timer->list, vec->prev);
}

/* Initialize both explicitly - let's try to have them in the same cache line */
spinlock_t timerlist_lock = SPIN_LOCK_UNLOCKED;

#ifdef CONFIG_SMP
volatile struct timer_list * volatile running_timer;
#define timer_enter(t) do { running_timer = t; mb(); } while (0)
#define timer_exit() do { running_timer = NULL; } while (0)
#define timer_is_running(t) (running_timer == t)
#define timer_synchronize(t) while (timer_is_running(t)) barrier()
#else
#define timer_enter(t)		do { } while (0)
#define timer_exit()		do { } while (0)
#endif

void add_timer(struct timer_list *timer)
{
	unsigned long flags;

	spin_lock_irqsave(&timerlist_lock, flags);
	if (timer_pending(timer))
		goto bug;
	internal_add_timer(timer);
	spin_unlock_irqrestore(&timerlist_lock, flags);
	return;
bug:
	spin_unlock_irqrestore(&timerlist_lock, flags);
	printk("bug: kernel timer added twice at %p.\n",
			__builtin_return_address(0));
}

static inline int detach_timer (struct timer_list *timer)
{
	if (!timer_pending(timer))
		return 0;
	list_del(&timer->list);
	return 1;
}

int mod_timer(struct timer_list *timer, unsigned long expires)
{
	int ret;
	unsigned long flags;

	spin_lock_irqsave(&timerlist_lock, flags);
	timer->expires = expires;
	ret = detach_timer(timer);
	internal_add_timer(timer);
	spin_unlock_irqrestore(&timerlist_lock, flags);
	return ret;
}

int del_timer(struct timer_list * timer)
{
	int ret;
	unsigned long flags;

	spin_lock_irqsave(&timerlist_lock, flags);
	ret = detach_timer(timer);
	timer->list.next = timer->list.prev = NULL;
	spin_unlock_irqrestore(&timerlist_lock, flags);
	return ret;
}

#ifdef CONFIG_SMP
void sync_timers(void)
{
	spin_unlock_wait(&global_bh_lock);
}

/*
 * SMP specific function to delete periodic timer.
 * Caller must disable by some means restarting the timer
 * for new. Upon exit the timer is not queued and handler is not running
 * on any CPU. It returns number of times, which timer was deleted
 * (for reference counting).
 */

int del_timer_sync(struct timer_list * timer)
{
	int ret = 0;

	for (;;) {
		unsigned long flags;
		int running;

		spin_lock_irqsave(&timerlist_lock, flags);
		ret += detach_timer(timer);
		timer->list.next = timer->list.prev = 0;
		running = timer_is_running(timer);
		spin_unlock_irqrestore(&timerlist_lock, flags);

		if (!running)
			break;

		timer_synchronize(timer);
	}

	return ret;
}
#endif


static inline void cascade_timers(struct timer_vec *tv)
{
	/* cascade all the timers from tv up one level */
	struct list_head *head, *curr, *next;

	head = tv->vec + tv->index;
	curr = head->next;
	/*
	 * We are removing _all_ timers from the list, so we don't  have to
	 * detach them individually, just clear the list afterwards.
	 */
	while (curr != head) {
		struct timer_list *tmp;

		tmp = list_entry(curr, struct timer_list, list);
		next = curr->next;
		list_del(curr); // not needed
		internal_add_timer(tmp);
		curr = next;
	}
	INIT_LIST_HEAD(head);
	tv->index = (tv->index + 1) & TVN_MASK;
}

static inline void run_timer_list(void)
{
	spin_lock_irq(&timerlist_lock);
	while ((long)(jiffies - timer_jiffies) >= 0) {
		LIST_HEAD(queued);
		struct list_head *head, *curr;
		if (!tv1.index) {
			int n = 1;
			do {
				cascade_timers(tvecs[n]);
			} while (tvecs[n]->index == 1 && ++n < NOOF_TVECS);
		}
		run_timer_list_running = &queued;
repeat:
		head = tv1.vec + tv1.index;
		curr = head->next;
		if (curr != head) {
			struct timer_list *timer;
			void (*fn)(unsigned long);
			unsigned long data;

			timer = list_entry(curr, struct timer_list, list);
 			fn = timer->function;
 			data= timer->data;

			detach_timer(timer);
			timer->list.next = timer->list.prev = NULL;
			timer_enter(timer);
			spin_unlock_irq(&timerlist_lock);
			fn(data);
			spin_lock_irq(&timerlist_lock);
			timer_exit();
			goto repeat;
		}
		run_timer_list_running = NULL;
		++timer_jiffies; 
		tv1.index = (tv1.index + 1) & TVR_MASK;

		curr = queued.next;
		while (curr != &queued) {
			struct timer_list *timer;

			timer = list_entry(curr, struct timer_list, list);
			curr = curr->next;
			internal_add_timer(timer);
		}			
	}
	spin_unlock_irq(&timerlist_lock);
}

spinlock_t tqueue_lock = SPIN_LOCK_UNLOCKED;

void tqueue_bh(void)
{
	run_task_queue(&tq_timer);
}

void immediate_bh(void)
{
	run_task_queue(&tq_immediate);
}

/*
 * this routine handles the overflow of the microsecond field
 *
 * The tricky bits of code to handle the accurate clock support
 * were provided by Dave Mills (Mills@UDEL.EDU) of NTP fame.
 * They were originally developed for SUN and DEC kernels.
 * All the kudos should go to Dave for this stuff.
 *
 */
static void second_overflow(void)
{
    long ltemp;

    /* Bump the maxerror field */
    time_maxerror += time_tolerance >> SHIFT_USEC;
    if ( time_maxerror > NTP_PHASE_LIMIT ) {
	time_maxerror = NTP_PHASE_LIMIT;
	time_status |= STA_UNSYNC;
    }

    /*
     * Leap second processing. If in leap-insert state at
     * the end of the day, the system clock is set back one
     * second; if in leap-delete state, the system clock is
     * set ahead one second. The microtime() routine or
     * external clock driver will insure that reported time
     * is always monotonic. The ugly divides should be
     * replaced.
     */
    switch (time_state) {

    case TIME_OK:
	if (time_status & STA_INS)
	    time_state = TIME_INS;
	else if (time_status & STA_DEL)
	    time_state = TIME_DEL;
	break;

    case TIME_INS:
	if (xtime.tv_sec % 86400 == 0) {
	    xtime.tv_sec--;
	    time_state = TIME_OOP;
	    printk(KERN_NOTICE "Clock: inserting leap second 23:59:60 UTC\n");
	}
	break;

    case TIME_DEL:
	if ((xtime.tv_sec + 1) % 86400 == 0) {
	    xtime.tv_sec++;
	    time_state = TIME_WAIT;
	    printk(KERN_NOTICE "Clock: deleting leap second 23:59:59 UTC\n");
	}
	break;

    case TIME_OOP:
	time_state = TIME_WAIT;
	break;

    case TIME_WAIT:
	if (!(time_status & (STA_INS | STA_DEL)))
	    time_state = TIME_OK;
    }

    /*
     * Compute the phase adjustment for the next second. In
     * PLL mode, the offset is reduced by a fixed factor
     * times the time constant. In FLL mode the offset is
     * used directly. In either mode, the maximum phase
     * adjustment for each second is clamped so as to spread
     * the adjustment over not more than the number of
     * seconds between updates.
     */
    if (time_offset < 0) {
	ltemp = -time_offset;
	if (!(time_status & STA_FLL))
	    ltemp >>= SHIFT_KG + time_constant;
	if (ltemp > (MAXPHASE / MINSEC) << SHIFT_UPDATE)
	    ltemp = (MAXPHASE / MINSEC) << SHIFT_UPDATE;
	time_offset += ltemp;
	time_adj = -ltemp << (SHIFT_SCALE - SHIFT_HZ - SHIFT_UPDATE);
    } else {
	ltemp = time_offset;
	if (!(time_status & STA_FLL))
	    ltemp >>= SHIFT_KG + time_constant;
	if (ltemp > (MAXPHASE / MINSEC) << SHIFT_UPDATE)
	    ltemp = (MAXPHASE / MINSEC) << SHIFT_UPDATE;
	time_offset -= ltemp;
	time_adj = ltemp << (SHIFT_SCALE - SHIFT_HZ - SHIFT_UPDATE);
    }

    /*
     * Compute the frequency estimate and additional phase
     * adjustment due to frequency error for the next
     * second. When the PPS signal is engaged, gnaw on the
     * watchdog counter and update the frequency computed by
     * the pll and the PPS signal.
     */
    pps_valid++;
    if (pps_valid == PPS_VALID) {	/* PPS signal lost */
	pps_jitter = MAXTIME;
	pps_stabil = MAXFREQ;
	time_status &= ~(STA_PPSSIGNAL | STA_PPSJITTER |
			 STA_PPSWANDER | STA_PPSERROR);
    }
    ltemp = time_freq + pps_freq;
    if (ltemp < 0)
	time_adj -= -ltemp >>
	    (SHIFT_USEC + SHIFT_HZ - SHIFT_SCALE);
    else
	time_adj += ltemp >>
	    (SHIFT_USEC + SHIFT_HZ - SHIFT_SCALE);

#if HZ == 100
    /* Compensate for (HZ==100) != (1 << SHIFT_HZ).
     * Add 25% and 3.125% to get 128.125; => only 0.125% error (p. 14)
     */
    if (time_adj < 0)
	time_adj -= (-time_adj >> 2) + (-time_adj >> 5);
    else
	time_adj += (time_adj >> 2) + (time_adj >> 5);
#endif
}

/* in the NTP reference this is called "hardclock()" */
static void update_wall_time_one_tick(void)
{
	if ( (time_adjust_step = time_adjust) != 0 ) {
	    /* We are doing an adjtime thing. 
	     *
	     * Prepare time_adjust_step to be within bounds.
	     * Note that a positive time_adjust means we want the clock
	     * to run faster.
	     *
	     * Limit the amount of the step to be in the range
	     * -tickadj .. +tickadj
	     */
	     if (time_adjust > tickadj)
		time_adjust_step = tickadj;
	     else if (time_adjust < -tickadj)
		time_adjust_step = -tickadj;
	     
	    /* Reduce by this step the amount of time left  */
	    time_adjust -= time_adjust_step;
	}
	xtime.tv_usec += tick + time_adjust_step;
	/*
	 * Advance the phase, once it gets to one microsecond, then
	 * advance the tick more.
	 */
	time_phase += time_adj;
	if (time_phase <= -FINEUSEC) {
		long ltemp = -time_phase >> SHIFT_SCALE;
		time_phase += ltemp << SHIFT_SCALE;
		xtime.tv_usec -= ltemp;
	}
	else if (time_phase >= FINEUSEC) {
		long ltemp = time_phase >> SHIFT_SCALE;
		time_phase -= ltemp << SHIFT_SCALE;
		xtime.tv_usec += ltemp;
	}
}

/*
 * Using a loop looks inefficient, but "ticks" is
 * usually just one (we shouldn't be losing ticks,
 * we're doing this this way mainly for interrupt
 * latency reasons, not because we think we'll
 * have lots of lost timer ticks
 */
static void update_wall_time(unsigned long ticks)
{
	do {
		ticks--;
		update_wall_time_one_tick();
	} while (ticks);

	if (xtime.tv_usec >= 1000000) {
	    xtime.tv_usec -= 1000000;
	    xtime.tv_sec++;
	    second_overflow();
	}
}

static inline void do_process_times(struct task_struct *p,
	unsigned long user, unsigned long system)
{
	unsigned long psecs;

	psecs = (p->times.tms_utime += user);
	psecs += (p->times.tms_stime += system);
	if (psecs / HZ > p->rlim[RLIMIT_CPU].rlim_cur) {
		/* Send SIGXCPU every second.. */
		if (!(psecs % HZ))
			send_sig(SIGXCPU, p, 1);
		/* and SIGKILL when we go over max.. */
		if (psecs / HZ > p->rlim[RLIMIT_CPU].rlim_max)
			send_sig(SIGKILL, p, 1);
	}
}

static inline void do_it_virt(struct task_struct * p, unsigned long ticks)
{
	unsigned long it_virt = p->it_virt_value;

	if (it_virt) {
		it_virt -= ticks;
		if (!it_virt) {
			it_virt = p->it_virt_incr;
			send_sig(SIGVTALRM, p, 1);
		}
		p->it_virt_value = it_virt;
	}
}

static inline void do_it_prof(struct task_struct *p)
{
	unsigned long it_prof = p->it_prof_value;

	if (it_prof) {
		if (--it_prof == 0) {
			it_prof = p->it_prof_incr;
			send_sig(SIGPROF, p, 1);
		}
		p->it_prof_value = it_prof;
	}
}

void update_one_process(struct task_struct *p, unsigned long user,
			unsigned long system, int cpu)
{
	p->per_cpu_utime[cpu] += user;
	p->per_cpu_stime[cpu] += system;
	do_process_times(p, user, system);
	do_it_virt(p, user);
	do_it_prof(p);
}	

/*
 * Called from the timer interrupt handler to charge one tick to the current 
 * process.  user_tick is 1 if the tick is user time, 0 for system.
 */
void update_process_times(int user_tick)
{
	struct task_struct *p = current;
	int cpu = smp_processor_id(), system = user_tick ^ 1;

	update_one_process(p, user_tick, system, cpu);
	if (p->pid) {
		if (--p->counter <= 0) {
			p->counter = 0;
			/*
			 * SCHED_FIFO is priority preemption, so this is 
			 * not the place to decide whether to reschedule a
			 * SCHED_FIFO task or not - Bhavesh Davda
			 */
			if (p->policy != SCHED_FIFO) {
				p->need_resched = 1;
			}
		}
		if (p->nice > 0)
			kstat.per_cpu_nice[cpu] += user_tick;
		else
			kstat.per_cpu_user[cpu] += user_tick;
		kstat.per_cpu_system[cpu] += system;
	} else if (local_bh_count(cpu) || local_irq_count(cpu) > 1)
		kstat.per_cpu_system[cpu] += system;
}

/*
 * Nr of active tasks - counted in fixed-point numbers
 */
static unsigned long count_active_tasks(void)
{
	struct task_struct *p;
	unsigned long nr = 0;

	read_lock(&tasklist_lock);
	for_each_task(p) {
		if ((p->state == TASK_RUNNING ||
		     (p->state & TASK_UNINTERRUPTIBLE)))
			nr += FIXED_1;
	}
	read_unlock(&tasklist_lock);
	return nr;
}

/*
 * Hmm.. Changed this, as the GNU make sources (load.c) seems to
 * imply that avenrun[] is the standard name for this kind of thing.
 * Nothing else seems to be standardized: the fractional size etc
 * all seem to differ on different machines.
 */
unsigned long avenrun[3];

static inline void calc_load(unsigned long ticks)
{
	unsigned long active_tasks; /* fixed-point */
	static int count = LOAD_FREQ;

	count -= ticks;
	if (count < 0) {
		count += LOAD_FREQ;
		active_tasks = count_active_tasks();
		CALC_LOAD(avenrun[0], EXP_1, active_tasks);
		CALC_LOAD(avenrun[1], EXP_5, active_tasks);
		CALC_LOAD(avenrun[2], EXP_15, active_tasks);
	}
}

/* jiffies at the most recent update of wall time */
unsigned long wall_jiffies;

/*
 * This spinlock protect us from races in SMP while playing with xtime. -arca
 */
rwlock_t xtime_lock = RW_LOCK_UNLOCKED;

static inline void update_times(void)
{
	unsigned long ticks;

	/*
	 * update_times() is run from the raw timer_bh handler so we
	 * just know that the irqs are locally enabled and so we don't
	 * need to save/restore the flags of the local CPU here. -arca
	 */
	write_lock_irq(&xtime_lock);
	vxtime_lock();

	ticks = jiffies - wall_jiffies;
	if (ticks) {
		wall_jiffies += ticks;
		update_wall_time(ticks);
	}
	vxtime_unlock();
	write_unlock_irq(&xtime_lock);
	calc_load(ticks);
}

void timer_bh(void)
{
	update_times();
	run_timer_list();
}

void do_timer(struct pt_regs *regs)
{
	(*(unsigned long *)&jiffies)++;
#ifndef CONFIG_SMP
	/* SMP process accounting uses the local APIC timer */

	update_process_times(user_mode(regs));
#endif
	mark_bh(TIMER_BH);
	if (TQ_ACTIVE(tq_timer))
		mark_bh(TQUEUE_BH);
}

#if !defined(__alpha__) && !defined(__ia64__)

/*
 * For backwards compatibility?  This can be done in libc so Alpha
 * and all newer ports shouldn't need it.
 */
asmlinkage unsigned long sys_alarm(unsigned int seconds)
{
	struct itimerval it_new, it_old;
	unsigned int oldalarm;

	it_new.it_interval.tv_sec = it_new.it_interval.tv_usec = 0;
	it_new.it_value.tv_sec = seconds;
	it_new.it_value.tv_usec = 0;
	do_setitimer(ITIMER_REAL, &it_new, &it_old);
	oldalarm = it_old.it_value.tv_sec;
	/* ehhh.. We can't return 0 if we have an alarm pending.. */
	/* And we'd better return too much than too little anyway */
	if (it_old.it_value.tv_usec)
		oldalarm++;
	return oldalarm;
}

#endif

#ifndef __alpha__

/*
 * The Alpha uses getxpid, getxuid, and getxgid instead.  Maybe this
 * should be moved into arch/i386 instead?
 */

/**
 * sys_getpid - return the thread group id of the current process
 *
 * Note, despite the name, this returns the tgid not the pid.  The tgid and
 * the pid are identical unless CLONE_THREAD was specified on clone() in
 * which case the tgid is the same in all threads of the same group.
 *
 * This is SMP safe as current->tgid does not change.
 */
asmlinkage long sys_getpid(void)
{
	return current->tgid;
}

/*
 * This is not strictly SMP safe: p_opptr could change
 * from under us. However, rather than getting any lock
 * we can use an optimistic algorithm: get the parent
 * pid, and go back and check that the parent is still
 * the same. If it has changed (which is extremely unlikely
 * indeed), we just try again..
 *
 * NOTE! This depends on the fact that even if we _do_
 * get an old value of "parent", we can happily dereference
 * the pointer: we just can't necessarily trust the result
 * until we know that the parent pointer is valid.
 *
 * The "mb()" macro is a memory barrier - a synchronizing
 * event. It also makes sure that gcc doesn't optimize
 * away the necessary memory references.. The barrier doesn't
 * have to have all that strong semantics: on x86 we don't
 * really require a synchronizing instruction, for example.
 * The barrier is more important for code generation than
 * for any real memory ordering semantics (even if there is
 * a small window for a race, using the old pointer is
 * harmless for a while).
 */
asmlinkage long sys_getppid(void)
{
	int pid;
	struct task_struct * me = current;
	struct task_struct * parent;

	parent = me->p_opptr;
	for (;;) {
		pid = parent->pid;
#if CONFIG_SMP
{
		struct task_struct *old = parent;
		mb();
		parent = me->p_opptr;
		if (old != parent)
			continue;
}
#endif
		break;
	}
	return pid;
}

asmlinkage long sys_getuid(void)
{
	/* Only we change this so SMP safe */
	return current->uid;
}

asmlinkage long sys_geteuid(void)
{
	/* Only we change this so SMP safe */
	return current->euid;
}

asmlinkage long sys_getgid(void)
{
	/* Only we change this so SMP safe */
	return current->gid;
}

asmlinkage long sys_getegid(void)
{
	/* Only we change this so SMP safe */
	return  current->egid;
}

#endif

/* Thread ID - the internal kernel "pid" */
asmlinkage long sys_gettid(void)
{
	return current->pid;
}

asmlinkage long sys_nanosleep(struct timespec *rqtp, struct timespec *rmtp)
{
	struct timespec t;
	unsigned long expire;

	if(copy_from_user(&t, rqtp, sizeof(struct timespec)))
		return -EFAULT;

	if (t.tv_nsec >= 1000000000L || t.tv_nsec < 0 || t.tv_sec < 0)
		return -EINVAL;


	if (t.tv_sec == 0 && t.tv_nsec <= 2000000L &&
	    current->policy != SCHED_OTHER)
	{
		/*
		 * Short delay requests up to 2 ms will be handled with
		 * high precision by a busy wait for all real-time processes.
		 *
		 * Its important on SMP not to do this holding locks.
		 */
		udelay((t.tv_nsec + 999) / 1000);
		return 0;
	}

	expire = timespec_to_jiffies(&t) + (t.tv_sec || t.tv_nsec);

	current->state = TASK_INTERRUPTIBLE;
	expire = schedule_timeout(expire);

	if (expire) {
		if (rmtp) {
			jiffies_to_timespec(expire, &t);
			if (copy_to_user(rmtp, &t, sizeof(struct timespec)))
				return -EFAULT;
		}
		return -EINTR;
	}
	return 0;
}

