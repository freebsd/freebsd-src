#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/module.h>
#include <sys/systm.h>
#include <sys/consio.h>
#include <sys/proc.h>
#include <sys/uio.h>
#include <sys/tty.h>
#include <sys/systm.h>
#include <sys/taskqueue.h>
#include <sys/conf.h>
#include <sys/kernel.h>
#include <sys/bus.h>
#include <machine/stdarg.h>
#include <machine/xen/xen-os.h>
#include <machine/xen/hypervisor.h>
#include <machine/xen/xen_intr.h>
#include <sys/cons.h>
#include <sys/priv.h>
#include <sys/proc.h>

#include <dev/xen/console/xencons_ring.h>
#include <xen/interface/io/console.h>


#include "opt_ddb.h"
#ifdef DDB
#include <ddb/ddb.h>
#endif

static char driver_name[] = "xc";
devclass_t xc_devclass; /* do not make static */
static void	xcstart (struct tty *);
static int	xcparam (struct tty *, struct termios *);
static void	xcstop (struct tty *, int);
static void	xc_timeout(void *);
static void __xencons_tx_flush(void);
static boolean_t xcons_putc(int c);

/* switch console so that shutdown can occur gracefully */
static void xc_shutdown(void *arg, int howto);
static int xc_mute;

static void xcons_force_flush(void);
static void xencons_priv_interrupt(void *);

static cn_probe_t       xccnprobe;
static cn_init_t        xccninit;
static cn_getc_t        xccngetc;
static cn_putc_t        xccnputc;
static cn_putc_t        xccnputc_dom0;
static cn_checkc_t      xccncheckc;

#define XC_POLLTIME 	(hz/10)

CONS_DRIVER(xc, xccnprobe, xccninit, NULL, xccngetc, 
	    xccncheckc, xccnputc, NULL);

static int xen_console_up;
static boolean_t xc_start_needed;
static struct callout xc_callout;
struct mtx              cn_mtx;

#define RBUF_SIZE     1024
#define RBUF_MASK(_i) ((_i)&(RBUF_SIZE-1))
#define WBUF_SIZE     4096
#define WBUF_MASK(_i) ((_i)&(WBUF_SIZE-1))
static char wbuf[WBUF_SIZE];
static char rbuf[RBUF_SIZE];
static int rc, rp;
static unsigned int cnsl_evt_reg;
static unsigned int wc, wp; /* write_cons, write_prod */

#define CDEV_MAJOR 12
#define	XCUNIT(x)	(minor(x))
#define ISTTYOPEN(tp)	((tp) && ((tp)->t_state & TS_ISOPEN))
#define CN_LOCK_INIT(x, _name) \
        mtx_init(&x, _name, NULL, MTX_SPIN|MTX_RECURSE)

#define CN_LOCK(l)        								\
		do {											\
				if (panicstr == NULL)					\
                        mtx_lock_spin(&(l));			\
		} while (0)
#define CN_UNLOCK(l)        							\
		do {											\
				if (panicstr == NULL)					\
                        mtx_unlock_spin(&(l));			\
		} while (0)
#define CN_LOCK_ASSERT(x)    mtx_assert(&x, MA_OWNED)
#define CN_LOCK_DESTROY(x)   mtx_destroy(&x)


static struct tty *xccons;

struct xc_softc {
	int    xc_unit;
	struct cdev *xc_dev;
};


static d_open_t  xcopen;
static d_close_t xcclose;
static d_ioctl_t xcioctl;

static struct cdevsw xc_cdevsw = {
	.d_version =    D_VERSION,
        .d_flags =      D_TTY | D_NEEDGIANT,
        .d_name =       driver_name,
        .d_open =       xcopen,
        .d_close =      xcclose,
        .d_read =       ttyread,
        .d_write =      ttywrite,
        .d_ioctl =      xcioctl,
        .d_poll =       ttypoll,
        .d_kqfilter =   ttykqfilter,
};

