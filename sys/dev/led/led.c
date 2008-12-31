/*-
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <phk@FreeBSD.org> wrote this file.  As long as you retain this notice you
 * can do whatever you want with this stuff. If we meet some day, and you think
 * this stuff is worth it, you can buy me a beer in return.   Poul-Henning Kamp
 * ----------------------------------------------------------------------------
 *
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/sys/dev/led/led.c,v 1.17.6.1 2008/11/25 02:59:29 kensmith Exp $");

#include <sys/param.h>
#include <sys/conf.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/ctype.h>
#include <sys/sbuf.h>
#include <sys/queue.h>
#include <dev/led/led.h>
#include <sys/uio.h>
#include <sys/sx.h>

struct ledsc {
	LIST_ENTRY(ledsc)	list;
	void			*private;
	int			unit;
	led_t			*func;
	struct cdev *dev;
	struct sbuf		*spec;
	char			*str;
	char			*ptr;
	int			count;
	time_t			last_second;
};

static struct unrhdr *led_unit;
static struct mtx led_mtx;
static struct sx led_sx;
static LIST_HEAD(, ledsc) led_list = LIST_HEAD_INITIALIZER(&led_list);
static struct callout led_ch;

static MALLOC_DEFINE(M_LED, "LED", "LED driver");

static void
led_timeout(void *p)
{
	struct ledsc	*sc;

	mtx_lock(&led_mtx);
	LIST_FOREACH(sc, &led_list, list) {
		if (sc->ptr == NULL)
			continue;
		if (sc->count > 0) {
			sc->count--;
			continue;
		}
		if (*sc->ptr == '.') {
			sc->ptr = NULL;
			continue;
		} else if (*sc->ptr == 'U' || *sc->ptr == 'u') {
			if (sc->last_second == time_second)
				continue;
			sc->last_second = time_second;
			sc->func(sc->private, *sc->ptr == 'U');
		} else if (*sc->ptr >= 'a' && *sc->ptr <= 'j') {
			sc->func(sc->private, 0);
			sc->count = (*sc->ptr & 0xf) - 1;
		} else if (*sc->ptr >= 'A' && *sc->ptr <= 'J') {
			sc->func(sc->private, 1);
			sc->count = (*sc->ptr & 0xf) - 1;
		}
		sc->ptr++;
		if (*sc->ptr == '\0')
			sc->ptr = sc->str;
	}
	mtx_unlock(&led_mtx);
	callout_reset(&led_ch, hz / 10, led_timeout, p);
	return;
}

static int
led_state(struct cdev *dev, struct sbuf *sb, int state)
{
	struct sbuf *sb2 = NULL;
	struct ledsc *sc;

	mtx_lock(&led_mtx);
	sc = dev->si_drv1;
	if (sc != NULL) {
		sb2 = sc->spec;
		sc->spec = sb;
		if (sb != NULL) {
			sc->str = sbuf_data(sb);
			sc->ptr = sc->str;
		} else {
			sc->str = NULL;
			sc->ptr = NULL;
			sc->func(sc->private, state);
		}
		sc->count = 0;
	}
	mtx_unlock(&led_mtx);
	if (sb2 != NULL)
		sbuf_delete(sb2);
	if (sc == NULL)
		return (ENXIO);
	return(0);
}

static int
led_write(struct cdev *dev, struct uio *uio, int ioflag)
{
	int error;
	char *s, *s2;
	struct sbuf *sb = NULL;
	int i;

	if (dev->si_drv1 == NULL)
		return (ENXIO);

	if (uio->uio_resid > 512)
		return (EINVAL);
	s2 = s = malloc(uio->uio_resid + 1, M_DEVBUF, M_WAITOK);
	s[uio->uio_resid] = '\0';
	error = uiomove(s, uio->uio_resid, uio);
	if (error) {
		free(s2, M_DEVBUF);
		return (error);
	}

	/*
	 * Handle "on" and "off" immediately so people can flash really
	 * fast from userland if they want to
	 */
	if (*s == '0' || *s == '1') {
		error = led_state(dev, NULL, *s & 1);
		free(s2, M_DEVBUF);
		return(error);
	}

	sb = sbuf_new(NULL, NULL, 0, SBUF_AUTOEXTEND);
	if (sb == NULL) {
		free(s2, M_DEVBUF);
		return (ENOMEM);
	}
		
	switch(s[0]) {
		/*
		 * Flash, default is 100msec/100msec.
		 * 'f2' sets 200msec/200msec etc.
		 */
		case 'f':
			if (s[1] >= '1' && s[1] <= '9')
				i = s[1] - '1';
			else
				i = 0;
			sbuf_printf(sb, "%c%c", 'A' + i, 'a' + i);
			break;
		/*
		 * Digits, flashes out numbers.
		 * 'd12' becomes -__________-_-______________________________
		 */
		case 'd':
			for(s++; *s; s++) {
				if (!isdigit(*s))
					continue;
				i = *s - '0';
				if (i == 0)
					i = 10;
				for (; i > 1; i--) 
					sbuf_cat(sb, "Aa");
				sbuf_cat(sb, "Aj");
			}
			sbuf_cat(sb, "jj");
			break;
		/*
		 * String, roll your own.
		 * 'a-j' gives "off" for n/10 sec.
		 * 'A-J' gives "on" for n/10 sec.
		 * no delay before repeat
		 * 'sAaAbBa' becomes _-_--__-
		 */
		case 's':
			for(s++; *s; s++) {
				if ((*s >= 'a' && *s <= 'j') ||
				    (*s >= 'A' && *s <= 'J') ||
				    *s == 'U' || *s <= 'u' ||
					*s == '.')
					sbuf_bcat(sb, s, 1);
			}
			break;
		/*
		 * Morse.
		 * '.' becomes _-
		 * '-' becomes _---
		 * ' ' becomes __
		 * '\n' becomes ____
		 * 1sec pause between repeats
		 * '... --- ...' -> _-_-_-___---_---_---___-_-_-__________
		 */
		case 'm':
			for(s++; *s; s++) {
				if (*s == '.')
					sbuf_cat(sb, "aA");
				else if (*s == '-')
					sbuf_cat(sb, "aC");
				else if (*s == ' ')
					sbuf_cat(sb, "b");
				else if (*s == '\n')
					sbuf_cat(sb, "d");
			}
			sbuf_cat(sb, "j");
			break;
		default:
			sbuf_delete(sb);
			free(s2, M_DEVBUF);
			return (EINVAL);
	}
	sbuf_finish(sb);
	free(s2, M_DEVBUF);
	if (sbuf_overflowed(sb)) {
		sbuf_delete(sb);
		return (ENOMEM);
	}
	if (sbuf_len(sb) == 0) {
		sbuf_delete(sb);
		return (0);
	}

	return (led_state(dev, sb, 0));
}

