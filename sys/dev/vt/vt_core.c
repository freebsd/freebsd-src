/*-
 * Copyright (c) 2009 The FreeBSD Foundation
 * All rights reserved.
 *
 * This software was developed by Ed Schouten under sponsorship from the
 * FreeBSD Foundation.
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/consio.h>
#include <sys/eventhandler.h>
#include <sys/fbio.h>
#include <sys/kbio.h>
#include <sys/kdb.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/reboot.h>
#include <sys/systm.h>
#include <sys/terminal.h>

#include <dev/kbd/kbdreg.h>
#include <dev/vt/vt.h>

static tc_bell_t	vtterm_bell;
static tc_cursor_t	vtterm_cursor;
static tc_putchar_t	vtterm_putchar;
static tc_fill_t	vtterm_fill;
static tc_copy_t	vtterm_copy;
static tc_param_t	vtterm_param;
static tc_done_t	vtterm_done;

static tc_cnprobe_t	vtterm_cnprobe;
static tc_cngetc_t	vtterm_cngetc;

static tc_opened_t	vtterm_opened;
static tc_ioctl_t	vtterm_ioctl;

const struct terminal_class vt_termclass = {
	.tc_bell	= vtterm_bell,
	.tc_cursor	= vtterm_cursor,
	.tc_putchar	= vtterm_putchar,
	.tc_fill	= vtterm_fill,
	.tc_copy	= vtterm_copy,
	.tc_param	= vtterm_param,
	.tc_done	= vtterm_done,

	.tc_cnprobe	= vtterm_cnprobe,
	.tc_cngetc	= vtterm_cngetc,

	.tc_opened	= vtterm_opened,
	.tc_ioctl	= vtterm_ioctl,
};

/*
 * Use a constant timer of 25 Hz to redraw the screen.
 *
 * XXX: In theory we should only fire up the timer when there is really
 * activity. Unfortunately we cannot always start timers. We really
 * don't want to process kernel messages synchronously, because it
 * really slows down the system.
 */
#define	VT_TIMERFREQ	25

/* Bell pitch/duration. */
#define VT_BELLDURATION	((5 * hz + 99) / 100)
#define VT_BELLPITCH	800

#define	VT_LOCK(vd)	mtx_lock(&(vd)->vd_lock)
#define	VT_UNLOCK(vd)	mtx_unlock(&(vd)->vd_lock)

#define	VT_UNIT(vw)	((vw)->vw_device->vd_unit * VT_MAXWINDOWS + \
			(vw)->vw_number)

static unsigned int vt_unit = 0;
static MALLOC_DEFINE(M_VT, "vt", "vt device");

/* Boot logo. */
extern unsigned int vt_logo_width;
extern unsigned int vt_logo_height;
extern unsigned int vt_logo_depth;
extern unsigned char vt_logo_image[];

/* Font. */
extern struct vt_font vt_font_default;

static void
vt_window_switch(struct vt_window *vw)
{
	struct vt_device *vd = vw->vw_device;
	keyboard_t *kbd;

	VT_LOCK(vd);
	if (vd->vd_curwindow == vw ||
	    !(vw->vw_flags & (VWF_OPENED|VWF_CONSOLE))) {
		VT_UNLOCK(vd);
		return;
	}
	vd->vd_curwindow = vw;
	vd->vd_flags |= VDF_INVALID;
	cv_broadcast(&vd->vd_winswitch);
	VT_UNLOCK(vd);

	/* Restore per-window keyboard mode. */
	mtx_lock(&Giant);
	kbd = kbd_get_keyboard(vd->vd_keyboard);
	if (kbd != NULL)
		kbdd_ioctl(kbd, KDSKBMODE, (void *)&vw->vw_kbdmode);
	mtx_unlock(&Giant);
}

static inline void
vt_termsize(struct vt_device *vd, struct vt_font *vf, term_pos_t *size)
{

	size->tp_row = vd->vd_height;
	size->tp_col = vd->vd_width;
	if (vf != NULL) {
		size->tp_row /= vf->vf_height;
		size->tp_col /= vf->vf_width;
	}
}

