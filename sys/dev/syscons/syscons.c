/*-
 * Copyright (c) 1992-1998 Søren Schmidt
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer,
 *    without modification, immediately at the beginning of the file.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *	$Id: syscons.c,v 1.314 1999/07/18 06:16:53 yokota Exp $
 */

#include "sc.h"
#include "splash.h"
#include "opt_syscons.h"
#include "opt_ddb.h"
#ifdef __i386__
#include "apm.h"
#endif

#if NSC > 0
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/reboot.h>
#include <sys/conf.h>
#include <sys/proc.h>
#include <sys/signalvar.h>
#include <sys/tty.h>
#include <sys/kernel.h>
#include <sys/malloc.h>

#include <machine/clock.h>
#include <machine/cons.h>
#include <machine/console.h>
#include <machine/psl.h>
#include <machine/pc/display.h>
#ifdef __i386__
#include <machine/apm_bios.h>
#include <machine/frame.h>
#include <machine/random.h>
#endif

#include <dev/kbd/kbdreg.h>
#include <dev/fb/fbreg.h>
#include <dev/fb/splashreg.h>
#include <dev/syscons/syscons.h>

#define COLD 0
#define WARM 1

#define DEFAULT_BLANKTIME	(5*60)		/* 5 minutes */
#define MAX_BLANKTIME		(7*24*60*60)	/* 7 days!? */

#define KEYCODE_BS		0x0e		/* "<-- Backspace" key, XXX */

static default_attr user_default = {
    SC_NORM_ATTR << 8,
    SC_NORM_REV_ATTR << 8,
};

static default_attr kernel_default = {
    SC_KERNEL_CONS_ATTR << 8,
    SC_KERNEL_CONS_REV_ATTR << 8,
};

static	int		sc_console_unit = -1;
static  scr_stat    	*sc_console;
static  term_stat   	kernel_console;
static  default_attr    *current_default;

static  char        	init_done = COLD;
static  char		shutdown_in_progress = FALSE;
static	char		sc_malloc = FALSE;

static	int		saver_mode = CONS_LKM_SAVER; /* LKM/user saver */
static	int		run_scrn_saver = FALSE;	/* should run the saver? */
static	long        	scrn_blank_time = 0;    /* screen saver timeout value */
#if NSPLASH > 0
static	int     	scrn_blanked;		/* # of blanked screen */
static	int		sticky_splash = FALSE;

static	void		none_saver(sc_softc_t *sc, int blank) { }
static	void		(*current_saver)(sc_softc_t *, int) = none_saver;
#endif

#if !defined(SC_NO_FONT_LOADING) && defined(SC_DFLT_FONT)
#include "font.h"
#endif

	d_ioctl_t	*sc_user_ioctl;

static	bios_values_t	bios_value;

static struct tty     	sccons[2];
#define SC_MOUSE 	128
#define SC_CONSOLECTL	255

#define VIRTUAL_TTY(sc, x) (&((sc)->tty[(x) - (sc)->first_vty]))
#define CONSOLE_TTY	(&sccons[0])
#define MOUSE_TTY	(&sccons[1])

#define debugger	FALSE

#ifdef __i386__
#ifdef DDB
extern int		in_Debugger;
#undef debugger
#define debugger	in_Debugger
#endif /* DDB */
#endif /* __i386__ */

/* prototypes */
static int scvidprobe(int unit, int flags, int cons);
static int sckbdprobe(int unit, int flags, int cons);
static void scmeminit(void *arg);
static int scdevtounit(dev_t dev);
static kbd_callback_func_t sckbdevent;
static int scparam(struct tty *tp, struct termios *t);
static void scstart(struct tty *tp);
static void scmousestart(struct tty *tp);
static void scinit(int unit, int flags);
#if __i386__
static void scterm(int unit, int flags);
#endif
static void scshutdown(int howto, void *arg);
static u_int scgetc(sc_softc_t *sc, u_int flags);
#define SCGETC_CN	1
#define SCGETC_NONBLOCK	2
static int sccngetch(int flags);
static void sccnupdate(scr_stat *scp);
static scr_stat *alloc_scp(sc_softc_t *sc, int vty);
static void init_scp(sc_softc_t *sc, int vty, scr_stat *scp);
static timeout_t scrn_timer;
static int and_region(int *s1, int *e1, int s2, int e2);
static void scrn_update(scr_stat *scp, int show_cursor);

#if NSPLASH > 0
static int scsplash_callback(int event, void *arg);
static void scsplash_saver(sc_softc_t *sc, int show);
static int add_scrn_saver(void (*this_saver)(sc_softc_t *, int));
static int remove_scrn_saver(void (*this_saver)(sc_softc_t *, int));
static int set_scrn_saver_mode(scr_stat *scp, int mode, u_char *pal, int border);
static int restore_scrn_saver_mode(scr_stat *scp, int changemode);
static void stop_scrn_saver(sc_softc_t *sc, void (*saver)(sc_softc_t *, int));
static int wait_scrn_saver_stop(sc_softc_t *sc);
#define scsplash_stick(stick)		(sticky_splash = (stick))
#else /* !NSPLASH */
#define scsplash_stick(stick)
#endif /* NSPLASH */

static int switch_scr(sc_softc_t *sc, u_int next_scr);
static int do_switch_scr(sc_softc_t *sc, int s);
static int vt_proc_alive(scr_stat *scp);
static int signal_vt_rel(scr_stat *scp);
static int signal_vt_acq(scr_stat *scp);
static void exchange_scr(sc_softc_t *sc);
static void scan_esc(scr_stat *scp, u_char c);
static void ansi_put(scr_stat *scp, u_char *buf, int len);
static void draw_cursor_image(scr_stat *scp);
static void remove_cursor_image(scr_stat *scp);
static void update_cursor_image(scr_stat *scp);
static void move_crsr(scr_stat *scp, int x, int y);
static int mask2attr(struct term_stat *term);
static int save_kbd_state(scr_stat *scp);
static int update_kbd_state(scr_stat *scp, int state, int mask);
static int update_kbd_leds(scr_stat *scp, int which);
static void do_bell(scr_stat *scp, int pitch, int duration);
static timeout_t blink_screen;

#define	CDEV_MAJOR	12

static cn_probe_t	sccnprobe;
static cn_init_t	sccninit;
static cn_getc_t	sccngetc;
static cn_checkc_t	sccncheckc;
static cn_putc_t	sccnputc;
static cn_term_t	sccnterm;

#if __alpha__
void sccnattach(void);
#endif

CONS_DRIVER(sc, sccnprobe, sccninit, sccnterm, sccngetc, sccncheckc, sccnputc);

static	d_open_t	scopen;
static	d_close_t	scclose;
static	d_read_t	scread;
static	d_write_t	scwrite;
static	d_ioctl_t	scioctl;
static	d_mmap_t	scmmap;

static struct cdevsw sc_cdevsw = {
	/* open */	scopen,
	/* close */	scclose,
	/* read */	scread,
	/* write */	scwrite,
	/* ioctl */	scioctl,
	/* stop */	nostop,
	/* reset */	noreset,
	/* devtotty */	scdevtotty,
	/* poll */	ttpoll,
	/* mmap */	scmmap,
	/* strategy */	nostrategy,
	/* name */	"sc",
	/* parms */	noparms,
	/* maj */	CDEV_MAJOR,
	/* dump */	nodump,
	/* psize */	nopsize,
	/* flags */	D_TTY,
	/* maxio */	0,
	/* bmaj */	-1
};

int
sc_probe_unit(int unit, int flags)
{
    if (!scvidprobe(unit, flags, FALSE)) {
	if (bootverbose)
	    printf("sc%d: no video adapter is found.\n", unit);
	return ENXIO;
    }

    /* syscons will be attached even when there is no keyboard */
    sckbdprobe(unit, flags, FALSE);

    return 0;
}

/* probe video adapters, return TRUE if found */ 
static int
scvidprobe(int unit, int flags, int cons)
{
    /*
     * Access the video adapter driver through the back door!
     * Video adapter drivers need to be configured before syscons.
     * However, when syscons is being probed as the low-level console,
     * they have not been initialized yet.  We force them to initialize
     * themselves here. XXX
     */
    vid_configure(cons ? VIO_PROBE_ONLY : 0);

    return (vid_find_adapter("*", unit) >= 0);
}

/* probe the keyboard, return TRUE if found */
static int
sckbdprobe(int unit, int flags, int cons)
{
    /* access the keyboard driver through the backdoor! */
    kbd_configure(cons ? KB_CONF_PROBE_ONLY : 0);

    return (kbd_find_keyboard("*", unit) >= 0);
}

static char
*adapter_name(video_adapter_t *adp)
{
    static struct {
	int type;
	char *name[2];
    } names[] = {
	{ KD_MONO,	{ "MDA",	"MDA" } },
	{ KD_HERCULES,	{ "Hercules",	"Hercules" } },
	{ KD_CGA,	{ "CGA",	"CGA" } },
	{ KD_EGA,	{ "EGA",	"EGA (mono)" } },
	{ KD_VGA,	{ "VGA",	"VGA (mono)" } },
	{ KD_PC98,	{ "PC-98x1",	"PC-98x1" } },
	{ KD_TGA,	{ "TGA",	"TGA" } },
	{ -1,		{ "Unknown",	"Unknown" } },
    };
    int i;

    for (i = 0; names[i].type != -1; ++i)
	if (names[i].type == adp->va_type)
	    break;
    return names[i].name[(adp->va_flags & V_ADP_COLOR) ? 0 : 1];
}

int
sc_attach_unit(int unit, int flags)
{
    sc_softc_t *sc;
    scr_stat *scp;
#ifdef SC_PIXEL_MODE
    video_info_t info;
#endif
    int vc;
    dev_t dev;

    scmeminit(NULL);		/* XXX */

    flags &= ~SC_KERNEL_CONSOLE;
    if (sc_console_unit == unit)
	flags |= SC_KERNEL_CONSOLE;
    scinit(unit, flags);
    sc = sc_get_softc(unit, flags & SC_KERNEL_CONSOLE);
    sc->config = flags;
    scp = sc->console[0];
    if (sc_console == NULL)	/* sc_console_unit < 0 */
	sc_console = scp;

#ifdef SC_PIXEL_MODE
    if ((sc->config & SC_VESA800X600)
	&& ((*vidsw[sc->adapter]->get_info)(sc->adp, M_VESA_800x600, &info) == 0)) {
#if NSPLASH > 0
	if (sc->flags & SC_SPLASH_SCRN)
	    splash_term(sc->adp);
#endif
	sc_set_graphics_mode(scp, NULL, M_VESA_800x600);
	sc_set_pixel_mode(scp, NULL, COL, ROW, 16);
	sc->initial_mode = M_VESA_800x600;
#if NSPLASH > 0
	/* put up the splash again! */
	if (sc->flags & SC_SPLASH_SCRN)
    	    splash_init(sc->adp, scsplash_callback, sc);
#endif
    }
#endif /* SC_PIXEL_MODE */

    /* initialize cursor */
    if (!ISGRAPHSC(scp))
    	update_cursor_image(scp);

    /* get screen update going */
    scrn_timer(sc);

    /* set up the keyboard */
    kbd_ioctl(sc->kbd, KDSKBMODE, (caddr_t)&scp->kbd_mode);
    update_kbd_state(scp, scp->status, LOCK_MASK);

    printf("sc%d: %s <%d virtual consoles, flags=0x%x>\n",
	   unit, adapter_name(sc->adp), sc->vtys, sc->config);
    if (bootverbose) {
	printf("sc%d:", unit);
    	if (sc->adapter >= 0)
	    printf(" fb%d", sc->adapter);
	if (sc->keyboard >= 0)
	    printf(" kbd%d", sc->keyboard);
	printf("\n");
    }

    /* register a shutdown callback for the kernel console */
    if (sc_console_unit == unit)
	at_shutdown(scshutdown, (void *)(uintptr_t)unit, SHUTDOWN_PRE_SYNC);

    /*
     * syscons's cdevsw must be registered from here. As syscons and
     * pcvt share the same major number, their cdevsw cannot be
     * registered at module loading/initialization time or by SYSINIT.
     */
    cdevsw_add(&sc_cdevsw);	/* XXX do this just once... */

    for (vc = 0; vc < MAXCONS; vc++) {
        dev = make_dev(&sc_cdevsw, vc,
	    UID_ROOT, GID_WHEEL, 0600, "ttyv%r", vc + unit * MAXCONS);
	dev->si_tty_tty = &sc->tty[vc];
    }
    if (scp == sc_console) {
#ifndef SC_NO_SYSMOUSE
	dev = make_dev(&sc_cdevsw, SC_MOUSE,
	    UID_ROOT, GID_WHEEL, 0600, "sysmouse");
	dev->si_tty_tty = MOUSE_TTY;
#endif /* SC_NO_SYSMOUSE */
	dev = make_dev(&sc_cdevsw, SC_CONSOLECTL,
	    UID_ROOT, GID_WHEEL, 0600, "consolectl");
	dev->si_tty_tty = CONSOLE_TTY;
    }

    return 0;
}

static void
scmeminit(void *arg)
{
    if (sc_malloc)
	return;
    sc_malloc = TRUE;

    /*
     * As soon as malloc() becomes functional, we had better allocate
     * various buffers for the kernel console.
     */

    if (sc_console_unit < 0)
	return;

    /* copy the temporary buffer to the final buffer */
    sc_alloc_scr_buffer(sc_console, FALSE, FALSE);

#ifndef SC_NO_CUTPASTE
    /* cut buffer is available only when the mouse pointer is used */
    if (ISMOUSEAVAIL(sc_console->sc->adp->va_flags))
	sc_alloc_cut_buffer(sc_console, FALSE);
#endif

#ifndef SC_NO_HISTORY
    /* initialize history buffer & pointers */
    sc_alloc_history_buffer(sc_console, 0, 0, FALSE);
#endif
}

/* XXX */
SYSINIT(sc_mem, SI_SUB_KMEM, SI_ORDER_ANY, scmeminit, NULL);

int
sc_resume_unit(int unit)
{
    /* XXX should be moved to the keyboard driver? */
    sc_softc_t *sc;

    sc = sc_get_softc(unit, (sc_console_unit == unit) ? SC_KERNEL_CONSOLE : 0);
    if (sc->kbd != NULL)
	kbd_clear_state(sc->kbd);
    return 0;
}

struct tty
*scdevtotty(dev_t dev)
{

    return (dev->si_tty_tty);
}

static int
scdevtounit(dev_t dev)
{
    int vty = SC_VTY(dev);

    if (vty == SC_CONSOLECTL)
	return ((sc_console != NULL) ? sc_console->sc->unit : -1);
    else if (vty == SC_MOUSE)
	return -1;
    else if ((vty < 0) || (vty >= MAXCONS*sc_max_unit()))
	return -1;
    else
	return vty/MAXCONS;
}

int
scopen(dev_t dev, int flag, int mode, struct proc *p)
{
    struct tty *tp = dev->si_tty_tty;
    int unit = scdevtounit(dev);
    sc_softc_t *sc;
    keyarg_t key;
    int error;

    if (!tp)
	return(ENXIO);

    DPRINTF(5, ("scopen: dev:%d,%d, unit:%d, vty:%d\n",
		major(dev), minor(dev), unit, SC_VTY(dev)));

    /* sc == NULL, if SC_VTY(dev) == SC_MOUSE */
    sc = sc_get_softc(unit, (sc_console_unit == unit) ? SC_KERNEL_CONSOLE : 0);

    tp->t_oproc = (SC_VTY(dev) == SC_MOUSE) ? scmousestart : scstart;
    tp->t_param = scparam;
    tp->t_dev = dev;
    if (!(tp->t_state & TS_ISOPEN)) {
	ttychars(tp);
        /* Use the current setting of the <-- key as default VERASE. */  
        /* If the Delete key is preferable, an stty is necessary     */
	if (sc != NULL) {
	    key.keynum = KEYCODE_BS;
	    kbd_ioctl(sc->kbd, GIO_KEYMAPENT, (caddr_t)&key);
            tp->t_cc[VERASE] = key.key.map[0];
	}
	tp->t_iflag = TTYDEF_IFLAG;
	tp->t_oflag = TTYDEF_OFLAG;
	tp->t_cflag = TTYDEF_CFLAG;
	tp->t_lflag = TTYDEF_LFLAG;
	tp->t_ispeed = tp->t_ospeed = TTYDEF_SPEED;
	scparam(tp, &tp->t_termios);
	(*linesw[tp->t_line].l_modem)(tp, 1);
#ifndef SC_NO_SYSMOUSE
    	if (SC_VTY(dev) == SC_MOUSE)
	    sc_mouse_set_level(0);	/* XXX */
#endif
    }
    else
	if (tp->t_state & TS_XCLUDE && suser(p))
	    return(EBUSY);

    error = (*linesw[tp->t_line].l_open)(dev, tp);

    if ((SC_VTY(dev) != SC_CONSOLECTL) && (SC_VTY(dev) != SC_MOUSE)) {
	/* assert(sc != NULL) */
	if (sc->console[SC_VTY(dev) - sc->first_vty] == NULL) {
	    sc->console[SC_VTY(dev) - sc->first_vty]
		= alloc_scp(sc, SC_VTY(dev));
	    if (ISGRAPHSC(sc->console[SC_VTY(dev) - sc->first_vty]))
		sc_set_pixel_mode(sc->console[SC_VTY(dev) - sc->first_vty],
				  NULL, COL, ROW, 16);
	}
	if (!tp->t_winsize.ws_col && !tp->t_winsize.ws_row) {
	    tp->t_winsize.ws_col
		= sc->console[SC_VTY(dev) - sc->first_vty]->xsize;
	    tp->t_winsize.ws_row
		= sc->console[SC_VTY(dev) - sc->first_vty]->ysize;
	}
    }
    return error;
}

