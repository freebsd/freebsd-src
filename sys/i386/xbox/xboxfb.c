/*-
 * Copyright (c) 2005 Rink Springer
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
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
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
 * $FreeBSD$
 */

/*
 * This will handles video output using the XBOX' frame buffer. It assumes
 * the graphics have been set up by Cromwell. This driver uses all video memory
 * to avoid expensive memcpy()'s.
 *
 * It is usuable as console (to see the initial boot) as well as for interactive
 * use. The latter is handeled using kbd_*() functionality. Keyboard hotplug is
 * fully supported, the console will periodically rescan if no keyboard was
 * found.
 *
 */
#include <sys/param.h>
#include <sys/systm.h>
#include <vm/vm_param.h>
#include <sys/kernel.h>
#include <sys/cons.h>
#include <sys/conf.h>
#include <sys/consio.h>
#include <sys/tty.h>
#include <sys/kbio.h>
#include <sys/fbio.h>
#include <dev/kbd/kbdreg.h>
#include <vm/vm.h>
#include <vm/pmap.h>
#include <machine/bus.h>
#include <machine/xbox.h>
#include <dev/fb/fbreg.h>
#include <dev/fb/gfb.h>

#define SCREEN_WIDTH	640
#define SCREEN_HEIGHT	480
#define SCREEN_BPP	4
#define SCREEN_SIZE	(SCREEN_WIDTH*SCREEN_HEIGHT*SCREEN_BPP)

/* FONT_xxx declares the dimensions of the charachter structure, CHAR_xxx is how
 * they appear on-screen. Having slightly more spacing improves readability.  */
#define FONT_HEIGHT	16
#define FONT_WIDTH	8

#define CHAR_HEIGHT	16
#define CHAR_WIDTH	10

#define RAM_SIZE	(arch_i386_xbox_memsize * 1024 * 1024)
#define FB_SIZE		(0x400000)
#define FB_START	(0xf0000000 | (RAM_SIZE - FB_SIZE))
#define FB_START_PTR	(0xFD600800)

/* colours */
#define CONSOLE_COL	0xFF88FF88	/* greenish */
#define NORM_COL	0xFFAAAAAA	/* grayish */
#define BLACK_COL	0x00000000	/* black */

static int xcon_x     = 0;
static int xcon_y     = 0;
static int xcon_yoffs = 0;

extern struct gfb_font bold8x16;

static char* xcon_map;
static int* xcon_memstartptr;

static struct tty* xboxfb_tp = NULL;
static struct keyboard* xbfb_kbd = NULL;
static int xbfb_keyboard = -1;
static d_open_t xboxfb_dev_open;
static d_close_t xboxfb_dev_close;
static int xboxfb_kbdevent(keyboard_t* thiskbd, int event, void* arg);

static struct cdevsw xboxfb_cdevsw = {
	.d_version = D_VERSION,
	.d_open    = xboxfb_dev_open,
	.d_close   = xboxfb_dev_close,
	.d_name    = "xboxfb",
	.d_flags   = D_TTY | D_NEEDGIANT,
};

static void
xcon_probe(struct consdev* cp)
{
	if (arch_i386_is_xbox)
		cp->cn_pri = CN_REMOTE;
	else
		cp->cn_pri = CN_DEAD;
}

static int
xcon_getc(struct consdev* cp)
{
	return 0;
}

static int
xcon_checkc(struct consdev* cp)
{
	return 0;
}