static inline void
vt_winsize(struct vt_device *vd, struct vt_font *vf, struct winsize *size)
{

	size->ws_row = size->ws_ypixel = vd->vd_height;
	size->ws_col = size->ws_xpixel = vd->vd_width;
	if (vf != NULL) {
		size->ws_row /= vf->vf_height;
		size->ws_col /= vf->vf_width;
	}
}

static int
vt_kbdevent(keyboard_t *kbd, int event, void *arg)
{
	struct vt_device *vd = arg;
	struct vt_window *vw = vd->vd_curwindow;
	u_int c;

	switch (event) {
	case KBDIO_KEYINPUT:
		break;
	case KBDIO_UNLOADING:
		mtx_lock(&Giant);
		vd->vd_keyboard = -1;
		kbd_release(kbd, (void *)&vd->vd_keyboard);
		mtx_unlock(&Giant);
		return (0);
	default:
		return (EINVAL);
	}

	c = kbdd_read_char(kbd, 0);
	if (c & RELKEY)
		return (0);

	if (c & SPCLKEY) {
		c &= ~SPCLKEY;

		if (c >= F_SCR && c <= MIN(L_SCR, F_SCR + VT_MAXWINDOWS - 1)) {
			vw = vd->vd_windows[c - F_SCR];
			if (vw != NULL)
				vt_window_switch(vw);
			return (0);
		}

		switch (c) {
		case DBG:
			kdb_enter(KDB_WHY_BREAK, "manual escape to debugger");
			break;
		case RBT:
			/* XXX: Make this configurable! */
			shutdown_nice(0);
			break;
		case HALT:
			shutdown_nice(RB_HALT);
			break;
		case PDWN:
			shutdown_nice(RB_HALT|RB_POWEROFF);
			break;
		case SLK: {
			int state = 0;

			kbdd_ioctl(kbd, KDGKBSTATE, (caddr_t)&state);
			VT_LOCK(vd);
			if (state & SLKED) {
				/* Turn scrolling on. */
				vw->vw_flags |= VWF_SCROLL;
			} else {
				/* Turn scrolling off. */
				vw->vw_flags &= ~VWF_SCROLL;
				vthistory_seek(&vw->vw_history, 0, VHS_SET);
			}
			VT_UNLOCK(vd);
			break;
		}
		case FKEY | F(1):  case FKEY | F(2):  case FKEY | F(3):
		case FKEY | F(4):  case FKEY | F(5):  case FKEY | F(6):
		case FKEY | F(7):  case FKEY | F(8):  case FKEY | F(9):
		case FKEY | F(10): case FKEY | F(11): case FKEY | F(12):
			/* F1 through F12 keys. */
			terminal_input_special(vw->vw_terminal,
			    TKEY_F1 + c - (FKEY | F(1)));
			break;
		case FKEY | F(49): /* Home key. */
			VT_LOCK(vd);
			if (vw->vw_flags & VWF_SCROLL) {
				vthistory_seek(&vw->vw_history, 0, VHS_END);
				VT_UNLOCK(vd);
				break;
			}
			VT_UNLOCK(vd);
			terminal_input_special(vw->vw_terminal, TKEY_HOME);
			break;
		case FKEY | F(50): /* Arrow up. */
			VT_LOCK(vd);
			if (vw->vw_flags & VWF_SCROLL) {
				vthistory_seek(&vw->vw_history, 1, VHS_CUR);
				VT_UNLOCK(vd);
				break;
			}
			VT_UNLOCK(vd);
			terminal_input_special(vw->vw_terminal, TKEY_UP);
			break;
		case FKEY | F(51): /* Page up. */
			VT_LOCK(vd);
			if (vw->vw_flags & VWF_SCROLL) {
				term_pos_t size;

				vt_termsize(vd, vw->vw_font, &size);
				vthistory_seek(&vw->vw_history, size.tp_row,
				    VHS_CUR);
				VT_UNLOCK(vd);
				break;
			}
			VT_UNLOCK(vd);
			terminal_input_special(vw->vw_terminal, TKEY_PAGE_UP);
			break;
		case FKEY | F(53): /* Arrow left. */
			terminal_input_special(vw->vw_terminal, TKEY_LEFT);
			break;
		case FKEY | F(55): /* Arrow right. */
			terminal_input_special(vw->vw_terminal, TKEY_RIGHT);
			break;
		case FKEY | F(57): /* End key. */
			VT_LOCK(vd);
			if (vw->vw_flags & VWF_SCROLL) {
				vthistory_seek(&vw->vw_history, 0, VHS_SET);
				VT_UNLOCK(vd);
				break;
			}
			VT_UNLOCK(vd);
			terminal_input_special(vw->vw_terminal, TKEY_END);
			break;
		case FKEY | F(58): /* Arrow down. */
			VT_LOCK(vd);
			if (vw->vw_flags & VWF_SCROLL) {
				vthistory_seek(&vw->vw_history, -1, VHS_CUR);
				VT_UNLOCK(vd);
				break;
			}
			VT_UNLOCK(vd);
			terminal_input_special(vw->vw_terminal, TKEY_DOWN);
			break;
		case FKEY | F(59): /* Page down. */
			VT_LOCK(vd);
			if (vw->vw_flags & VWF_SCROLL) {
				term_pos_t size;

				vt_termsize(vd, vw->vw_font, &size);
				vthistory_seek(&vw->vw_history, -size.tp_row,
				    VHS_CUR);
				VT_UNLOCK(vd);
				break;
			}
			VT_UNLOCK(vd);
			terminal_input_special(vw->vw_terminal, TKEY_PAGE_DOWN);
			break;
		case FKEY | F(60): /* Insert key. */
			terminal_input_special(vw->vw_terminal, TKEY_INSERT);
			break;
		case FKEY | F(61): /* Delete key. */
			terminal_input_special(vw->vw_terminal, TKEY_DELETE);
			break;
		}
	} else if (KEYFLAGS(c) == 0) {
		c = KEYCHAR(c);

		/* Don't do UTF-8 conversion when doing raw mode. */
		if (vw->vw_kbdmode == K_XLATE)
			terminal_input_char(vw->vw_terminal, c);
		else
			terminal_input_raw(vw->vw_terminal, c);
	}
	
	return (0);
}

