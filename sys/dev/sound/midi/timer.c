/*
 * This is the timer engine of /dev/music for FreeBSD.
 * 
 * (C) 2002 Seigo Tanimura
 * 
 * Redistribution and use in source and binary forms, with or
 * without modification, are permitted provided that the following
 * conditions are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials provided
 *    with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS
 * IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
 * AUTHOR OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD$
 *
 */

#include <dev/sound/midi/midi.h>
#include <dev/sound/midi/sequencer.h>

#define TMR2TICKS(scp, tmr_val)	\
	((((tmr_val) * (scp)->tempo * (scp)->timebase) + (30 * hz)) / (60 * hz))
#define CURTICKS(scp)	\
	((scp)->ticks_offset + (scp)->ticks_cur - (scp)->ticks_base)

struct systmr_timer_softc {
	int	running;

	u_long	ticks_offset;
	u_long	ticks_base;
	u_long	ticks_cur;

	int	tempo;
	int	timebase;

	u_long	nexteventtime;
	u_long	preveventtime;

	struct callout	timer;
};

static timeout_t	systmr_timer;
static void		systmr_reset(timerdev_info *tmd);
static u_long		systmr_time(void);

static tmr_open_t	systmr_open;
static tmr_close_t	systmr_close;
static tmr_event_t	systmr_event;
static tmr_gettime_t	systmr_gettime;
static tmr_ioctl_t	systmr_ioctl;
static tmr_armtimer_t	systmr_armtimer;

static timerdev_info systmr_timerdev = {
	"System clock",
	0,
	0,
	systmr_open,
	systmr_close,
	systmr_event,
	systmr_gettime,
	systmr_ioctl,
	systmr_armtimer,
};

static TAILQ_HEAD(,_timerdev_info) timer_info;
static struct mtx timerinfo_mtx;
static int timerinfo_mtx_init;
static int ntimer;


/* Install a system timer. */
int
timerdev_install(void)
{
	int ret;
	timerdev_info *tmd;
	struct systmr_timer_softc *scp;

	SEQ_DEBUG(printf("timerdev_install: install a new timer.\n"));

	ret = 0;
	tmd = NULL;
	scp = NULL;

	scp = malloc(sizeof(*scp), M_DEVBUF, M_ZERO);
	if (scp == NULL) {
		ret = ENOMEM;
		goto fail;
	}

	tmd = create_timerdev_info_unit(&systmr_timerdev);
	if (tmd == NULL) {
		ret = ENOMEM;
		goto fail;
	}

	tmd->softc = scp;
	callout_init(&scp->timer, 0);

	mtx_unlock(&tmd->mtx);

	SEQ_DEBUG(printf("timerdev_install: installed a new timer, unit %d.\n", tmd->unit));

	return (0);

fail:
	if (scp != NULL)
		free(scp, M_DEVBUF);
	if (tmd != NULL) {
		TAILQ_REMOVE(&timer_info, tmd, tmd_link);
		free(tmd, M_DEVBUF);
	}

	SEQ_DEBUG(printf("timerdev_install: installation failed.\n"));

	return (ret);
}