static void
xccnprobe(struct consdev *cp)
{
	cp->cn_pri = CN_REMOTE;
	cp->cn_tp = xccons;
	sprintf(cp->cn_name, "%s0", driver_name);
}


static void
xccninit(struct consdev *cp)
{ 
	CN_LOCK_INIT(cn_mtx,"XCONS LOCK");

}
int
xccngetc(struct consdev *dev)
{
	int c;
	if (xc_mute)
	    	return 0;
	do {
		if ((c = xccncheckc(dev)) == -1) {
			/* polling without sleeping in Xen doesn't work well. 
			 * Sleeping gives other things like clock a chance to 
			 * run
			 */
			tsleep(&cn_mtx, PWAIT | PCATCH, "console sleep", 
			       XC_POLLTIME);
		}
	} while(c == -1);
	return c;
}

int
xccncheckc(struct consdev *dev)
{
	int ret = (xc_mute ? 0 : -1);
	if (xencons_has_input()) 
			xencons_handle_input(NULL);
	
	CN_LOCK(cn_mtx);
	if ((rp - rc)) {
		/* we need to return only one char */
		ret = (int)rbuf[RBUF_MASK(rc)];
		rc++;
	}
	CN_UNLOCK(cn_mtx);
	return(ret);
}

static void
xccnputc(struct consdev *dev, int c)
{
	xcons_putc(c);
}

static void
xccnputc_dom0(struct consdev *dev, int c)
{
	HYPERVISOR_console_io(CONSOLEIO_write, 1, (char *)&c);
}

extern int db_active;
static boolean_t
xcons_putc(int c)
{
	int force_flush = xc_mute ||
#ifdef DDB
		db_active ||
#endif
		panicstr;	/* we're not gonna recover, so force
				 * flush 
				 */

	if ((wp-wc) < (WBUF_SIZE-1)) {
		if ((wbuf[WBUF_MASK(wp++)] = c) == '\n') {
        		wbuf[WBUF_MASK(wp++)] = '\r';
#ifdef notyet
			if (force_flush)
				xcons_force_flush();
#endif
		}
	} else if (force_flush) {
#ifdef notyet
		xcons_force_flush();
#endif	    	
	}
	if (cnsl_evt_reg)
		__xencons_tx_flush();
	
	/* inform start path that we're pretty full */
	return ((wp - wc) >= WBUF_SIZE - 100) ? TRUE : FALSE;
}

static void
xc_identify(driver_t *driver, device_t parent)
{
	device_t child;
	child = BUS_ADD_CHILD(parent, 0, driver_name, 0);
	device_set_driver(child, driver);
	device_set_desc(child, "Xen Console");
}

static int
xc_probe(device_t dev)
{
	struct xc_softc *sc = (struct xc_softc *)device_get_softc(dev);

	sc->xc_unit = device_get_unit(dev);
	return (0);
}

static int
xc_attach(device_t dev) 
{
	struct xc_softc *sc = (struct xc_softc *)device_get_softc(dev);


	if (xen_start_info->flags & SIF_INITDOMAIN) {
		xc_consdev.cn_putc = xccnputc_dom0;
	} 

	sc->xc_dev = make_dev(&xc_cdevsw, 0, UID_ROOT, GID_WHEEL, 0600, "xc%r", 0);
	xccons = ttyalloc();

	sc->xc_dev->si_drv1 = (void *)sc;
	sc->xc_dev->si_tty = xccons;
			     
	xccons->t_oproc = xcstart;
	xccons->t_param = xcparam;
	xccons->t_stop = xcstop;
	xccons->t_dev = sc->xc_dev;

	callout_init(&xc_callout, 0);

	xencons_ring_init();

	cnsl_evt_reg = 1;
	callout_reset(&xc_callout, XC_POLLTIME, xc_timeout, xccons);
    
	if (xen_start_info->flags & SIF_INITDOMAIN) {
		PANIC_IF(bind_virq_to_irqhandler(
				 VIRQ_CONSOLE,
				 0,
				 "console",
				 NULL,
				 xencons_priv_interrupt,
				 INTR_TYPE_TTY) < 0);
		
	}


	/* register handler to flush console on shutdown */
	if ((EVENTHANDLER_REGISTER(shutdown_post_sync, xc_shutdown,
				   NULL, SHUTDOWN_PRI_DEFAULT)) == NULL)
		printf("xencons: shutdown event registration failed!\n");
	
	return (0);
}