static int
vt_allocate_keyboard(struct vt_device *vd)
{
	int		 idx0, idx;
	keyboard_t	*k0, *k;
	keyboard_info_t	 ki;

	idx0 = kbd_allocate("kbdmux", -1, (void *)&vd->vd_keyboard,
	    vt_kbdevent, vd);
	if (idx0 != -1) {
		k0 = kbd_get_keyboard(idx0);

		for (idx = kbd_find_keyboard2("*", -1, 0);
		     idx != -1;
		     idx = kbd_find_keyboard2("*", -1, idx + 1)) {
			k = kbd_get_keyboard(idx);

			if (idx == idx0 || KBD_IS_BUSY(k))
				continue;

			bzero(&ki, sizeof(ki));
			strcpy(ki.kb_name, k->kb_name);
			ki.kb_unit = k->kb_unit;

			kbdd_ioctl(k0, KBADDKBD, (caddr_t) &ki);
		}
	} else
		idx0 = kbd_allocate("*", -1, (void *)&vd->vd_keyboard,
		    vt_kbdevent, vd);

	return (idx0);
}

static void
vtterm_bell(struct terminal *tm)
{

	sysbeep(1193182 / VT_BELLPITCH, VT_BELLDURATION);
}

static void
vtterm_cursor(struct terminal *tm, const term_pos_t *p)
{
	struct vt_window *vw = tm->tm_softc;

	vtbuf_cursor_position(&vw->vw_buf, p);
}

static void
vtterm_putchar(struct terminal *tm, const term_pos_t *p, term_char_t c)
{
	struct vt_window *vw = tm->tm_softc;

	vtbuf_putchar(&vw->vw_buf, p, c);
}

static void
vtterm_fill(struct terminal *tm, const term_rect_t *r, term_char_t c)
{
	struct vt_window *vw = tm->tm_softc;

	vtbuf_fill(&vw->vw_buf, r, c);
}

