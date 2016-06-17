/*
 *  IBM/3270 Driver -- Copyright (C) 2000, 2001 UTS Global LLC
 *
 *  tubttyscl.c -- Linemode tty driver scroll-timing functions
 *
 *
 *
 *
 *
 *  Author:  Richard Hitt
 */
#include "tubio.h"
       void tty3270_scl_settimer(tub_t *);
       void tty3270_scl_resettimer(tub_t *);
static void tty3270_scl_timeout(unsigned long);

void
tty3270_scl_settimer(tub_t *tubp)
{
	struct timer_list *tp = &tubp->tty_stimer;

	if (tubp->flags & TUB_SCROLLTIMING)
		return;
	if (tubp->tty_scrolltime == 0)
		return;

	init_timer(tp);
	tp->expires = jiffies + HZ * tubp->tty_scrolltime;
	tp->data = (unsigned long)tubp;
	tp->function = tty3270_scl_timeout;
	add_timer(tp);
	tubp->flags |= TUB_SCROLLTIMING;
}

void
tty3270_scl_resettimer(tub_t *tubp)
{
	struct timer_list *tp = &tubp->tty_stimer;

	if ((tubp->flags & TUB_SCROLLTIMING) == 0)
		return;

	del_timer(tp);
	tubp->flags &= ~TUB_SCROLLTIMING;
}

static void
tty3270_scl_timeout(unsigned long data)
{
	tub_t *tubp = (void *)data;
	long flags;

	TUBLOCK(tubp->irq, flags);
	tubp->stat = TBS_RUNNING;
	tty3270_scl_resettimer(tubp);
	tubp->cmd = TBC_CLRUPDLOG;
	tty3270_build(tubp);
	TUBUNLOCK(tubp->irq, flags);
}

int
tty3270_scl_set(tub_t *tubp, char *buf, int count)
{
	if (strncmp(buf, "scrolltime=", 11) == 0) {
		tubp->tty_scrolltime =
			simple_strtoul(buf + 11, 0, 0);
		return count;
	}
	return 0;
}

int
tty3270_scl_init(tub_t *tubp)
{
	extern int tubscrolltime;

	tubp->tty_scrolltime = tubscrolltime;
	if (tubp->tty_scrolltime < 0)
		tubp->tty_scrolltime = DEFAULT_SCROLLTIME;
	return 0;
}

void
tty3270_scl_fini(tub_t *tubp)
{
	if ((tubp->flags & TUB_OPEN_STET) == 0)
		tty3270_scl_resettimer(tubp);
}