int
scclose(dev_t dev, int flag, int mode, struct proc *p)
{
    struct tty *tp = dev->si_tty_tty;
    struct scr_stat *scp;
    int s;

    if (!tp)
	return(ENXIO);
    if ((SC_VTY(dev) != SC_CONSOLECTL) && (SC_VTY(dev) != SC_MOUSE)) {
	scp = sc_get_scr_stat(tp->t_dev);
	/* were we in the middle of the VT switching process? */
	DPRINTF(5, ("sc%d: scclose(), ", scp->sc->unit));
	s = spltty();
	if ((scp == scp->sc->cur_scp) && (scp->sc->unit == sc_console_unit))
	    cons_unavail = FALSE;
	if (scp->status & SWITCH_WAIT_REL) {
	    /* assert(scp == scp->sc->cur_scp) */
	    DPRINTF(5, ("reset WAIT_REL, "));
	    scp->status &= ~SWITCH_WAIT_REL;
	    do_switch_scr(scp->sc, s);
	}
	if (scp->status & SWITCH_WAIT_ACQ) {
	    /* assert(scp == scp->sc->cur_scp) */
	    DPRINTF(5, ("reset WAIT_ACQ, "));
	    scp->status &= ~SWITCH_WAIT_ACQ;
	    scp->sc->switch_in_progress = 0;
	}
#if not_yet_done
	if (scp == &main_console) {
	    scp->pid = 0;
	    scp->proc = NULL;
	    scp->smode.mode = VT_AUTO;
	}
	else {
	    sc_vtb_destroy(&scp->vtb);
	    sc_vtb_destroy(&scp->scr);
	    sc_free_history_buffer(scp, scp->ysize);
	    free(scp, M_DEVBUF);
	    sc->console[SC_VTY(dev) - sc->first_vty] = NULL;
	}
#else
	scp->pid = 0;
	scp->proc = NULL;
	scp->smode.mode = VT_AUTO;
#endif
	scp->kbd_mode = K_XLATE;
	if (scp == scp->sc->cur_scp)
	    kbd_ioctl(scp->sc->kbd, KDSKBMODE, (caddr_t)&scp->kbd_mode);
	DPRINTF(5, ("done.\n"));
    }
    spltty();
    (*linesw[tp->t_line].l_close)(tp, flag);
    ttyclose(tp);
    spl0();
    return(0);
}

int
scread(dev_t dev, struct uio *uio, int flag)
{
    struct tty *tp = dev->si_tty_tty;

    if (!tp)
	return(ENXIO);
    sc_touch_scrn_saver();
    return((*linesw[tp->t_line].l_read)(tp, uio, flag));
}

int
scwrite(dev_t dev, struct uio *uio, int flag)
{
    struct tty *tp = dev->si_tty_tty;

    if (!tp)
	return(ENXIO);
    return((*linesw[tp->t_line].l_write)(tp, uio, flag));
}

static int
sckbdevent(keyboard_t *thiskbd, int event, void *arg)
{
    sc_softc_t *sc;
    struct tty *cur_tty;
    int c; 
    size_t len;
    u_char *cp;

    sc = (sc_softc_t *)arg;
    /* assert(thiskbd == sc->kbd) */

    switch (event) {
    case KBDIO_KEYINPUT:
	break;
    case KBDIO_UNLOADING:
	sc->kbd = NULL;
	sc->keyboard = -1;
	kbd_release(thiskbd, (void *)&sc->keyboard);
	return 0;
    default:
	return EINVAL;
    }

    /* 
     * Loop while there is still input to get from the keyboard.
     * I don't think this is nessesary, and it doesn't fix
     * the Xaccel-2.1 keyboard hang, but it can't hurt.		XXX
     */
    while ((c = scgetc(sc, SCGETC_NONBLOCK)) != NOKEY) {

	cur_tty = VIRTUAL_TTY(sc, sc->cur_scp->index);
	/* XXX */
	if (!(cur_tty->t_state & TS_ISOPEN))
	    if (!((cur_tty = CONSOLE_TTY)->t_state & TS_ISOPEN))
		continue;

	switch (KEYFLAGS(c)) {
	case 0x0000: /* normal key */
	    (*linesw[cur_tty->t_line].l_rint)(KEYCHAR(c), cur_tty);
	    break;
	case FKEY:  /* function key, return string */
	    cp = kbd_get_fkeystr(thiskbd, KEYCHAR(c), &len);
	    if (cp != NULL) {
	    	while (len-- >  0)
		    (*linesw[cur_tty->t_line].l_rint)(*cp++, cur_tty);
	    }
	    break;
	case MKEY:  /* meta is active, prepend ESC */
	    (*linesw[cur_tty->t_line].l_rint)(0x1b, cur_tty);
	    (*linesw[cur_tty->t_line].l_rint)(KEYCHAR(c), cur_tty);
	    break;
	case BKEY:  /* backtab fixed sequence (esc [ Z) */
	    (*linesw[cur_tty->t_line].l_rint)(0x1b, cur_tty);
	    (*linesw[cur_tty->t_line].l_rint)('[', cur_tty);
	    (*linesw[cur_tty->t_line].l_rint)('Z', cur_tty);
	    break;
	}
    }

#ifndef SC_NO_CUTPASTE
    if (sc->cur_scp->status & MOUSE_VISIBLE) {
	sc_remove_mouse_image(sc->cur_scp);
	sc->cur_scp->status &= ~MOUSE_VISIBLE;
    }
#endif /* SC_NO_CUTPASTE */

    return 0;
}

static int
scparam(struct tty *tp, struct termios *t)
{
    tp->t_ispeed = t->c_ispeed;
    tp->t_ospeed = t->c_ospeed;
    tp->t_cflag = t->c_cflag;
    return 0;
}

int
scioctl(dev_t dev, u_long cmd, caddr_t data, int flag, struct proc *p)
{
    int error;
    int i;
    struct tty *tp;
    sc_softc_t *sc;
    scr_stat *scp;
    int s;

    tp = dev->si_tty_tty;
    if (!tp)
	return ENXIO;

    /* If there is a user_ioctl function call that first */
    if (sc_user_ioctl) {
	error = (*sc_user_ioctl)(dev, cmd, data, flag, p);
	if (error != ENOIOCTL)
	    return error;
    }

    error = sc_vid_ioctl(tp, cmd, data, flag, p);
    if (error != ENOIOCTL)
	return error;

#ifndef SC_NO_HISTORY
    error = sc_hist_ioctl(tp, cmd, data, flag, p);
    if (error != ENOIOCTL)
	return error;
#endif

#ifndef SC_NO_SYSMOUSE
    error = sc_mouse_ioctl(tp, cmd, data, flag, p);
    if (error != ENOIOCTL)
	return error;
    if (SC_VTY(dev) == SC_MOUSE) {
	error = (*linesw[tp->t_line].l_ioctl)(tp, cmd, data, flag, p);
	if (error != ENOIOCTL)
	    return error;
	error = ttioctl(tp, cmd, data, flag);
	if (error != ENOIOCTL)
	    return error;
	return ENOTTY;
    }
#endif

    scp = sc_get_scr_stat(tp->t_dev);
    /* assert(scp != NULL) */
    /* scp is sc_console, if SC_VTY(dev) == SC_CONSOLECTL. */
    sc = scp->sc;

    switch (cmd) {  		/* process console hardware related ioctl's */

    case GIO_ATTR:      	/* get current attributes */
	*(int*)data = (scp->term.cur_attr >> 8) & 0xFF;
	return 0;

    case GIO_COLOR:     	/* is this a color console ? */
	*(int *)data = (sc->adp->va_flags & V_ADP_COLOR) ? 1 : 0;
	return 0;

    case CONS_BLANKTIME:    	/* set screen saver timeout (0 = no saver) */
	if (*(int *)data < 0 || *(int *)data > MAX_BLANKTIME)
            return EINVAL;
	s = spltty();
	scrn_blank_time = *(int *)data;
	run_scrn_saver = (scrn_blank_time != 0);
	splx(s);
	return 0;

    case CONS_CURSORTYPE:   	/* set cursor type blink/noblink */
	if (!ISGRAPHSC(sc->cur_scp))
	    remove_cursor_image(sc->cur_scp);
	if ((*(int*)data) & 0x01)
	    sc->flags |= SC_BLINK_CURSOR;
	else
	    sc->flags &= ~SC_BLINK_CURSOR;
	if ((*(int*)data) & 0x02) {
	    sc->flags |= SC_CHAR_CURSOR;
	} else
	    sc->flags &= ~SC_CHAR_CURSOR;
	/* 
	 * The cursor shape is global property; all virtual consoles
	 * are affected. Update the cursor in the current console...
	 */
	if (!ISGRAPHSC(sc->cur_scp)) {
	    s = spltty();
	    sc_set_cursor_image(sc->cur_scp);
	    draw_cursor_image(sc->cur_scp);
	    splx(s);
	}
	return 0;

    case CONS_BELLTYPE: 	/* set bell type sound/visual */
	if ((*(int *)data) & 0x01)
	    sc->flags |= SC_VISUAL_BELL;
	else
	    sc->flags &= ~SC_VISUAL_BELL;
	if ((*(int *)data) & 0x02)
	    sc->flags |= SC_QUIET_BELL;
	else
	    sc->flags &= ~SC_QUIET_BELL;
	return 0;

    case CONS_GETINFO:  	/* get current (virtual) console info */
    {
	vid_info_t *ptr = (vid_info_t*)data;
	if (ptr->size == sizeof(struct vid_info)) {
	    ptr->m_num = sc->cur_scp->index;
	    ptr->mv_col = scp->xpos;
	    ptr->mv_row = scp->ypos;
	    ptr->mv_csz = scp->xsize;
	    ptr->mv_rsz = scp->ysize;
	    ptr->mv_norm.fore = (scp->term.std_color & 0x0f00)>>8;
	    ptr->mv_norm.back = (scp->term.std_color & 0xf000)>>12;
	    ptr->mv_rev.fore = (scp->term.rev_color & 0x0f00)>>8;
	    ptr->mv_rev.back = (scp->term.rev_color & 0xf000)>>12;
	    ptr->mv_grfc.fore = 0;      /* not supported */
	    ptr->mv_grfc.back = 0;      /* not supported */
	    ptr->mv_ovscan = scp->border;
	    if (scp == sc->cur_scp)
		save_kbd_state(scp);
	    ptr->mk_keylock = scp->status & LOCK_MASK;
	    return 0;
	}
	return EINVAL;
    }

    case CONS_GETVERS:  	/* get version number */
	*(int*)data = 0x200;    /* version 2.0 */
	return 0;

    case CONS_IDLE:		/* see if the screen has been idle */
	/*
	 * When the screen is in the GRAPHICS_MODE or UNKNOWN_MODE,
	 * the user process may have been writing something on the
	 * screen and syscons is not aware of it. Declare the screen
	 * is NOT idle if it is in one of these modes. But there is
	 * an exception to it; if a screen saver is running in the 
	 * graphics mode in the current screen, we should say that the
	 * screen has been idle.
	 */
	*(int *)data = (sc->flags & SC_SCRN_IDLE)
		       && (!ISGRAPHSC(sc->cur_scp)
			   || (sc->cur_scp->status & SAVER_RUNNING));
	return 0;

    case CONS_SAVERMODE:	/* set saver mode */
	switch(*(int *)data) {
	case CONS_USR_SAVER:
	    /* if a LKM screen saver is running, stop it first. */
	    scsplash_stick(FALSE);
	    saver_mode = *(int *)data;
	    s = spltty();
#if NSPLASH > 0
	    if ((error = wait_scrn_saver_stop(NULL))) {
		splx(s);
		return error;
	    }
#endif /* NSPLASH */
	    run_scrn_saver = TRUE;
	    scp->status |= SAVER_RUNNING;
	    scsplash_stick(TRUE);
	    splx(s);
	    break;
	case CONS_LKM_SAVER:
	    s = spltty();
	    if ((saver_mode == CONS_USR_SAVER) && (scp->status & SAVER_RUNNING))
		scp->status &= ~SAVER_RUNNING;
	    saver_mode = *(int *)data;
	    splx(s);
	    break;
	default:
	    return EINVAL;
	}
	return 0;

    case CONS_SAVERSTART:	/* immediately start/stop the screen saver */
	/*
	 * Note that this ioctl does not guarantee the screen saver 
	 * actually starts or stops. It merely attempts to do so...
	 */
	s = spltty();
	run_scrn_saver = (*(int *)data != 0);
	if (run_scrn_saver)
	    sc->scrn_time_stamp -= scrn_blank_time;
	splx(s);
	return 0;

    case VT_SETMODE:    	/* set screen switcher mode */
    {
	struct vt_mode *mode;

	mode = (struct vt_mode *)data;
	DPRINTF(5, ("sc%d: VT_SETMODE ", sc->unit));
	if (scp->smode.mode == VT_PROCESS) {
    	    if (scp->proc == pfind(scp->pid) && scp->proc != p) {
		DPRINTF(5, ("error EPERM\n"));
		return EPERM;
	    }
	}
	s = spltty();
	if (mode->mode == VT_AUTO) {
	    scp->smode.mode = VT_AUTO;
	    scp->proc = NULL;
	    scp->pid = 0;
	    DPRINTF(5, ("VT_AUTO, "));
	    if ((scp == sc->cur_scp) && (sc->unit == sc_console_unit))
		cons_unavail = FALSE;
	    /* were we in the middle of the vty switching process? */
	    if (scp->status & SWITCH_WAIT_REL) {
		/* assert(scp == scp->sc->cur_scp) */
		DPRINTF(5, ("reset WAIT_REL, "));
		scp->status &= ~SWITCH_WAIT_REL;
		s = do_switch_scr(sc, s);
	    }
	    if (scp->status & SWITCH_WAIT_ACQ) {
		/* assert(scp == scp->sc->cur_scp) */
		DPRINTF(5, ("reset WAIT_ACQ, "));
		scp->status &= ~SWITCH_WAIT_ACQ;
		sc->switch_in_progress = 0;
	    }
	} else {
	    if (!ISSIGVALID(mode->relsig) || !ISSIGVALID(mode->acqsig)
		|| !ISSIGVALID(mode->frsig)) {
		splx(s);
		DPRINTF(5, ("error EINVAL\n"));
		return EINVAL;
	    }
	    DPRINTF(5, ("VT_PROCESS %d, ", p->p_pid));
	    bcopy(data, &scp->smode, sizeof(struct vt_mode));
	    scp->proc = p;
	    scp->pid = scp->proc->p_pid;
	    if ((scp == sc->cur_scp) && (sc->unit == sc_console_unit))
		cons_unavail = TRUE;
	}
	splx(s);
	DPRINTF(5, ("\n"));
	return 0;
    }

    case VT_GETMODE:    	/* get screen switcher mode */
	bcopy(&scp->smode, data, sizeof(struct vt_mode));
	return 0;

    case VT_RELDISP:    	/* screen switcher ioctl */
	s = spltty();
	/*
	 * This must be the current vty which is in the VT_PROCESS
	 * switching mode...
	 */
	if ((scp != sc->cur_scp) || (scp->smode.mode != VT_PROCESS)) {
	    splx(s);
	    return EINVAL;
	}
	/* ...and this process is controlling it. */
	if (scp->proc != p) {
	    splx(s);
	    return EPERM;
	}
	error = EINVAL;
	switch(*(int *)data) {
	case VT_FALSE:  	/* user refuses to release screen, abort */
	    if ((scp == sc->old_scp) && (scp->status & SWITCH_WAIT_REL)) {
		sc->old_scp->status &= ~SWITCH_WAIT_REL;
		sc->switch_in_progress = 0;
		DPRINTF(5, ("sc%d: VT_FALSE\n", sc->unit));
		error = 0;
	    }
	    break;

	case VT_TRUE:   	/* user has released screen, go on */
	    if ((scp == sc->old_scp) && (scp->status & SWITCH_WAIT_REL)) {
		scp->status &= ~SWITCH_WAIT_REL;
		s = do_switch_scr(sc, s);
		DPRINTF(5, ("sc%d: VT_TRUE\n", sc->unit));
		error = 0;
	    }
	    break;

	case VT_ACKACQ: 	/* acquire acknowledged, switch completed */
	    if ((scp == sc->new_scp) && (scp->status & SWITCH_WAIT_ACQ)) {
		scp->status &= ~SWITCH_WAIT_ACQ;
		sc->switch_in_progress = 0;
		DPRINTF(5, ("sc%d: VT_ACKACQ\n", sc->unit));
		error = 0;
	    }
	    break;

	default:
	    break;
	}
	splx(s);
	return error;

    case VT_OPENQRY:    	/* return free virtual console */
	for (i = sc->first_vty; i < sc->first_vty + sc->vtys; i++) {
	    tp = VIRTUAL_TTY(sc, i);
	    if (!(tp->t_state & TS_ISOPEN)) {
		*(int *)data = i + 1;
		return 0;
	    }
	}
	return EINVAL;

    case VT_ACTIVATE:   	/* switch to screen *data */
	s = spltty();
	sc_clean_up(sc->cur_scp);
	splx(s);
	return switch_scr(sc, *(int *)data - 1);

    case VT_WAITACTIVE: 	/* wait for switch to occur */
	if ((*(int *)data >= sc->first_vty + sc->vtys)
		|| (*(int *)data < sc->first_vty))
	    return EINVAL;
	s = spltty();
	error = sc_clean_up(sc->cur_scp);
	splx(s);
	if (error)
	    return error;
	if (*(int *)data != 0)
	    scp = sc->console[*(int *)data - 1 - sc->first_vty];
	if (scp == scp->sc->cur_scp)
	    return 0;
	while ((error=tsleep((caddr_t)&scp->smode, PZERO|PCATCH,
			     "waitvt", 0)) == ERESTART) ;
	return error;

    case VT_GETACTIVE:		/* get active vty # */
	*(int *)data = sc->cur_scp->index + 1;
	return 0;

    case VT_GETINDEX:		/* get this vty # */
	*(int *)data = scp->index + 1;
	return 0;

    case KDENABIO:      	/* allow io operations */
	error = suser(p);
	if (error != 0)
	    return error;
	if (securelevel > 0)
	    return EPERM;
#ifdef __i386__
	p->p_md.md_regs->tf_eflags |= PSL_IOPL;
#endif
	return 0;

    case KDDISABIO:     	/* disallow io operations (default) */
#ifdef __i386__
	p->p_md.md_regs->tf_eflags &= ~PSL_IOPL;
#endif
	return 0;

    case KDSKBSTATE:    	/* set keyboard state (locks) */
	if (*(int *)data & ~LOCK_MASK)
	    return EINVAL;
	scp->status &= ~LOCK_MASK;
	scp->status |= *(int *)data;
	if (scp == sc->cur_scp)
	    update_kbd_state(scp, scp->status, LOCK_MASK);
	return 0;

    case KDGKBSTATE:    	/* get keyboard state (locks) */
	if (scp == sc->cur_scp)
	    save_kbd_state(scp);
	*(int *)data = scp->status & LOCK_MASK;
	return 0;

    case KDSETREPEAT:      	/* set keyboard repeat & delay rates (new) */
	error = kbd_ioctl(sc->kbd, cmd, data);
	if (error == ENOIOCTL)
	    error = ENODEV;
	return error;

    case KDSETRAD:      	/* set keyboard repeat & delay rates (old) */
	if (*(int *)data & ~0x7f)
	    return EINVAL;
	error = kbd_ioctl(sc->kbd, cmd, data);
	if (error == ENOIOCTL)
	    error = ENODEV;
	return error;

    case KDSKBMODE:     	/* set keyboard mode */
	switch (*(int *)data) {
	case K_XLATE:   	/* switch to XLT ascii mode */
	case K_RAW: 		/* switch to RAW scancode mode */
	case K_CODE: 		/* switch to CODE mode */
	    scp->kbd_mode = *(int *)data;
	    if (scp == sc->cur_scp)
		kbd_ioctl(sc->kbd, cmd, data);
	    return 0;
	default:
	    return EINVAL;
	}
	/* NOT REACHED */

    case KDGKBMODE:     	/* get keyboard mode */
	*(int *)data = scp->kbd_mode;
	return 0;

    case KDGKBINFO:
	error = kbd_ioctl(sc->kbd, cmd, data);
	if (error == ENOIOCTL)
	    error = ENODEV;
	return error;

    case KDMKTONE:      	/* sound the bell */
	if (*(int*)data)
	    do_bell(scp, (*(int*)data)&0xffff,
		    (((*(int*)data)>>16)&0xffff)*hz/1000);
	else
	    do_bell(scp, scp->bell_pitch, scp->bell_duration);
	return 0;

    case KIOCSOUND:     	/* make tone (*data) hz */
	if (scp == sc->cur_scp) {
	    if (*(int *)data)
		return sc_tone(*(int *)data);
	    else
		return sc_tone(0);
	}
	return 0;

    case KDGKBTYPE:     	/* get keyboard type */
	error = kbd_ioctl(sc->kbd, cmd, data);
	if (error == ENOIOCTL) {
	    /* always return something? XXX */
	    *(int *)data = 0;
	}
	return 0;

    case KDSETLED:      	/* set keyboard LED status */
	if (*(int *)data & ~LED_MASK)	/* FIXME: LOCK_MASK? */
	    return EINVAL;
	scp->status &= ~LED_MASK;
	scp->status |= *(int *)data;
	if (scp == sc->cur_scp)
	    update_kbd_leds(scp, scp->status);
	return 0;

    case KDGETLED:      	/* get keyboard LED status */
	if (scp == sc->cur_scp)
	    save_kbd_state(scp);
	*(int *)data = scp->status & LED_MASK;
	return 0;

    case CONS_SETKBD: 		/* set the new keyboard */
	{
	    keyboard_t *newkbd;

	    s = spltty();
	    newkbd = kbd_get_keyboard(*(int *)data);
	    if (newkbd == NULL) {
		splx(s);
		return EINVAL;
	    }
	    error = 0;
	    if (sc->kbd != newkbd) {
		i = kbd_allocate(newkbd->kb_name, newkbd->kb_unit,
				 (void *)&sc->keyboard, sckbdevent, sc);
		/* i == newkbd->kb_index */
		if (i >= 0) {
		    if (sc->kbd != NULL) {
			save_kbd_state(sc->cur_scp);
			kbd_release(sc->kbd, (void *)&sc->keyboard);
		    }
		    sc->kbd = kbd_get_keyboard(i); /* sc->kbd == newkbd */
		    sc->keyboard = i;
		    kbd_ioctl(sc->kbd, KDSKBMODE,
			      (caddr_t)&sc->cur_scp->kbd_mode);
		    update_kbd_state(sc->cur_scp, sc->cur_scp->status,
				     LOCK_MASK);
		} else {
		    error = EPERM;	/* XXX */
		}
	    }
	    splx(s);
	    return error;
	}

    case CONS_RELKBD: 		/* release the current keyboard */
	s = spltty();
	error = 0;
	if (sc->kbd != NULL) {
	    save_kbd_state(sc->cur_scp);
	    error = kbd_release(sc->kbd, (void *)&sc->keyboard);
	    if (error == 0) {
		sc->kbd = NULL;
		sc->keyboard = -1;
	    }
	}
	splx(s);
	return error;

    case GIO_SCRNMAP:   	/* get output translation table */
	bcopy(&sc->scr_map, data, sizeof(sc->scr_map));
	return 0;

    case PIO_SCRNMAP:   	/* set output translation table */
	bcopy(data, &sc->scr_map, sizeof(sc->scr_map));
	for (i=0; i<sizeof(sc->scr_map); i++) {
	    sc->scr_rmap[sc->scr_map[i]] = i;
	}
	return 0;

    case GIO_KEYMAP:		/* get keyboard translation table */
    case PIO_KEYMAP:		/* set keyboard translation table */
    case GIO_DEADKEYMAP:	/* get accent key translation table */
    case PIO_DEADKEYMAP:	/* set accent key translation table */
    case GETFKEY:		/* get function key string */
    case SETFKEY:		/* set function key string */
	error = kbd_ioctl(sc->kbd, cmd, data);
	if (error == ENOIOCTL)
	    error = ENODEV;
	return error;

#ifndef SC_NO_FONT_LOADING

    case PIO_FONT8x8:   	/* set 8x8 dot font */
	if (!ISFONTAVAIL(sc->adp->va_flags))
	    return ENXIO;
	bcopy(data, sc->font_8, 8*256);
	sc->fonts_loaded |= FONT_8;
	/*
	 * FONT KLUDGE
	 * Always use the font page #0. XXX
	 * Don't load if the current font size is not 8x8.
	 */
	if (ISTEXTSC(sc->cur_scp) && (sc->cur_scp->font_size < 14))
	    copy_font(sc->cur_scp, LOAD, 8, sc->font_8);
	return 0;

    case GIO_FONT8x8:   	/* get 8x8 dot font */
	if (!ISFONTAVAIL(sc->adp->va_flags))
	    return ENXIO;
	if (sc->fonts_loaded & FONT_8) {
	    bcopy(sc->font_8, data, 8*256);
	    return 0;
	}
	else
	    return ENXIO;

    case PIO_FONT8x14:  	/* set 8x14 dot font */
	if (!ISFONTAVAIL(sc->adp->va_flags))
	    return ENXIO;
	bcopy(data, sc->font_14, 14*256);
	sc->fonts_loaded |= FONT_14;
	/*
	 * FONT KLUDGE
	 * Always use the font page #0. XXX
	 * Don't load if the current font size is not 8x14.
	 */
	if (ISTEXTSC(sc->cur_scp)
	    && (sc->cur_scp->font_size >= 14)
	    && (sc->cur_scp->font_size < 16))
	    copy_font(sc->cur_scp, LOAD, 14, sc->font_14);
	return 0;

    case GIO_FONT8x14:  	/* get 8x14 dot font */
	if (!ISFONTAVAIL(sc->adp->va_flags))
	    return ENXIO;
	if (sc->fonts_loaded & FONT_14) {
	    bcopy(sc->font_14, data, 14*256);
	    return 0;
	}
	else
	    return ENXIO;

    case PIO_FONT8x16:  	/* set 8x16 dot font */
	if (!ISFONTAVAIL(sc->adp->va_flags))
	    return ENXIO;
	bcopy(data, sc->font_16, 16*256);
	sc->fonts_loaded |= FONT_16;
	/*
	 * FONT KLUDGE
	 * Always use the font page #0. XXX
	 * Don't load if the current font size is not 8x16.
	 */
	if (ISTEXTSC(sc->cur_scp) && (sc->cur_scp->font_size >= 16))
	    copy_font(sc->cur_scp, LOAD, 16, sc->font_16);
	return 0;

    case GIO_FONT8x16:  	/* get 8x16 dot font */
	if (!ISFONTAVAIL(sc->adp->va_flags))
	    return ENXIO;
	if (sc->fonts_loaded & FONT_16) {
	    bcopy(sc->font_16, data, 16*256);
	    return 0;
	}
	else
	    return ENXIO;

#endif /* SC_NO_FONT_LOADING */

    default:
	break;
    }

    error = (*linesw[tp->t_line].l_ioctl)(tp, cmd, data, flag, p);
    if (error != ENOIOCTL)
	return(error);
    error = ttioctl(tp, cmd, data, flag);
    if (error != ENOIOCTL)
	return(error);
    return(ENOTTY);
}

