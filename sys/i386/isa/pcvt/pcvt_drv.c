/*
 * Copyright (c) 1999, 2000 Hellmuth Michaelis
 *
 * Copyright (c) 1992, 1995 Hellmuth Michaelis and Joerg Wunsch.
 *
 * Copyright (c) 1992, 1993 Brian Dunford-Shore and Scott Turner.
 *
 * Copyright (c) 1993 Charles Hannum.
 *
 * All rights reserved.
 *
 * Parts of this code regarding the NetBSD interface were written
 * by Charles Hannum.
 * 
 * This code is derived from software contributed to Berkeley by
 * William Jolitz and Don Ahn.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by
 *	Hellmuth Michaelis, Brian Dunford-Shore, Joerg Wunsch, Scott Turner
 *	and Charles Hannum.
 * 4. The name authors may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*---------------------------------------------------------------------------*
 *
 *	pcvt_drv.c	VT220 Driver Main Module / OS - Interface
 *	---------------------------------------------------------
 *
 *	Last Edit-Date: [Sat Jul 15 15:06:06 2000]
 *
 * $FreeBSD$
 *
 *---------------------------------------------------------------------------*/

#define MAIN
#include <i386/isa/pcvt/pcvt_hdr.h>
#undef MAIN


#include <sys/resource.h>
#include <sys/bus.h>
#include <sys/rman.h>

#include <machine/resource.h>
#include <machine/bus.h>

#include <isa/isareg.h>
#include <isa/isavar.h>

static kbd_callback_func_t pcvt_event;
static int pcvt_kbd_wptr = 0;
static u_char pcvt_timeout_scheduled = 0;

static void vgapelinit(void);
static void detect_kbd(void *arg);
static void pcvt_start(register struct tty *tp);
static int pcvt_param(struct tty *tp, struct termios *t);

static cn_probe_t	pcvt_cn_probe;
static cn_init_t	pcvt_cn_init;
static cn_term_t	pcvt_cn_term;
static cn_getc_t	pcvt_cn_getc;
static cn_checkc_t	pcvt_cn_checkc;
static cn_putc_t	pcvt_cn_putc;

CONS_DRIVER(vt, pcvt_cn_probe, pcvt_cn_init, pcvt_cn_term, pcvt_cn_getc,
		pcvt_cn_checkc, pcvt_cn_putc, NULL);

static	d_open_t	pcvt_open;
static	d_close_t	pcvt_close;
static	d_ioctl_t	pcvt_ioctl;
static	d_mmap_t	pcvt_mmap;

#define	CDEV_MAJOR	12

static struct cdevsw vt_cdevsw = {
	/* open */	pcvt_open,
	/* close */	pcvt_close,
	/* read */	ttyread,
	/* write */	ttywrite,
	/* ioctl */	pcvt_ioctl,
	/* poll */	ttypoll,
	/* mmap */	pcvt_mmap,
	/* strategy */	nostrategy,
	/* name */	"vt",
	/* maj */	CDEV_MAJOR,
	/* dump */	nodump,
	/* psize */	nopsize,
	/* flags */	D_TTY | D_KQFILTER,
	/* kqfilter */	ttykqfilter,
};

static int pcvt_probe(device_t dev);
static int pcvt_attach(device_t dev);
static void pcvt_identify (driver_t *driver, device_t parent);

static device_method_t pcvt_methods[] = {
	DEVMETHOD(device_identify,	pcvt_identify),
	DEVMETHOD(device_probe,		pcvt_probe),
	DEVMETHOD(device_attach,	pcvt_attach),
	DEVMETHOD(bus_print_child,	bus_generic_print_child),
	{ 0, 0 }
};

static driver_t pcvt_driver = {
	"vt",
	pcvt_methods,
	0
};

static devclass_t pcvt_devclass;

DRIVER_MODULE(pcvt, isa, pcvt_driver, pcvt_devclass, 0, 0);

/*---------------------------------------------------------------------------*
 *	driver identify
 *---------------------------------------------------------------------------*/
static void
pcvt_identify (driver_t *driver, device_t parent)
{
	BUS_ADD_CHILD(parent, ISA_ORDER_SPECULATIVE, "vt", 0);
}

/*---------------------------------------------------------------------------*
 *	driver probe
 *---------------------------------------------------------------------------*/