/* Create a new timer device info structure. */
timerdev_info *
create_timerdev_info_unit(timerdev_info *tmdinf)
{
	int unit;
	timerdev_info *tmd, *tmdnew;

	/* XXX */
	if (!timerinfo_mtx_init) {
		timerinfo_mtx_init = 1;
		mtx_init(&timerinfo_mtx, "tmrinf", NULL, MTX_DEF);
		TAILQ_INIT(&timer_info);
	}

	/* As malloc(9) might block, allocate timerdev_info now. */
	tmdnew = malloc(sizeof(timerdev_info), M_DEVBUF, M_ZERO);
	if (tmdnew == NULL)
		return NULL;
	bcopy(tmdinf, tmdnew, sizeof(timerdev_info));
	mtx_init(&tmdnew->mtx, "tmrmtx", NULL, MTX_DEF);

	mtx_lock(&timerinfo_mtx);

	ntimer++;

	for (unit = 0 ; ; unit++) {
		TAILQ_FOREACH(tmd, &timer_info, tmd_link) {
			if (tmd->unit == unit)
				break;
		}
		if (tmd == NULL)
			break;
	}

	tmdnew->unit = unit;
	mtx_lock(&tmdnew->mtx);
	tmd = TAILQ_FIRST(&timer_info);
	while (tmd != NULL) {
		if (tmd->prio < tmdnew->prio)
			break;
		tmd = TAILQ_NEXT(tmd, tmd_link);
	}
	if (tmd != NULL)
		TAILQ_INSERT_BEFORE(tmd, tmdnew, tmd_link);
	else
		TAILQ_INSERT_TAIL(&timer_info, tmdnew, tmd_link);

	mtx_unlock(&timerinfo_mtx);

	return (tmdnew);
}

/*
 * a small utility function which, given a unit number, returns
 * a pointer to the associated timerdev_info struct.
 */
timerdev_info *
get_timerdev_info_unit(int unit)
{
	timerdev_info *tmd;

	/* XXX */
	if (!timerinfo_mtx_init) {
		timerinfo_mtx_init = 1;
		mtx_init(&timerinfo_mtx, "tmrinf", NULL, MTX_DEF);
		TAILQ_INIT(&timer_info);
	}

	mtx_lock(&timerinfo_mtx);
	TAILQ_FOREACH(tmd, &timer_info, tmd_link) {
		mtx_lock(&tmd->mtx);
		if (tmd->unit == unit && tmd->seq == NULL)
			break;
		mtx_unlock(&tmd->mtx);
	}
	mtx_unlock(&timerinfo_mtx);

	return tmd;
}

/*
 * a small utility function which returns a pointer
 * to the best preferred timerdev_info struct with
 * no sequencer.
 */
timerdev_info *
get_timerdev_info(void)
{
	timerdev_info *tmd;

	/* XXX */
	if (!timerinfo_mtx_init) {
		timerinfo_mtx_init = 1;
		mtx_init(&timerinfo_mtx, "tmrinf", NULL, MTX_DEF);
		TAILQ_INIT(&timer_info);
	}

	mtx_lock(&timerinfo_mtx);
	TAILQ_FOREACH(tmd, &timer_info, tmd_link) {
		mtx_lock(&tmd->mtx);
		if (tmd->seq == NULL)
			break;
		mtx_unlock(&tmd->mtx);
	}
	mtx_unlock(&timerinfo_mtx);

	return tmd;
}


/* ARGSUSED */
static void
systmr_timer(void *d)
{
	timerdev_info *tmd;
	struct systmr_timer_softc *scp;
	void *seq;

	tmd = (timerdev_info *)d;
	scp = (struct systmr_timer_softc *)tmd->softc;
	seq = NULL;

	mtx_lock(&tmd->mtx);

	if (tmd->opened) {
		callout_reset(&scp->timer, 1, systmr_timer, tmd);

		if (scp->running) {
			scp->ticks_cur = TMR2TICKS(scp, systmr_time());

			if (CURTICKS(scp) >= scp->nexteventtime) {
				SEQ_DEBUG(printf("systmr_timer: CURTICKS %lu, call the sequencer.\n", CURTICKS(scp)));
				scp->nexteventtime = ULONG_MAX;
				seq = tmd->seq;
			}
		}
	}

	mtx_unlock(&tmd->mtx);

	if (seq != NULL)
		seq_timer(seq);
}

static void
systmr_reset(timerdev_info *tmd)
{
	struct systmr_timer_softc *scp;

	scp = (struct systmr_timer_softc *)tmd->softc;

	mtx_assert(&tmd->mtx, MA_OWNED);

	SEQ_DEBUG(printf("systmr_reset: unit %d.\n", tmd->unit));

	scp->ticks_offset = 0;
	scp->ticks_base = scp->ticks_cur = TMR2TICKS(scp, systmr_time());

	scp->nexteventtime = ULONG_MAX;
	scp->preveventtime = 0;
}