static void
scstart(struct tty *tp)
{
    struct clist *rbp;
    int s, len;
    u_char buf[PCBURST];
    scr_stat *scp = sc_get_scr_stat(tp->t_dev);

    if (scp->status & SLKED || scp->sc->blink_in_progress)
	return;
    s = spltty();
    if (!(tp->t_state & (TS_TIMEOUT | TS_BUSY | TS_TTSTOP))) {
	tp->t_state |= TS_BUSY;
	rbp = &tp->t_outq;
	while (rbp->c_cc) {
	    len = q_to_b(rbp, buf, PCBURST);
	    splx(s);
	    ansi_put(scp, buf, len);
	    s = spltty();
	}
	tp->t_state &= ~TS_BUSY;
	ttwwakeup(tp);
    }
    splx(s);
}

static void
scmousestart(struct tty *tp)
{
    struct clist *rbp;
    int s;
    u_char buf[PCBURST];

    s = spltty();
    if (!(tp->t_state & (TS_TIMEOUT | TS_BUSY | TS_TTSTOP))) {
	tp->t_state |= TS_BUSY;
	rbp = &tp->t_outq;
	while (rbp->c_cc) {
	    q_to_b(rbp, buf, PCBURST);
	}
	tp->t_state &= ~TS_BUSY;
	ttwwakeup(tp);
    }
    splx(s);
}

static void
sccnprobe(struct consdev *cp)
{
#if __i386__
    int unit;
    int flags;

    cp->cn_pri = sc_get_cons_priority(&unit, &flags);

    /* a video card is always required */
    if (!scvidprobe(unit, flags, TRUE))
	cp->cn_pri = CN_DEAD;

    /* syscons will become console even when there is no keyboard */
    sckbdprobe(unit, flags, TRUE);

    if (cp->cn_pri == CN_DEAD)
	return;

    /* initialize required fields */
    cp->cn_dev = makedev(CDEV_MAJOR, SC_CONSOLECTL);
#endif /* __i386__ */

#if __alpha__
    /*
     * alpha use sccnattach() rather than cnprobe()/cninit()/cnterm()
     * interface to install the console.  Always return CN_DEAD from
     * here.
     */
    cp->cn_pri = CN_DEAD;
#endif /* __alpha__ */
}

static void
sccninit(struct consdev *cp)
{
#if __i386__
    int unit;
    int flags;

    sc_get_cons_priority(&unit, &flags);
    scinit(unit, flags | SC_KERNEL_CONSOLE);
    sc_console_unit = unit;
    sc_console = sc_get_softc(unit, SC_KERNEL_CONSOLE)->console[0];
#endif /* __i386__ */

#if __alpha__
    /* SHOULDN'T REACH HERE */
#endif /* __alpha__ */
}

static void
sccnterm(struct consdev *cp)
{
    /* we are not the kernel console any more, release everything */

    if (sc_console_unit < 0)
	return;			/* shouldn't happen */

#if __i386__
#if 0 /* XXX */
    sc_clear_screen(sc_console);
    sccnupdate(sc_console);
#endif
    scterm(sc_console_unit, SC_KERNEL_CONSOLE);
    sc_console_unit = -1;
    sc_console = NULL;
#endif /* __i386__ */

#if __alpha__
    /* do nothing XXX */
#endif /* __alpha__ */
}

#ifdef __alpha__

extern struct consdev *cn_tab;

void
sccnattach(void)
{
    static struct consdev consdev;
    int unit;
    int flags;

    bcopy(&sc_consdev, &consdev, sizeof(sc_consdev));
    consdev.cn_pri = sc_get_cons_priority(&unit, &flags);

    /* a video card is always required */
    if (!scvidprobe(unit, flags, TRUE))
	consdev.cn_pri = CN_DEAD;

    /* alpha doesn't allow the console being without a keyboard... Why? */
    if (!sckbdprobe(unit, flags, TRUE))
	consdev.cn_pri = CN_DEAD;

    if (consdev.cn_pri == CN_DEAD)
	return;

    scinit(unit, flags | SC_KERNEL_CONSOLE);
    sc_console_unit = unit;
    sc_console = sc_get_softc(unit, SC_KERNEL_CONSOLE)->console[0];
    consdev.cn_dev = makedev(CDEV_MAJOR, 0);
    cn_tab = &consdev;
}

#endif /* __alpha__ */

static void
sccnputc(dev_t dev, int c)
{
    u_char buf[1];
    scr_stat *scp = sc_console;
    term_stat save = scp->term;
    struct tty *tp;
    int s;

    /* assert(sc_console != NULL) */

#ifndef SC_NO_HISTORY
    if (scp == scp->sc->cur_scp && scp->status & SLKED) {
	scp->status &= ~SLKED;
	update_kbd_state(scp, scp->status, SLKED);
	if (scp->status & BUFFER_SAVED) {
	    if (!sc_hist_restore(scp))
		sc_remove_cutmarking(scp);
	    scp->status &= ~BUFFER_SAVED;
	    scp->status |= CURSOR_ENABLED;
	    draw_cursor_image(scp);
	}
	tp = VIRTUAL_TTY(scp->sc, scp->index);
	if (tp->t_state & TS_ISOPEN)
	    scstart(tp);
    }
#endif /* !SC_NO_HISTORY */

    scp->term = kernel_console;
    current_default = &kernel_default;
    buf[0] = c;
    ansi_put(scp, buf, 1);
    kernel_console = scp->term;
    current_default = &user_default;
    scp->term = save;

    s = spltty();	/* block sckbdevent and scrn_timer */
    sccnupdate(scp);
    splx(s);
}

static int
sccngetc(dev_t dev)
{
    return sccngetch(0);
}

static int
sccncheckc(dev_t dev)
{
    return sccngetch(SCGETC_NONBLOCK);
}

static int
sccngetch(int flags)
{
    static struct fkeytab fkey;
    static int fkeycp;
    scr_stat *scp;
    u_char *p;
    int cur_mode;
    int s = spltty();	/* block sckbdevent and scrn_timer while we poll */
    int c;

    /* assert(sc_console != NULL) */

    /* 
     * Stop the screen saver and update the screen if necessary.
     * What if we have been running in the screen saver code... XXX
     */
    sc_touch_scrn_saver();
    scp = sc_console->sc->cur_scp;	/* XXX */
    sccnupdate(scp);

    if (fkeycp < fkey.len) {
	splx(s);
	return fkey.str[fkeycp++];
    }

    if (scp->sc->kbd == NULL) {
	splx(s);
	return -1;
    }

    /* 
     * Make sure the keyboard is accessible even when the kbd device
     * driver is disabled.
     */
    kbd_enable(scp->sc->kbd);

    /* we shall always use the keyboard in the XLATE mode here */
    cur_mode = scp->kbd_mode;
    scp->kbd_mode = K_XLATE;
    kbd_ioctl(scp->sc->kbd, KDSKBMODE, (caddr_t)&scp->kbd_mode);

    kbd_poll(scp->sc->kbd, TRUE);
    c = scgetc(scp->sc, SCGETC_CN | flags);
    kbd_poll(scp->sc->kbd, FALSE);

    scp->kbd_mode = cur_mode;
    kbd_ioctl(scp->sc->kbd, KDSKBMODE, (caddr_t)&scp->kbd_mode);
    kbd_disable(scp->sc->kbd);
    splx(s);

    switch (KEYFLAGS(c)) {
    case 0:	/* normal char */
	return KEYCHAR(c);
    case FKEY:	/* function key */
	p = kbd_get_fkeystr(scp->sc->kbd, KEYCHAR(c), (size_t *)&fkeycp);
	fkey.len = fkeycp;
	if ((p != NULL) && (fkey.len > 0)) {
	    bcopy(p, fkey.str, fkey.len);
	    fkeycp = 1;
	    return fkey.str[0];
	}
	return c;	/* XXX */
    case NOKEY:
    case ERRKEY:
    default:
	return -1;
    }
    /* NOT REACHED */
}

