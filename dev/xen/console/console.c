#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/module.h>
#include <sys/systm.h>
#include <sys/consio.h>
#include <sys/priv.h>
#include <sys/proc.h>
#include <sys/uio.h>
#include <sys/tty.h>
#include <sys/systm.h>
#include <sys/taskqueue.h>
#include <sys/conf.h>
#include <sys/kernel.h>
#include <sys/bus.h>
#include <machine/stdarg.h>
#include <xen/xen-os.h>
#include <xen/hypervisor.h>
#include <xen/xen_intr.h>
#include <sys/cons.h>
#include <sys/kdb.h>
#include <sys/proc.h>

#include <dev/xen/console/xencons_ring.h>
#include <xen/interface/io/console.h>


#include "opt_ddb.h"
#include "opt_printf.h"

#ifdef DDB
#include <ddb/ddb.h>
#endif

static char driver_name[] = "xc";
devclass_t xc_devclass; /* do not make static */
static void	xcoutwakeup(struct tty *);
static void	xc_timeout(void *);
static void __xencons_tx_flush(void);
static boolean_t xcons_putc(int c);

/* switch console so that shutdown can occur gracefully */
static void xc_shutdown(void *arg, int howto);
static int xc_mute;

static void xcons_force_flush(void);
static void xencons_priv_interrupt(void *);

static cn_probe_t       xc_cnprobe;
static cn_init_t        xc_cninit;
static cn_term_t        xc_cnterm;
static cn_getc_t        xc_cngetc;
static cn_putc_t        xc_cnputc;
static cn_grab_t        xc_cngrab;
static cn_ungrab_t      xc_cnungrab;

#define XC_POLLTIME 	(hz/10)

CONSOLE_DRIVER(xc);

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
bool cnsl_evt_reg;
static unsigned int wc, wp; /* write_cons, write_prod */
xen_intr_handle_t xen_intr_handle;
device_t xencons_dev;

/* Virtual address of the shared console page */
char *console_page;

#ifdef KDB
static int	xc_altbrk;
#endif

#define CDEV_MAJOR 12
#define	XCUNIT(x)	(dev2unit(x))
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

static tsw_open_t	xcopen;
static tsw_close_t	xcclose;

static struct ttydevsw xc_ttydevsw = {
        .tsw_flags	= TF_NOPREFIX,
        .tsw_open	= xcopen,
        .tsw_close	= xcclose,
        .tsw_outwakeup	= xcoutwakeup,
};

/*----------------------------- Debug function -------------------------------*/
struct putchar_arg {
	char	*buf;
	size_t	size;
	size_t	n_next;
};

static void
putchar(int c, void *arg)
{
	struct putchar_arg *pca;

	pca = (struct putchar_arg *)arg;

	if (pca->buf == NULL) {
		/*
		 * We have no buffer, output directly to the
		 * console char by char.
		 */
		HYPERVISOR_console_write((char *)&c, 1);
	} else {
		pca->buf[pca->n_next++] = c;
		if ((pca->size == pca->n_next) || (c = '\0')) {
			/* Flush the buffer */
			HYPERVISOR_console_write(pca->buf, pca->n_next);
			pca->n_next = 0;
		}
	}
}

void
xc_printf(const char *fmt, ...)
{
	va_list ap;
	struct putchar_arg pca;
#ifdef PRINTF_BUFR_SIZE
	char buf[PRINTF_BUFR_SIZE];

	pca.buf = buf;
	pca.size = sizeof(buf);
	pca.n_next = 0;
#else
	pca.buf = NULL;
	pca.size = 0;
#endif

	KASSERT((xen_domain()), ("call to xc_printf from non Xen guest"));

	va_start(ap, fmt);
	kvprintf(fmt, putchar, &pca, 10, ap);
	va_end(ap);

#ifdef PRINTF_BUFR_SIZE
	if (pca.n_next != 0)
		HYPERVISOR_console_write(buf, pca.n_next);
#endif
}

static void
xc_cnprobe(struct consdev *cp)
{
	if (!xen_pv_domain())
		return;

	cp->cn_pri = CN_REMOTE;
	sprintf(cp->cn_name, "%s0", driver_name);
}


static void
xc_cninit(struct consdev *cp)
{ 
	CN_LOCK_INIT(cn_mtx,"XCONS LOCK");

}

static void
xc_cnterm(struct consdev *cp)
{ 
}

static void
xc_cngrab(struct consdev *cp)
{
}

static void
xc_cnungrab(struct consdev *cp)
{
}

static int
xc_cngetc(struct consdev *dev)
{
	int ret;

	if (xencons_has_input())
		xencons_handle_input(NULL);
	
	CN_LOCK(cn_mtx);
	if ((rp - rc) && !xc_mute) {
		/* we need to return only one char */
		ret = (int)rbuf[RBUF_MASK(rc)];
		rc++;
	} else
		ret = -1;
	CN_UNLOCK(cn_mtx);
	return(ret);
}

static void
xc_cnputc_domu(struct consdev *dev, int c)
{
	xcons_putc(c);
}