static int
pcvt_probe(device_t dev)
{
	int i;
	device_t bus;
	
	int unit = device_get_unit(dev);

	/* No pnp support */
	if(isa_get_vendorid(dev))
		return ENXIO;

	if(unit != 0)
		return ENXIO;	
	
	device_set_desc(dev, "pcvt VT220 console driver");
	
	bus = device_get_parent(dev);
	bus_set_resource(dev, SYS_RES_IOPORT, 0, 0x3b0, 0x30);
	bus_set_resource(dev, SYS_RES_MEMORY, 0, (u_long) Crtat, 0x8000);

	if (kbd == NULL)
	{
		reset_keyboard = 0;
		kbd_configure(KB_CONF_PROBE_ONLY);
		i = kbd_allocate("*", -1, (void *)&kbd, pcvt_event, (void *)&vs[unit]);
		if ((i < 0) || ((kbd = kbd_get_keyboard(i)) == NULL))
			return 0;
	}
	reset_keyboard = 1;		/* it's now safe to do kbd reset */

	kbd_code_init();

	return 0;
}

/*---------------------------------------------------------------------------*
 *	driver attach
 *---------------------------------------------------------------------------*/
static int
pcvt_attach(device_t dev)
{
	int i;
        struct resource *port;
        struct resource *mem;
                
	int unit = device_get_unit(dev);
	
	if(unit != 0)
		return ENXIO;	

        i = 0;
	port = bus_alloc_resource(dev, SYS_RES_IOPORT, &i, 0, ~0, 0,
					RF_ACTIVE | RF_SHAREABLE);

	i = 0;
	mem = bus_alloc_resource(dev, SYS_RES_MEMORY, &i, 0, ~0, 0,
					RF_ACTIVE | RF_SHAREABLE);

	vt_coldmalloc();		/* allocate memory for screens */

	if (kbd == NULL)
		timeout(detect_kbd, (void *)&vs[unit], hz*2);

	printf("vt%d: ", unit);

	switch(adaptor_type)
	{
		case MDA_ADAPTOR:
			printf("MDA");
			break;

		case CGA_ADAPTOR:
			printf("CGA");
			break;

		case EGA_ADAPTOR:
			printf("EGA");
			break;

		case VGA_ADAPTOR:
			printf("%s VGA, ", (char *)vga_string(vga_type));
			if(can_do_132col)
				printf("80/132 columns");
			else
				printf("80 columns");
			vgapelinit();
			break;

		default:
			printf("unknown");
			break;
	}

	if(color == 0)
		printf(", mono");
	else
		printf(", color");

	printf(", %d screens, ", totalscreens);

	switch(keyboard_type)
	{
		case KB_AT:
			printf("AT-");
			break;

		case KB_MFII:
			printf("MF2-");
			break;

		default:
			printf("unknown ");
			break;
	}

	printf("keyboard\n");

	for(i = 0; i < totalscreens; i++)
	{
		ttyregister(&pcvt_tty[i]);
		vs[i].vs_tty = &pcvt_tty[i];
		make_dev(&vt_cdevsw, i, UID_ROOT, GID_WHEEL, 0600, "ttyv%r", i);
	}

	async_update(UPDATE_START);	/* start asynchronous updates */

	return 0;
}

/*---------------------------------------------------------------------------*
 *	driver open
 *---------------------------------------------------------------------------*/
static int
pcvt_open(dev_t dev, int flag, int mode, struct thread *td)
{
	register struct tty *tp;
	register struct video_state *vsx;
	int s, retval;
	int winsz = 0;
	int i = minor(dev);

	vsx = &vs[i];

	if(i >= PCVT_NSCREENS)
		return ENXIO;

	tp = &pcvt_tty[i];

	dev->si_tty = tp;

	vsx->openf++;

	tp->t_oproc = pcvt_start;
	tp->t_param = pcvt_param;
	tp->t_stop = nottystop;
	tp->t_dev = dev;

	if ((tp->t_state & TS_ISOPEN) == 0)
	{
		ttychars(tp);
		tp->t_iflag = TTYDEF_IFLAG;
		tp->t_oflag = TTYDEF_OFLAG;
		tp->t_cflag = TTYDEF_CFLAG;
		tp->t_lflag = TTYDEF_LFLAG;
		tp->t_ispeed = tp->t_ospeed = TTYDEF_SPEED;
		pcvt_param(tp, &tp->t_termios);
		(*linesw[tp->t_line].l_modem)(tp, 1);	/* fake connection */
		winsz = 1;			/* set winsize later */
	}
	else if (tp->t_state & TS_XCLUDE && suser(td))
	{
		return (EBUSY);
	}

	retval = ((*linesw[tp->t_line].l_open)(dev, tp));

	if(winsz == 1)
	{
		/*
		 * The line discipline has clobbered t_winsize if TS_ISOPEN
	         * was clear. (NetBSD PR #400 from Bill Sommerfeld)
	         * We have to do this after calling the open routine, because
	         * it does some other things in other/older *BSD releases -hm
		 */

		s = spltty();

		tp->t_winsize.ws_col = vsx->maxcol;
		tp->t_winsize.ws_row = vsx->screen_rows;
		tp->t_winsize.ws_xpixel = (vsx->maxcol == 80)? 720: 1056;
		tp->t_winsize.ws_ypixel = 400;

		splx(s);
	}
	return(retval);
}