static void
sccnupdate(scr_stat *scp)
{
    /* this is a cut-down version of scrn_timer()... */

    if (scp->sc->font_loading_in_progress || scp->sc->videoio_in_progress)
	return;

    if (debugger || panicstr || shutdown_in_progress) {
	sc_touch_scrn_saver();
    } else if (scp != scp->sc->cur_scp) {
	return;
    }

    if (!run_scrn_saver)
	scp->sc->flags &= ~SC_SCRN_IDLE;
#if NSPLASH > 0
    if ((saver_mode != CONS_LKM_SAVER) || !(scp->sc->flags & SC_SCRN_IDLE))
	if (scp->sc->flags & SC_SCRN_BLANKED)
            stop_scrn_saver(scp->sc, current_saver);
#endif /* NSPLASH */

    if (scp != scp->sc->cur_scp || scp->sc->blink_in_progress
	|| scp->sc->switch_in_progress)
	return;
    /*
     * FIXME: unlike scrn_timer(), we call scrn_update() from here even
     * when write_in_progress is non-zero.  XXX
     */

    if (!ISGRAPHSC(scp) && !(scp->status & SAVER_RUNNING))
	scrn_update(scp, TRUE);
}

scr_stat
*sc_get_scr_stat(dev_t dev)
{
    sc_softc_t *sc;
    int vty = SC_VTY(dev);
    int unit;

    if (vty < 0)
	return NULL;
    if (vty == SC_CONSOLECTL)
	return sc_console;

    unit = scdevtounit(dev);
    sc = sc_get_softc(unit, (sc_console_unit == unit) ? SC_KERNEL_CONSOLE : 0);
    if (sc == NULL)
	return NULL;
    if ((sc->first_vty <= vty) && (vty < sc->first_vty + sc->vtys))
	return sc->console[vty - sc->first_vty];
    return NULL;
}

static void
scrn_timer(void *arg)
{
    static int kbd_interval = 0;
    struct timeval tv;
    sc_softc_t *sc;
    scr_stat *scp;
    int again;
    int s;

    again = (arg != NULL);
    if (arg != NULL)
	sc = (sc_softc_t *)arg;
    else if (sc_console != NULL)
	sc = sc_console->sc;
    else
	return;

    /* don't do anything when we are performing some I/O operations */
    if (sc->font_loading_in_progress || sc->videoio_in_progress) {
	if (again)
	    timeout(scrn_timer, sc, hz / 10);
	return;
    }
    s = spltty();

    if ((sc->kbd == NULL) && (sc->config & SC_AUTODETECT_KBD)) {
	/* try to allocate a keyboard automatically */
	if (++kbd_interval >= 25) {
	    sc->keyboard = kbd_allocate("*", -1, (void *)&sc->keyboard,
					sckbdevent, sc);
	    if (sc->keyboard >= 0) {
		sc->kbd = kbd_get_keyboard(sc->keyboard);
		kbd_ioctl(sc->kbd, KDSKBMODE,
			  (caddr_t)&sc->cur_scp->kbd_mode);
		update_kbd_state(sc->cur_scp, sc->cur_scp->status,
				 LOCK_MASK);
	    }
	    kbd_interval = 0;
	}
    }

    /* find the vty to update */
    scp = sc->cur_scp;

    /* should we stop the screen saver? */
    getmicrouptime(&tv);
    if (debugger || panicstr || shutdown_in_progress)
	sc_touch_scrn_saver();
    if (run_scrn_saver) {
	if (tv.tv_sec > sc->scrn_time_stamp + scrn_blank_time)
	    sc->flags |= SC_SCRN_IDLE;
	else
	    sc->flags &= ~SC_SCRN_IDLE;
    } else {
	sc->scrn_time_stamp = tv.tv_sec;
	sc->flags &= ~SC_SCRN_IDLE;
	if (scrn_blank_time > 0)
	    run_scrn_saver = TRUE;
    }
#if NSPLASH > 0
    if ((saver_mode != CONS_LKM_SAVER) || !(sc->flags & SC_SCRN_IDLE))
	if (sc->flags & SC_SCRN_BLANKED)
            stop_scrn_saver(sc, current_saver);
#endif /* NSPLASH */

    /* should we just return ? */
    if (sc->blink_in_progress || sc->switch_in_progress
	|| sc->write_in_progress) {
	if (again)
	    timeout(scrn_timer, sc, hz / 10);
	splx(s);
	return;
    }

    /* Update the screen */
    scp = sc->cur_scp;		/* cur_scp may have changed... */
    if (!ISGRAPHSC(scp) && !(scp->status & SAVER_RUNNING))
	scrn_update(scp, TRUE);

#if NSPLASH > 0
    /* should we activate the screen saver? */
    if ((saver_mode == CONS_LKM_SAVER) && (sc->flags & SC_SCRN_IDLE))
	if (!ISGRAPHSC(scp) || (sc->flags & SC_SCRN_BLANKED))
	    (*current_saver)(sc, TRUE);
#endif /* NSPLASH */

    if (again)
	timeout(scrn_timer, sc, hz / 25);
    splx(s);
}

static int
and_region(int *s1, int *e1, int s2, int e2)
{
    if (*e1 < s2 || e2 < *s1)
	return FALSE;
    *s1 = imax(*s1, s2);
    *e1 = imin(*e1, e2);
    return TRUE;
}

static void 
scrn_update(scr_stat *scp, int show_cursor)
{
    int start;
    int end;
    int s;
    int e;

    /* assert(scp == scp->sc->cur_scp) */

    ++scp->sc->videoio_in_progress;

#ifndef SC_NO_CUTPASTE
    /* remove the previous mouse pointer image if necessary */
    if ((scp->status & (MOUSE_VISIBLE | MOUSE_MOVED))
	== (MOUSE_VISIBLE | MOUSE_MOVED)) {
	/* FIXME: I don't like this... XXX */
	sc_remove_mouse_image(scp);
        if (scp->end >= scp->xsize*scp->ysize)
	    scp->end = scp->xsize*scp->ysize - 1;
    }
#endif /* !SC_NO_CUTPASTE */

#if 1
    /* debug: XXX */
    if (scp->end >= scp->xsize*scp->ysize) {
	printf("scrn_update(): scp->end %d > size_of_screen!!\n", scp->end);
	scp->end = scp->xsize*scp->ysize - 1;
    }
    if (scp->start < 0) {
	printf("scrn_update(): scp->start %d < 0\n", scp->start);
	scp->start = 0;
    }
#endif

    /* update screen image */
    if (scp->start <= scp->end)  {
	if (scp->mouse_cut_end >= 0) {
	    /* there is a marked region for cut & paste */
	    if (scp->mouse_cut_start <= scp->mouse_cut_end) {
		start = scp->mouse_cut_start;
		end = scp->mouse_cut_end;
	    } else {
		start = scp->mouse_cut_end;
		end = scp->mouse_cut_start - 1;
	    }
	    s = start;
	    e = end;
	    /* does the cut-mark region overlap with the update region? */
	    if (and_region(&s, &e, scp->start, scp->end)) {
		(*scp->rndr->draw)(scp, s, e - s + 1, TRUE);
		s = 0;
		e = start - 1;
		if (and_region(&s, &e, scp->start, scp->end))
		    (*scp->rndr->draw)(scp, s, e - s + 1, FALSE);
		s = end + 1;
		e = scp->xsize*scp->ysize - 1;
		if (and_region(&s, &e, scp->start, scp->end))
		    (*scp->rndr->draw)(scp, s, e - s + 1, FALSE);
	    } else {
		(*scp->rndr->draw)(scp, scp->start,
				   scp->end - scp->start + 1, FALSE);
	    }
	} else {
	    (*scp->rndr->draw)(scp, scp->start,
			       scp->end - scp->start + 1, FALSE);
	}
    }

    /* we are not to show the cursor and the mouse pointer... */
    if (!show_cursor) {
        scp->end = 0;
        scp->start = scp->xsize*scp->ysize - 1;
	--scp->sc->videoio_in_progress;
	return;
    }

    /* update cursor image */
    if (scp->status & CURSOR_ENABLED) {
        /* did cursor move since last time ? */
        if (scp->cursor_pos != scp->cursor_oldpos) {
            /* do we need to remove old cursor image ? */
            if (scp->cursor_oldpos < scp->start ||
                scp->cursor_oldpos > scp->end) {
                remove_cursor_image(scp);
            }
            scp->cursor_oldpos = scp->cursor_pos;
            draw_cursor_image(scp);
        }
        else {
            /* cursor didn't move, has it been overwritten ? */
            if (scp->cursor_pos >= scp->start && scp->cursor_pos <= scp->end) {
                draw_cursor_image(scp);
            } else {
                /* if its a blinking cursor, we may have to update it */
		if (scp->sc->flags & SC_BLINK_CURSOR)
                    (*scp->rndr->blink_cursor)(scp, scp->cursor_pos,
					       sc_inside_cutmark(scp,
							scp->cursor_pos));
            }
        }
    }

#ifndef SC_NO_CUTPASTE
    /* update "pseudo" mouse pointer image */
    if (scp->status & MOUSE_VISIBLE) {
        /* did mouse move since last time ? */
        if (scp->status & MOUSE_MOVED) {
            /* the previous pointer image has been removed, see above */
            scp->status &= ~MOUSE_MOVED;
            sc_draw_mouse_image(scp);
        } else {
            /* mouse didn't move, has it been overwritten ? */
            if (scp->mouse_pos + scp->xsize + 1 >= scp->start &&
                scp->mouse_pos <= scp->end) {
                sc_draw_mouse_image(scp);
            } else if (scp->cursor_pos == scp->mouse_pos ||
            	scp->cursor_pos == scp->mouse_pos + 1 ||
            	scp->cursor_pos == scp->mouse_pos + scp->xsize ||
            	scp->cursor_pos == scp->mouse_pos + scp->xsize + 1) {
                sc_draw_mouse_image(scp);
	    }
        }
    }
#endif /* SC_NO_CUTPASTE */

    scp->end = 0;
    scp->start = scp->xsize*scp->ysize - 1;

    --scp->sc->videoio_in_progress;
}

#if NSPLASH > 0
static int
scsplash_callback(int event, void *arg)
{
    sc_softc_t *sc;
    int error;

    sc = (sc_softc_t *)arg;

    switch (event) {
    case SPLASH_INIT:
	if (add_scrn_saver(scsplash_saver) == 0) {
	    sc->flags &= ~SC_SAVER_FAILED;
	    run_scrn_saver = TRUE;
	    if (cold && !(boothowto & (RB_VERBOSE | RB_CONFIG))) {
		scsplash_stick(TRUE);
		(*current_saver)(sc, TRUE);
	    }
	}
	return 0;

    case SPLASH_TERM:
	if (current_saver == scsplash_saver) {
	    scsplash_stick(FALSE);
	    error = remove_scrn_saver(scsplash_saver);
	    if (error)
		return error;
	}
	return 0;

    default:
	return EINVAL;
    }
}

static void
scsplash_saver(sc_softc_t *sc, int show)
{
    static int busy = FALSE;
    scr_stat *scp;

    if (busy)
	return;
    busy = TRUE;

    scp = sc->cur_scp;
    if (show) {
	if (!(sc->flags & SC_SAVER_FAILED)) {
	    if (!(sc->flags & SC_SCRN_BLANKED))
		set_scrn_saver_mode(scp, -1, NULL, 0);
	    switch (splash(sc->adp, TRUE)) {
	    case 0:		/* succeeded */
		break;
	    case EAGAIN:	/* try later */
		restore_scrn_saver_mode(scp, FALSE);
		sc_touch_scrn_saver();		/* XXX */
		break;
	    default:
		sc->flags |= SC_SAVER_FAILED;
		scsplash_stick(FALSE);
		restore_scrn_saver_mode(scp, TRUE);
		printf("scsplash_saver(): failed to put up the image\n");
		break;
	    }
	}
    } else if (!sticky_splash) {
	if ((sc->flags & SC_SCRN_BLANKED) && (splash(sc->adp, FALSE) == 0))
	    restore_scrn_saver_mode(scp, TRUE);
    }
    busy = FALSE;
}

static int
add_scrn_saver(void (*this_saver)(sc_softc_t *, int))
{
#if 0
    int error;

    if (current_saver != none_saver) {
	error = remove_scrn_saver(current_saver);
	if (error)
	    return error;
    }
#endif
    if (current_saver != none_saver)
	return EBUSY;

    run_scrn_saver = FALSE;
    saver_mode = CONS_LKM_SAVER;
    current_saver = this_saver;
    return 0;
}

static int
remove_scrn_saver(void (*this_saver)(sc_softc_t *, int))
{
    if (current_saver != this_saver)
	return EINVAL;

#if 0
    /*
     * In order to prevent `current_saver' from being called by
     * the timeout routine `scrn_timer()' while we manipulate 
     * the saver list, we shall set `current_saver' to `none_saver' 
     * before stopping the current saver, rather than blocking by `splXX()'.
     */
    current_saver = none_saver;
    if (scrn_blanked)
        stop_scrn_saver(this_saver);
#endif

    /* unblank all blanked screens */
    wait_scrn_saver_stop(NULL);
    if (scrn_blanked)
	return EBUSY;

    current_saver = none_saver;
    return 0;
}

static int
set_scrn_saver_mode(scr_stat *scp, int mode, u_char *pal, int border)
{
    int s;

    /* assert(scp == scp->sc->cur_scp) */
    s = spltty();
    if (!ISGRAPHSC(scp))
	remove_cursor_image(scp);
    scp->splash_save_mode = scp->mode;
    scp->splash_save_status = scp->status & (GRAPHICS_MODE | PIXEL_MODE);
    scp->status &= ~(GRAPHICS_MODE | PIXEL_MODE);
    scp->status |= (UNKNOWN_MODE | SAVER_RUNNING);
    scp->sc->flags |= SC_SCRN_BLANKED;
    ++scrn_blanked;
    splx(s);
    if (mode < 0)
	return 0;
    scp->mode = mode;
    if (set_mode(scp) == 0) {
	if (scp->sc->adp->va_info.vi_flags & V_INFO_GRAPHICS)
	    scp->status |= GRAPHICS_MODE;
#ifndef SC_NO_PALETTE_LOADING
	if (pal != NULL)
	    load_palette(scp->sc->adp, pal);
#endif
	set_border(scp, border);
	return 0;
    } else {
	s = spltty();
	scp->mode = scp->splash_save_mode;
	scp->status &= ~(UNKNOWN_MODE | SAVER_RUNNING);
	scp->status |= scp->splash_save_status;
	splx(s);
	return 1;
    }
}

static int
restore_scrn_saver_mode(scr_stat *scp, int changemode)
{
    int mode;
    int status;
    int s;

    /* assert(scp == scp->sc->cur_scp) */
    s = spltty();
    mode = scp->mode;
    status = scp->status;
    scp->mode = scp->splash_save_mode;
    scp->status &= ~(UNKNOWN_MODE | SAVER_RUNNING);
    scp->status |= scp->splash_save_status;
    scp->sc->flags &= ~SC_SCRN_BLANKED;
    if (!changemode) {
	if (!ISGRAPHSC(scp))
	    draw_cursor_image(scp);
	--scrn_blanked;
	splx(s);
	return 0;
    }
    if (set_mode(scp) == 0) {
#ifndef SC_NO_PALETTE_LOADING
	load_palette(scp->sc->adp, scp->sc->palette);
#endif
	--scrn_blanked;
	splx(s);
	return 0;
    } else {
	scp->mode = mode;
	scp->status = status;
	splx(s);
	return 1;
    }
}

static void
stop_scrn_saver(sc_softc_t *sc, void (*saver)(sc_softc_t *, int))
{
    (*saver)(sc, FALSE);
    run_scrn_saver = FALSE;
    /* the screen saver may have chosen not to stop after all... */
    if (sc->flags & SC_SCRN_BLANKED)
	return;

    mark_all(sc->cur_scp);
    if (sc->delayed_next_scr)
	switch_scr(sc, sc->delayed_next_scr - 1);
    wakeup((caddr_t)&scrn_blanked);
}

static int
wait_scrn_saver_stop(sc_softc_t *sc)
{
    int error = 0;

    while (scrn_blanked > 0) {
	run_scrn_saver = FALSE;
	if (sc && !(sc->flags & SC_SCRN_BLANKED)) {
	    error = 0;
	    break;
	}
	error = tsleep((caddr_t)&scrn_blanked, PZERO | PCATCH, "scrsav", 0);
	if ((error != 0) && (error != ERESTART))
	    break;
    }
    run_scrn_saver = FALSE;
    return error;
}
#endif /* NSPLASH */

void
sc_touch_scrn_saver(void)
{
    scsplash_stick(FALSE);
    run_scrn_saver = FALSE;
}

void
sc_clear_screen(scr_stat *scp)
{
    move_crsr(scp, 0, 0);
    scp->cursor_oldpos = scp->cursor_pos;
    sc_vtb_clear(&scp->vtb, scp->sc->scr_map[0x20], scp->term.cur_color);
    mark_all(scp);
    sc_remove_cutmarking(scp);
}