static void
xcon_real_putc(int basecol, int c)
{
	int i, j, ch = c, col;
	char mask;
	int* ptri = (int*)xcon_map;

	/* special control chars */
	switch (ch) {
	case '\r': /* carriage return */
		xcon_x = 0;
		return;
	case '\n': /* newline */
		xcon_y += CHAR_HEIGHT;
		goto scroll;
	case 7: /* beep */
		return;
	case 8: /* backspace */
		if (xcon_x > 0) {
			xcon_x -= CHAR_WIDTH;
		} else {
			if (xcon_y > CHAR_HEIGHT) {
				xcon_y -= CHAR_HEIGHT;
				xcon_x = (SCREEN_WIDTH - CHAR_WIDTH);
			}
		}
		return;
	case 9: /* tab */
		xcon_real_putc (basecol, ' ');
		while ((xcon_x % (8 * CHAR_WIDTH)) != 0) {
			xcon_real_putc (basecol, ' ');
		}
		return;
	}
	ptri += (xcon_y * SCREEN_WIDTH) + xcon_x;

	/* we plot the font pixel-by-pixel. bit 7 is skipped as it renders the
	 * console unreadable ... */
	for (i = 0; i < FONT_HEIGHT; i++) {
		mask = 0x40;
		for (j = 0; j < FONT_WIDTH; j++) {
			col = (bold8x16.data[(ch * FONT_HEIGHT) + i] & mask) ? basecol : BLACK_COL;
			*ptri++ = col;
			mask >>= 1;
		}
		ptri += (SCREEN_WIDTH - FONT_WIDTH);
	}

	xcon_x += CHAR_WIDTH;
	if (xcon_x >= SCREEN_WIDTH) {
		xcon_x = 0;
		xcon_y += CHAR_HEIGHT;
	}

scroll:
	if (((xcon_yoffs + CHAR_HEIGHT) * SCREEN_WIDTH * SCREEN_BPP) > (FB_SIZE - SCREEN_SIZE)) {
		/* we are about to run out of video memory, so move everything
		 * back to the beginning of the video memory */
		memcpy ((char*)xcon_map,
		        (char*)(xcon_map + (xcon_yoffs * SCREEN_WIDTH * SCREEN_BPP)),
		        SCREEN_SIZE);
		xcon_y -= xcon_yoffs; xcon_yoffs = 0;
		*xcon_memstartptr = FB_START;
	}

	/* we achieve much faster scrolling by just altering the video memory
	 * address base. once all memory is used, we return to the beginning
	 * again */
	while ((xcon_y - xcon_yoffs) >= SCREEN_HEIGHT) {
		xcon_yoffs += CHAR_HEIGHT;
		memset ((char*)(xcon_map + (xcon_y * SCREEN_WIDTH * SCREEN_BPP)), 0, CHAR_HEIGHT * SCREEN_WIDTH * SCREEN_BPP);
		*xcon_memstartptr = FB_START + (xcon_yoffs * SCREEN_WIDTH * SCREEN_BPP);
	}
}

static void
xcon_putc(struct consdev* cp, int c)
{
	xcon_real_putc (CONSOLE_COL, c);
}

static void
xcon_init(struct consdev* cp)
{
	int i;
	int* iptr;

	/* Don't init the framebuffer on non-XBOX-es */
	if (!arch_i386_is_xbox)
		return;

	/*
	 * We must make a mapping from video framebuffer memory to real. This is
	 * very crude:  we map the entire videomemory to PAGE_SIZE! Since our
	 * kernel lives at it's relocated address range (0xc0xxxxxx), it won't
	 * care.
	 *
	 * We use address PAGE_SIZE and up so we can still trap NULL pointers.
	 * Once xboxfb_drvinit() is called, the mapping will be done via the OS
	 * and stored in a more sensible location ... but since we're not fully
	 * initialized, this is our only way to go :-(
	 */
	for (i = 0; i < (FB_SIZE / PAGE_SIZE); i++) {
		pmap_kenter (((i + 1) * PAGE_SIZE), FB_START + (i * PAGE_SIZE));
	}
	pmap_kenter ((i + 1) * PAGE_SIZE, FB_START_PTR - FB_START_PTR % PAGE_SIZE);
	xcon_map = (char*)PAGE_SIZE;
	xcon_memstartptr = (int*)((i + 1) * PAGE_SIZE + FB_START_PTR % PAGE_SIZE); 

	/* clear the screen */
	iptr = (int*)xcon_map;
	for (i = 0; i < SCREEN_HEIGHT * SCREEN_WIDTH; i++)
		*iptr++ = BLACK_COL;

	sprintf(cp->cn_name, "xboxfb");
	cp->cn_tp = xboxfb_tp;
}

static void
xboxfb_timer(void* arg)
{
	int i;

	if (xbfb_kbd != NULL)
		return;

	i = kbd_allocate ("*", 0, (void*)&xbfb_keyboard, xboxfb_kbdevent, NULL);
	if (i != -1) {
		/* allocation was successfull; xboxfb_kbdevent() is called to
		 * feed the keystrokes to the tty driver */
		xbfb_kbd = kbd_get_keyboard (i);
		xbfb_keyboard = i;
		return;
	}

	/* probe again in a few */
	timeout (xboxfb_timer, NULL, hz / 10);
}

