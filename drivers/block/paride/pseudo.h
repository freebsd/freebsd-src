/* 
        pseudo.h    (c) 1997-8  Grant R. Guenther <grant@torque.net>
                                Under the terms of the GNU General Public License.

	This is the "pseudo-interrupt" logic for parallel port drivers.

        This module is #included into each driver.  It makes one
        function available:

		ps_set_intr( void (*continuation)(void),
			     int  (*ready)(void),
			     int timeout,
			     int nice )

	Which will arrange for ready() to be evaluated frequently and
	when either it returns true, or timeout jiffies have passed,
	continuation() will be invoked.

	If nice is 1, the test will done approximately once a
	jiffy.  If nice is 0, the test will also be done whenever
	the scheduler runs (by adding it to a task queue).  If
	nice is greater than 1, the test will be done once every
	(nice-1) jiffies. 

*/

/* Changes:

	1.01	1998.05.03	Switched from cli()/sti() to spinlocks
	1.02    1998.12.14      Added support for nice > 1
*/
	
#define PS_VERSION	"1.02"

#include <linux/sched.h>
#include <linux/timer.h>
#include <linux/tqueue.h>

static void ps_timer_int( unsigned long data);
static void ps_tq_int( void *data);

static void (* ps_continuation)(void);
static int (* ps_ready)(void);
static int ps_then;
static int ps_timeout;
static int ps_timer_active = 0;
static int ps_tq_active = 0;
static int ps_nice = 0;

static spinlock_t ps_spinlock __attribute__((unused)) = SPIN_LOCK_UNLOCKED;

static struct timer_list ps_timer = { function: ps_timer_int };
static struct tq_struct ps_tq = { routine: ps_tq_int };

static void ps_set_intr( void (*continuation)(void), 
			 int (*ready)(void),
			 int timeout, int nice )

{       unsigned long	flags;

	spin_lock_irqsave(&ps_spinlock,flags);

	ps_continuation = continuation;
	ps_ready = ready;
        ps_then = jiffies;
	ps_timeout = jiffies + timeout;
	ps_nice = nice;

        if (!ps_nice && !ps_tq_active) {
#ifdef HAVE_DISABLE_HLT
                disable_hlt();
#endif
		ps_tq_active = 1;
                schedule_task(&ps_tq);
	}

        if (!ps_timer_active) {
		ps_timer_active = 1;
                ps_timer.expires = jiffies + ((ps_nice>0)?(ps_nice-1):0);
                add_timer(&ps_timer);
        }

	spin_unlock_irqrestore(&ps_spinlock,flags);
}

static void ps_tq_int( void *data )

{       void (*con)(void);
	unsigned long flags;

	spin_lock_irqsave(&ps_spinlock,flags);

        con = ps_continuation;

#ifdef HAVE_DISABLE_HLT
        enable_hlt();
#endif

        ps_tq_active = 0;

        if (!con) {
		spin_unlock_irqrestore(&ps_spinlock,flags);
		return;
	}
        if (!ps_ready || ps_ready() || time_after_eq(jiffies, ps_timeout)) {
                ps_continuation = NULL;
        	spin_unlock_irqrestore(&ps_spinlock,flags);
                con();
                return;
                }

#ifdef HAVE_DISABLE_HLT
        disable_hlt();
#endif

        ps_tq_active = 1;
	schedule_task(&ps_tq);
        spin_unlock_irqrestore(&ps_spinlock,flags);
}

static void ps_timer_int( unsigned long data)

{       void (*con)(void);
	unsigned long	flags;

	spin_lock_irqsave(&ps_spinlock,flags);

	con = ps_continuation;
	ps_timer_active = 0;
	if (!con) {
	        spin_unlock_irqrestore(&ps_spinlock,flags);
		return;
	}
        if (!ps_ready || ps_ready() || time_after_eq(jiffies, ps_timeout)) {
                ps_continuation = NULL;
	        spin_unlock_irqrestore(&ps_spinlock,flags);
                con();
		return;
		}
	ps_timer_active = 1;
        ps_timer.expires = jiffies + ((ps_nice>0)?(ps_nice-1):0);
        add_timer(&ps_timer);
        spin_unlock_irqrestore(&ps_spinlock,flags);
}

/* end of pseudo.h */