static int
switch_scr(sc_softc_t *sc, u_int next_scr)
{
    struct tty *tp;
    int s;

    DPRINTF(5, ("sc0: switch_scr() %d ", next_scr + 1));

    /* delay switch if the screen is blanked or being updated */
    if ((sc->flags & SC_SCRN_BLANKED) || sc->write_in_progress
	|| sc->blink_in_progress || sc->videoio_in_progress) {
	sc->delayed_next_scr = next_scr + 1;
	sc_touch_scrn_saver();
	DPRINTF(5, ("switch delayed\n"));
	return 0;
    }

    s = spltty();

    /* we are in the middle of the vty switching process... */
    if (sc->switch_in_progress
	&& (sc->cur_scp->smode.mode == VT_PROCESS)
	&& sc->cur_scp->proc) {
	if (sc->cur_scp->proc != pfind(sc->cur_scp->pid)) {
	    /* 
	     * The controlling process has died!!.  Do some clean up.
	     * NOTE:`cur_scp->proc' and `cur_scp->smode.mode' 
	     * are not reset here yet; they will be cleared later.
	     */
	    DPRINTF(5, ("cur_scp controlling process %d died, ",
	       sc->cur_scp->pid));
	    if (sc->cur_scp->status & SWITCH_WAIT_REL) {
		/*
		 * Force the previous switch to finish, but return now 
		 * with error.
		 */
		DPRINTF(5, ("reset WAIT_REL, "));
		sc->cur_scp->status &= ~SWITCH_WAIT_REL; 
		s = do_switch_scr(sc, s);
		splx(s);
		DPRINTF(5, ("finishing previous switch\n"));
		return EINVAL;
	    } else if (sc->cur_scp->status & SWITCH_WAIT_ACQ) {
		/* let's assume screen switch has been completed. */
		DPRINTF(5, ("reset WAIT_ACQ, "));
		sc->cur_scp->status &= ~SWITCH_WAIT_ACQ;
		sc->switch_in_progress = 0;
	    } else {
		/* 
	 	 * We are in between screen release and acquisition, and
		 * reached here via scgetc() or scrn_timer() which has 
		 * interrupted exchange_scr(). Don't do anything stupid.
		 */
		DPRINTF(5, ("waiting nothing, "));
	    }
	} else {
	    /*
	     * The controlling process is alive, but not responding... 
	     * It is either buggy or it may be just taking time.
	     * The following code is a gross kludge to cope with this
	     * problem for which there is no clean solution. XXX
	     */
	    if (sc->cur_scp->status & SWITCH_WAIT_REL) {
		switch (sc->switch_in_progress++) {
		case 1:
		    break;
		case 2:
		    DPRINTF(5, ("sending relsig again, "));
		    signal_vt_rel(sc->cur_scp);
		    break;
		case 3:
		    break;
		case 4:
		default:
		    /*
		     * Clear the flag and force the previous switch to finish,
		     * but return now with error.
		     */
		    DPRINTF(5, ("force reset WAIT_REL, "));
		    sc->cur_scp->status &= ~SWITCH_WAIT_REL; 
		    s = do_switch_scr(sc, s);
		    splx(s);
		    DPRINTF(5, ("force finishing previous switch\n"));
		    return EINVAL;
		}
	    } else if (sc->cur_scp->status & SWITCH_WAIT_ACQ) {
		switch (sc->switch_in_progress++) {
		case 1:
		    break;
		case 2:
		    DPRINTF(5, ("sending acqsig again, "));
		    signal_vt_acq(sc->cur_scp);
		    break;
		case 3:
		    break;
		case 4:
		default:
		     /* clear the flag and finish the previous switch */
		    DPRINTF(5, ("force reset WAIT_ACQ, "));
		    sc->cur_scp->status &= ~SWITCH_WAIT_ACQ;
		    sc->switch_in_progress = 0;
		    break;
		}
	    }
	}
    }

    /*
     * Return error if an invalid argument is given, or vty switch
     * is still in progress.
     */
    if ((next_scr < sc->first_vty) || (next_scr >= sc->first_vty + sc->vtys)
	|| sc->switch_in_progress) {
	splx(s);
	do_bell(sc->cur_scp, bios_value.bell_pitch, BELL_DURATION);
	DPRINTF(5, ("error 1\n"));
	return EINVAL;
    }

    /*
     * Don't allow switching away from the graphics mode vty
     * if the switch mode is VT_AUTO, unless the next vty is the same 
     * as the current or the current vty has been closed (but showing).
     */
    tp = VIRTUAL_TTY(sc, sc->cur_scp->index);
    if ((sc->cur_scp->index != next_scr)
	&& (tp->t_state & TS_ISOPEN)
	&& (sc->cur_scp->smode.mode == VT_AUTO)
	&& ISGRAPHSC(sc->cur_scp)) {
	splx(s);
	do_bell(sc->cur_scp, bios_value.bell_pitch, BELL_DURATION);
	DPRINTF(5, ("error, graphics mode\n"));
	return EINVAL;
    }

    /*
     * Is the wanted vty open? Don't allow switching to a closed vty.
     * Note that we always allow the user to switch to the kernel 
     * console even if it is closed.
     */
    if ((sc_console == NULL) || (next_scr != sc_console->index)) {
	tp = VIRTUAL_TTY(sc, next_scr);
	if (!(tp->t_state & TS_ISOPEN)) {
	    splx(s);
	    do_bell(sc->cur_scp, bios_value.bell_pitch, BELL_DURATION);
	    DPRINTF(5, ("error 2, requested vty isn't open!\n"));
	    return EINVAL;
	}
    }

    /* this is the start of vty switching process... */
    ++sc->switch_in_progress;
    sc->delayed_next_scr = 0;
    sc->old_scp = sc->cur_scp;
    sc->new_scp = sc->console[next_scr - sc->first_vty];
    if (sc->new_scp == sc->old_scp) {
	sc->switch_in_progress = 0;
	wakeup((caddr_t)&sc->new_scp->smode);
	splx(s);
	DPRINTF(5, ("switch done (new == old)\n"));
	return 0;
    }

    /* has controlling process died? */
    vt_proc_alive(sc->old_scp);
    vt_proc_alive(sc->new_scp);

    /* wait for the controlling process to release the screen, if necessary */
    if (signal_vt_rel(sc->old_scp)) {
	splx(s);
	return 0;
    }

    /* go set up the new vty screen */
    splx(s);
    exchange_scr(sc);
    s = spltty();

    /* wake up processes waiting for this vty */
    wakeup((caddr_t)&sc->cur_scp->smode);

    /* wait for the controlling process to acknowledge, if necessary */
    if (signal_vt_acq(sc->cur_scp)) {
	splx(s);
	return 0;
    }

    sc->switch_in_progress = 0;
    if (sc->unit == sc_console_unit)
	cons_unavail = FALSE;
    splx(s);
    DPRINTF(5, ("switch done\n"));

    return 0;
}

static int
do_switch_scr(sc_softc_t *sc, int s)
{
    vt_proc_alive(sc->new_scp);

    splx(s);
    exchange_scr(sc);
    s = spltty();
    /* sc->cur_scp == sc->new_scp */
    wakeup((caddr_t)&sc->cur_scp->smode);

    /* wait for the controlling process to acknowledge, if necessary */
    if (!signal_vt_acq(sc->cur_scp)) {
	sc->switch_in_progress = 0;
	if (sc->unit == sc_console_unit)
	    cons_unavail = FALSE;
    }

    return s;
}

static int
vt_proc_alive(scr_stat *scp)
{
    if (scp->proc) {
	if (scp->proc == pfind(scp->pid))
	    return TRUE;
	scp->proc = NULL;
	scp->smode.mode = VT_AUTO;
	DPRINTF(5, ("vt controlling process %d died\n", scp->pid));
    }
    return FALSE;
}

static int
signal_vt_rel(scr_stat *scp)
{
    if (scp->smode.mode != VT_PROCESS)
	return FALSE;
    scp->status |= SWITCH_WAIT_REL;
    psignal(scp->proc, scp->smode.relsig);
    DPRINTF(5, ("sending relsig to %d\n", scp->pid));
    return TRUE;
}

static int
signal_vt_acq(scr_stat *scp)
{
    if (scp->smode.mode != VT_PROCESS)
	return FALSE;
    if (scp->sc->unit == sc_console_unit)
	cons_unavail = TRUE;
    scp->status |= SWITCH_WAIT_ACQ;
    psignal(scp->proc, scp->smode.acqsig);
    DPRINTF(5, ("sending acqsig to %d\n", scp->pid));
    return TRUE;
}

static void
exchange_scr(sc_softc_t *sc)
{
    scr_stat *scp;

    /* save the current state of video and keyboard */
    move_crsr(sc->old_scp, sc->old_scp->xpos, sc->old_scp->ypos);
    if (sc->old_scp->kbd_mode == K_XLATE)
	save_kbd_state(sc->old_scp);

    /* set up the video for the new screen */
    scp = sc->cur_scp = sc->new_scp;
    if (sc->old_scp->mode != scp->mode || ISUNKNOWNSC(sc->old_scp))
	set_mode(scp);
    else
	sc_vtb_init(&scp->scr, VTB_FRAMEBUFFER, scp->xsize, scp->ysize,
		    (void *)sc->adp->va_window, FALSE);
    move_crsr(scp, scp->xpos, scp->ypos);
    if (!ISGRAPHSC(scp))
	sc_set_cursor_image(scp);
#ifndef SC_NO_PALETTE_LOADING
    if (ISGRAPHSC(sc->old_scp))
	load_palette(sc->adp, sc->palette);
#endif
    set_border(scp, scp->border);

    /* set up the keyboard for the new screen */
    if (sc->old_scp->kbd_mode != scp->kbd_mode)
	kbd_ioctl(sc->kbd, KDSKBMODE, (caddr_t)&scp->kbd_mode);
    update_kbd_state(scp, scp->status, LOCK_MASK);

    mark_all(scp);
}