static void
vtterm_copy(struct terminal *tm, const term_rect_t *r,
    const term_pos_t *p)
{
	struct vt_window *vw = tm->tm_softc;
	struct vt_device *vd = vw->vw_device;
	term_pos_t size;

	/*
	 * We copy lines into the history buffer when we have to do a
	 * copy of the entire width of the screen to a region above it.
	 */
	vt_termsize(vd, vw->vw_font, &size);
	if (r->tr_begin.tp_row > p->tp_row &&
	    r->tr_begin.tp_col == 0 && r->tr_end.tp_col == size.tp_col) {
		term_rect_t area;

		area.tr_begin.tp_row = p->tp_row;
		area.tr_begin.tp_col = 0;
		area.tr_end.tp_row = r->tr_begin.tp_row;
		area.tr_end.tp_col = size.tp_col;
		vthistory_add(&vw->vw_history, &vw->vw_buf, &area);
	}

	vtbuf_copy(&vw->vw_buf, r, p);
}

static void
vtterm_param(struct terminal *tm, int cmd, unsigned int arg)
{
	struct vt_window *vw = tm->tm_softc;

	switch (cmd) {
	case TP_SHOWCURSOR:
		vtbuf_cursor_visibility(&vw->vw_buf, arg);
		break;
	}
}

static inline void
vt_determine_colors(term_char_t c, int cursor,
    term_color_t *fg, term_color_t *bg)
{

	*fg = TCHAR_FGCOLOR(c);
	if (TCHAR_FORMAT(c) & TF_BOLD)
		*fg = TCOLOR_LIGHT(*fg);
	*bg = TCHAR_BGCOLOR(c);

	if (TCHAR_FORMAT(c) & TF_REVERSE) {
		term_color_t tmp;

		tmp = *fg;
		*fg = *bg;
		*bg = tmp;
	}

	if (cursor) {
		*fg = *bg;
		*bg = TC_WHITE;
	}
}

static void
vt_bitblt_char(struct vt_device *vd, struct vt_font *vf, term_char_t c,
    int iscursor, unsigned int row, unsigned int col)
{
	term_color_t fg, bg;

	vt_determine_colors(c, iscursor, &fg, &bg);

	if (vf != NULL) {
		const uint8_t *src;
		vt_axis_t top, left;

		src = vtfont_lookup(vf, c);

		/*
		 * Align the terminal to the centre of the screen.
		 * Fonts may not always be able to fill the entire
		 * screen.
		 */
		top = row * vf->vf_height +
		    (vd->vd_height % vf->vf_height) / 2;
		left = col * vf->vf_width +
		    (vd->vd_width % vf->vf_width) / 2;

		vd->vd_driver->vd_bitblt(vd, src, top, left,
		    vf->vf_width, vf->vf_height, fg, bg);
	} else {
		vd->vd_driver->vd_putchar(vd, TCHAR_CHARACTER(c),
		    row, col, fg, bg);
	}
}

static void
vt_flush(struct vt_device *vd)
{
	struct vt_window *vw = vd->vd_curwindow;
	struct vt_font *vf = vw->vw_font;
	term_pos_t size;
	term_rect_t tarea;
	struct vt_bufmask tmask;
	unsigned int row, col, scrollpos;
	term_char_t c;

	if (vd->vd_flags & VDF_SPLASH || vw->vw_flags & VWF_BUSY)
		return;

	vtbuf_undirty(&vw->vw_buf, &tarea, &tmask);
	vthistory_getpos(&vw->vw_history, &scrollpos);
	vt_termsize(vd, vf, &size);

	/* Force a full redraw when the screen contents are invalid. */
	if (vd->vd_scrollpos != scrollpos || vd->vd_flags & VDF_INVALID) {
		tarea.tr_begin.tp_row = tarea.tr_begin.tp_col = 0;
		tarea.tr_end = size;
		tmask.vbm_row = tmask.vbm_col = VBM_DIRTY;

		/*
		 * Blank to prevent borders with artifacts.  This is
		 * only required when the font doesn't exactly fill the
		 * screen.
		 */
		if (vd->vd_flags & VDF_INVALID && vf != NULL &&
		    (vd->vd_width % vf->vf_width != 0 ||
		    vd->vd_height % vf->vf_height != 0))
			vd->vd_driver->vd_blank(vd, TC_BLACK);

		/* Draw the scrollback history. */
		for (row = 0; row < scrollpos; row++) {
			for (col = 0; col < size.tp_col; col++) {
				c = VTHISTORY_FIELD(&vw->vw_history, row, col);
				vt_bitblt_char(vd, vf, c, 0, row, col);
			}
		}

		vd->vd_flags &= ~VDF_INVALID;
		vd->vd_scrollpos = scrollpos;
	}

	/*
	 * Clamp the terminal rendering size if it exceeds the window
	 * size, because of scrollback.
	 */
	if (tarea.tr_end.tp_row + scrollpos > size.tp_row) {
		if (size.tp_row <= scrollpos)
			/* Terminal completely invisible. */
			tarea.tr_end.tp_row = 0;
		else
			/* Terminal partially visible. */
			tarea.tr_end.tp_row = size.tp_row - scrollpos;
	}

	for (row = tarea.tr_begin.tp_row; row < tarea.tr_end.tp_row; row++) {
		if (!VTBUF_DIRTYROW(&tmask, row))
			continue;
		for (col = tarea.tr_begin.tp_col;
		    col < tarea.tr_end.tp_col; col++) {
			if (!VTBUF_DIRTYCOL(&tmask, col))
				continue;

			c = VTBUF_FIELD(&vw->vw_buf, row, col);
			vt_bitblt_char(vd, vf, c,
			    VTBUF_ISCURSOR(&vw->vw_buf, row, col),
			    row + scrollpos, col);
		}
	}
}

