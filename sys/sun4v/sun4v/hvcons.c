/*-
 * Copyright (C) 2006 Kip Macy
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
 * THIS SOFTWARE IS PROVIDED BY Kip Macy ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL Kip Macy BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/module.h>
#include <sys/bus.h>
#include <sys/kdb.h>
#include <sys/kernel.h>
#include <sys/conf.h>
#include <sys/cons.h>
#include <sys/consio.h>
#include <sys/priv.h>
#include <sys/rman.h>
#include <sys/tty.h>

#include <machine/mdesc_bus.h>
#include <machine/cddl/mdesc.h>

#include "mdesc_bus_if.h"

#include "opt_simulator.h"

#include <machine/resource.h>
#include <machine/hypervisorvar.h>
#include <machine/hv_api.h>

#define HVCN_POLL_FREQ 10


static d_open_t  hvcn_open;
static d_close_t hvcn_close;

static struct cdevsw hvcn_cdevsw = {
	.d_version =	D_VERSION,
	.d_open =	hvcn_open,
	.d_close =	hvcn_close,
	.d_name =	"hvcn",
	.d_flags =	D_TTY | D_NEEDGIANT,
};

#define PCBURST 16
static struct tty		*hvcn_tp = NULL;
static struct resource          *hvcn_irq;
static void                     *hvcn_intrhand;

static int bufindex;
static int buflen;
static u_char buf[PCBURST];
static int			polltime;
static struct callout_handle	hvcn_timeouthandle
    = CALLOUT_HANDLE_INITIALIZER(&hvcn_timeouthandle);

#if defined(KDB)
static int			alt_break_state;
#endif

static void	hvcn_tty_start(struct tty *);
static int	hvcn_tty_param(struct tty *, struct termios *);
static void	hvcn_tty_stop(struct tty *, int);
static void     hvcn_timeout(void *);

static cn_probe_t	hvcn_cnprobe;
static cn_init_t	hvcn_cninit;
static cn_getc_t	hvcn_cngetc;
static cn_putc_t	hvcn_cnputc;
static cn_term_t	hvcn_cnterm;


CONSOLE_DRIVER(hvcn);

void
hv_cnputs(char *p)
{
	int c, error;

	while ((c = *p++) != '\0') {
		if (c == '\n') {
			do {
				error = hv_cons_putchar('\r');
			} while (error == H_EWOULDBLOCK);
		}
		do {
			error = hv_cons_putchar(c);
		} while (error == H_EWOULDBLOCK);
	}
}

static int
hvcn_open(struct cdev *dev, int flag, int mode, struct thread *td)
{
	struct	tty *tp;
	int	error, setuptimeout;

	setuptimeout = 0;

	if (dev->si_tty == NULL) {
		hvcn_tp = ttyalloc();
		dev->si_tty = hvcn_tp;
		hvcn_tp->t_dev = dev;
	}
	tp = dev->si_tty;

	tp->t_oproc = hvcn_tty_start;
	tp->t_param = hvcn_tty_param;
	tp->t_stop = hvcn_tty_stop;

	if ((tp->t_state & TS_ISOPEN) == 0) {
		tp->t_state |= TS_CARR_ON;
		ttyconsolemode(tp, 0);

		setuptimeout = 1;
	} else if ((tp->t_state & TS_XCLUDE) && priv_check(td,
	     PRIV_TTY_EXCLUSIVE)) {
		return (EBUSY);
	}

	error = ttyld_open(tp, dev);
#if defined(SIMULATOR) || 1
	if (error == 0 && setuptimeout) {
		int polltime;

		polltime = hz / HVCN_POLL_FREQ;
		if (polltime < 1) {
			polltime = 1;
		}

		hvcn_timeouthandle = timeout(hvcn_timeout, tp, polltime);
	}
#endif
	return (error);
}
 
static int
hvcn_close(struct cdev *dev, int flag, int mode, struct thread *td)
{
	int	unit;
	struct	tty *tp;

	unit = minor(dev);
	tp = dev->si_tty;

	if (unit != 0) 
		return (ENXIO);
	
	untimeout(hvcn_timeout, tp, hvcn_timeouthandle);
	ttyld_close(tp, flag);
	tty_close(tp);

	return (0);
}

static void
hvcn_cnprobe(struct consdev *cp)
{

#if 0
	char name[64];

	node = OF_peer(0);
	if (node == -1)
		panic("%s: OF_peer failed.", __func__);
	
	for (node = OF_child(node); node > 0; node = OF_peer(node)) {
		OF_getprop(node, "name", name, sizeof(name));
		if (!strcmp(name, "virtual-devices"))
			break;
	}
	
	if (node == 0)
		goto done;

	for (node = OF_child(node); node > 0; node = OF_peer(node)) {
		OF_getprop(node, "name", name, sizeof(name));
		if (!strcmp(name, "console"))
			break;
	}
done:
#endif	
	cp->cn_pri = CN_NORMAL;

}

static void
hvcn_cninit(struct consdev *cp)
{
	sprintf(cp->cn_name, "hvcn");
}

static int
hvcn_cngetc(struct consdev *cp)
{
	unsigned char ch;
	int l;

	ch = '\0';

	while ((l = hv_cons_getchar(&ch)) != H_EOK) {
#if defined(KDB)
		if (l == H_BREAK || l ==  H_HUP)
			kdb_enter(KDB_WHY_BREAK, "Break sequence on console");

	if (kdb_alt_break(ch, &alt_break_state))
		kdb_enter(KDB_WHY_BREAK, "Break sequence on console");
#endif
		if (l != -2 && l != 0) {
			return (-1);
		}
	}



	return (ch);
}

static int
hvcn_cncheckc(struct consdev *cp)
{
	unsigned char ch;
	int l;

	if ((l = hv_cons_getchar(&ch)) == H_EOK) {
#if defined(KDB)
		if (l == H_BREAK || l ==  H_HUP)
			kdb_enter(KDB_WHY_BREAK, "Break sequence on console");
		if (kdb_alt_break(ch, &alt_break_state))
			kdb_enter(KDB_WHY_BREAK, "Break sequence on console");
#endif
		return (ch);
	}

	return (-1);
}


static void
hvcn_cnterm(struct consdev *cp)
{
	;
}

static void
hvcn_cnputc(struct consdev *cp, int c)
{

	int error;

	error = 0;
	do {
		if (c == '\n') 
			error = hv_cons_putchar('\r');
	} while (error == H_EWOULDBLOCK);
	do {
		error = hv_cons_putchar(c);
	} while (error == H_EWOULDBLOCK);
}

static int
hvcn_tty_param(struct tty *tp, struct termios *t)
{
        tp->t_ispeed = t->c_ispeed;
        tp->t_ospeed = t->c_ospeed;
        tp->t_cflag = t->c_cflag;

	return (0);
}

static void
hvcn_tty_start(struct tty *tp)
{

        if (!(tp->t_state & (TS_TIMEOUT | TS_BUSY | TS_TTSTOP))) {
		tp->t_state |= TS_BUSY;

		do {
			if (buflen == 0) {
				buflen = q_to_b(&tp->t_outq, buf, PCBURST);
				bufindex = 0;
			}
			while (buflen) {
				if (hv_cons_putchar(buf[bufindex]) == H_EWOULDBLOCK)
					goto done;
				bufindex++;
				buflen--;
			}
		} while (tp->t_outq.c_cc != 0);
	done:
		tp->t_state &= ~TS_BUSY;
		ttwwakeup(tp);
	}
}

static void
hvcn_tty_stop(struct tty *tp, int flag)
{
	if ((tp->t_state & TS_BUSY) && !(tp->t_state & TS_TTSTOP)) 
		tp->t_state |= TS_FLUSH;
		
	
}

static void
hvcn_intr(void *v)
{
	struct tty *tp;
	int c;

	tp = (struct tty *)v;
	
	while ((c = hvcn_cncheckc(NULL)) != -1) 
		if (tp->t_state & TS_ISOPEN) 
			ttyld_rint(tp, c);

	if (tp->t_outq.c_cc != 0 || buflen != 0) 
		hvcn_tty_start(tp);
}

static void
hvcn_timeout(void *v)
{
	hvcn_intr(v);

	hvcn_timeouthandle = timeout(hvcn_timeout, v, polltime);
}


static int
hvcn_dev_probe(device_t dev)
{

	if (strcmp(mdesc_bus_get_name(dev), "console"))
		return (ENXIO);
	
	device_set_desc(dev, "sun4v virtual console");	

	return (0);
}


static int
hvcn_dev_attach(device_t dev)
{
      
	struct cdev *cdev;
	int error, rid;

	/* belongs in attach - but attach is getting called multiple times
	 * for reasons I have not delved into
	 */

	if (hvcn_consdev.cn_pri == CN_DEAD || 
	    hvcn_consdev.cn_name[0] == '\0') 
		return (ENXIO);

	cdev = make_dev(&hvcn_cdevsw, 0, UID_ROOT, GID_WHEEL, 0600, "ttyv%r", 1);
	make_dev_alias(cdev, "hvcn");
	
	rid = 0;

	if ((hvcn_irq = bus_alloc_resource_any(dev, SYS_RES_IRQ, &rid,
					RF_SHAREABLE | RF_ACTIVE)) == NULL) {
		device_printf(dev, "couldn't map interrupt\n");
		error = ENXIO;
		goto fail;
		
	}
	error = bus_setup_intr(dev, hvcn_irq, INTR_TYPE_TTY, NULL, hvcn_intr, hvcn_tp, 
			       hvcn_intrhand);
	
	if (error)
		device_printf(dev, "couldn't set up irq\n");
	
		
fail:
	return (error);
      
}

static device_method_t hvcn_methods[] = {
        DEVMETHOD(device_probe, hvcn_dev_probe),
        DEVMETHOD(device_attach, hvcn_dev_attach),
        {0, 0}
};


static driver_t hvcn_driver = {
        "hvcn",
        hvcn_methods,
	0,
};


static devclass_t hvcn_devclass;

DRIVER_MODULE(hvcn, vnex, hvcn_driver, hvcn_devclass, 0, 0);