/*---------------------------------------------------------------------------*
 *	driver close
 *---------------------------------------------------------------------------*/
static int
pcvt_close(dev_t dev, int flag, int mode, struct thread *td)
{
	register struct tty *tp;
	register struct video_state *vsx;
	int i = minor(dev);

	vsx = &vs[i];

	if(i >= PCVT_NSCREENS)
		return ENXIO;

	tp = &pcvt_tty[i];

	(*linesw[tp->t_line].l_close)(tp, flag);

	ttyclose(tp);

	vsx->openf = 0;

#ifdef XSERVER
	reset_usl_modes(vsx);
#endif /* XSERVER */

	return(0);
}

/*---------------------------------------------------------------------------*
 *	driver ioctl
 *---------------------------------------------------------------------------*/
static int
pcvt_ioctl(dev_t dev, u_long cmd, caddr_t data, int flag, struct thread *td)
{
	register int error;
	register struct tty *tp;
	int i = minor(dev);
	
	if(i >= PCVT_NSCREENS)
		return ENXIO;

	tp = &pcvt_tty[i];

	/* note that some ioctl's are global, e.g.  KBSTPMAT: There is
	 * only one keyboard and different repeat rates for instance between
	 * sessions are a suspicious wish. If you really need this make the
	 * appropriate variables arrays
	 */

#ifdef XSERVER
	if((error = usl_vt_ioctl(dev, cmd, data, flag, td)) >= 0)
		return error;
#endif /* XSERVER */

	if((error = kbdioctl(dev,cmd,data,flag)) >= 0)
		return error;

	if((error = vgaioctl(dev,cmd,data,flag)) >= 0)
		return error;

	if((error = (*linesw[tp->t_line].l_ioctl)(tp, cmd, data, flag, td))
	    != ENOIOCTL)
		return (error);

	if((error = ttioctl(tp, cmd, data, flag)) != ENOIOCTL)
		return (error);

	return (ENOTTY);
}

/*---------------------------------------------------------------------------*
 *	driver mmap
 *---------------------------------------------------------------------------*/
static int
pcvt_mmap(dev_t dev, vm_offset_t offset, int nprot)
{
	if (offset > 0x20000 - PAGE_SIZE)
		return -1;
	return i386_btop((0xa0000 + offset));
}

/*---------------------------------------------------------------------------*
 *	timeout handler
 *---------------------------------------------------------------------------*/
static void
pcvt_timeout(void *arg)
{
	u_char *cp;

#if PCVT_SLOW_INTERRUPT
	int	s;
#endif

	pcvt_timeout_scheduled = 0;

#if PCVT_SCREENSAVER
	pcvt_scrnsv_reset();
#endif /* PCVT_SCREENSAVER */

	while (pcvt_kbd_count)
	{
		if (((cp = sgetc(1)) != 0) &&
		    (vs[current_video_screen].openf))
		{

#if PCVT_NULLCHARS
			if(*cp == '\0')
			{
				/* pass a NULL character */
				(*linesw[pcvt_ttyp->t_line].l_rint)('\0', pcvt_ttyp);
			}
/* XXX */		else
#endif /* PCVT_NULLCHARS */

			while (*cp)
				(*linesw[pcvt_ttyp->t_line].l_rint)(*cp++ & 0xff, pcvt_ttyp);
		}

		PCVT_DISABLE_INTR ();

		if (!pcvt_kbd_count)
			pcvt_timeout_scheduled = 0;

		PCVT_ENABLE_INTR ();
	}

	return;
}

/*---------------------------------------------------------------------------*
 *	check for keyboard
 *---------------------------------------------------------------------------*/
static void
detect_kbd(void *arg)
{
	int unit = (int)arg;
	int i;

	if (kbd != NULL)
		return;
	i = kbd_allocate("*", -1, (void *)&kbd, pcvt_event, (void *)unit);
	if (i >= 0)
		kbd = kbd_get_keyboard(i);
	if (kbd != NULL)
	{
		reset_keyboard = 1;	/* ok to reset the keyboard */
		kbd_code_init();
		return;
	}
	reset_keyboard = 0;
	timeout(detect_kbd, (void *)unit, hz*2);
}

