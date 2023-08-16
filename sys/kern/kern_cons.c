/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 1988 University of Utah.
 * Copyright (c) 1991 The Regents of the University of California.
 * Copyright (c) 1999 Michael Smith
 * Copyright (c) 2005 Pawel Jakub Dawidek <pjd@FreeBSD.org>
 *
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * the Systems Programming Group of the University of Utah Computer
 * Science Department.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	from: @(#)cons.c	7.2 (Berkeley) 5/9/91
 */

#include <sys/cdefs.h>
#include "opt_ddb.h"
#include "opt_syscons.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/conf.h>
#include <sys/cons.h>
#include <sys/fcntl.h>
#include <sys/kbio.h>
#include <sys/kdb.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/msgbuf.h>
#include <sys/namei.h>
#include <sys/priv.h>
#include <sys/proc.h>
#include <sys/queue.h>
#include <sys/reboot.h>
#include <sys/sysctl.h>
#include <sys/sbuf.h>
#include <sys/tslog.h>
#include <sys/tty.h>
#include <sys/uio.h>
#include <sys/vnode.h>

#include <ddb/ddb.h>

#include <dev/kbd/kbdreg.h>

#include <machine/cpu.h>
#include <machine/clock.h>

static MALLOC_DEFINE(M_TTYCONS, "tty console", "tty console handling");

struct cn_device {
	STAILQ_ENTRY(cn_device) cnd_next;
	struct		consdev *cnd_cn;
};

#define CNDEVPATHMAX	32
#define CNDEVTAB_SIZE	4
static struct cn_device cn_devtab[CNDEVTAB_SIZE];
static STAILQ_HEAD(, cn_device) cn_devlist =
    STAILQ_HEAD_INITIALIZER(cn_devlist);

int	cons_avail_mask = 0;	/* Bit mask. Each registered low level console
				 * which is currently unavailable for inpit
				 * (i.e., if it is in graphics mode) will have
				 * this bit cleared.
				 */

static int cn_mute;
SYSCTL_INT(_kern, OID_AUTO, consmute, CTLFLAG_RW, &cn_mute, 0,
    "State of the console muting");

static char *consbuf;			/* buffer used by `consmsgbuf' */
static struct callout conscallout;	/* callout for outputting to constty */
struct msgbuf consmsgbuf;		/* message buffer for console tty */
static bool console_pausing;		/* pause after each line during probe */
static const char console_pausestr[] =
"<pause; press any key to proceed to next line or '.' to end pause mode>";
struct tty *constty;			/* pointer to console "window" tty */
static struct mtx constty_mtx;		/* Mutex for constty assignment. */
MTX_SYSINIT(constty_mtx, &constty_mtx, "constty_mtx", MTX_DEF);
static struct mtx cnputs_mtx;		/* Mutex for cnputs(). */
MTX_SYSINIT(cnputs_mtx, &cnputs_mtx, "cnputs_mtx", MTX_SPIN | MTX_NOWITNESS);

static void constty_timeout(void *arg);

static struct consdev cons_consdev;
DATA_SET(cons_set, cons_consdev);
SET_DECLARE(cons_set, struct consdev);

/*
 * Stub for configurations that don't actually have a keyboard driver. Inclusion
 * of kbd.c is contingent on any number of keyboard/console drivers being
 * present in the kernel; rather than trying to catch them all, we'll just
 * maintain this weak kbdinit that will be overridden by the strong version in
 * kbd.c if it's present.
 */
__weak_symbol void
kbdinit(void)
{

}

void
cninit(void)
{
	struct consdev *best_cn, *cn, **list;

	TSENTER();
	/*
	 * Check if we should mute the console (for security reasons perhaps)
	 * It can be changes dynamically using sysctl kern.consmute
	 * once we are up and going.
	 * 
	 */
        cn_mute = ((boothowto & (RB_MUTE
			|RB_SINGLE
			|RB_VERBOSE
			|RB_ASKNAME)) == RB_MUTE);

	/*
	 * Bring up the kbd layer just in time for cnprobe.  Console drivers
	 * have a dependency on kbd being ready, so this fits nicely between the
	 * machdep callers of cninit() and MI probing/initialization of consoles
	 * here.
	 */
	kbdinit();

	/*
	 * Find the first console with the highest priority.
	 */
	best_cn = NULL;
	SET_FOREACH(list, cons_set) {
		cn = *list;
		cnremove(cn);
		/* Skip cons_consdev. */
		if (cn->cn_ops == NULL)
			continue;
		cn->cn_ops->cn_probe(cn);
		if (cn->cn_pri == CN_DEAD)
			continue;
		if (best_cn == NULL || cn->cn_pri > best_cn->cn_pri)
			best_cn = cn;
		if (boothowto & RB_MULTIPLE) {
			/*
			 * Initialize console, and attach to it.
			 */
			cn->cn_ops->cn_init(cn);
			cnadd(cn);
		}
	}
	if (best_cn == NULL)
		return;
	if ((boothowto & RB_MULTIPLE) == 0) {
		best_cn->cn_ops->cn_init(best_cn);
		cnadd(best_cn);
	}
	if (boothowto & RB_PAUSE)
		console_pausing = true;
	/*
	 * Make the best console the preferred console.
	 */
	cnselect(best_cn);

#ifdef EARLY_PRINTF
	/*
	 * Release early console.
	 */
	early_putc = NULL;
#endif
	TSEXIT();
}

void
cninit_finish(void)
{
	console_pausing = false;
} 

/* add a new physical console to back the virtual console */
int
cnadd(struct consdev *cn)
{
	struct cn_device *cnd;
	int i;

	STAILQ_FOREACH(cnd, &cn_devlist, cnd_next)
		if (cnd->cnd_cn == cn)
			return (0);
	for (i = 0; i < CNDEVTAB_SIZE; i++) {
		cnd = &cn_devtab[i];
		if (cnd->cnd_cn == NULL)
			break;
	}
	if (cnd->cnd_cn != NULL)
		return (ENOMEM);
	cnd->cnd_cn = cn;
	if (cn->cn_name[0] == '\0') {
		/* XXX: it is unclear if/where this print might output */
		printf("WARNING: console at %p has no name\n", cn);
	}
	STAILQ_INSERT_TAIL(&cn_devlist, cnd, cnd_next);
	if (STAILQ_FIRST(&cn_devlist) == cnd)
		ttyconsdev_select(cnd->cnd_cn->cn_name);

	/* Add device to the active mask. */
	cnavailable(cn, (cn->cn_flags & CN_FLAG_NOAVAIL) == 0);

	return (0);
}

void
cnremove(struct consdev *cn)
{
	struct cn_device *cnd;
	int i;

	STAILQ_FOREACH(cnd, &cn_devlist, cnd_next) {
		if (cnd->cnd_cn != cn)
			continue;
		if (STAILQ_FIRST(&cn_devlist) == cnd)
			ttyconsdev_select(NULL);
		STAILQ_REMOVE(&cn_devlist, cnd, cn_device, cnd_next);
		cnd->cnd_cn = NULL;

		/* Remove this device from available mask. */
		for (i = 0; i < CNDEVTAB_SIZE; i++) 
			if (cnd == &cn_devtab[i]) {
				cons_avail_mask &= ~(1 << i);
				break;
			}
#if 0
		/*
		 * XXX
		 * syscons gets really confused if console resources are
		 * freed after the system has initialized.
		 */
		if (cn->cn_term != NULL)
			cn->cn_ops->cn_term(cn);
#endif
		return;
	}
}

void
cnselect(struct consdev *cn)
{
	struct cn_device *cnd;

	STAILQ_FOREACH(cnd, &cn_devlist, cnd_next) {
		if (cnd->cnd_cn != cn)
			continue;
		if (cnd == STAILQ_FIRST(&cn_devlist))
			return;
		STAILQ_REMOVE(&cn_devlist, cnd, cn_device, cnd_next);
		STAILQ_INSERT_HEAD(&cn_devlist, cnd, cnd_next);
		ttyconsdev_select(cnd->cnd_cn->cn_name);
		return;
	}
}

void
cnavailable(struct consdev *cn, int available)
{
	int i;

	for (i = 0; i < CNDEVTAB_SIZE; i++) {
		if (cn_devtab[i].cnd_cn == cn)
			break;
	}
	if (available) {
		if (i < CNDEVTAB_SIZE)
			cons_avail_mask |= (1 << i); 
		cn->cn_flags &= ~CN_FLAG_NOAVAIL;
	} else {
		if (i < CNDEVTAB_SIZE)
			cons_avail_mask &= ~(1 << i);
		cn->cn_flags |= CN_FLAG_NOAVAIL;
	}
}

int
cnunavailable(void)
{

	return (cons_avail_mask == 0);
}

/*
 * sysctl_kern_console() provides output parseable in conscontrol(1).
 */
static int
sysctl_kern_console(SYSCTL_HANDLER_ARGS)
{
	struct cn_device *cnd;
	struct consdev *cp, **list;
	char *p;
	bool delete;
	int error;
	struct sbuf *sb;

	sb = sbuf_new(NULL, NULL, CNDEVPATHMAX * 2, SBUF_AUTOEXTEND |
	    SBUF_INCLUDENUL);
	if (sb == NULL)
		return (ENOMEM);
	sbuf_clear(sb);
	STAILQ_FOREACH(cnd, &cn_devlist, cnd_next)
		sbuf_printf(sb, "%s,", cnd->cnd_cn->cn_name);
	sbuf_printf(sb, "/");
	SET_FOREACH(list, cons_set) {
		cp = *list;
		if (cp->cn_name[0] != '\0')
			sbuf_printf(sb, "%s,", cp->cn_name);
	}
	sbuf_finish(sb);
	error = sysctl_handle_string(oidp, sbuf_data(sb), sbuf_len(sb), req);
	if (error == 0 && req->newptr != NULL) {
		p = sbuf_data(sb);
		error = ENXIO;
		delete = false;
		if (*p == '-') {
			delete = true;
			p++;
		}
		SET_FOREACH(list, cons_set) {
			cp = *list;
			if (strcmp(p, cp->cn_name) != 0)
				continue;
			if (delete) {
				cnremove(cp);
				error = 0;
			} else {
				error = cnadd(cp);
				if (error == 0)
					cnselect(cp);
			}
			break;
		}
	}
	sbuf_delete(sb);
	return (error);
}

SYSCTL_PROC(_kern, OID_AUTO, console,
    CTLTYPE_STRING | CTLFLAG_RW | CTLFLAG_NEEDGIANT, 0, 0,
    sysctl_kern_console, "A",
    "Console device control");

void
cngrab(void)
{
	struct cn_device *cnd;
	struct consdev *cn;

	STAILQ_FOREACH(cnd, &cn_devlist, cnd_next) {
		cn = cnd->cnd_cn;
		if (!kdb_active || !(cn->cn_flags & CN_FLAG_NODEBUG))
			cn->cn_ops->cn_grab(cn);
	}
}

void
cnungrab(void)
{
	struct cn_device *cnd;
	struct consdev *cn;

	STAILQ_FOREACH(cnd, &cn_devlist, cnd_next) {
		cn = cnd->cnd_cn;
		if (!kdb_active || !(cn->cn_flags & CN_FLAG_NODEBUG))
			cn->cn_ops->cn_ungrab(cn);
	}
}

void
cnresume(void)
{
	struct cn_device *cnd;
	struct consdev *cn;

	STAILQ_FOREACH(cnd, &cn_devlist, cnd_next) {
		cn = cnd->cnd_cn;
		if (cn->cn_ops->cn_resume != NULL)
			cn->cn_ops->cn_resume(cn);
	}
}

/*
 * Low level console routines.
 */
int
cngetc(void)
{
	int c;

	if (cn_mute)
		return (-1);
	while ((c = cncheckc()) == -1)
		cpu_spinwait();
	if (c == '\r')
		c = '\n';		/* console input is always ICRNL */
	return (c);
}

int
cncheckc(void)
{
	struct cn_device *cnd;
	struct consdev *cn;
	int c;

	if (cn_mute)
		return (-1);
	STAILQ_FOREACH(cnd, &cn_devlist, cnd_next) {
		cn = cnd->cnd_cn;
		if (!kdb_active || !(cn->cn_flags & CN_FLAG_NODEBUG)) {
			c = cn->cn_ops->cn_getc(cn);
			if (c != -1)
				return (c);
		}
	}
	return (-1);
}

void
cngets(char *cp, size_t size, int visible)
{
	char *lp, *end;
	int c;

	cngrab();

	lp = cp;
	end = cp + size - 1;
	for (;;) {
		c = cngetc() & 0177;
		switch (c) {
		case '\n':
		case '\r':
			cnputc(c);
			*lp = '\0';
			cnungrab();
			return;
		case '\b':
		case '\177':
			if (lp > cp) {
				if (visible)
					cnputs("\b \b");
				lp--;
			}
			continue;
		case '\0':
			continue;
		default:
			if (lp < end) {
				switch (visible) {
				case GETS_NOECHO:
					break;
				case GETS_ECHOPASS:
					cnputc('*');
					break;
				default:
					cnputc(c);
					break;
				}
				*lp++ = c;
			}
		}
	}
}

void
cnputc(int c)
{
	struct cn_device *cnd;
	struct consdev *cn;
	const char *cp;

#ifdef EARLY_PRINTF
	if (early_putc != NULL) {
		if (c == '\n')
			early_putc('\r');
		early_putc(c);
		return;
	}
#endif

	if (cn_mute || c == '\0')
		return;
	STAILQ_FOREACH(cnd, &cn_devlist, cnd_next) {
		cn = cnd->cnd_cn;
		if (!kdb_active || !(cn->cn_flags & CN_FLAG_NODEBUG)) {
			if (c == '\n')
				cn->cn_ops->cn_putc(cn, '\r');
			cn->cn_ops->cn_putc(cn, c);
		}
	}
	if (console_pausing && c == '\n' && !kdb_active) {
		for (cp = console_pausestr; *cp != '\0'; cp++)
			cnputc(*cp);
		cngrab();
		if (cngetc() == '.')
			console_pausing = false;
		cnungrab();
		cnputc('\r');
		for (cp = console_pausestr; *cp != '\0'; cp++)
			cnputc(' ');
		cnputc('\r');
	}
}

void
cnputsn(const char *p, size_t n)
{
	size_t i;
	bool unlock_reqd = false;

	if (mtx_initialized(&cnputs_mtx)) {
		/*
		 * NOTE: Debug prints and/or witness printouts in
		 * console driver clients can cause the "cnputs_mtx"
		 * mutex to recurse. Simply return if that happens.
		 */
		if (mtx_owned(&cnputs_mtx))
			return;
		mtx_lock_spin(&cnputs_mtx);
		unlock_reqd = true;
	}

	for (i = 0; i < n; i++)
		cnputc(p[i]);

	if (unlock_reqd)
		mtx_unlock_spin(&cnputs_mtx);
}

void
cnputs(const char *p)
{
	cnputsn(p, strlen(p));
}

static unsigned int consmsgbuf_size = 65536;
SYSCTL_UINT(_kern, OID_AUTO, consmsgbuf_size, CTLFLAG_RWTUN, &consmsgbuf_size,
    0, "Console tty buffer size");

/*
 * Redirect console output to a tty.
 */
int
constty_set(struct tty *tp)
{
	int size = consmsgbuf_size;
	void *buf = NULL;

	tty_assert_locked(tp);
	if (constty == tp)
		return (0);
	if (constty != NULL)
		return (EBUSY);

	if (consbuf == NULL) {
		tty_unlock(tp);
		buf = malloc(size, M_TTYCONS, M_WAITOK);
		tty_lock(tp);
	}
	mtx_lock(&constty_mtx);
	if (constty != NULL) {
		mtx_unlock(&constty_mtx);
		free(buf, M_TTYCONS);
		return (EBUSY);
	}
	if (consbuf == NULL) {
		consbuf = buf;
		msgbuf_init(&consmsgbuf, buf, size);
	} else
		free(buf, M_TTYCONS);
	constty = tp;
	mtx_unlock(&constty_mtx);

	callout_init_mtx(&conscallout, tty_getlock(tp), 0);
	constty_timeout(tp);
	return (0);
}

/*
 * Disable console redirection to a tty.
 */
int
constty_clear(struct tty *tp)
{
	int c;

	tty_assert_locked(tp);
	if (constty != tp)
		return (ENXIO);
	callout_stop(&conscallout);
	mtx_lock(&constty_mtx);
	constty = NULL;
	mtx_unlock(&constty_mtx);
	while ((c = msgbuf_getchar(&consmsgbuf)) != -1)
		cnputc(c);
	/* We never free consbuf because it can still be in use. */
	return (0);
}

/* Times per second to check for pending console tty messages. */
static int constty_wakeups_per_second = 15;
SYSCTL_INT(_kern, OID_AUTO, constty_wakeups_per_second, CTLFLAG_RW,
    &constty_wakeups_per_second, 0,
    "Times per second to check for pending console tty messages");

static void
constty_timeout(void *arg)
{
	struct tty *tp = arg;
	int c;

	tty_assert_locked(tp);
	while ((c = msgbuf_getchar(&consmsgbuf)) != -1) {
		if (tty_putchar(tp, c) < 0) {
			constty_clear(tp);
			return;
		}
	}
	callout_reset_sbt(&conscallout, SBT_1S / constty_wakeups_per_second,
	    0, constty_timeout, tp, C_PREL(1));
}

/*
 * Sysbeep(), if we have hardware for it
 */

#ifdef HAS_TIMER_SPKR

static bool beeping;
static struct callout beeping_timer;

static void
sysbeepstop(void *chan)
{

	timer_spkr_release();
	beeping = false;
}

int
sysbeep(int pitch, sbintime_t duration)
{

	if (timer_spkr_acquire()) {
		if (!beeping) {
			/* Something else owns it. */
			return (EBUSY);
		}
	}
	timer_spkr_setfreq(pitch);
	if (!beeping) {
		beeping = true;
		callout_reset_sbt(&beeping_timer, duration, 0, sysbeepstop,
		    NULL, C_PREL(5));
	}
	return (0);
}

static void
sysbeep_init(void *unused)
{

	callout_init(&beeping_timer, 1);
}
SYSINIT(sysbeep, SI_SUB_SOFTINTR, SI_ORDER_ANY, sysbeep_init, NULL);
#else

/*
 * No hardware, no sound
 */

int
sysbeep(int pitch __unused, sbintime_t duration __unused)
{

	return (ENODEV);
}

#endif

/*
 * Temporary support for sc(4) to vt(4) transition.
 */
static unsigned vty_prefer;
static char vty_name[16];
SYSCTL_STRING(_kern, OID_AUTO, vty, CTLFLAG_RDTUN | CTLFLAG_NOFETCH, vty_name,
    0, "Console vty driver");

int
vty_enabled(unsigned vty)
{
	static unsigned vty_selected = 0;

	if (vty_selected == 0) {
		TUNABLE_STR_FETCH("kern.vty", vty_name, sizeof(vty_name));
		do {
#if defined(DEV_SC)
			if (strcmp(vty_name, "sc") == 0) {
				vty_selected = VTY_SC;
				break;
			}
#endif
#if defined(DEV_VT)
			if (strcmp(vty_name, "vt") == 0) {
				vty_selected = VTY_VT;
				break;
			}
#endif
			if (vty_prefer != 0) {
				vty_selected = vty_prefer;
				break;
			}
#if defined(DEV_VT)
			vty_selected = VTY_VT;
#elif defined(DEV_SC)
			vty_selected = VTY_SC;
#endif
		} while (0);

		if (vty_selected == VTY_VT)
			strcpy(vty_name, "vt");
		else if (vty_selected == VTY_SC)
			strcpy(vty_name, "sc");
	}
	return ((vty_selected & vty) != 0);
}

void
vty_set_preferred(unsigned vty)
{

	vty_prefer = vty;
#if !defined(DEV_SC)
	vty_prefer &= ~VTY_SC;
#endif
#if !defined(DEV_VT)
	vty_prefer &= ~VTY_VT;
#endif
}