static u_long
systmr_time(void)
{
	struct timeval  timecopy;

	getmicrotime(&timecopy);
	return timecopy.tv_usec / (1000000 / hz) + (u_long) timecopy.tv_sec * hz;
}


/* ARGSUSED */
static int
systmr_open(timerdev_info *tmd, int oflags, int devtype, struct thread *td)
{
	struct systmr_timer_softc *scp;

	scp = (struct systmr_timer_softc *)tmd->softc;

	SEQ_DEBUG(printf("systmr_open: unit %d.\n", tmd->unit));

	mtx_lock(&tmd->mtx);

	if (tmd->opened) {
		mtx_unlock(&tmd->mtx);
		return (EBUSY);
	}

	systmr_reset(tmd);
	scp->tempo = 60;
	scp->timebase = hz;
	tmd->opened = 1;

	callout_reset(&scp->timer, 1, systmr_timer, tmd);

	mtx_unlock(&tmd->mtx);

	return (0);
}

static int
systmr_close(timerdev_info *tmd, int fflag, int devtype, struct thread *td)
{
	struct systmr_timer_softc *scp;

	scp = (struct systmr_timer_softc *)tmd->softc;

	SEQ_DEBUG(printf("systmr_close: unit %d.\n", tmd->unit));

	mtx_lock(&tmd->mtx);

	tmd->opened = 0;
	scp->running = 0;

	callout_stop(&scp->timer);

	mtx_unlock(&tmd->mtx);

	return (0);
}

static int
systmr_event(timerdev_info *tmd, u_char *ev)
{
	struct systmr_timer_softc	*scp;
	u_char	cmd;
	u_long	parm, t;
	int	ret;
	void *	seq;

	scp = (struct systmr_timer_softc *)tmd->softc;
	cmd = ev[1];
	parm = *(int *)&ev[4];
	ret = MORE;

	SEQ_DEBUG(printf("systmr_event: unit %d, cmd %s, parm %lu.\n",
			 tmd->unit, midi_cmdname(cmd, cmdtab_timer), parm));

	mtx_lock(&tmd->mtx);

	switch (cmd) {
	case TMR_WAIT_REL:
		parm += scp->preveventtime;
		/* FALLTHRU */
	case TMR_WAIT_ABS:
		if (parm > 0) {
			if (parm <= CURTICKS(scp))
				break;
			t = parm;
			scp->nexteventtime = scp->preveventtime = t;
			ret = TIMERARMED;
			break;
		}
		break;

	case TMR_START:
		systmr_reset(tmd);
		scp->running = 1;
		break;

	case TMR_STOP:
		scp->running = 0;
		break;

	case TMR_CONTINUE:
		scp->running = 1;
		break;

	case TMR_TEMPO:
		if (parm > 0) {
			RANGE(parm, 8, 360);
			scp->ticks_offset += scp->ticks_cur
			    - scp->ticks_base;
			scp->ticks_base = scp->ticks_cur;
			scp->tempo = parm;
		}
		break;

	case TMR_ECHO:
		seq = tmd->seq;
		mtx_unlock(&tmd->mtx);
		seq_copytoinput(seq, ev, 8);
		mtx_lock(&tmd->mtx);
		break;
	}

	mtx_unlock(&tmd->mtx);

	SEQ_DEBUG(printf("systmr_event: timer %s.\n",
			 ret == TIMERARMED ? "armed" : "not armed"));

	return (ret);
}