/*
 * return 0 for all console input, force flush all output.
 */
static void
xc_shutdown(void *arg, int howto)
{
	xc_mute = 1;
	xcons_force_flush();
}

void 
xencons_rx(char *buf, unsigned len)
{
	int           i;
	struct tty *tp = xccons;
	
	for (i = 0; i < len; i++) {
		if (xen_console_up) 
			(*linesw[tp->t_line]->l_rint)(buf[i], tp);
		else
			rbuf[RBUF_MASK(rp++)] = buf[i];
	}
}

static void 
__xencons_tx_flush(void)
{
	int        sz, work_done = 0;

	CN_LOCK(cn_mtx);
	while (wc != wp) {
		int sent;
		sz = wp - wc;
		if (sz > (WBUF_SIZE - WBUF_MASK(wc)))
			sz = WBUF_SIZE - WBUF_MASK(wc);
		if (xen_start_info->flags & SIF_INITDOMAIN) {
			HYPERVISOR_console_io(CONSOLEIO_write, sz, &wbuf[WBUF_MASK(wc)]);
			wc += sz;
		} else {
			sent = xencons_ring_send(&wbuf[WBUF_MASK(wc)], sz);
			if (sent == 0) 
				break;
			wc += sent;
		}
		work_done = 1;
	}
	CN_UNLOCK(cn_mtx);

	/*
	 * ttwakeup calls routines using blocking locks
	 *
	 */
	if (work_done && xen_console_up && curthread->td_critnest == 0)
		ttwakeup(xccons);
}

void
xencons_tx(void)
{
	__xencons_tx_flush();
}

static void
xencons_priv_interrupt(void *arg)
{

	static char rbuf[16];
	int         l;

	while ((l = HYPERVISOR_console_io(CONSOLEIO_read, 16, rbuf)) > 0)
		xencons_rx(rbuf, l);

	xencons_tx();
}

int
xcopen(struct cdev *dev, int flag, int mode, struct thread *td)
{
	struct xc_softc *sc;
	int unit = XCUNIT(dev);
	struct tty *tp;
	int s, error;

	sc = (struct xc_softc *)device_get_softc(
		devclass_get_device(xc_devclass, unit));
	if (sc == NULL)
		return (ENXIO);
    
	tp = dev->si_tty;
	s = spltty();
	if (!ISTTYOPEN(tp)) {
		tp->t_state |= TS_CARR_ON;
		ttychars(tp);
		tp->t_iflag = TTYDEF_IFLAG;
		tp->t_oflag = TTYDEF_OFLAG;
		tp->t_cflag = TTYDEF_CFLAG|CLOCAL;
		tp->t_lflag = TTYDEF_LFLAG;
		tp->t_ispeed = tp->t_ospeed = TTYDEF_SPEED;
		xcparam(tp, &tp->t_termios);
		ttsetwater(tp);
	} else if (tp->t_state & TS_XCLUDE && priv_check(td, PRIV_ROOT)) {
		splx(s);
		return (EBUSY);
	}
	splx(s);

	xen_console_up = 1;

	error =  (*linesw[tp->t_line]->l_open)(dev, tp);
	return error;
}

int
xcclose(struct cdev *dev, int flag, int mode, struct thread *td)
{
	struct tty *tp = dev->si_tty;
    
	if (tp == NULL)
		return (0);
	xen_console_up = 0;
    
	spltty();
	(*linesw[tp->t_line]->l_close)(tp, flag);
	tty_close(tp);
	spl0();
	return (0);
}


