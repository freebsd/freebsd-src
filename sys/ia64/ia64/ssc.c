/*-
 * Copyright (c) 2000 Doug Rabson
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	$FreeBSD$
 */
#include <sys/param.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/cons.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/priv.h>
#include <sys/proc.h>
#include <sys/systm.h>
#include <sys/tty.h>
#include <machine/md_var.h>

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/vm_kern.h>
#include <vm/vm_page.h>
#include <vm/vm_map.h>
#include <vm/vm_object.h>
#include <vm/vm_extern.h>
#include <vm/vm_pageout.h>
#include <vm/vm_pager.h>

#define SSC_GETCHAR			21
#define SSC_PUTCHAR			31

#define	SSC_POLL_HZ	50

static tsw_open_t	ssc_open;
static tsw_outwakeup_t	ssc_outwakeup;
static tsw_close_t	ssc_close;

static struct ttydevsw ssc_class = {
	.tsw_flags	= TF_NOPREFIX,
	.tsw_open	= ssc_open,
	.tsw_outwakeup	= ssc_outwakeup,
	.tsw_close	= ssc_close,
};

static int polltime;
static struct callout_handle ssc_timeouthandle
	= CALLOUT_HANDLE_INITIALIZER(&ssc_timeouthandle);

static void	ssc_timeout(void *);

static u_int64_t
ssc(u_int64_t in0, u_int64_t in1, u_int64_t in2, u_int64_t in3, int which)
{
	register u_int64_t ret0 __asm("r8");

	__asm __volatile("mov r15=%1\n\t"
			 "break 0x80001"
			 : "=r"(ret0)
			 : "r"(which), "r"(in0), "r"(in1), "r"(in2), "r"(in3));
	return ret0;
}

static void
ssc_cnprobe(struct consdev *cp)
{

	strcpy(cp->cn_name, "ssccons");
	cp->cn_pri = CN_INTERNAL;
}

static void
ssc_cninit(struct consdev *cp)
{
}

static void
ssc_cnterm(struct consdev *cp)
{
}

static void
ssc_cnattach(void *arg)
{
	struct tty *tp;

	tp = tty_alloc(&ssc_class, NULL);
	tty_makedev(tp, NULL, "ssccons");
}

SYSINIT(ssc_cnattach, SI_SUB_DRIVERS, SI_ORDER_ANY, ssc_cnattach, 0);

static void
ssc_cnputc(struct consdev *cp, int c)
{
	ssc(c, 0, 0, 0, SSC_PUTCHAR);
}

static int
ssc_cngetc(struct consdev *cp)
{
    int c;
    c = ssc(0, 0, 0, 0, SSC_GETCHAR);
    if (!c)
	    return -1;
    return c;
}

static int
ssc_open(struct tty *tp)
{

	polltime = hz / SSC_POLL_HZ;
	if (polltime < 1)
		polltime = 1;
	ssc_timeouthandle = timeout(ssc_timeout, tp, polltime);

	return (0);
}
 
static void
ssc_close(struct tty *tp)
{

	untimeout(ssc_timeout, tp, ssc_timeouthandle);
}

static void
ssc_outwakeup(struct tty *tp)
{
	char buf[128];
	size_t len, c;

	for (;;) {
		len = ttydisc_getc(tp, buf, sizeof buf);
		if (len == 0)
			break;

		c = 0;
		while (len-- > 0)
			ssc_cnputc(NULL, buf[c++]);
	}
}

static void
ssc_timeout(void *v)
{
	struct tty *tp = v;
	int c;

	tty_lock(tp);
	while ((c = ssc_cngetc(NULL)) != -1)
		ttydisc_rint(tp, c, 0);
	ttydisc_rint_done(tp);
	tty_unlock(tp);

	ssc_timeouthandle = timeout(ssc_timeout, tp, polltime);
}

CONSOLE_DRIVER(ssc);