static void
vt_timer(void *arg)
{
	struct vt_device *vd = arg;

	vt_flush(vd);
	callout_schedule(&vd->vd_timer, hz / VT_TIMERFREQ);
}

static void
vtterm_done(struct terminal *tm)
{
	struct vt_window *vw = tm->tm_softc;
	struct vt_device *vd = vw->vw_device;

	if (kdb_active || panicstr != NULL) {
		/* Switch to the debugger. */
		if (vd->vd_curwindow != vw) {
			vd->vd_curwindow = vw;
			vd->vd_flags |= VDF_INVALID;
		}
		vd->vd_flags &= ~VDF_SPLASH;
		vt_flush(vd);
	} else if (!(vd->vd_flags & VDF_ASYNC)) {
		vt_flush(vd);
	}
}

static void
vtterm_cnprobe(struct terminal *tm, struct consdev *cp)
{
	struct vt_window *vw = tm->tm_softc;
	struct vt_device *vd = vw->vw_device;
	struct winsize wsz;

	cp->cn_pri = vd->vd_driver->vd_init(vd);
	if (cp->cn_pri == CN_DEAD) {
		vd->vd_flags |= VDF_DEAD;
		return;
	}

	/* Initialize any early-boot keyboard drivers */
	kbd_configure(KB_CONF_PROBE_ONLY);

	vd->vd_unit = atomic_fetchadd_int(&vt_unit, 1);
	sprintf(cp->cn_name, "ttyv%r", VT_UNIT(vw));

	if (!(vd->vd_flags & VDF_TEXTMODE))
		vw->vw_font = vtfont_ref(&vt_font_default);

	vtbuf_init_early(&vw->vw_buf);
	vt_winsize(vd, vw->vw_font, &wsz);
	terminal_set_winsize(tm, &wsz);

	/* Display a nice boot splash. */
	if (!(vd->vd_flags & VDF_TEXTMODE)) {
		vt_axis_t top, left;

		top = (vd->vd_height - vt_logo_height) / 2;
		left = (vd->vd_width - vt_logo_width) / 2;
		switch (vt_logo_depth) {
		case 1:
			/* XXX: Unhardcode colors! */
			vd->vd_driver->vd_bitblt(vd, vt_logo_image, top, left,
			    vt_logo_width, vt_logo_height, 0xf, 0x0);
		}
		vd->vd_flags |= VDF_SPLASH;
	}
}