static struct cdevsw led_cdevsw = {
	.d_version =	D_VERSION,
	.d_write =	led_write,
	.d_name =	"LED",
};

struct cdev *
led_create(led_t *func, void *priv, char const *name)
{

	return (led_create_state(func, priv, name, 0));
}
struct cdev *
led_create_state(led_t *func, void *priv, char const *name, int state)
{
	struct ledsc	*sc;

	sc = malloc(sizeof *sc, M_LED, M_WAITOK | M_ZERO);

	sx_xlock(&led_sx);
	sc->unit = alloc_unr(led_unit);
	sc->private = priv;
	sc->func = func;
	sc->dev = make_dev(&led_cdevsw, unit2minor(sc->unit),
	    UID_ROOT, GID_WHEEL, 0600, "led/%s", name);
	sx_xunlock(&led_sx);

	mtx_lock(&led_mtx);
	sc->dev->si_drv1 = sc;
	if (LIST_EMPTY(&led_list))
		callout_reset(&led_ch, hz / 10, led_timeout, NULL);
	LIST_INSERT_HEAD(&led_list, sc, list);
	sc->func(sc->private, state != 0);
	mtx_unlock(&led_mtx);

	return (sc->dev);
}

void
led_destroy(struct cdev *dev)
{
	struct ledsc *sc;

	mtx_lock(&led_mtx);
	sc = dev->si_drv1;
	dev->si_drv1 = NULL;

	LIST_REMOVE(sc, list);
	if (LIST_EMPTY(&led_list))
		callout_stop(&led_ch);
	mtx_unlock(&led_mtx);

	sx_xlock(&led_sx);
	free_unr(led_unit, sc->unit);
	destroy_dev(dev);
	if (sc->spec != NULL)
		sbuf_delete(sc->spec);
	free(sc, M_LED);
	sx_xunlock(&led_sx);
}

static void
led_drvinit(void *unused)
{

	led_unit = new_unrhdr(0, minor2unit(MAXMINOR), NULL);
	mtx_init(&led_mtx, "LED mtx", NULL, MTX_DEF);
	sx_init(&led_sx, "LED sx");
	callout_init(&led_ch, CALLOUT_MPSAFE);
}

SYSINIT(leddev, SI_SUB_DRIVERS, SI_ORDER_MIDDLE, led_drvinit, NULL);