/*---------------------------------------------------------------------------*
 *	keyboard event handler
 *---------------------------------------------------------------------------*/
static int
pcvt_event(keyboard_t *thiskbd, int event, void *arg)
{
	int unit = (int)arg;

	if (thiskbd != kbd)
		return EINVAL;		/* shouldn't happen */

	switch (event) {
	case KBDIO_KEYINPUT:
		pcvt_rint(unit);
		return 0;
	case KBDIO_UNLOADING:
		reset_keyboard = 0;
		kbd = NULL;
		kbd_release(thiskbd, (void *)&kbd);
		timeout(detect_kbd, (void *)unit, hz*4);
		return 0;
	default:
		return EINVAL;
	}
}

/*---------------------------------------------------------------------------*
 *	(keyboard) interrupt handler
 *---------------------------------------------------------------------------*/
void
pcvt_rint(int unit)
{
	u_char	dt;
	u_char	ret = -1;
	int	c;
	
# if PCVT_SLOW_INTERRUPT
	int	s;
# endif

#if PCVT_SCREENSAVER
	pcvt_scrnsv_reset();
#endif /* PCVT_SCREENSAVER */

	if (kbd_polling)
	{
		sgetc(1);
		return;
	}

	while ((c = (*kbdsw[kbd->kb_index]->read)(kbd, FALSE)) != -1)
	{
		ret = 1;				/* got something */
		dt = c;

		if (pcvt_kbd_count >= PCVT_KBD_FIFO_SZ)	/* fifo overflow ? */
		{
			log (LOG_WARNING, "pcvt: keyboard buffer overflow\n");
		}
		else
		{
			pcvt_kbd_fifo[pcvt_kbd_wptr++] = dt; /* data -> fifo */

			PCVT_DISABLE_INTR ();	/* XXX necessary ? */
			pcvt_kbd_count++;		/* update fifo count */
			PCVT_ENABLE_INTR ();

			if (pcvt_kbd_wptr >= PCVT_KBD_FIFO_SZ)
				pcvt_kbd_wptr = 0;	/* wraparound pointer */
		}
	}

	if (ret == 1)	/* got data from keyboard ? */
	{
		if (!pcvt_timeout_scheduled)	/* if not already active .. */
		{
			PCVT_DISABLE_INTR ();
			pcvt_timeout_scheduled = 1;	/* flag active */
			timeout(pcvt_timeout, NULL, hz / 100);	/* fire off */
			PCVT_ENABLE_INTR ();
		}
	}
}

/*---------------------------------------------------------------------------*
 *	start output
 *---------------------------------------------------------------------------*/
static void
pcvt_start(register struct tty *tp)
{
	register struct clist *rbp;
	int s, len;
	u_char buf[PCVT_PCBURST];

	s = spltty();

	if (tp->t_state & (TS_TIMEOUT|TS_BUSY|TS_TTSTOP))
		goto out;

	tp->t_state |= TS_BUSY;

	splx(s);

	async_update(UPDATE_KERN);

	rbp = &tp->t_outq;

	/*
	 * Call q_to_b() at spltty() to ensure that the queue is empty when
	 * the loop terminates.
	 */

	s = spltty();

	while((len = q_to_b(rbp, buf, PCVT_PCBURST)) > 0)
	{
		if(vs[minor(tp->t_dev)].scrolling)
			sgetc(SCROLLBACK_COOKIE);
		
		/*
		 * We need to do this outside spl since it could be fairly
		 * expensive and we don't want our serial ports to overflow.
		 */
		splx(s);
		sput(&buf[0], 0, len, minor(tp->t_dev));
		s = spltty();
	}

	tp->t_state &= ~TS_BUSY;

	ttwwakeup(tp);

out:
	splx(s);
}

/*---------------------------------------------------------------------------*
 *	console probe
 *---------------------------------------------------------------------------*/
static void
pcvt_cn_probe(struct consdev *cp)
{
	int unit = 0;
	int i;

	/* See if this driver is disabled in probe hint. */ 
	if (resource_int_value("vt", unit, "disabled", &i) == 0 && i)
	{
		cp->cn_pri = CN_DEAD;
		return;
	}

	kbd_configure(KB_CONF_PROBE_ONLY);

	if (kbd_find_keyboard("*", unit) < 0)
	{
		cp->cn_pri = CN_DEAD;
		return;
	}

	/* initialize required fields */

	cp->cn_dev = makedev(CDEV_MAJOR, 0);
	cp->cn_pri = CN_INTERNAL;
	cp->cn_tp = &pcvt_tty[0];
}

/*---------------------------------------------------------------------------*
 *	console init
 *---------------------------------------------------------------------------*/