static int
vtterm_cngetc(struct terminal *tm)
{
	struct vt_window *vw = tm->tm_softc;
	struct vt_device *vd = vw->vw_device;
	keyboard_t *kbd;
	u_int c;

	/* Make sure the splash screen is not there. */
	if (vd->vd_flags & VDF_SPLASH) {
		vd->vd_flags &= ~VDF_SPLASH;
		vt_flush(vd);
	}

	/* Stripped down keyboard handler. */
	kbd = kbd_get_keyboard(vd->vd_keyboard);
	if (kbd == NULL)
		return (-1);

	/* Switch the keyboard to polling to make it work here. */
	kbdd_poll(kbd, TRUE);
	c = kbdd_read_char(kbd, 0);
	kbdd_poll(kbd, FALSE);
	if (c & RELKEY)
		return (-1);
	
	/* Stripped down handling of vt_kbdevent(), without locking, etc. */
	if (c & SPCLKEY) {
		c &= ~SPCLKEY;

		switch (c) {
		case SLK: {
			int state = 0;

			kbdd_ioctl(kbd, KDGKBSTATE, (caddr_t)&state);
			if (state & SLKED) {
				/* Turn scrolling on. */
				vw->vw_flags |= VWF_SCROLL;
			} else {
				/* Turn scrolling off. */
				vw->vw_flags &= ~VWF_SCROLL;
				vthistory_seek(&vw->vw_history, 0, VHS_SET);
			}
			break;
		}
		case FKEY | F(49): /* Home key. */
			if (vw->vw_flags & VWF_SCROLL)
				vthistory_seek(&vw->vw_history, 0, VHS_END);
			break;
		case FKEY | F(50): /* Arrow up. */
			if (vw->vw_flags & VWF_SCROLL)
				vthistory_seek(&vw->vw_history, 1, VHS_CUR);
			break;
		case FKEY | F(51): /* Page up. */
			if (vw->vw_flags & VWF_SCROLL) {
				term_pos_t size;

				vt_termsize(vd, vw->vw_font, &size);
				vthistory_seek(&vw->vw_history, size.tp_row,
				    VHS_CUR);
			}
			break;
		case FKEY | F(57): /* End key. */
			if (vw->vw_flags & VWF_SCROLL)
				vthistory_seek(&vw->vw_history, 0, VHS_SET);
			break;
		case FKEY | F(58): /* Arrow down. */
			if (vw->vw_flags & VWF_SCROLL)
				vthistory_seek(&vw->vw_history, -1, VHS_CUR);
			break;
		case FKEY | F(59): /* Page down. */
			if (vw->vw_flags & VWF_SCROLL) {
				term_pos_t size;

				vt_termsize(vd, vw->vw_font, &size);
				vthistory_seek(&vw->vw_history, -size.tp_row,
				    VHS_CUR);
			}
			break;
		}

		/* Force refresh to make scrollback work. */
		vt_flush(vd);
	} else if (KEYFLAGS(c) == 0) {
		return KEYCHAR(c);
	}
	
	return (-1);
}

static void
vtterm_opened(struct terminal *tm, int opened)
{
	struct vt_window *vw = tm->tm_softc;
	struct vt_device *vd = vw->vw_device;

	VT_LOCK(vd);
	vd->vd_flags &= ~VDF_SPLASH;
	if (opened)
		vw->vw_flags |= VWF_OPENED;
	else
		vw->vw_flags &= ~VWF_OPENED;
	VT_UNLOCK(vd);
}

static int
vt_change_font(struct vt_window *vw, struct vt_font *vf)
{
	struct vt_device *vd = vw->vw_device;
	struct terminal *tm = vw->vw_terminal;
	term_pos_t size;
	struct winsize wsz;

	/*
	 * Changing fonts.
	 *
	 * Changing fonts is a little tricky.  We must prevent
	 * simultaneous access to the device, so we must stop
	 * the display timer and the terminal from accessing.
	 * We need to switch fonts and grow our screen buffer.
	 *
	 * XXX: Right now the code uses terminal_mute() to
	 * prevent data from reaching the console driver while
	 * resizing the screen buffer.  This isn't elegant...
	 */

	VT_LOCK(vd);
	if (vw->vw_flags & VWF_BUSY) {
		/* Another process is changing the font. */
		VT_UNLOCK(vd);
		return (EBUSY);
	}
	if (vw->vw_font == NULL) {
		/* Our device doesn't need fonts. */
		VT_UNLOCK(vd);
		return (ENOTTY);
	}
	vw->vw_flags |= VWF_BUSY;
	VT_UNLOCK(vd);

	vt_termsize(vd, vf, &size);
	vt_winsize(vd, vf, &wsz);

	/* Grow the screen buffer and terminal. */
	terminal_mute(tm, 1);
	vtbuf_grow(&vw->vw_buf, &size);
	terminal_set_winsize(tm, &wsz);
	terminal_mute(tm, 0);

	/* Actually apply the font to the current window. */
	VT_LOCK(vd);
	vtfont_unref(vw->vw_font);
	vw->vw_font = vtfont_ref(vf);

	/* Force a full redraw the next timer tick. */
	if (vd->vd_curwindow == vw)
		vd->vd_flags |= VDF_INVALID;
	vw->vw_flags &= ~VWF_BUSY;
	VT_UNLOCK(vd);
	return (0);
}