static int
xboxfb_kbdevent(keyboard_t* thiskbd, int event, void* arg)
{
	int c;

	if (event == KBDIO_UNLOADING) {
		/* keyboard was unplugged; clean up and enable probing */
		xbfb_kbd = NULL;
		xbfb_keyboard = -1;
		kbd_release (thiskbd, (void*)&xbfb_keyboard);
		timeout (xboxfb_timer, NULL, hz / 10);
		return 0;
	}

	for (;;) {
		c = (kbdsw[xbfb_kbd->kb_index])->read_char (xbfb_kbd, 0);
		if (c == NOKEY)
			return 0;

		/* only feed non-special keys to an open console */
		if (c != ERRKEY) {
			if ((KEYFLAGS(c)) == 0x0)
				if (xboxfb_tp->t_state & TS_ISOPEN)
					ttyld_rint (xboxfb_tp, KEYCHAR(c));
		}
	}

	return 0;
}

static void
xboxfb_drvinit (void* unused)
{
	struct cdev* dev;
	int i;

	/* Don't init the framebuffer on non-XBOX-es */
	if (!arch_i386_is_xbox)
		return;

	/*
	 * When this function is called, the OS is capable of doing
	 * device-memory mappings using pmap_mapdev(). Therefore, we ditch the
	 * ugly PAGE_SIZE-based mapping and ask the OS to create a decent
	 * mapping for us.
	 */
	dev = make_dev (&xboxfb_cdevsw, 0, UID_ROOT, GID_WHEEL, 0600, "%s", "xboxfb");
	xcon_map = pmap_mapdev (FB_START, FB_SIZE);
	xcon_memstartptr = (int*)pmap_mapdev (FB_START_PTR, PAGE_SIZE);
	*xcon_memstartptr = FB_START;

	/* ditch all ugly previous mappings */
	for (i = 0; i < (FB_SIZE / PAGE_SIZE); i++) {
		pmap_kremove (((i + 1) * PAGE_SIZE));
	}
	pmap_kremove (PAGE_SIZE + FB_SIZE);

	/* probe for a keyboard */
	xboxfb_timer (NULL);
}

static void
xboxfb_tty_start(struct tty* tp)
{
	struct clist* cl;
	int len, i;
	u_char buf[128];
	
	if (tp->t_state & TS_BUSY)
		return;

	/* simply feed all outstanding tty data to real_putc() */
	tp->t_state |= TS_BUSY;
	cl = &tp->t_outq;
	len = q_to_b(cl, buf, 128);
	for (i = 0; i < len; i++)
		xcon_real_putc(NORM_COL, buf[i]);
	tp->t_state &= ~TS_BUSY;
}

static void
xboxfb_tty_stop(struct tty* tp, int flag) {
	if (tp->t_state & TS_BUSY)
		if ((tp->t_state & TS_TTSTOP) == 0)
			tp->t_state |= TS_FLUSH;
}

static int
xboxfb_tty_param(struct tty* tp, struct termios* t)
{
	return 0;
}

static int
xboxfb_dev_open(struct cdev* dev, int flag, int mode, struct thread* td)
{
	struct tty* tp;

	tp = xboxfb_tp = dev->si_tty = ttymalloc (xboxfb_tp);

	tp->t_oproc = xboxfb_tty_start;
	tp->t_param = xboxfb_tty_param;
	tp->t_stop = xboxfb_tty_stop;
	tp->t_dev = dev;

	if ((tp->t_state & TS_ISOPEN) == 0) {
		tp->t_state |= TS_CARR_ON;
		ttychars(tp);
		tp->t_iflag = TTYDEF_IFLAG;
		tp->t_oflag = TTYDEF_OFLAG;
		tp->t_cflag = TTYDEF_CFLAG | CLOCAL;
		tp->t_lflag = TTYDEF_LFLAG;
		tp->t_ispeed = tp->t_ospeed = TTYDEF_SPEED;
		ttsetwater(tp);
	}

	return ttyld_open (tp, dev);
}

static int
xboxfb_dev_close(struct cdev* dev, int flag, int mode, struct thread* td)
{
	struct tty* tp;

	tp = xboxfb_tp;
	ttyld_close (tp, flag);
	tty_close(tp);
	return 0;
}

SYSINIT(xboxfbdev, SI_SUB_CONFIGURE, SI_ORDER_MIDDLE, xboxfb_drvinit, NULL)

CONS_DRIVER(xcon, xcon_probe, xcon_init, NULL, xcon_getc, xcon_checkc, xcon_putc, NULL);