static void
pcvt_cn_init(struct consdev *cp)
{
	int unit = 0;
	int i;

	pcvt_is_console = 1;

	/*
	 * Don't reset the keyboard via `kbdio' just yet.
	 * The system clock has not been calibrated...
	 */
	reset_keyboard = 0;

	if (kbd)
	{
		kbd_release(kbd, (void *)&kbd);
		kbd = NULL;
	}

	i = kbd_allocate("*", -1, (void *)&kbd, pcvt_event, (void *)unit);

	if (i >= 0)
		kbd = kbd_get_keyboard(i);

#if PCVT_SCANSET == 2
	/*
	 * Turn off scancode translation early so that UserConfig 
	 * and DDB can read the keyboard.
	 */
	if (kbd)
	{
		empty_both_buffers(*(KBDC *)kbd->kb_data, 10);
		set_controller_command_byte(*(KBDC *)kbd->kb_data,
					    KBD_TRANSLATION, 0);
	}
#endif /* PCVT_SCANSET == 2 */
}

/*---------------------------------------------------------------------------*
 *	console finish
 *---------------------------------------------------------------------------*/
static void
pcvt_cn_term(struct consdev *cp)
{
	if (kbd)
	{
		kbd_release(kbd, (void *)&kbd);
		kbd = NULL;
	}
}

/*---------------------------------------------------------------------------*
 *	console put char
 *---------------------------------------------------------------------------*/
static void
pcvt_cn_putc(dev_t dev, int c)
{
	if (c == '\n')
		sput("\r", 1, 1, 0);

	sput((char *) &c, 1, 1, 0);

 	async_update(UPDATE_KERN);
}

/*---------------------------------------------------------------------------*
 *	console get char
 *---------------------------------------------------------------------------*/
static int
pcvt_cn_getc(dev_t dev)
{
	register int s;
	static u_char *cp, cbuf[4]; /* Temp buf for multi-char key sequence. */
	register u_char c;

#ifdef XSERVER
	if (pcvt_kbd_raw)
		return 0;
#endif /* XSERVER */

	if (cp && *cp)
	{
		/*
		 * We still have a pending key sequence, e.g.
		 * from an arrow key.  Deliver this one first.
		 */
		return (*cp++);
	}

	if (kbd == NULL)
		return 0;

	s = spltty();		/* block pcvt_rint while we poll */
	kbd_polling = 1;
	(*kbdsw[kbd->kb_index]->enable)(kbd);
	cp = sgetc(0);
	(*kbdsw[kbd->kb_index]->disable)(kbd);
	kbd_polling = 0;
	splx(s);

	c = *cp++;

	if (c && *cp)
	{
		/* Preserve the multi-char sequence for the next call. */
		bcopy(cp, cbuf, 3); /* take care for a trailing '\0' */
		cp = cbuf;
	}
	else
	{
		cp = 0;
	}
	return c;
}

/*---------------------------------------------------------------------------*
 *	console check for char
 *---------------------------------------------------------------------------*/
static int
pcvt_cn_checkc(dev_t dev)
{
	char *cp;
	int x;

	if (kbd == NULL)
		return 0;

	x = spltty();
	kbd_polling = 1;
	(*kbdsw[kbd->kb_index]->enable)(kbd);
	cp = sgetc(1);
	(*kbdsw[kbd->kb_index]->disable)(kbd);
	kbd_polling = 0;
	splx(x);

	return (cp == NULL ? -1 : *cp);
}

/*---------------------------------------------------------------------------*
 *	Set line parameters
 *---------------------------------------------------------------------------*/
static int
pcvt_param(struct tty *tp, struct termios *t)
{
        tp->t_ispeed = t->c_ispeed;
        tp->t_ospeed = t->c_ospeed;
        tp->t_cflag  = t->c_cflag;

	return(0);
}

/*----------------------------------------------------------------------*
 *	read initial VGA palette (as stored by VGA ROM BIOS) into
 *	palette save area
 *----------------------------------------------------------------------*/
static void
vgapelinit(void)
{
	register unsigned idx;
	register struct rgb *val;

	/* first, read all and store to first screen's save buffer */
	for(idx = 0, val = vs[0].palette; idx < NVGAPEL; idx++, val++)
		vgapaletteio(idx, val, 0 /* read it */);

	/* now, duplicate for remaining screens */
	for(idx = 1; idx < PCVT_NSCREENS; idx++)
		bcopy(vs[0].palette, vs[idx].palette,
		      NVGAPEL * sizeof(struct rgb));
}

/*-------------------------- E O F -------------------------------------*/