static int
vtterm_ioctl(struct terminal *tm, u_long cmd, caddr_t data,
    struct thread *td)
{
	struct vt_window *vw = tm->tm_softc;
	struct vt_device *vd = vw->vw_device;

	switch (cmd) {
	case GIO_KEYMAP:
	case PIO_KEYMAP:
	case GIO_DEADKEYMAP:
	case PIO_DEADKEYMAP:
	case GETFKEY:
	case SETFKEY:
	case KDGKBINFO: {
		keyboard_t *kbd;
		int error = 0;

		mtx_lock(&Giant);
		kbd = kbd_get_keyboard(vd->vd_keyboard);
		if (kbd != NULL)
			error = kbdd_ioctl(kbd, cmd, data);
		mtx_unlock(&Giant);
		if (error == ENOIOCTL)
			return (ENODEV);
		return (error);
	}
	case KDSKBMODE: {
		int mode;

		mode = *(int *)data;
		switch (mode) {
		case K_XLATE:
		case K_RAW:
		case K_CODE:
			vw->vw_kbdmode = mode;
			if (vw == vd->vd_curwindow) {
				keyboard_t *kbd;

				mtx_lock(&Giant);
				kbd = kbd_get_keyboard(vd->vd_keyboard);
				if (kbd != NULL)
					kbdd_ioctl(kbd, KDSKBMODE,
					    (void *)&mode);
				mtx_unlock(&Giant);
			}
			return (0);
		default:
			return (EINVAL);
		}
	}
	case CONS_BLANKTIME:
		/* XXX */
		return (0);
	case CONS_GET:
		/* XXX */
		*(int *)data = M_CG640x480;
		return (0);
	case CONS_GETINFO: {
		vid_info_t *vi = (vid_info_t *)data;

		vi->m_num = vd->vd_curwindow->vw_number + 1;
		/* XXX: other fields! */
		return (0);
	}
	case CONS_GETVERS: 
		*(int *)data = 0x200;
		return 0;
	case CONS_MODEINFO:
		/* XXX */
		return (0);
	case CONS_MOUSECTL: {
		mouse_info_t *mouse = (mouse_info_t*)data;

		/*
		 * This has no effect on vt(4).  We don't draw any mouse
		 * cursor.  Just ignore MOUSE_HIDE and MOUSE_SHOW to
		 * prevent excessive errors.  All the other commands
		 * should not be applied to individual TTYs, but only to
		 * consolectl.
		 */
		switch (mouse->operation) {
		case MOUSE_HIDE:
		case MOUSE_SHOW:
			return (0);
		default:
			return (EINVAL);
		}
	}
	case PIO_VFONT: {
		struct vt_font *vf;
		int error;

		error = vtfont_load((void *)data, &vf);
		if (error != 0)
			return (error);

		error = vt_change_font(vw, vf);
		vtfont_unref(vf);
		return (error);
	}
	case GIO_SCRNMAP: {
		scrmap_t *sm = (scrmap_t *)data;
		int i;

		/* We don't have screen maps, so return a handcrafted one. */
		for (i = 0; i < 256; i++)
			sm->scrmap[i] = i;
		return (0);
	}
	case KDGETLED:
		/* XXX */
		return (0);
	case KDSETLED:
		/* XXX */
		return (0);
	case KDSETMODE:
		/* XXX */
		return (0);
	case KDSETRAD:
		/* XXX */
		return (0);
	case VT_ACTIVATE:
		vt_window_switch(vw);
		return (0);
	case VT_GETACTIVE:
		*(int *)data = vd->vd_curwindow->vw_number + 1;
		return (0);
	case VT_GETINDEX:
		*(int *)data = vw->vw_number + 1;
		return (0);
	case VT_OPENQRY: {
		unsigned int i;

		VT_LOCK(vd);
		for (i = 0; i < VT_MAXWINDOWS; i++) {
			vw = vd->vd_windows[i];
			if (vw == NULL)
				continue;
			if (!(vw->vw_flags & VWF_OPENED)) {
				*(int *)data = vw->vw_number + 1;
				VT_UNLOCK(vd);
				return (0);
			}
		}
		VT_UNLOCK(vd);
		return (EINVAL);
	}
	case VT_WAITACTIVE: {
		unsigned int i;
		int error = 0;

		i = *(unsigned int *)data;
		if (i > VT_MAXWINDOWS)
			return (EINVAL);
		if (i != 0)
			vw = vd->vd_windows[i - 1];

		VT_LOCK(vd);
		while (vd->vd_curwindow != vw && error == 0)
			error = cv_wait_sig(&vd->vd_winswitch, &vd->vd_lock);
		VT_UNLOCK(vd);
		return (error);
	}
	case VT_GETMODE:
		/* XXX */
		return (0);
	case VT_SETMODE:
		/* XXX */
		return (0);
	}