static void
scan_esc(scr_stat *scp, u_char c)
{
    static u_char ansi_col[16] =
	{0, 4, 2, 6, 1, 5, 3, 7, 8, 12, 10, 14, 9, 13, 11, 15};
    sc_softc_t *sc;
    int i, n;
    int count;

    i = n = 0;
    sc = scp->sc; 
    if (scp->term.esc == 1) {	/* seen ESC */
	switch (c) {

	case '7':   /* Save cursor position */
	    scp->saved_xpos = scp->xpos;
	    scp->saved_ypos = scp->ypos;
	    break;

	case '8':   /* Restore saved cursor position */
	    if (scp->saved_xpos >= 0 && scp->saved_ypos >= 0)
		move_crsr(scp, scp->saved_xpos, scp->saved_ypos);
	    break;

	case '[':   /* Start ESC [ sequence */
	    scp->term.esc = 2;
	    scp->term.last_param = -1;
	    for (i = scp->term.num_param; i < MAX_ESC_PAR; i++)
		scp->term.param[i] = 1;
	    scp->term.num_param = 0;
	    return;

	case 'M':   /* Move cursor up 1 line, scroll if at top */
	    if (scp->ypos > 0)
		move_crsr(scp, scp->xpos, scp->ypos - 1);
	    else {
		sc_vtb_ins(&scp->vtb, 0, scp->xsize,
			   sc->scr_map[0x20], scp->term.cur_color);
    		mark_all(scp);
	    }
	    break;
#if notyet
	case 'Q':
	    scp->term.esc = 4;
	    return;
#endif
	case 'c':   /* Clear screen & home */
	    sc_clear_screen(scp);
	    break;

	case '(':   /* iso-2022: designate 94 character set to G0 */
	    scp->term.esc = 5;
	    return;
	}
    }
    else if (scp->term.esc == 2) {	/* seen ESC [ */
	if (c >= '0' && c <= '9') {
	    if (scp->term.num_param < MAX_ESC_PAR) {
	    if (scp->term.last_param != scp->term.num_param) {
		scp->term.last_param = scp->term.num_param;
		scp->term.param[scp->term.num_param] = 0;
	    }
	    else
		scp->term.param[scp->term.num_param] *= 10;
	    scp->term.param[scp->term.num_param] += c - '0';
	    return;
	    }
	}
	scp->term.num_param = scp->term.last_param + 1;
	switch (c) {

	case ';':
	    if (scp->term.num_param < MAX_ESC_PAR)
		return;
	    break;

	case '=':
	    scp->term.esc = 3;
	    scp->term.last_param = -1;
	    for (i = scp->term.num_param; i < MAX_ESC_PAR; i++)
		scp->term.param[i] = 1;
	    scp->term.num_param = 0;
	    return;

	case 'A':   /* up n rows */
	    n = scp->term.param[0]; if (n < 1) n = 1;
	    move_crsr(scp, scp->xpos, scp->ypos - n);
	    break;

	case 'B':   /* down n rows */
	    n = scp->term.param[0]; if (n < 1) n = 1;
	    move_crsr(scp, scp->xpos, scp->ypos + n);
	    break;

	case 'C':   /* right n columns */
	    n = scp->term.param[0]; if (n < 1) n = 1;
	    move_crsr(scp, scp->xpos + n, scp->ypos);
	    break;

	case 'D':   /* left n columns */
	    n = scp->term.param[0]; if (n < 1) n = 1;
	    move_crsr(scp, scp->xpos - n, scp->ypos);
	    break;

	case 'E':   /* cursor to start of line n lines down */
	    n = scp->term.param[0]; if (n < 1) n = 1;
	    move_crsr(scp, 0, scp->ypos + n);
	    break;

	case 'F':   /* cursor to start of line n lines up */
	    n = scp->term.param[0]; if (n < 1) n = 1;
	    move_crsr(scp, 0, scp->ypos - n);
	    break;

	case 'f':   /* Cursor move */
	case 'H':
	    if (scp->term.num_param == 0)
		move_crsr(scp, 0, 0);
	    else if (scp->term.num_param == 2)
		move_crsr(scp, scp->term.param[1] - 1, scp->term.param[0] - 1);
	    break;

	case 'J':   /* Clear all or part of display */
	    if (scp->term.num_param == 0)
		n = 0;
	    else
		n = scp->term.param[0];
	    switch (n) {
	    case 0: /* clear form cursor to end of display */
		sc_vtb_erase(&scp->vtb, scp->cursor_pos,
			     scp->xsize * scp->ysize - scp->cursor_pos,
			     sc->scr_map[0x20], scp->term.cur_color);
		mark_for_update(scp, scp->cursor_pos);
    		mark_for_update(scp, scp->xsize * scp->ysize - 1);
		sc_remove_cutmarking(scp);
		break;
	    case 1: /* clear from beginning of display to cursor */
		sc_vtb_erase(&scp->vtb, 0, scp->cursor_pos,
			     sc->scr_map[0x20], scp->term.cur_color);
		mark_for_update(scp, 0);
		mark_for_update(scp, scp->cursor_pos);
		sc_remove_cutmarking(scp);
		break;
	    case 2: /* clear entire display */
		sc_vtb_clear(&scp->vtb, sc->scr_map[0x20], scp->term.cur_color);
		mark_all(scp);
		sc_remove_cutmarking(scp);
		break;
	    }
	    break;

	case 'K':   /* Clear all or part of line */
	    if (scp->term.num_param == 0)
		n = 0;
	    else
		n = scp->term.param[0];
	    switch (n) {
	    case 0: /* clear form cursor to end of line */
		sc_vtb_erase(&scp->vtb, scp->cursor_pos,
			     scp->xsize - scp->xpos,
			     sc->scr_map[0x20], scp->term.cur_color);
    		mark_for_update(scp, scp->cursor_pos);
    		mark_for_update(scp, scp->cursor_pos +
				scp->xsize - 1 - scp->xpos);
		break;
	    case 1: /* clear from beginning of line to cursor */
		sc_vtb_erase(&scp->vtb, scp->cursor_pos - scp->xpos,
			     scp->xpos + 1,
			     sc->scr_map[0x20], scp->term.cur_color);
    		mark_for_update(scp, scp->ypos * scp->xsize);
    		mark_for_update(scp, scp->cursor_pos);
		break;
	    case 2: /* clear entire line */
		sc_vtb_erase(&scp->vtb, scp->cursor_pos - scp->xpos,
			     scp->xsize,
			     sc->scr_map[0x20], scp->term.cur_color);
    		mark_for_update(scp, scp->ypos * scp->xsize);
    		mark_for_update(scp, (scp->ypos + 1) * scp->xsize - 1);
		break;
	    }
	    break;

	case 'L':   /* Insert n lines */
	    n = scp->term.param[0]; if (n < 1) n = 1;
	    if (n > scp->ysize - scp->ypos)
		n = scp->ysize - scp->ypos;
	    sc_vtb_ins(&scp->vtb, scp->ypos * scp->xsize, n * scp->xsize,
		       sc->scr_map[0x20], scp->term.cur_color);
	    mark_for_update(scp, scp->ypos * scp->xsize);
	    mark_for_update(scp, scp->xsize * scp->ysize - 1);
	    break;

	case 'M':   /* Delete n lines */
	    n = scp->term.param[0]; if (n < 1) n = 1;
	    if (n > scp->ysize - scp->ypos)
		n = scp->ysize - scp->ypos;
	    sc_vtb_delete(&scp->vtb, scp->ypos * scp->xsize, n * scp->xsize,
			  sc->scr_map[0x20], scp->term.cur_color);
	    mark_for_update(scp, scp->ypos * scp->xsize);
	    mark_for_update(scp, scp->xsize * scp->ysize - 1);
	    break;

	case 'P':   /* Delete n chars */
	    n = scp->term.param[0]; if (n < 1) n = 1;
	    if (n > scp->xsize - scp->xpos)
		n = scp->xsize - scp->xpos;
	    count = scp->xsize - (scp->xpos + n);
	    sc_vtb_move(&scp->vtb, scp->cursor_pos + n, scp->cursor_pos, count);
	    sc_vtb_erase(&scp->vtb, scp->cursor_pos + count, n,
			 sc->scr_map[0x20], scp->term.cur_color);
	    mark_for_update(scp, scp->cursor_pos);
	    mark_for_update(scp, scp->cursor_pos + n + count - 1);
	    break;

	case '@':   /* Insert n chars */
	    n = scp->term.param[0]; if (n < 1) n = 1;
	    if (n > scp->xsize - scp->xpos)
		n = scp->xsize - scp->xpos;
	    count = scp->xsize - (scp->xpos + n);
	    sc_vtb_move(&scp->vtb, scp->cursor_pos, scp->cursor_pos + n, count);
	    sc_vtb_erase(&scp->vtb, scp->cursor_pos, n,
			 sc->scr_map[0x20], scp->term.cur_color);
	    mark_for_update(scp, scp->cursor_pos);
	    mark_for_update(scp, scp->cursor_pos + n + count - 1);
	    break;

	case 'S':   /* scroll up n lines */
	    n = scp->term.param[0]; if (n < 1)  n = 1;
	    if (n > scp->ysize)
		n = scp->ysize;
	    sc_vtb_delete(&scp->vtb, 0, n * scp->xsize,
			  sc->scr_map[0x20], scp->term.cur_color);
    	    mark_all(scp);
	    break;

	case 'T':   /* scroll down n lines */
	    n = scp->term.param[0]; if (n < 1)  n = 1;
	    if (n > scp->ysize)
		n = scp->ysize;
	    sc_vtb_ins(&scp->vtb, 0, n * scp->xsize,
		       sc->scr_map[0x20], scp->term.cur_color);
    	    mark_all(scp);
	    break;

	case 'X':   /* erase n characters in line */
	    n = scp->term.param[0]; if (n < 1)  n = 1;
	    if (n > scp->xsize - scp->xpos)
		n = scp->xsize - scp->xpos;
	    sc_vtb_erase(&scp->vtb, scp->cursor_pos, n,
			 sc->scr_map[0x20], scp->term.cur_color);
	    mark_for_update(scp, scp->cursor_pos);
	    mark_for_update(scp, scp->cursor_pos + n - 1);
	    break;

	case 'Z':   /* move n tabs backwards */
	    n = scp->term.param[0]; if (n < 1)  n = 1;
	    if ((i = scp->xpos & 0xf8) == scp->xpos)
		i -= 8*n;
	    else
		i -= 8*(n-1);
	    if (i < 0)
		i = 0;
	    move_crsr(scp, i, scp->ypos);
	    break;

	case '`':   /* move cursor to column n */
	    n = scp->term.param[0]; if (n < 1)  n = 1;
	    move_crsr(scp, n - 1, scp->ypos);
	    break;

	case 'a':   /* move cursor n columns to the right */
	    n = scp->term.param[0]; if (n < 1)  n = 1;
	    move_crsr(scp, scp->xpos + n, scp->ypos);
	    break;

	case 'd':   /* move cursor to row n */
	    n = scp->term.param[0]; if (n < 1)  n = 1;
	    move_crsr(scp, scp->xpos, n - 1);
	    break;

	case 'e':   /* move cursor n rows down */
	    n = scp->term.param[0]; if (n < 1)  n = 1;
	    move_crsr(scp, scp->xpos, scp->ypos + n);
	    break;

	case 'm':   /* change attribute */
	    if (scp->term.num_param == 0) {
		scp->term.attr_mask = NORMAL_ATTR;
		scp->term.cur_attr =
		    scp->term.cur_color = scp->term.std_color;
		break;
	    }
	    for (i = 0; i < scp->term.num_param; i++) {
		switch (n = scp->term.param[i]) {
		case 0: /* back to normal */
		    scp->term.attr_mask = NORMAL_ATTR;
		    scp->term.cur_attr =
			scp->term.cur_color = scp->term.std_color;
		    break;
		case 1: /* bold */
		    scp->term.attr_mask |= BOLD_ATTR;
		    scp->term.cur_attr = mask2attr(&scp->term);
		    break;
		case 4: /* underline */
		    scp->term.attr_mask |= UNDERLINE_ATTR;
		    scp->term.cur_attr = mask2attr(&scp->term);
		    break;
		case 5: /* blink */
		    scp->term.attr_mask |= BLINK_ATTR;
		    scp->term.cur_attr = mask2attr(&scp->term);
		    break;
		case 7: /* reverse video */
		    scp->term.attr_mask |= REVERSE_ATTR;
		    scp->term.cur_attr = mask2attr(&scp->term);
		    break;
		case 30: case 31: /* set fg color */
		case 32: case 33: case 34:
		case 35: case 36: case 37:
		    scp->term.attr_mask |= FOREGROUND_CHANGED;
		    scp->term.cur_color =
			(scp->term.cur_color&0xF000) | (ansi_col[(n-30)&7]<<8);
		    scp->term.cur_attr = mask2attr(&scp->term);
		    break;
		case 40: case 41: /* set bg color */
		case 42: case 43: case 44:
		case 45: case 46: case 47:
		    scp->term.attr_mask |= BACKGROUND_CHANGED;
		    scp->term.cur_color =
			(scp->term.cur_color&0x0F00) | (ansi_col[(n-40)&7]<<12);
		    scp->term.cur_attr = mask2attr(&scp->term);
		    break;
		}
	    }
	    break;

	case 's':   /* Save cursor position */
	    scp->saved_xpos = scp->xpos;
	    scp->saved_ypos = scp->ypos;
	    break;

	case 'u':   /* Restore saved cursor position */
	    if (scp->saved_xpos >= 0 && scp->saved_ypos >= 0)
		move_crsr(scp, scp->saved_xpos, scp->saved_ypos);
	    break;

	case 'x':
	    if (scp->term.num_param == 0)
		n = 0;
	    else
		n = scp->term.param[0];
	    switch (n) {
	    case 0:     /* reset attributes */
		scp->term.attr_mask = NORMAL_ATTR;
		scp->term.cur_attr =
		    scp->term.cur_color = scp->term.std_color =
		    current_default->std_color;
		scp->term.rev_color = current_default->rev_color;
		break;
	    case 1:     /* set ansi background */
		scp->term.attr_mask &= ~BACKGROUND_CHANGED;
		scp->term.cur_color = scp->term.std_color =
		    (scp->term.std_color & 0x0F00) |
		    (ansi_col[(scp->term.param[1])&0x0F]<<12);
		scp->term.cur_attr = mask2attr(&scp->term);
		break;
	    case 2:     /* set ansi foreground */
		scp->term.attr_mask &= ~FOREGROUND_CHANGED;
		scp->term.cur_color = scp->term.std_color =
		    (scp->term.std_color & 0xF000) |
		    (ansi_col[(scp->term.param[1])&0x0F]<<8);
		scp->term.cur_attr = mask2attr(&scp->term);
		break;
	    case 3:     /* set ansi attribute directly */
		scp->term.attr_mask &= ~(FOREGROUND_CHANGED|BACKGROUND_CHANGED);
		scp->term.cur_color = scp->term.std_color =
		    (scp->term.param[1]&0xFF)<<8;
		scp->term.cur_attr = mask2attr(&scp->term);
		break;
	    case 5:     /* set ansi reverse video background */
		scp->term.rev_color =
		    (scp->term.rev_color & 0x0F00) |
		    (ansi_col[(scp->term.param[1])&0x0F]<<12);
		scp->term.cur_attr = mask2attr(&scp->term);
		break;
	    case 6:     /* set ansi reverse video foreground */
		scp->term.rev_color =
		    (scp->term.rev_color & 0xF000) |
		    (ansi_col[(scp->term.param[1])&0x0F]<<8);
		scp->term.cur_attr = mask2attr(&scp->term);
		break;
	    case 7:     /* set ansi reverse video directly */
		scp->term.rev_color =
		    (scp->term.param[1]&0xFF)<<8;
		scp->term.cur_attr = mask2attr(&scp->term);
		break;
	    }
	    break;

	case 'z':   /* switch to (virtual) console n */
	    if (scp->term.num_param == 1)
		switch_scr(sc, scp->term.param[0]);
	    break;
	}
    }
    else if (scp->term.esc == 3) {	/* seen ESC [0-9]+ = */
	if (c >= '0' && c <= '9') {
	    if (scp->term.num_param < MAX_ESC_PAR) {
	    if (scp->term.last_param != scp->term.num_param) {
		scp->term.last_param = scp->term.num_param;
		scp->term.param[scp->term.num_param] = 0;
	    }
	    else
		scp->term.param[scp->term.num_param] *= 10;
	    scp->term.param[scp->term.num_param] += c - '0';
	    return;
	    }
	}
	scp->term.num_param = scp->term.last_param + 1;
	switch (c) {

	case ';':
	    if (scp->term.num_param < MAX_ESC_PAR)
		return;
	    break;

	case 'A':   /* set display border color */
	    if (scp->term.num_param == 1) {
		scp->border=scp->term.param[0] & 0xff;
		if (scp == sc->cur_scp)
		    set_border(scp, scp->border);
            }
	    break;

	case 'B':   /* set bell pitch and duration */
	    if (scp->term.num_param == 2) {
		scp->bell_pitch = scp->term.param[0];
		scp->bell_duration = scp->term.param[1];
	    }
	    break;

	case 'C':   /* set cursor type & shape */
	    if (!ISGRAPHSC(sc->cur_scp))
		remove_cursor_image(sc->cur_scp);
	    if (scp->term.num_param == 1) {
		if (scp->term.param[0] & 0x01)
		    sc->flags |= SC_BLINK_CURSOR;
		else
		    sc->flags &= ~SC_BLINK_CURSOR;
		if (scp->term.param[0] & 0x02) 
		    sc->flags |= SC_CHAR_CURSOR;
		else
		    sc->flags &= ~SC_CHAR_CURSOR;
	    }
	    else if (scp->term.num_param == 2) {
		sc->cursor_base = scp->font_size 
					- (scp->term.param[1] & 0x1F) - 1;
		sc->cursor_height = (scp->term.param[1] & 0x1F) 
					- (scp->term.param[0] & 0x1F) + 1;
	    }
	    /* 
	     * The cursor shape is global property; all virtual consoles
	     * are affected. Update the cursor in the current console...
	     */
	    if (!ISGRAPHSC(sc->cur_scp)) {
		i = spltty();
		sc_set_cursor_image(sc->cur_scp);
		draw_cursor_image(sc->cur_scp);
		splx(i);
	    }
	    break;

	case 'F':   /* set ansi foreground */
	    if (scp->term.num_param == 1) {
		scp->term.attr_mask &= ~FOREGROUND_CHANGED;
		scp->term.cur_color = scp->term.std_color =
		    (scp->term.std_color & 0xF000)
		    | ((scp->term.param[0] & 0x0F) << 8);
		scp->term.cur_attr = mask2attr(&scp->term);
	    }
	    break;

	case 'G':   /* set ansi background */
	    if (scp->term.num_param == 1) {
		scp->term.attr_mask &= ~BACKGROUND_CHANGED;
		scp->term.cur_color = scp->term.std_color =
		    (scp->term.std_color & 0x0F00)
		    | ((scp->term.param[0] & 0x0F) << 12);
		scp->term.cur_attr = mask2attr(&scp->term);
	    }
	    break;

	case 'H':   /* set ansi reverse video foreground */
	    if (scp->term.num_param == 1) {
		scp->term.rev_color =
		    (scp->term.rev_color & 0xF000)
		    | ((scp->term.param[0] & 0x0F) << 8);
		scp->term.cur_attr = mask2attr(&scp->term);
	    }
	    break;

	case 'I':   /* set ansi reverse video background */
	    if (scp->term.num_param == 1) {
		scp->term.rev_color =
		    (scp->term.rev_color & 0x0F00)
		    | ((scp->term.param[0] & 0x0F) << 12);
		scp->term.cur_attr = mask2attr(&scp->term);
	    }
	    break;
	}
    }
#if notyet
    else if (scp->term.esc == 4) {	/* seen ESC Q */
	/* to be filled */
    }
#endif
    else if (scp->term.esc == 5) {	/* seen ESC ( */
	switch (c) {
	case 'B':   /* iso-2022: desginate ASCII into G0 */
	    break;
	/* other items to be filled */
	default:
	    break;
	}
    }
    scp->term.esc = 0;
}

static void
ansi_put(scr_stat *scp, u_char *buf, int len)
{
    u_char *ptr = buf;

#if NSPLASH > 0
    /* make screensaver happy */
    if (!sticky_splash && scp == scp->sc->cur_scp)
	run_scrn_saver = FALSE;
#endif

outloop:
    scp->sc->write_in_progress++;
    if (scp->term.esc) {
	scan_esc(scp, *ptr++);
	len--;
    }
    else if (PRINTABLE(*ptr)) {     /* Print only printables */
	vm_offset_t p;
	u_char *map;
 	int cnt;
	int attr;
	int i;

	p = sc_vtb_pointer(&scp->vtb, scp->cursor_pos);
	map = scp->sc->scr_map;
	attr = scp->term.cur_attr;

	cnt = (len <= scp->xsize - scp->xpos) ? len : (scp->xsize - scp->xpos);
	i = cnt;
	do {
	    /*
	     * gcc-2.6.3 generates poor (un)sign extension code.  Casting the
	     * pointers in the following to volatile should have no effect,
	     * but in fact speeds up this inner loop from 26 to 18 cycles
	     * (+ cache misses) on i486's.
	     */
#define	UCVP(ucp)	((u_char volatile *)(ucp))
	    p = sc_vtb_putchar(&scp->vtb, p, UCVP(map)[*UCVP(ptr)], attr);
	    ++ptr;
	    --i;
	} while (i > 0 && PRINTABLE(*ptr));

	len -= cnt - i;
	mark_for_update(scp, scp->cursor_pos);
	scp->cursor_pos += cnt - i;
	mark_for_update(scp, scp->cursor_pos - 1);
	scp->xpos += cnt - i;

	if (scp->xpos >= scp->xsize) {
	    scp->xpos = 0;
	    scp->ypos++;
	}
    }
    else  {
	switch(*ptr) {
	case 0x07:
	    do_bell(scp, scp->bell_pitch, scp->bell_duration);
	    break;

	case 0x08:      /* non-destructive backspace */
	    if (scp->cursor_pos > 0) {
		mark_for_update(scp, scp->cursor_pos);
		scp->cursor_pos--;
		mark_for_update(scp, scp->cursor_pos);
		if (scp->xpos > 0)
		    scp->xpos--;
		else {
		    scp->xpos += scp->xsize - 1;
		    scp->ypos--;
		}
	    }
	    break;

	case 0x09:  /* non-destructive tab */
	    mark_for_update(scp, scp->cursor_pos);
	    scp->cursor_pos += (8 - scp->xpos % 8u);
	    mark_for_update(scp, scp->cursor_pos);
	    if ((scp->xpos += (8 - scp->xpos % 8u)) >= scp->xsize) {
	        scp->xpos = 0;
	        scp->ypos++;
	    }
	    break;

	case 0x0a:  /* newline, same pos */
	    mark_for_update(scp, scp->cursor_pos);
	    scp->cursor_pos += scp->xsize;
	    mark_for_update(scp, scp->cursor_pos);
	    scp->ypos++;
	    break;

	case 0x0c:  /* form feed, clears screen */
	    sc_clear_screen(scp);
	    break;

	case 0x0d:  /* return, return to pos 0 */
	    mark_for_update(scp, scp->cursor_pos);
	    scp->cursor_pos -= scp->xpos;
	    mark_for_update(scp, scp->cursor_pos);
	    scp->xpos = 0;
	    break;

	case 0x1b:  /* start escape sequence */
	    scp->term.esc = 1;
	    scp->term.num_param = 0;
	    break;
	}
	ptr++; len--;
    }
    /* do we have to scroll ?? */
    if (scp->cursor_pos >= scp->ysize * scp->xsize) {
	sc_remove_cutmarking(scp);
#ifndef SC_NO_HISTORY
	if (scp->history != NULL)
	    sc_hist_save_one_line(scp, 0);
#endif
	sc_vtb_delete(&scp->vtb, 0, scp->xsize,
		      scp->sc->scr_map[0x20], scp->term.cur_color);
	scp->cursor_pos -= scp->xsize;
	scp->ypos--;
    	mark_all(scp);
    }
    scp->sc->write_in_progress--;
    if (len)
	goto outloop;
    if (scp->sc->delayed_next_scr)
	switch_scr(scp->sc, scp->sc->delayed_next_scr - 1);
}

static void
draw_cursor_image(scr_stat *scp)
{
    /* assert(scp == scp->sc->cur_scp); */
    ++scp->sc->videoio_in_progress;
    (*scp->rndr->draw_cursor)(scp, scp->cursor_pos,
			      scp->sc->flags & SC_BLINK_CURSOR, TRUE,
			      sc_inside_cutmark(scp, scp->cursor_pos));
    --scp->sc->videoio_in_progress;
}

static void
remove_cursor_image(scr_stat *scp)
{
    /* assert(scp == scp->sc->cur_scp); */
    ++scp->sc->videoio_in_progress;
    (*scp->rndr->draw_cursor)(scp, scp->cursor_oldpos,
			      scp->sc->flags & SC_BLINK_CURSOR, FALSE,
			      sc_inside_cutmark(scp, scp->cursor_oldpos));
    --scp->sc->videoio_in_progress;
}