static int
systmr_gettime(timerdev_info *tmd, u_long *t)
{
	struct systmr_timer_softc *scp;
	int	ret;

	scp = (struct systmr_timer_softc *)tmd->softc;

	SEQ_DEBUG(printf("systmr_gettime: unit %d.\n", tmd->unit));

	mtx_lock(&tmd->mtx);

	if (!tmd->opened || t == NULL) {
		ret = EINVAL;
		goto fail;
	}

	*t = CURTICKS(scp);
	SEQ_DEBUG(printf("systmr_gettime: ticks %lu.\n", *t));

fail:
	mtx_unlock(&tmd->mtx);

	return (0);
}

static int
systmr_ioctl(timerdev_info *tmd, u_long cmd, caddr_t data, int fflag, struct thread *td)
{
	struct systmr_timer_softc *scp;
	int	ret, val;

	scp = (struct systmr_timer_softc *)tmd->softc;
	ret = 0;

	SEQ_DEBUG(printf("systmr_ioctl: unit %d, cmd %s.\n",
			 tmd->unit, midi_cmdname(cmd, cmdtab_seqioctl)));

	switch (cmd) {
	case SNDCTL_TMR_SOURCE:
		*(int *)data = TMR_INTERNAL;
		break;

	case SNDCTL_TMR_START:
		mtx_lock(&tmd->mtx);
		systmr_reset(tmd);
		scp->running = 1;
		mtx_unlock(&tmd->mtx);
		break;

	case SNDCTL_TMR_STOP:
		mtx_lock(&tmd->mtx);
		scp->running = 0;
		mtx_unlock(&tmd->mtx);
		break;

	case SNDCTL_TMR_CONTINUE:
		mtx_lock(&tmd->mtx);
		scp->running = 1;
		mtx_unlock(&tmd->mtx);
		break;

	case SNDCTL_TMR_TIMEBASE:
		val = *(int *)data;
		mtx_lock(&tmd->mtx);
		if (val > 0) {
			RANGE(val, 1, 1000);
			scp->timebase = val;
		}
		*(int *)data = scp->timebase;
		mtx_unlock(&tmd->mtx);
		SEQ_DEBUG(printf("systmr_ioctl: timebase %d.\n", *(int *)data));
		break;

	case SNDCTL_TMR_TEMPO:
		val = *(int *)data;
		mtx_lock(&tmd->mtx);
		if (val > 0) {
			RANGE(val, 8, 360);
			scp->ticks_offset += scp->ticks_cur
			    - scp->ticks_base;
			scp->ticks_base = scp->ticks_cur;
			scp->tempo = val;
		}
		*(int *)data = scp->tempo;
		SEQ_DEBUG(printf("systmr_ioctl: tempo %d.\n", *(int *)data));
		mtx_unlock(&tmd->mtx);
		break;

	case SNDCTL_SEQ_CTRLRATE:
		val = *(int *)data;
		if (val > 0)
			ret = EINVAL;
		else {
			mtx_lock(&tmd->mtx);
			*(int *)data = ((scp->tempo * scp->timebase) + 30) / 60;
			mtx_unlock(&tmd->mtx);
			SEQ_DEBUG(printf("systmr_ioctl: ctrlrate %d.\n", *(int *)data));
		}
		break;

	case SNDCTL_TMR_METRONOME:
		/* NOP. */
		break;

	case SNDCTL_TMR_SELECT:
		/* NOP. */
		break;

	default:
		ret = EINVAL;
	}

	return (ret);
}

static int
systmr_armtimer(timerdev_info *tmd, u_long t)
{
	struct systmr_timer_softc *scp;

	scp = (struct systmr_timer_softc *)tmd->softc;

	SEQ_DEBUG(printf("systmr_armtimer: unit %d, t %lu.\n", tmd->unit, t));

	mtx_lock(&tmd->mtx);

	if (t < 0)
		t = CURTICKS(scp) + 1;
	else if (t > CURTICKS(scp))
		scp->nexteventtime = scp->preveventtime = t;

	mtx_unlock(&tmd->mtx);

	return (0);
}