	return (ENOIOCTL);
}

static struct vt_window *
vt_allocate_window(struct vt_device *vd, unsigned int window)
{
	struct vt_window *vw;
	struct terminal *tm;
	term_pos_t size;
	struct winsize wsz;

	vw = malloc(sizeof *vw, M_VT, M_WAITOK|M_ZERO);
	vw->vw_device = vd;
	vw->vw_number = window;
	vw->vw_kbdmode = K_XLATE;

	if (!(vd->vd_flags & VDF_TEXTMODE))
		vw->vw_font = vtfont_ref(&vt_font_default);
	
	vt_termsize(vd, vw->vw_font, &size);
	vt_winsize(vd, vw->vw_font, &wsz);
	vtbuf_init(&vw->vw_buf, &size);

	tm = vw->vw_terminal = terminal_alloc(&vt_termclass, vw);
	terminal_set_winsize(tm, &wsz);
	vd->vd_windows[window] = vw;

	return (vw);
}

void
vt_upgrade(struct vt_device *vd)
{
	struct vt_window *vw;
	unsigned int i;

	/* Device didn't pass vd_init(). */
	if (vd->vd_flags & VDF_DEAD)
		return;

	mtx_init(&vd->vd_lock, "vtdev", NULL, MTX_DEF);
	cv_init(&vd->vd_winswitch, "vtwswt");

	/* Start 25 Hz timer. */
	callout_init_mtx(&vd->vd_timer, &vd->vd_lock, 0);
	callout_reset(&vd->vd_timer, hz / VT_TIMERFREQ, vt_timer, vd);
	vd->vd_flags |= VDF_ASYNC;

	for (i = 0; i < VT_MAXWINDOWS; i++) {
		vw = vd->vd_windows[i];
		if (vw == NULL) {
			/* New window. */
			vw = vt_allocate_window(vd, i);
		} else {
			/* Console window. */
			EVENTHANDLER_REGISTER(shutdown_pre_sync,
			    vt_window_switch, vw, SHUTDOWN_PRI_DEFAULT);
		}
		terminal_maketty(vw->vw_terminal, "v%r", VT_UNIT(vw));
	}

	/* Attach keyboard. */
	vt_allocate_keyboard(vd);
}

void
vt_allocate(struct vt_driver *drv, void *softc)
{
	struct vt_device *vd;

	vd = malloc(sizeof *vd, M_VT, M_WAITOK|M_ZERO);
	vd->vd_driver = drv;
	vd->vd_softc = softc;
	vd->vd_unit = atomic_fetchadd_int(&vt_unit, 1);
	vd->vd_flags = VDF_INVALID;
	vd->vd_driver->vd_init(vd);
	
	vt_upgrade(vd);
	vd->vd_curwindow = vd->vd_windows[0];
}