static void
update_cursor_image(scr_stat *scp)
{
    int blink;

    if (scp->sc->flags & SC_CHAR_CURSOR) {
	scp->cursor_base = scp->sc->cursor_base;
	scp->cursor_height = imin(scp->sc->cursor_height, scp->font_size);
    } else {
	scp->cursor_base = 0;
	scp->cursor_height = scp->font_size;
    }
    blink = scp->sc->flags & SC_BLINK_CURSOR;

    /* assert(scp == scp->sc->cur_scp); */
    ++scp->sc->videoio_in_progress;
    (*scp->rndr->draw_cursor)(scp, scp->cursor_oldpos, blink, FALSE, 
			      sc_inside_cutmark(scp, scp->cursor_pos));
    (*scp->rndr->set_cursor)(scp, scp->cursor_base, scp->cursor_height, blink);
    (*scp->rndr->draw_cursor)(scp, scp->cursor_pos, blink, TRUE, 
			      sc_inside_cutmark(scp, scp->cursor_pos));
    --scp->sc->videoio_in_progress;
}

void
sc_set_cursor_image(scr_stat *scp)
{
    if (scp->sc->flags & SC_CHAR_CURSOR) {
	scp->cursor_base = scp->sc->cursor_base;
	scp->cursor_height = imin(scp->sc->cursor_height, scp->font_size);
    } else {
	scp->cursor_base = 0;
	scp->cursor_height = scp->font_size;
    }

    /* assert(scp == scp->sc->cur_scp); */
    ++scp->sc->videoio_in_progress;
    (*scp->rndr->set_cursor)(scp, scp->cursor_base, scp->cursor_height,
			     scp->sc->flags & SC_BLINK_CURSOR);
    --scp->sc->videoio_in_progress;
}

static void
move_crsr(scr_stat *scp, int x, int y)
{
    if (x < 0)
	x = 0;
    if (y < 0)
	y = 0;
    if (x >= scp->xsize)
	x = scp->xsize-1;
    if (y >= scp->ysize)
	y = scp->ysize-1;
    scp->xpos = x;
    scp->ypos = y;
    scp->cursor_pos = scp->ypos * scp->xsize + scp->xpos;
}

static void
scinit(int unit, int flags)
{
    /*
     * When syscons is being initialized as the kernel console, malloc()
     * is not yet functional, because various kernel structures has not been
     * fully initialized yet.  Therefore, we need to declare the following
     * static buffers for the console.  This is less than ideal, 
     * but is necessry evil for the time being.  XXX
     */
    static scr_stat main_console;
    static scr_stat *main_vtys[MAXCONS];
    static struct tty main_tty[MAXCONS];
    static u_short sc_buffer[ROW*COL];	/* XXX */
#ifndef SC_NO_FONT_LOADING
    static u_char font_8[256*8];
    static u_char font_14[256*14];
    static u_char font_16[256*16];
#endif

    sc_softc_t *sc;
    scr_stat *scp;
    video_adapter_t *adp;
    int col;
    int row;
    int i;

    /* one time initialization */
    if (init_done == COLD) {
	sc_get_bios_values(&bios_value);
	current_default = &user_default;
	/* kernel console attributes */
	kernel_console.esc = 0;
	kernel_console.attr_mask = NORMAL_ATTR;
	kernel_console.cur_attr =
	    kernel_console.cur_color = kernel_console.std_color =
	    kernel_default.std_color;
	kernel_console.rev_color = kernel_default.rev_color;
    }
    init_done = WARM;

    /*
     * Allocate resources.  Even if we are being called for the second
     * time, we must allocate them again, because they might have 
     * disappeared...
     */
    sc = sc_get_softc(unit, flags & SC_KERNEL_CONSOLE);
    adp = NULL;
    if (sc->adapter >= 0) {
	vid_release(sc->adp, (void *)&sc->adapter);
	adp = sc->adp;
	sc->adp = NULL;
    }
    if (sc->keyboard >= 0) {
	DPRINTF(5, ("sc%d: releasing kbd%d\n", unit, sc->keyboard));
	i = kbd_release(sc->kbd, (void *)&sc->keyboard);
	DPRINTF(5, ("sc%d: kbd_release returned %d\n", unit, i));
	if (sc->kbd != NULL) {
	    DPRINTF(5, ("sc%d: kbd != NULL!, index:%d, minor:%d, flags:0x%x\n",
		unit, sc->kbd->kb_index, sc->kbd->kb_minor, sc->kbd->kb_flags));
	}
	sc->kbd = NULL;
    }
    sc->adapter = vid_allocate("*", unit, (void *)&sc->adapter);
    sc->adp = vid_get_adapter(sc->adapter);
    /* assert((sc->adapter >= 0) && (sc->adp != NULL)) */
    sc->keyboard = kbd_allocate("*", unit, (void *)&sc->keyboard,
				sckbdevent, sc);
    DPRINTF(1, ("sc%d: keyboard %d\n", unit, sc->keyboard));
    sc->kbd = kbd_get_keyboard(sc->keyboard);
    if (sc->kbd != NULL) {
	DPRINTF(1, ("sc%d: kbd index:%d, minor:%d, flags:0x%x\n",
		unit, sc->kbd->kb_index, sc->kbd->kb_minor, sc->kbd->kb_flags));
    }

    if (!(sc->flags & SC_INIT_DONE) || (adp != sc->adp)) {

	sc->initial_mode = sc->adp->va_initial_mode;

#ifndef SC_NO_FONT_LOADING
	if (flags & SC_KERNEL_CONSOLE) {
	    sc->font_8 = font_8;
	    sc->font_14 = font_14;
	    sc->font_16 = font_16;
	} else if (sc->font_8 == NULL) {
	    /* assert(sc_malloc) */
	    sc->font_8 = malloc(sizeof(font_8), M_DEVBUF, M_WAITOK);
	    sc->font_14 = malloc(sizeof(font_14), M_DEVBUF, M_WAITOK);
	    sc->font_16 = malloc(sizeof(font_16), M_DEVBUF, M_WAITOK);
	}
#endif

	/* extract the hardware cursor location and hide the cursor for now */
	(*vidsw[sc->adapter]->read_hw_cursor)(sc->adp, &col, &row);
	(*vidsw[sc->adapter]->set_hw_cursor)(sc->adp, -1, -1);

	/* set up the first console */
	sc->first_vty = unit*MAXCONS;
	if (flags & SC_KERNEL_CONSOLE) {
	    sc->vtys = MAXCONS;
	    sc->tty = main_tty;
	    sc->console = main_vtys;
	    scp = main_vtys[0] = &main_console;
	    init_scp(sc, sc->first_vty, scp);
	    sc_vtb_init(&scp->vtb, VTB_MEMORY, scp->xsize, scp->ysize,
			(void *)sc_buffer, FALSE);
	} else {
	    /* assert(sc_malloc) */
	    sc->vtys = MAXCONS;
	    sc->tty = malloc(sizeof(struct tty)*MAXCONS, M_DEVBUF, M_WAITOK);
	    sc->console = malloc(sizeof(struct scr_stat *)*MAXCONS,
				 M_DEVBUF, M_WAITOK);
	    scp = sc->console[0] = alloc_scp(sc, sc->first_vty);
	}
	bzero(sc->tty, sizeof(struct tty)*MAXCONS);
	for (i = 0; i < MAXCONS; i++)
	    ttyregister(&sc->tty[i]);
	sc->cur_scp = scp;

	/* copy screen to temporary buffer */
	sc_vtb_init(&scp->scr, VTB_FRAMEBUFFER, scp->xsize, scp->ysize,
		    (void *)scp->sc->adp->va_window, FALSE);
	if (ISTEXTSC(scp))
	    sc_vtb_copy(&scp->scr, 0, &scp->vtb, 0, scp->xsize*scp->ysize);

	/* move cursors to the initial positions */
	scp->mouse_pos = scp->mouse_oldpos = 0;
	if (col >= scp->xsize)
	    col = 0;
	if (row >= scp->ysize)
	    row = scp->ysize - 1;
	scp->xpos = col;
	scp->ypos = row;
	scp->cursor_pos = scp->cursor_oldpos = row*scp->xsize + col;
	if (bios_value.cursor_end < scp->font_size)
	    sc->cursor_base = scp->font_size - bios_value.cursor_end - 1;
	else
	    sc->cursor_base = 0;
	i = bios_value.cursor_end - bios_value.cursor_start + 1;
	sc->cursor_height = imin(i, scp->font_size);
	if (!ISGRAPHSC(scp)) {
    	    sc_set_cursor_image(scp);
    	    draw_cursor_image(scp);
	}

	/* save font and palette */
#ifndef SC_NO_FONT_LOADING
	sc->fonts_loaded = 0;
	if (ISFONTAVAIL(sc->adp->va_flags)) {
#ifdef SC_DFLT_FONT
	    bcopy(dflt_font_8, sc->font_8, sizeof(dflt_font_8));
	    bcopy(dflt_font_14, sc->font_14, sizeof(dflt_font_14));
	    bcopy(dflt_font_16, sc->font_16, sizeof(dflt_font_16));
	    sc->fonts_loaded = FONT_16 | FONT_14 | FONT_8;
	    if (scp->font_size < 14) {
		copy_font(scp, LOAD, 8, sc->font_8);
	    } else if (scp->font_size >= 16) {
		copy_font(scp, LOAD, 16, sc->font_16);
	    } else {
		copy_font(scp, LOAD, 14, sc->font_14);
	    }
#else /* !SC_DFLT_FONT */
	    if (scp->font_size < 14) {
		copy_font(scp, SAVE, 8, sc->font_8);
		sc->fonts_loaded = FONT_8;
	    } else if (scp->font_size >= 16) {
		copy_font(scp, SAVE, 16, sc->font_16);
		sc->fonts_loaded = FONT_16;
	    } else {
		copy_font(scp, SAVE, 14, sc->font_14);
		sc->fonts_loaded = FONT_14;
	    }
#endif /* SC_DFLT_FONT */
	    /* FONT KLUDGE: always use the font page #0. XXX */
	    (*vidsw[sc->adapter]->show_font)(sc->adp, 0);
	}
#endif /* !SC_NO_FONT_LOADING */

#ifndef SC_NO_PALETTE_LOADING
	save_palette(sc->adp, sc->palette);
#endif

#if NSPLASH > 0
	if (!(sc->flags & SC_SPLASH_SCRN) && (flags & SC_KERNEL_CONSOLE)) {
	    /* we are ready to put up the splash image! */
	    splash_init(sc->adp, scsplash_callback, sc);
	    sc->flags |= SC_SPLASH_SCRN;
	}
#endif /* NSPLASH */
    }

    /* the rest is not necessary, if we have done it once */
    if (sc->flags & SC_INIT_DONE)
	return;

    /* clear structures */
    for (i = 1; i < sc->vtys; i++)
	sc->console[i] = NULL;

    /* initialize mapscrn arrays to a one to one map */
    for (i = 0; i < sizeof(sc->scr_map); i++)
	sc->scr_map[i] = sc->scr_rmap[i] = i;

    sc->flags |= SC_INIT_DONE;
}

#if __i386__
static void
scterm(int unit, int flags)
{
    sc_softc_t *sc;

    sc = sc_get_softc(unit, flags & SC_KERNEL_CONSOLE);
    if (sc == NULL)
	return;			/* shouldn't happen */

#if NSPLASH > 0
    /* this console is no longer available for the splash screen */
    if (sc->flags & SC_SPLASH_SCRN) {
	splash_term(sc->adp);
	sc->flags &= ~SC_SPLASH_SCRN;
    }
#endif /* NSPLASH */

#if 0 /* XXX */
    /* move the hardware cursor to the upper-left corner */
    (*vidsw[sc->adapter]->set_hw_cursor)(sc->adp, 0, 0);
#endif

    /* release the keyboard and the video card */
    if (sc->keyboard >= 0)
	kbd_release(sc->kbd, &sc->keyboard);
    if (sc->adapter >= 0)
	vid_release(sc->adp, &sc->adapter);

    /* clear the structure */
    if (!(flags & SC_KERNEL_CONSOLE)) {
	free(sc->console, M_DEVBUF);
#if 0
	/* XXX: We need a ttyunregister for this */
	free(sc->tty, M_DEVBUF);
#endif
#ifndef SC_NO_FONT_LOADING
	free(sc->font_8, M_DEVBUF);
	free(sc->font_14, M_DEVBUF);
	free(sc->font_16, M_DEVBUF);
#endif
	/* XXX vtb, history */
    }
    bzero(sc, sizeof(*sc));
    sc->keyboard = -1;
    sc->adapter = -1;
}
#endif

static void
scshutdown(int howto, void *arg)
{
    /* assert(sc_console != NULL) */

    sc_touch_scrn_saver();
    if (!cold && sc_console
	&& sc_console->sc->cur_scp->smode.mode == VT_AUTO 
	&& sc_console->smode.mode == VT_AUTO)
	switch_scr(sc_console->sc, sc_console->index);
    shutdown_in_progress = TRUE;
}

int
sc_clean_up(scr_stat *scp)
{
#if NSPLASH > 0
    int error;
#endif /* NSPLASH */

    sc_touch_scrn_saver();
#if NSPLASH > 0
    if ((error = wait_scrn_saver_stop(scp->sc)))
	return error;
#endif /* NSPLASH */
    scp->status &= ~MOUSE_VISIBLE;
    sc_remove_cutmarking(scp);
    return 0;
}

void
sc_alloc_scr_buffer(scr_stat *scp, int wait, int discard)
{
    sc_vtb_t new;
    sc_vtb_t old;
    int s;

    old = scp->vtb;
    sc_vtb_init(&new, VTB_MEMORY, scp->xsize, scp->ysize, NULL, wait);
    if (!discard && (old.vtb_flags & VTB_VALID)) {
	/* retain the current cursor position and buffer contants */
	scp->cursor_oldpos = scp->cursor_pos;
	/* 
	 * This works only if the old buffer has the same size as or larger 
	 * than the new one. XXX
	 */
	sc_vtb_copy(&old, 0, &new, 0, scp->xsize*scp->ysize);
	scp->vtb = new;
    } else {
        /* clear the screen and move the text cursor to the top-left position */
	s = splhigh();
	scp->vtb = new;
	sc_clear_screen(scp);
	splx(s);
	sc_vtb_destroy(&old);
    }

#ifndef SC_NO_SYSMOUSE
    /* move the mouse cursor at the center of the screen */
    sc_mouse_move(scp, scp->xpixel / 2, scp->ypixel / 2);
#endif
}

static scr_stat
*alloc_scp(sc_softc_t *sc, int vty)
{
    scr_stat *scp;

    /* assert(sc_malloc) */

    scp = (scr_stat *)malloc(sizeof(scr_stat), M_DEVBUF, M_WAITOK);
    init_scp(sc, vty, scp);

    sc_alloc_scr_buffer(scp, TRUE, TRUE);

#ifndef SC_NO_SYSMOUSE
    if (ISMOUSEAVAIL(sc->adp->va_flags))
	sc_alloc_cut_buffer(scp, TRUE);
#endif

#ifndef SC_NO_HISTORY
    sc_alloc_history_buffer(scp, 0, 0, TRUE);
#endif

    sc_clear_screen(scp);
    return scp;
}

static void
init_scp(sc_softc_t *sc, int vty, scr_stat *scp)
{
    video_info_t info;

    scp->index = vty;
    scp->sc = sc;
    scp->status = 0;
    scp->mode = sc->initial_mode;
    (*vidsw[sc->adapter]->get_info)(sc->adp, scp->mode, &info);
    if (info.vi_flags & V_INFO_GRAPHICS) {
	scp->status |= GRAPHICS_MODE;
	scp->xpixel = info.vi_width;
	scp->ypixel = info.vi_height;
	scp->xsize = info.vi_width/8;
	scp->ysize = info.vi_height/info.vi_cheight;
	scp->font_size = FONT_NONE;
	scp->font = NULL;
    } else {
	scp->xsize = info.vi_width;
	scp->ysize = info.vi_height;
	scp->xpixel = scp->xsize*8;
	scp->ypixel = scp->ysize*info.vi_cheight;
	if (info.vi_cheight < 14) {
	    scp->font_size = 8;
#ifndef SC_NO_FONT_LOADING
	    scp->font = sc->font_8;
#else
	    scp->font = NULL;
#endif
	} else if (info.vi_cheight >= 16) {
	    scp->font_size = 16;
#ifndef SC_NO_FONT_LOADING
	    scp->font = sc->font_16;
#else
	    scp->font = NULL;
#endif
	} else {
	    scp->font_size = 14;
#ifndef SC_NO_FONT_LOADING
	    scp->font = sc->font_14;
#else
	    scp->font = NULL;
#endif
	}
    }
    sc_vtb_init(&scp->vtb, VTB_MEMORY, 0, 0, NULL, FALSE);
    sc_vtb_init(&scp->scr, VTB_FRAMEBUFFER, 0, 0, NULL, FALSE);
    scp->xoff = scp->yoff = 0;
    scp->xpos = scp->ypos = 0;
    scp->saved_xpos = scp->saved_ypos = -1;
    scp->start = scp->xsize * scp->ysize - 1;
    scp->end = 0;
    scp->term.esc = 0;
    scp->term.attr_mask = NORMAL_ATTR;
    scp->term.cur_attr =
	scp->term.cur_color = scp->term.std_color =
	current_default->std_color;
    scp->term.rev_color = current_default->rev_color;
    scp->border = BG_BLACK;
    scp->cursor_base = sc->cursor_base;
    scp->cursor_height = imin(sc->cursor_height, scp->font_size);
    scp->mouse_xpos = scp->xoff*8 + scp->xsize*8/2;
    scp->mouse_ypos = (scp->ysize + scp->yoff)*scp->font_size/2;
    scp->mouse_cut_start = scp->xsize*scp->ysize;
    scp->mouse_cut_end = -1;
    scp->mouse_signal = 0;
    scp->mouse_pid = 0;
    scp->mouse_proc = NULL;
    scp->kbd_mode = K_XLATE;
    scp->bell_pitch = bios_value.bell_pitch;
    scp->bell_duration = BELL_DURATION;
    scp->status |= (bios_value.shift_state & NLKED);
    scp->status |= CURSOR_ENABLED;
    scp->pid = 0;
    scp->proc = NULL;
    scp->smode.mode = VT_AUTO;
    scp->history = NULL;
    scp->history_pos = 0;
    scp->history_size = 0;

    /* what if the following call fails... XXX */
    scp->rndr = sc_render_match(scp, scp->sc->adp,
				scp->status & (GRAPHICS_MODE | PIXEL_MODE));
}