static void
xc_cnputc_dom0(struct consdev *dev, int c)
{
	HYPERVISOR_console_io(CONSOLEIO_write, 1, (char *)&c);
}

static void
xc_cnputc(struct consdev *dev, int c)
{

	if (xen_initial_domain())
		xc_cnputc_dom0(dev, c);
	else
		xc_cnputc_domu(dev, c);
}

static boolean_t
xcons_putc(int c)
{
	int force_flush = xc_mute ||
#ifdef DDB
		kdb_active ||
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
	__xencons_tx_flush();
	
	/* inform start path that we're pretty full */
	return ((wp - wc) >= WBUF_SIZE - 100) ? TRUE : FALSE;
}

static void
xc_identify(driver_t *driver, device_t parent)
{
	device_t child;

	if (!xen_pv_domain())
		return;

	child = BUS_ADD_CHILD(parent, 0, driver_name, 0);
	device_set_driver(child, driver);
	device_set_desc(child, "Xen Console");
}

static int
xc_probe(device_t dev)
{

	return (BUS_PROBE_NOWILDCARD);
}

static int
xc_attach(device_t dev) 
{
	int error;

	xencons_dev = dev;
	xccons = tty_alloc(&xc_ttydevsw, NULL);
	tty_makedev(xccons, NULL, "xc%r", 0);

	callout_init(&xc_callout, 0);

	xencons_ring_init();

	cnsl_evt_reg = true;
	callout_reset(&xc_callout, XC_POLLTIME, xc_timeout, xccons);
    
	if (xen_initial_domain()) {
		error = xen_intr_bind_virq(dev, VIRQ_CONSOLE, 0, NULL,
		                           xencons_priv_interrupt, NULL,
		                           INTR_TYPE_TTY, &xen_intr_handle);
		KASSERT(error >= 0, ("can't register console interrupt"));
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

	if (xen_console_up
#ifdef DDB
	    && !kdb_active
#endif
		) {
		tty_lock(tp);
		for (i = 0; i < len; i++) {
#ifdef KDB
			kdb_alt_break(buf[i], &xc_altbrk);
#endif
			ttydisc_rint(tp, buf[i], 0);
		}
		ttydisc_rint_done(tp);
		tty_unlock(tp);
	} else {
		CN_LOCK(cn_mtx);
		for (i = 0; i < len; i++)
			rbuf[RBUF_MASK(rp++)] = buf[i];
		CN_UNLOCK(cn_mtx);
	}
}

static void 
__xencons_tx_flush(void)
{
	int        sz;

	CN_LOCK(cn_mtx);
	while (wc != wp) {
		int sent;
		sz = wp - wc;
		if (sz > (WBUF_SIZE - WBUF_MASK(wc)))
			sz = WBUF_SIZE - WBUF_MASK(wc);
		if (xen_initial_domain()) {
			HYPERVISOR_console_io(CONSOLEIO_write, sz, &wbuf[WBUF_MASK(wc)]);
			wc += sz;
		} else {
			sent = xencons_ring_send(&wbuf[WBUF_MASK(wc)], sz);
			if (sent == 0) 
				break;
			wc += sent;
		}
	}
	CN_UNLOCK(cn_mtx);
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

static int
xcopen(struct tty *tp)
{

	xen_console_up = 1;
	return (0);
}

static void
xcclose(struct tty *tp)
{

	xen_console_up = 0;
}

#if 0
static inline int 
__xencons_put_char(int ch)
{
	char _ch = (char)ch;
	if ((wp - wc) == WBUF_SIZE)
		return 0;
	wbuf[WBUF_MASK(wp++)] = _ch;
	return 1;
}
#endif


static void
xcoutwakeup(struct tty *tp)
{
	boolean_t cons_full = FALSE;
	char c;

	while (ttydisc_getc(tp, &c, 1) == 1 && !cons_full)
		cons_full = xcons_putc(c);

	if (cons_full) {
	    	/* let the timeout kick us in a bit */
	    	xc_start_needed = TRUE;
	}

}

static void
xc_timeout(void *v)
{
	struct	tty *tp;
	int 	c;

	tp = (struct tty *)v;

	tty_lock(tp);
	while ((c = xc_cngetc(NULL)) != -1)
		ttydisc_rint(tp, c, 0);

	if (xc_start_needed) {
	    	xc_start_needed = FALSE;
		xcoutwakeup(tp);
	}
	tty_unlock(tp);

	callout_reset(&xc_callout, XC_POLLTIME, xc_timeout, tp);
}

static device_method_t xc_methods[] = {
	DEVMETHOD(device_identify, xc_identify),
	DEVMETHOD(device_probe, xc_probe),
	DEVMETHOD(device_attach, xc_attach),

	DEVMETHOD_END
};

static driver_t xc_driver = {
	driver_name,
	xc_methods,
	0,
};

/*** Forcibly flush console data before dying. ***/
void 
xcons_force_flush(void)
{
	int        sz;

	if (xen_initial_domain())
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

DRIVER_MODULE(xc, xenpv, xc_driver, xc_devclass, 0, 0);