int
xcioctl(struct cdev *dev, u_long cmd, caddr_t data, int flag, struct thread *td)
{
	struct tty *tp = dev->si_tty;
	int error;
    
	error = (*linesw[tp->t_line]->l_ioctl)(tp, cmd, data, flag, td);
	if (error != ENOIOCTL)
		return (error);

	error = ttioctl(tp, cmd, data, flag);

	if (error != ENOIOCTL)
		return (error);

	return (ENOTTY);
}

static inline int 
__xencons_put_char(int ch)
{
	char _ch = (char)ch;
	if ((wp - wc) == WBUF_SIZE)
		return 0;
	wbuf[WBUF_MASK(wp++)] = _ch;
	return 1;
}


static void
xcstart(struct tty *tp)
{
	boolean_t cons_full = FALSE;

	CN_LOCK(cn_mtx);
	if (tp->t_state & (TS_TIMEOUT | TS_TTSTOP)) {
			CN_UNLOCK(cn_mtx);

		ttwwakeup(tp);
		return;
	}

	tp->t_state |= TS_BUSY;
	CN_UNLOCK(cn_mtx);

	while (tp->t_outq.c_cc != 0 && !cons_full)
		cons_full = xcons_putc(getc(&tp->t_outq));

	/* if the console is close to full leave our state as busy */
	if (!cons_full) {
			CN_LOCK(cn_mtx);
			tp->t_state &= ~TS_BUSY;
			CN_UNLOCK(cn_mtx);
			ttwwakeup(tp);
	} else {
	    	/* let the timeout kick us in a bit */
	    	xc_start_needed = TRUE;
	}

}

static void
xcstop(struct tty *tp, int flag)
{

	if (tp->t_state & TS_BUSY) {
		if ((tp->t_state & TS_TTSTOP) == 0) {
			tp->t_state |= TS_FLUSH;
		}
	}
}

static void
xc_timeout(void *v)
{
	struct	tty *tp;
	int 	c;

	tp = (struct tty *)v;

	while ((c = xccncheckc(NULL)) != -1) {
		if (tp->t_state & TS_ISOPEN) {
			(*linesw[tp->t_line]->l_rint)(c, tp);
		}
	}

	if (xc_start_needed) {
	    	xc_start_needed = FALSE;
		xcstart(tp);
	}

	callout_reset(&xc_callout, XC_POLLTIME, xc_timeout, tp);
}

/*
 * Set line parameters.
 */
int
xcparam(struct tty *tp, struct termios *t)
{
	tp->t_ispeed = t->c_ispeed;
	tp->t_ospeed = t->c_ospeed;
	tp->t_cflag = t->c_cflag;
	return (0);
}


static device_method_t xc_methods[] = {
	DEVMETHOD(device_identify, xc_identify),
	DEVMETHOD(device_probe, xc_probe),
	DEVMETHOD(device_attach, xc_attach),
	{0, 0}
};

static driver_t xc_driver = {
	driver_name,
	xc_methods,
	sizeof(struct xc_softc),
};

/*** Forcibly flush console data before dying. ***/
void 
xcons_force_flush(void)
{
	int        sz;

	if (xen_start_info->flags & SIF_INITDOMAIN)
		return;

	/* Spin until console data is flushed through to the domain controller. */
	while (wc != wp) {
		int sent = 0;
		if ((sz = wp - wc) == 0)
			continue;
		
		sent = xencons_ring_send(&wbuf[WBUF_MASK(wc)], sz);
		if (sent > 0)
			wc += sent;		
	}
}

DRIVER_MODULE(xc, nexus, xc_driver, xc_devclass, 0, 0);
/*
 * Local variables:
 * mode: C
 * c-set-style: "BSD"
 * c-basic-offset: 8
 * tab-width: 4
 * indent-tabs-mode: t
 * End:
 */