/*
 * scgetc(flags) - get character from keyboard.
 * If flags & SCGETC_CN, then avoid harmful side effects.
 * If flags & SCGETC_NONBLOCK, then wait until a key is pressed, else
 * return NOKEY if there is nothing there.
 */
static u_int
scgetc(sc_softc_t *sc, u_int flags)
{
    scr_stat *scp;
    struct tty *tp;
    u_int c;
    int this_scr;
    int f;
    int i;

    if (sc->kbd == NULL)
	return NOKEY;

next_code:
#if 1
    /* I don't like this, but... XXX */
    if (flags & SCGETC_CN)
	sccnupdate(sc->cur_scp);
#endif
    scp = sc->cur_scp;
    /* first see if there is something in the keyboard port */
    for (;;) {
	c = kbd_read_char(sc->kbd, !(flags & SCGETC_NONBLOCK));
	if (c == ERRKEY) {
	    if (!(flags & SCGETC_CN))
		do_bell(scp, bios_value.bell_pitch, BELL_DURATION);
	} else if (c == NOKEY)
	    return c;
	else
	    break;
    }

    /* make screensaver happy */
    if (!(c & RELKEY))
	sc_touch_scrn_saver();

#ifdef __i386__
    if (!(flags & SCGETC_CN))
	/* do the /dev/random device a favour */
	add_keyboard_randomness(c);
#endif

    if (scp->kbd_mode != K_XLATE)
	return KEYCHAR(c);

    /* if scroll-lock pressed allow history browsing */
    if (!ISGRAPHSC(scp) && scp->history && scp->status & SLKED) {

	scp->status &= ~CURSOR_ENABLED;
	remove_cursor_image(scp);

#ifndef SC_NO_HISTORY
	if (!(scp->status & BUFFER_SAVED)) {
	    scp->status |= BUFFER_SAVED;
	    sc_hist_save(scp);
	}
	switch (c) {
	/* FIXME: key codes */
	case SPCLKEY | FKEY | F(49):  /* home key */
	    sc_remove_cutmarking(scp);
	    sc_hist_home(scp);
	    goto next_code;

	case SPCLKEY | FKEY | F(57):  /* end key */
	    sc_remove_cutmarking(scp);
	    sc_hist_end(scp);
	    goto next_code;

	case SPCLKEY | FKEY | F(50):  /* up arrow key */
	    sc_remove_cutmarking(scp);
	    if (sc_hist_up_line(scp))
		if (!(flags & SCGETC_CN))
		    do_bell(scp, bios_value.bell_pitch, BELL_DURATION);
	    goto next_code;

	case SPCLKEY | FKEY | F(58):  /* down arrow key */
	    sc_remove_cutmarking(scp);
	    if (sc_hist_down_line(scp))
		if (!(flags & SCGETC_CN))
		    do_bell(scp, bios_value.bell_pitch, BELL_DURATION);
	    goto next_code;

	case SPCLKEY | FKEY | F(51):  /* page up key */
	    sc_remove_cutmarking(scp);
	    for (i=0; i<scp->ysize; i++)
	    if (sc_hist_up_line(scp)) {
		if (!(flags & SCGETC_CN))
		    do_bell(scp, bios_value.bell_pitch, BELL_DURATION);
		break;
	    }
	    goto next_code;

	case SPCLKEY | FKEY | F(59):  /* page down key */
	    sc_remove_cutmarking(scp);
	    for (i=0; i<scp->ysize; i++)
	    if (sc_hist_down_line(scp)) {
		if (!(flags & SCGETC_CN))
		    do_bell(scp, bios_value.bell_pitch, BELL_DURATION);
		break;
	    }
	    goto next_code;
	}
#endif /* SC_NO_HISTORY */
    }

    /* 
     * Process and consume special keys here.  Return a plain char code
     * or a char code with the META flag or a function key code.
     */
    if (c & RELKEY) {
	/* key released */
	/* goto next_code */
    } else {
	/* key pressed */
	if (c & SPCLKEY) {
	    c &= ~SPCLKEY;
	    switch (KEYCHAR(c)) {
	    /* LOCKING KEYS */
	    case NLK: case CLK: case ALK:
		break;
	    case SLK:
		kbd_ioctl(sc->kbd, KDGKBSTATE, (caddr_t)&f);
		if (f & SLKED) {
		    scp->status |= SLKED;
		} else {
		    if (scp->status & SLKED) {
			scp->status &= ~SLKED;
#ifndef SC_NO_HISTORY
			if (scp->status & BUFFER_SAVED) {
			    if (!sc_hist_restore(scp))
				sc_remove_cutmarking(scp);
			    scp->status &= ~BUFFER_SAVED;
			    scp->status |= CURSOR_ENABLED;
			    draw_cursor_image(scp);
			}
			tp = VIRTUAL_TTY(sc, scp->index);
			if (tp->t_state & TS_ISOPEN)
			    scstart(tp);
#endif
		    }
		}
		break;

	    /* NON-LOCKING KEYS */
	    case NOP:
	    case LSH:  case RSH:  case LCTR: case RCTR:
	    case LALT: case RALT: case ASH:  case META:
		break;

	    case BTAB:
		if (!(sc->flags & SC_SCRN_BLANKED))
		    return c;
		break;

	    case SPSC:
#if NSPLASH > 0
		/* force activatation/deactivation of the screen saver */
		if (!(sc->flags & SC_SCRN_BLANKED)) {
		    run_scrn_saver = TRUE;
		    sc->scrn_time_stamp -= scrn_blank_time;
		}
		if (cold) {
		    /*
		     * While devices are being probed, the screen saver need
		     * to be invoked explictly. XXX
		     */
		    if (sc->flags & SC_SCRN_BLANKED) {
			scsplash_stick(FALSE);
			stop_scrn_saver(sc, current_saver);
		    } else {
			if (!ISGRAPHSC(scp)) {
			    scsplash_stick(TRUE);
			    (*current_saver)(sc, TRUE);
			}
		    }
		}
#endif /* NSPLASH */
		break;

	    case RBT:
#ifndef SC_DISABLE_REBOOT
		shutdown_nice();
#endif
		break;

#if NAPM > 0
	    case SUSP:
		apm_suspend(PMST_SUSPEND);
		break;
	    case STBY:
		apm_suspend(PMST_STANDBY);
		break;
#else
	    case SUSP:
	    case STBY:
		break;
#endif

	    case DBG:
#ifndef SC_DISABLE_DDBKEY
#ifdef DDB
		if (debugger)
		    break;
		/* try to switch to the kernel console screen */
		if (sc_console) {
		    /*
		     * TRY to make sure the screen saver is stopped, 
		     * and the screen is updated before switching to 
		     * the vty0.
		     */
		    scrn_timer(NULL);
		    if (!cold
			&& sc_console->sc->cur_scp->smode.mode == VT_AUTO
			&& sc_console->smode.mode == VT_AUTO)
			switch_scr(sc_console->sc, sc_console->index);
		}
		Debugger("manual escape to debugger");
#else
		printf("No debugger in kernel\n");
#endif
#else /* SC_DISABLE_DDBKEY */
		/* do nothing */
#endif /* SC_DISABLE_DDBKEY */
		break;

	    case NEXT:
		this_scr = scp->index;
		for (i = (this_scr - sc->first_vty + 1)%sc->vtys;
			sc->first_vty + i != this_scr; 
			i = (i + 1)%sc->vtys) {
		    struct tty *tp = VIRTUAL_TTY(sc, sc->first_vty + i);
		    if (tp->t_state & TS_ISOPEN) {
			switch_scr(scp->sc, sc->first_vty + i);
			break;
		    }
		}
		break;

	    case PREV:
		this_scr = scp->index;
		for (i = (this_scr - sc->first_vty + sc->vtys - 1)%sc->vtys;
			sc->first_vty + i != this_scr;
			i = (i + sc->vtys - 1)%sc->vtys) {
		    struct tty *tp = VIRTUAL_TTY(sc, sc->first_vty + i);
		    if (tp->t_state & TS_ISOPEN) {
			switch_scr(scp->sc, sc->first_vty + i);
			break;
		    }
		}
		break;

	    default:
		if (KEYCHAR(c) >= F_SCR && KEYCHAR(c) <= L_SCR) {
		    switch_scr(scp->sc, sc->first_vty + KEYCHAR(c) - F_SCR);
		    break;
		}
		/* assert(c & FKEY) */
		if (!(sc->flags & SC_SCRN_BLANKED))
		    return c;
		break;
	    }
	    /* goto next_code */
	} else {
	    /* regular keys (maybe MKEY is set) */
	    if (!(sc->flags & SC_SCRN_BLANKED))
		return c;
	}
    }

    goto next_code;
}

int
scmmap(dev_t dev, vm_offset_t offset, int nprot)
{
    struct tty *tp;
    struct scr_stat *scp;

    tp = dev->si_tty_tty;
    if (!tp)
	return ENXIO;
    scp = sc_get_scr_stat(tp->t_dev);
    return (*vidsw[scp->sc->adapter]->mmap)(scp->sc->adp, offset, nprot);
}

/*
 * Calculate hardware attributes word using logical attributes mask and
 * hardware colors
 */

static int
mask2attr(struct term_stat *term)
{
    int attr, mask = term->attr_mask;

    if (mask & REVERSE_ATTR) {
	attr = ((mask & FOREGROUND_CHANGED) ?
		((term->cur_color & 0xF000) >> 4) :
		(term->rev_color & 0x0F00)) |
	       ((mask & BACKGROUND_CHANGED) ?
		((term->cur_color & 0x0F00) << 4) :
		(term->rev_color & 0xF000));
    } else
	attr = term->cur_color;

    /* XXX: underline mapping for Hercules adapter can be better */
    if (mask & (BOLD_ATTR | UNDERLINE_ATTR))
	attr ^= 0x0800;
    if (mask & BLINK_ATTR)
	attr ^= 0x8000;

    return attr;
}

static int
save_kbd_state(scr_stat *scp)
{
    int state;
    int error;

    error = kbd_ioctl(scp->sc->kbd, KDGKBSTATE, (caddr_t)&state);
    if (error == ENOIOCTL)
	error = ENODEV;
    if (error == 0) {
	scp->status &= ~LOCK_MASK;
	scp->status |= state;
    }
    return error;
}

static int
update_kbd_state(scr_stat *scp, int new_bits, int mask)
{
    int state;
    int error;

    if (mask != LOCK_MASK) {
	error = kbd_ioctl(scp->sc->kbd, KDGKBSTATE, (caddr_t)&state);
	if (error == ENOIOCTL)
	    error = ENODEV;
	if (error)
	    return error;
	state &= ~mask;
	state |= new_bits & mask;
    } else {
	state = new_bits & LOCK_MASK;
    }
    error = kbd_ioctl(scp->sc->kbd, KDSKBSTATE, (caddr_t)&state);
    if (error == ENOIOCTL)
	error = ENODEV;
    return error;
}

static int
update_kbd_leds(scr_stat *scp, int which)
{
    int error;

    which &= LOCK_MASK;
    error = kbd_ioctl(scp->sc->kbd, KDSETLED, (caddr_t)&which);
    if (error == ENOIOCTL)
	error = ENODEV;
    return error;
}

int
set_mode(scr_stat *scp)
{
    video_info_t info;

    /* reject unsupported mode */
    if ((*vidsw[scp->sc->adapter]->get_info)(scp->sc->adp, scp->mode, &info))
	return 1;

    /* if this vty is not currently showing, do nothing */
    if (scp != scp->sc->cur_scp)
	return 0;

    /* setup video hardware for the given mode */
    (*vidsw[scp->sc->adapter]->set_mode)(scp->sc->adp, scp->mode);
    sc_vtb_init(&scp->scr, VTB_FRAMEBUFFER, scp->xsize, scp->ysize,
		(void *)scp->sc->adp->va_window, FALSE);

#ifndef SC_NO_FONT_LOADING
    /* load appropriate font */
    if (!(scp->status & GRAPHICS_MODE)) {
	if (!(scp->status & PIXEL_MODE) && ISFONTAVAIL(scp->sc->adp->va_flags)) {
	    if (scp->font_size < 14) {
		if (scp->sc->fonts_loaded & FONT_8)
		    copy_font(scp, LOAD, 8, scp->sc->font_8);
	    } else if (scp->font_size >= 16) {
		if (scp->sc->fonts_loaded & FONT_16)
		    copy_font(scp, LOAD, 16, scp->sc->font_16);
	    } else {
		if (scp->sc->fonts_loaded & FONT_14)
		    copy_font(scp, LOAD, 14, scp->sc->font_14);
	    }
	    /*
	     * FONT KLUDGE:
	     * This is an interim kludge to display correct font.
	     * Always use the font page #0 on the video plane 2.
	     * Somehow we cannot show the font in other font pages on
	     * some video cards... XXX
	     */ 
	    (*vidsw[scp->sc->adapter]->show_font)(scp->sc->adp, 0);
	}
	mark_all(scp);
    }
#endif /* !SC_NO_FONT_LOADING */

    set_border(scp, scp->border);
    sc_set_cursor_image(scp);

    return 0;
}

void
set_border(scr_stat *scp, int color)
{
    ++scp->sc->videoio_in_progress;
    (*scp->rndr->draw_border)(scp, color);
    --scp->sc->videoio_in_progress;
}

#ifndef SC_NO_FONT_LOADING
void
copy_font(scr_stat *scp, int operation, int font_size, u_char *buf)
{
    /*
     * FONT KLUDGE:
     * This is an interim kludge to display correct font.
     * Always use the font page #0 on the video plane 2.
     * Somehow we cannot show the font in other font pages on
     * some video cards... XXX
     */ 
    scp->sc->font_loading_in_progress = TRUE;
    if (operation == LOAD) {
	(*vidsw[scp->sc->adapter]->load_font)(scp->sc->adp, 0, font_size,
					      buf, 0, 256);
    } else if (operation == SAVE) {
	(*vidsw[scp->sc->adapter]->save_font)(scp->sc->adp, 0, font_size,
					      buf, 0, 256);
    }
    scp->sc->font_loading_in_progress = FALSE;
}
#endif /* !SC_NO_FONT_LOADING */

#ifndef SC_NO_SYSMOUSE
struct tty
*sc_get_mouse_tty(void)
{
    return MOUSE_TTY;
}
#endif /* !SC_NO_SYSMOUSE */

#ifndef SC_NO_CUTPASTE
void
sc_paste(scr_stat *scp, u_char *p, int count) 
{
    struct tty *tp;
    u_char *rmap;

    if (scp->status & MOUSE_VISIBLE) {
	tp = VIRTUAL_TTY(scp->sc, scp->sc->cur_scp->index);
	rmap = scp->sc->scr_rmap;
	for (; count > 0; --count)
	    (*linesw[tp->t_line].l_rint)(rmap[*p++], tp);
    }
}
#endif /* SC_NO_CUTPASTE */

static void
do_bell(scr_stat *scp, int pitch, int duration)
{
    if (cold || shutdown_in_progress)
	return;

    if (scp != scp->sc->cur_scp && (scp->sc->flags & SC_QUIET_BELL))
	return;

    if (scp->sc->flags & SC_VISUAL_BELL) {
	if (scp->sc->blink_in_progress)
	    return;
	scp->sc->blink_in_progress = 3;
	if (scp != scp->sc->cur_scp)
	    scp->sc->blink_in_progress += 2;
	blink_screen(scp->sc->cur_scp);
    } else {
	if (scp != scp->sc->cur_scp)
	    pitch *= 2;
	sysbeep(pitch, duration);
    }
}

static void
blink_screen(void *arg)
{
    scr_stat *scp = arg;
    struct tty *tp;

    if (ISGRAPHSC(scp) || (scp->sc->blink_in_progress <= 1)) {
	scp->sc->blink_in_progress = 0;
    	mark_all(scp);
	tp = VIRTUAL_TTY(scp->sc, scp->index);
	if (tp->t_state & TS_ISOPEN)
	    scstart(tp);
	if (scp->sc->delayed_next_scr)
	    switch_scr(scp->sc, scp->sc->delayed_next_scr - 1);
    }
    else {
	(*scp->rndr->draw)(scp, 0, scp->xsize*scp->ysize, 
			   scp->sc->blink_in_progress & 1);
	scp->sc->blink_in_progress--;
	timeout(blink_screen, scp, hz / 10);
    }
}

#endif /* NSC */
