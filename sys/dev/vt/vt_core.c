/*-
 * Copyright (c) 2009, 2013 The FreeBSD Foundation
 * All rights reserved.
 *
 * This software was developed by Ed Schouten under sponsorship from the
 * FreeBSD Foundation.
 *
 * Portions of this software were developed by Oleksandr Rybalko
 * under sponsorship from the FreeBSD Foundation.
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

#include "opt_compat.h"

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
#include <sys/priv.h>
#include <sys/proc.h>
#include <sys/reboot.h>
#include <sys/systm.h>
#include <sys/terminal.h>

#include <dev/kbd/kbdreg.h>
#include <dev/vt/vt.h>

#if defined(__i386__) || defined(__amd64__)
#include <machine/psl.h>
#include <machine/frame.h>
#endif

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
static tc_mmap_t	vtterm_mmap;

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
	.tc_mmap	= vtterm_mmap,
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

/* XXX while syscons is here. */
int sc_txtmouse_no_retrace_wait;

static SYSCTL_NODE(_kern, OID_AUTO, vt, CTLFLAG_RD, 0, "vt(9) parameters");
VT_SYSCTL_INT(enable_altgr, 1, "Enable AltGr key (Do not assume R.Alt as Alt)");
VT_SYSCTL_INT(debug, 0, "vt(9) debug level");
VT_SYSCTL_INT(deadtimer, 15, "Time to wait busy process in VT_PROCESS mode");
VT_SYSCTL_INT(suspendswitch, 1, "Switch to VT0 before suspend");

static unsigned int vt_unit = 0;
static MALLOC_DEFINE(M_VT, "vt", "vt device");
struct vt_device *main_vd = NULL;

/* Boot logo. */
extern unsigned int vt_logo_width;
extern unsigned int vt_logo_height;
extern unsigned int vt_logo_depth;
extern unsigned char vt_logo_image[];

/* Font. */
extern struct vt_font vt_font_default;
#ifndef SC_NO_CUTPASTE
extern struct mouse_cursor vt_default_mouse_pointer;
#endif

static int signal_vt_rel(struct vt_window *);
static int signal_vt_acq(struct vt_window *);
static int finish_vt_rel(struct vt_window *, int, int *);
static int finish_vt_acq(struct vt_window *);
static int vt_window_switch(struct vt_window *);
static int vt_late_window_switch(struct vt_window *);
static int vt_proc_alive(struct vt_window *);
static void vt_resize(struct vt_device *);

static void
vt_switch_timer(void *arg)
{

	vt_late_window_switch((struct vt_window *)arg);
}

static int
vt_window_preswitch(struct vt_window *vw, struct vt_window *curvw)
{

	DPRINTF(40, "%s\n", __func__);
	curvw->vw_switch_to = vw;
	/* Set timer to allow switch in case when process hang. */
	callout_reset(&vw->vw_proc_dead_timer, hz * vt_deadtimer,
	    vt_switch_timer, (void *)vw);
	/* Notify process about vt switch attempt. */
	DPRINTF(30, "%s: Notify process.\n", __func__);
	signal_vt_rel(curvw);

	return (0);
}

static int
vt_window_postswitch(struct vt_window *vw)
{

	signal_vt_acq(vw);
	return (0);
}

/* vt_late_window_switch will done VT switching for regular case. */
static int
vt_late_window_switch(struct vt_window *vw)
{
	int ret;

	callout_stop(&vw->vw_proc_dead_timer);

	ret = vt_window_switch(vw);
	if (ret)
		return (ret);

	/* Notify owner process about terminal availability. */
	if (vw->vw_smode.mode == VT_PROCESS) {
		ret = vt_window_postswitch(vw);
	}
	return (ret);
}

/* Switch window. */
static int
vt_proc_window_switch(struct vt_window *vw)
{
	struct vt_window *curvw;
	struct vt_device *vd;
	int ret;

	if (vw->vw_flags & VWF_VTYLOCK)
		return (EBUSY);

	vd = vw->vw_device;
	curvw = vd->vd_curwindow;

	/* Ask current process permitions to switch away. */
	if (curvw->vw_smode.mode == VT_PROCESS) {
		DPRINTF(30, "%s: VT_PROCESS ", __func__);
		if (vt_proc_alive(curvw) == FALSE) {
			DPRINTF(30, "Dead. Cleaning.");
			/* Dead */
		} else {
			DPRINTF(30, "%s: Signaling process.\n", __func__);
			/* Alive, try to ask him. */
			ret = vt_window_preswitch(vw, curvw);
			/* Wait for process answer or timeout. */
			return (ret);
		}
		DPRINTF(30, "\n");
	}

	ret = vt_late_window_switch(vw);
	return (ret);
}

/* Switch window ignoring process locking. */
static int
vt_window_switch(struct vt_window *vw)
{
	struct vt_device *vd = vw->vw_device;
	struct vt_window *curvw = vd->vd_curwindow;
	keyboard_t *kbd;

	VT_LOCK(vd);
	if (curvw == vw) {
		/* Nothing to do. */
		VT_UNLOCK(vd);
		return (0);
	}
	if (!(vw->vw_flags & (VWF_OPENED|VWF_CONSOLE))) {
		VT_UNLOCK(vd);
		return (EINVAL);
	}

	vd->vd_curwindow = vw;
	vd->vd_flags |= VDF_INVALID;
	cv_broadcast(&vd->vd_winswitch);
	VT_UNLOCK(vd);

	if (vd->vd_driver->vd_postswitch)
		vd->vd_driver->vd_postswitch(vd);

	/* Restore per-window keyboard mode. */
	mtx_lock(&Giant);
	kbd = kbd_get_keyboard(vd->vd_keyboard);
	if (kbd != NULL) {
		kbdd_ioctl(kbd, KDSKBMODE, (void *)&vw->vw_kbdmode);
	}
	mtx_unlock(&Giant);
	DPRINTF(10, "%s(ttyv%d) done\n", __func__, vw->vw_number);

	return (0);
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

static void
vt_scroll(struct vt_window *vw, int offset, int whence)
{
	int diff;
	term_pos_t size;

	if ((vw->vw_flags & VWF_SCROLL) == 0)
		return;

	vt_termsize(vw->vw_device, vw->vw_font, &size);

	diff = vthistory_seek(&vw->vw_buf, offset, whence);
	/*
	 * Offset changed, please update Nth lines on sceen.
	 * +N - Nth lines at top;
	 * -N - Nth lines at bottom.
	 */

	if (diff < -size.tp_row || diff > size.tp_row) {
		vw->vw_device->vd_flags |= VDF_INVALID;
		return;
	}
	vw->vw_device->vd_flags |= VDF_INVALID; /*XXX*/
}

static int
vt_machine_kbdevent(int c)
{

	switch (c) {
	case SPCLKEY | DBG:
		kdb_enter(KDB_WHY_BREAK, "manual escape to debugger");
		return (1);
	case SPCLKEY | RBT:
		/* XXX: Make this configurable! */
		shutdown_nice(0);
		return (1);
	case SPCLKEY | HALT:
		shutdown_nice(RB_HALT);
		return (1);
	case SPCLKEY | PDWN:
		shutdown_nice(RB_HALT|RB_POWEROFF);
		return (1);
	};

	return (0);
}

static void
vt_scrollmode_kbdevent(struct vt_window *vw, int c, int console)
{
	struct vt_device *vd;
	term_pos_t size;

	vd = vw->vw_device;
	/* Only special keys handled in ScrollLock mode */
	if ((c & SPCLKEY) == 0)
		return;

	c &= ~SPCLKEY;

	if (console == 0) {
		if (c >= F_SCR && c <= MIN(L_SCR, F_SCR + VT_MAXWINDOWS - 1)) {
			vw = vd->vd_windows[c - F_SCR];
			if (vw != NULL)
				vt_proc_window_switch(vw);
			return;
		}
		VT_LOCK(vd);
	}

	switch (c) {
	case SLK: {
		/* Turn scrolling off. */
		vt_scroll(vw, 0, VHS_END);
		VTBUF_SLCK_DISABLE(&vw->vw_buf);
		vw->vw_flags &= ~VWF_SCROLL;
		break;
	}
	case FKEY | F(49): /* Home key. */
		vt_scroll(vw, 0, VHS_SET);
		break;
	case FKEY | F(50): /* Arrow up. */
		vt_scroll(vw, -1, VHS_CUR);
		break;
	case FKEY | F(51): /* Page up. */
		vt_termsize(vd, vw->vw_font, &size);
		vt_scroll(vw, -size.tp_row, VHS_CUR);
		break;
	case FKEY | F(57): /* End key. */
		vt_scroll(vw, 0, VHS_END);
		break;
	case FKEY | F(58): /* Arrow down. */
		vt_scroll(vw, 1, VHS_CUR);
		break;
	case FKEY | F(59): /* Page down. */
		vt_termsize(vd, vw->vw_font, &size);
		vt_scroll(vw, size.tp_row, VHS_CUR);
		break;
	}

	if (console == 0)
		VT_UNLOCK(vd);
}

static int
vt_processkey(keyboard_t *kbd, struct vt_device *vd, int c)
{
	struct vt_window *vw = vd->vd_curwindow;
	int state = 0;

#if VT_ALT_TO_ESC_HACK
	if (c & RELKEY) {
		switch (c & ~RELKEY) {
		case (SPCLKEY | RALT):
			if (vt_enable_altgr != 0)
				break;
		case (SPCLKEY | LALT):
			vd->vd_kbstate &= ~ALKED;
		}
		/* Other keys ignored for RELKEY event. */
		return (0);
	} else {
		switch (c & ~RELKEY) {
		case (SPCLKEY | RALT):
			if (vt_enable_altgr != 0)
				break;
		case (SPCLKEY | LALT):
			vd->vd_kbstate |= ALKED;
		}
	}
#else
	if (c & RELKEY)
		/* Other keys ignored for RELKEY event. */
		return (0);
#endif

	if (vt_machine_kbdevent(c))
		return (0);

	if (vw->vw_flags & VWF_SCROLL) {
		vt_scrollmode_kbdevent(vw, c, 0/* Not a console */);
		/* Scroll mode keys handled, nothing to do more. */
		return (0);
	}

	if (c & SPCLKEY) {
		c &= ~SPCLKEY;

		if (c >= F_SCR && c <= MIN(L_SCR, F_SCR + VT_MAXWINDOWS - 1)) {
			vw = vd->vd_windows[c - F_SCR];
			if (vw != NULL)
				vt_proc_window_switch(vw);
			return (0);
		}

		switch (c) {
		case SLK: {

			kbdd_ioctl(kbd, KDGKBSTATE, (caddr_t)&state);
			VT_LOCK(vd);
			if (state & SLKED) {
				/* Turn scrolling on. */
				vw->vw_flags |= VWF_SCROLL;
				VTBUF_SLCK_ENABLE(&vw->vw_buf);
			} else {
				/* Turn scrolling off. */
				vw->vw_flags &= ~VWF_SCROLL;
				VTBUF_SLCK_DISABLE(&vw->vw_buf);
				vt_scroll(vw, 0, VHS_END);
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
			terminal_input_special(vw->vw_terminal, TKEY_HOME);
			break;
		case FKEY | F(50): /* Arrow up. */
			terminal_input_special(vw->vw_terminal, TKEY_UP);
			break;
		case FKEY | F(51): /* Page up. */
			terminal_input_special(vw->vw_terminal, TKEY_PAGE_UP);
			break;
		case FKEY | F(53): /* Arrow left. */
			terminal_input_special(vw->vw_terminal, TKEY_LEFT);
			break;
		case FKEY | F(55): /* Arrow right. */
			terminal_input_special(vw->vw_terminal, TKEY_RIGHT);
			break;
		case FKEY | F(57): /* End key. */
			terminal_input_special(vw->vw_terminal, TKEY_END);
			break;
		case FKEY | F(58): /* Arrow down. */
			terminal_input_special(vw->vw_terminal, TKEY_DOWN);
			break;
		case FKEY | F(59): /* Page down. */
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
		/* Don't do UTF-8 conversion when doing raw mode. */
		if (vw->vw_kbdmode == K_XLATE) {
#if VT_ALT_TO_ESC_HACK
			if (vd->vd_kbstate & ALKED) {
				/*
				 * Prepend ESC sequence if one of ALT keys down.
				 */
				terminal_input_char(vw->vw_terminal, 0x1b);
			}
#endif

			terminal_input_char(vw->vw_terminal, KEYCHAR(c));
		} else
			terminal_input_raw(vw->vw_terminal, c);
	}
	return (0);
}

static int
vt_kbdevent(keyboard_t *kbd, int event, void *arg)
{
	struct vt_device *vd = arg;
	int c;

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

	while ((c = kbdd_read_char(kbd, 0)) != NOKEY)
		vt_processkey(kbd, vd, c);

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
	/* XXX: kb_token lost */
	vd->vd_keyboard = idx0;
	if (idx0 != -1) {
		DPRINTF(20, "%s: kbdmux allocated, idx = %d\n", __func__, idx0);
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
	} else {
		DPRINTF(20, "%s: no kbdmux allocated\n", __func__);
		idx0 = kbd_allocate("*", -1, (void *)&vd->vd_keyboard,
		    vt_kbdevent, vd);
	}
	DPRINTF(20, "%s: vd_keyboard = %d\n", __func__, vd->vd_keyboard);

	return (idx0);
}

static void
vtterm_bell(struct terminal *tm)
{
	struct vt_window *vw = tm->tm_softc;
	struct vt_device *vd = vw->vw_device;

	if (vd->vd_flags & VDF_QUIET_BELL)
		return;

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

	vtbuf_fill_locked(&vw->vw_buf, r, c);
}

static void
vtterm_copy(struct terminal *tm, const term_rect_t *r,
    const term_pos_t *p)
{
	struct vt_window *vw = tm->tm_softc;

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
	case TP_MOUSE:
		vw->vw_mouse_level = arg;
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
		top = row * vf->vf_height + vd->vd_offset.tp_row;
		left = col * vf->vf_width + vd->vd_offset.tp_col;

		vd->vd_driver->vd_bitbltchr(vd, src, NULL, 0, top, left,
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
	struct vt_bufmask tmask;
	unsigned int row, col;
	term_rect_t tarea;
	term_pos_t size;
	term_char_t *r;
#ifndef SC_NO_CUTPASTE
	struct mouse_cursor *m;
	int bpl, h, w;
#endif

	if (vd->vd_flags & VDF_SPLASH || vw->vw_flags & VWF_BUSY)
		return;

	vtbuf_undirty(&vw->vw_buf, &tarea, &tmask);
	vt_termsize(vd, vf, &size);

	/* Force a full redraw when the screen contents are invalid. */
	if (vd->vd_flags & VDF_INVALID) {
		tarea.tr_begin.tp_row = tarea.tr_begin.tp_col = 0;
		tarea.tr_end = size;
		tmask.vbm_row = tmask.vbm_col = VBM_DIRTY;

		vd->vd_flags &= ~VDF_INVALID;
	}

#ifndef SC_NO_CUTPASTE
	if ((vw->vw_flags & VWF_MOUSE_HIDE) == 0) {
		/* Mark last mouse position as dirty to erase. */
		vtbuf_mouse_cursor_position(&vw->vw_buf, vd->vd_mdirtyx,
		    vd->vd_mdirtyy);
	}
#endif

	for (row = tarea.tr_begin.tp_row; row < tarea.tr_end.tp_row; row++) {
		if (!VTBUF_DIRTYROW(&tmask, row))
			continue;
		r = VTBUF_GET_ROW(&vw->vw_buf, row);
		for (col = tarea.tr_begin.tp_col;
		    col < tarea.tr_end.tp_col; col++) {
			if (!VTBUF_DIRTYCOL(&tmask, col))
				continue;

			vt_bitblt_char(vd, vf, r[col],
			    VTBUF_ISCURSOR(&vw->vw_buf, row, col), row, col);
		}
	}

#ifndef SC_NO_CUTPASTE
	/* Mouse disabled. */
	if (vw->vw_flags & VWF_MOUSE_HIDE)
		return;

	/* No mouse for DDB. */
	if (kdb_active || panicstr != NULL)
		return;

	if ((vd->vd_flags & (VDF_MOUSECURSOR|VDF_TEXTMODE)) ==
	    VDF_MOUSECURSOR) {
		m = &vt_default_mouse_pointer;
		bpl = (m->w + 7) >> 3; /* Bytes per sorce line. */
		w = m->w;
		h = m->h;

		if ((vd->vd_mx + m->w) > (size.tp_col * vf->vf_width))
			w = (size.tp_col * vf->vf_width) - vd->vd_mx - 1;
		if ((vd->vd_my + m->h) > (size.tp_row * vf->vf_height))
			h = (size.tp_row * vf->vf_height) - vd->vd_my - 1;

		vd->vd_driver->vd_bitbltchr(vd, m->map, m->mask, bpl,
		    vd->vd_offset.tp_row + vd->vd_my,
		    vd->vd_offset.tp_col + vd->vd_mx,
		    w, h, TC_WHITE, TC_BLACK);
		/* Save point of last mouse cursor to erase it later. */
		vd->vd_mdirtyx = vd->vd_mx / vf->vf_width;
		vd->vd_mdirtyy = vd->vd_my / vf->vf_height;
	}
#endif
}

static void
vt_timer(void *arg)
{
	struct vt_device *vd;

	vd = arg;
	/* Update screen if required. */
	vt_flush(vd);
	/* Schedule for next update. */
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
			if (vd->vd_driver->vd_postswitch)
				vd->vd_driver->vd_postswitch(vd);
		}
		vd->vd_flags &= ~VDF_SPLASH;
		vt_flush(vd);
	} else if (!(vd->vd_flags & VDF_ASYNC)) {
		vt_flush(vd);
	}
}

#ifdef DEV_SPLASH
static void
vtterm_splash(struct vt_device *vd)
{
	vt_axis_t top, left;

	/* Display a nice boot splash. */
	if (!(vd->vd_flags & VDF_TEXTMODE) && (boothowto & RB_MUTE)) {

		top = (vd->vd_height - vt_logo_height) / 2;
		left = (vd->vd_width - vt_logo_width) / 2;
		switch (vt_logo_depth) {
		case 1:
			/* XXX: Unhardcode colors! */
			vd->vd_driver->vd_bitbltchr(vd, vt_logo_image, NULL, 0,
			    top, left, vt_logo_width, vt_logo_height, 0xf, 0x0);
		}
		vd->vd_flags |= VDF_SPLASH;
	}
}
#endif

static void
vtterm_cnprobe(struct terminal *tm, struct consdev *cp)
{
	struct vt_window *vw = tm->tm_softc;
	struct vt_device *vd = vw->vw_device;
	struct winsize wsz;

	if (vd->vd_flags & VDF_INITIALIZED)
		/* Initialization already done. */
		return;

	cp->cn_pri = vd->vd_driver->vd_init(vd);
	if (cp->cn_pri == CN_DEAD) {
		vd->vd_flags |= VDF_DEAD;
		return;
	}

	/* Initialize any early-boot keyboard drivers */
	kbd_configure(KB_CONF_PROBE_ONLY);

	vd->vd_unit = atomic_fetchadd_int(&vt_unit, 1);
	vd->vd_windows[VT_CONSWINDOW] = vw;
	sprintf(cp->cn_name, "ttyv%r", VT_UNIT(vw));

	if (!(vd->vd_flags & VDF_TEXTMODE))
		vw->vw_font = vtfont_ref(&vt_font_default);

	vtbuf_init_early(&vw->vw_buf);
	vt_winsize(vd, vw->vw_font, &wsz);
	terminal_set_winsize(tm, &wsz);

#ifdef DEV_SPLASH
	vtterm_splash(vd);
#endif

	vd->vd_flags |= VDF_INITIALIZED;
	main_vd = vd;
}

static int
vtterm_cngetc(struct terminal *tm)
{
	struct vt_window *vw = tm->tm_softc;
	struct vt_device *vd = vw->vw_device;
	keyboard_t *kbd;
	int state;
	u_int c;

	if (vw->vw_kbdsq && *vw->vw_kbdsq)
		return (*vw->vw_kbdsq++);

	state = 0;
	/* Make sure the splash screen is not there. */
	if (vd->vd_flags & VDF_SPLASH) {
		/* Remove splash */
		vd->vd_flags &= ~VDF_SPLASH;
		/* Mark screen as invalid to force update */
		vd->vd_flags |= VDF_INVALID;
		vt_flush(vd);
	}

	/* Stripped down keyboard handler. */
	kbd = kbd_get_keyboard(vd->vd_keyboard);
	if (kbd == NULL)
		return (-1);

	/* Force keyboard input mode to K_XLATE */
	c = K_XLATE;
	kbdd_ioctl(kbd, KDSKBMODE, (void *)&c);

	/* Switch the keyboard to polling to make it work here. */
	kbdd_poll(kbd, TRUE);
	c = kbdd_read_char(kbd, 0);
	kbdd_poll(kbd, FALSE);
	if (c & RELKEY)
		return (-1);

	if (vw->vw_flags & VWF_SCROLL) {
		vt_scrollmode_kbdevent(vw, c, 1/* Console mode */);
		vt_flush(vd);
		return (-1);
	}

	/* Stripped down handling of vt_kbdevent(), without locking, etc. */
	if (c & SPCLKEY) {
		switch (c) {
		case SPCLKEY | SLK:
			kbdd_ioctl(kbd, KDGKBSTATE, (caddr_t)&state);
			if (state & SLKED) {
				/* Turn scrolling on. */
				vw->vw_flags |= VWF_SCROLL;
				VTBUF_SLCK_ENABLE(&vw->vw_buf);
			} else {
				/* Turn scrolling off. */
				vt_scroll(vw, 0, VHS_END);
				vw->vw_flags &= ~VWF_SCROLL;
				VTBUF_SLCK_DISABLE(&vw->vw_buf);
			}
			break;
		/* XXX: KDB can handle history. */
		case SPCLKEY | FKEY | F(50): /* Arrow up. */
			vw->vw_kbdsq = "\x1b[A";
			break;
		case SPCLKEY | FKEY | F(58): /* Arrow down. */
			vw->vw_kbdsq = "\x1b[B";
			break;
		case SPCLKEY | FKEY | F(55): /* Arrow right. */
			vw->vw_kbdsq = "\x1b[C";
			break;
		case SPCLKEY | FKEY | F(53): /* Arrow left. */
			vw->vw_kbdsq = "\x1b[D";
			break;
		}

		/* Force refresh to make scrollback work. */
		vt_flush(vd);
	} else if (KEYFLAGS(c) == 0) {
		return (KEYCHAR(c));
	}

	if (vw->vw_kbdsq && *vw->vw_kbdsq)
		return (*vw->vw_kbdsq++);

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
	else {
		vw->vw_flags &= ~VWF_OPENED;
		/* TODO: finish ACQ/REL */
	}
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
	/* Save offset to font aligned area. */
	vd->vd_offset.tp_col = (vd->vd_width % vf->vf_width) / 2;
	vd->vd_offset.tp_row = (vd->vd_height % vf->vf_height) / 2;

	/* Grow the screen buffer and terminal. */
	terminal_mute(tm, 1);
	vtbuf_grow(&vw->vw_buf, &size, vw->vw_buf.vb_history_size);
	terminal_set_winsize_blank(tm, &wsz, 0);
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
vt_proc_alive(struct vt_window *vw)
{
	struct proc *p;

	if (vw->vw_smode.mode != VT_PROCESS)
		return (FALSE);

	if (vw->vw_proc) {
		if ((p = pfind(vw->vw_pid)) != NULL)
			PROC_UNLOCK(p);
		if (vw->vw_proc == p)
			return (TRUE);
		vw->vw_proc = NULL;
		vw->vw_smode.mode = VT_AUTO;
		DPRINTF(1, "vt controlling process %d died\n", vw->vw_pid);
		vw->vw_pid = 0;
	}
	return (FALSE);
}

static int
signal_vt_rel(struct vt_window *vw)
{

	if (vw->vw_smode.mode != VT_PROCESS)
		return (FALSE);
	if (vw->vw_proc == NULL || vt_proc_alive(vw) == FALSE) {
		vw->vw_proc = NULL;
		vw->vw_pid = 0;
		return (TRUE);
	}
	vw->vw_flags |= VWF_SWWAIT_REL;
	PROC_LOCK(vw->vw_proc);
	kern_psignal(vw->vw_proc, vw->vw_smode.relsig);
	PROC_UNLOCK(vw->vw_proc);
	DPRINTF(1, "sending relsig to %d\n", vw->vw_pid);
	return (TRUE);
}

static int
signal_vt_acq(struct vt_window *vw)
{

	if (vw->vw_smode.mode != VT_PROCESS)
		return (FALSE);
	if (vw == vw->vw_device->vd_windows[VT_CONSWINDOW])
		cnavailable(vw->vw_terminal->consdev, FALSE);
	if (vw->vw_proc == NULL || vt_proc_alive(vw) == FALSE) {
		vw->vw_proc = NULL;
		vw->vw_pid = 0;
		return (TRUE);
	}
	vw->vw_flags |= VWF_SWWAIT_ACQ;
	PROC_LOCK(vw->vw_proc);
	kern_psignal(vw->vw_proc, vw->vw_smode.acqsig);
	PROC_UNLOCK(vw->vw_proc);
	DPRINTF(1, "sending acqsig to %d\n", vw->vw_pid);
	return (TRUE);
}

static int
finish_vt_rel(struct vt_window *vw, int release, int *s)
{

	if (vw->vw_flags & VWF_SWWAIT_REL) {
		vw->vw_flags &= ~VWF_SWWAIT_REL;
		if (release) {
			callout_drain(&vw->vw_proc_dead_timer);
			vt_late_window_switch(vw->vw_switch_to);
		}
		return (0);
	}
	return (EINVAL);
}

static int
finish_vt_acq(struct vt_window *vw)
{

	if (vw->vw_flags & VWF_SWWAIT_ACQ) {
		vw->vw_flags &= ~VWF_SWWAIT_ACQ;
		return (0);
	}
	return (EINVAL);
}

#ifndef SC_NO_CUTPASTE
static void
vt_mouse_terminput_button(struct vt_device *vd, int button)
{
	struct vt_window *vw;
	struct vt_font *vf;
	char mouseb[6] = "\x1B[M";
	int i, x, y;

	vw = vd->vd_curwindow;
	vf = vw->vw_font;

	/* Translate to char position. */
	x = vd->vd_mx / vf->vf_width;
	y = vd->vd_my / vf->vf_height;
	/* Avoid overflow. */
	x = MIN(x, 255 - '!');
	y = MIN(y, 255 - '!');

	mouseb[3] = ' ' + button;
	mouseb[4] = '!' + x;
	mouseb[5] = '!' + y;

	for (i = 0; i < sizeof(mouseb); i++ )
		terminal_input_char(vw->vw_terminal, mouseb[i]);
}

static void
vt_mouse_terminput(struct vt_device *vd, int type, int x, int y, int event,
    int cnt)
{

	switch (type) {
	case MOUSE_BUTTON_EVENT:
		if (cnt > 0) {
			/* Mouse button pressed. */
			if (event & MOUSE_BUTTON1DOWN)
				vt_mouse_terminput_button(vd, 0);
			if (event & MOUSE_BUTTON2DOWN)
				vt_mouse_terminput_button(vd, 1);
			if (event & MOUSE_BUTTON3DOWN)
				vt_mouse_terminput_button(vd, 2);
		} else {
			/* Mouse button released. */
			vt_mouse_terminput_button(vd, 3);
		}
		break;
#ifdef notyet
	case MOUSE_MOTION_EVENT:
		if (mouse->u.data.z < 0) {
			/* Scroll up. */
			sc_mouse_input_button(vd, 64);
		} else if (mouse->u.data.z > 0) {
			/* Scroll down. */
			sc_mouse_input_button(vd, 65);
		}
		break;
#endif
	}
}

void
vt_mouse_event(int type, int x, int y, int event, int cnt, int mlevel)
{
	struct vt_device *vd;
	struct vt_window *vw;
	struct vt_font *vf;
	term_pos_t size;
	term_char_t *buf;
	int i, len, mark;

	vd = main_vd;
	vw = vd->vd_curwindow;
	vf = vw->vw_font;
	mark = 0;

	if (vw->vw_flags & VWF_MOUSE_HIDE)
		return; /* Mouse disabled. */

	if (vf == NULL)	/* Text mode. */
		return;

	/*
	 * TODO: add flag about pointer position changed, to not redraw chars
	 * under mouse pointer when nothing changed.
	 */

	if (vw->vw_mouse_level > 0)
		vt_mouse_terminput(vd, type, x, y, event, cnt);

	switch (type) {
	case MOUSE_ACTION:
	case MOUSE_MOTION_EVENT:
		/* Movement */
		x += vd->vd_mx;
		y += vd->vd_my;

		vt_termsize(vd, vf, &size);

		/* Apply limits. */
		x = MAX(x, 0);
		y = MAX(y, 0);
		x = MIN(x, (size.tp_col * vf->vf_width) - 1);
		y = MIN(y, (size.tp_row * vf->vf_height) - 1);

		vd->vd_mx = x;
		vd->vd_my = y;
		if ((vd->vd_mstate & MOUSE_BUTTON1DOWN) &&
		    (vtbuf_set_mark(&vw->vw_buf, VTB_MARK_MOVE,
			vd->vd_mx / vf->vf_width, 
			vd->vd_my / vf->vf_height) == 1)) {

			/*
			 * We have something marked to copy, so update pointer
			 * to window with selection.
			 */
			vd->vd_markedwin = vw;
		}
		return; /* Done */
	case MOUSE_BUTTON_EVENT:
		/* Buttons */
		break;
	default:
		return; /* Done */
	}

	switch (event) {
	case MOUSE_BUTTON1DOWN:
		switch (cnt % 4) {
		case 0:	/* up */
			mark = VTB_MARK_END;
			break;
		case 1: /* single click: start cut operation */
			mark = VTB_MARK_START;
			break;
		case 2:	/* double click: cut a word */
			mark = VTB_MARK_WORD;
			break;
		case 3:	/* triple click: cut a line */
			mark = VTB_MARK_ROW;
			break;
		}
		break;
	case VT_MOUSE_PASTEBUTTON:
		switch (cnt) {
		case 0:	/* up */
			break;
		default:
			if (vd->vd_markedwin == NULL)
				return;
			/* Get current selecton size in bytes. */
			len = vtbuf_get_marked_len(&vd->vd_markedwin->vw_buf);
			if (len <= 0)
				return;

			buf = malloc(len, M_VT, M_WAITOK | M_ZERO);
			/* Request cupy/paste buffer data, no more than `len' */
			vtbuf_extract_marked(&vd->vd_markedwin->vw_buf, buf,
			    len);

			len /= sizeof(term_char_t);
			for (i = 0; i < len; i++ ) {
				if (buf[i] == '\0')
					continue;
				terminal_input_char(vw->vw_terminal, buf[i]);
			}

			/* Done, so cleanup. */
			free(buf, M_VT);
			break;
		}
		return; /* Done */
	case VT_MOUSE_EXTENDBUTTON:
		switch (cnt) {
		case 0:	/* up */
			if (!(vd->vd_mstate & MOUSE_BUTTON1DOWN))
				mark = VTB_MARK_EXTEND;
			else
				mark = 0;
			break;
		default:
			mark = VTB_MARK_EXTEND;
			break;
		}
		break;
	default:
		return; /* Done */
	}

	/* Save buttons state. */
	if (cnt > 0)
		vd->vd_mstate |= event;
	else
		vd->vd_mstate &= ~event;

	if (vtbuf_set_mark(&vw->vw_buf, mark, vd->vd_mx / vf->vf_width,
	    vd->vd_my / vf->vf_height) == 1) {
		/*
		 * We have something marked to copy, so update pointer to
		 * window with selection.
		 */
		vd->vd_markedwin = vw;
	}
}

void
vt_mouse_state(int show)
{
	struct vt_device *vd;
	struct vt_window *vw;

	vd = main_vd;
	vw = vd->vd_curwindow;

	switch (show) {
	case VT_MOUSE_HIDE:
		vw->vw_flags |= VWF_MOUSE_HIDE;
		break;
	case VT_MOUSE_SHOW:
		vw->vw_flags &= ~VWF_MOUSE_HIDE;
		break;
	}
}
#endif

static int
vtterm_mmap(struct terminal *tm, vm_ooffset_t offset, vm_paddr_t * paddr,
    int nprot, vm_memattr_t *memattr)
{
	struct vt_window *vw = tm->tm_softc;
	struct vt_device *vd = vw->vw_device;

	if (vd->vd_driver->vd_fb_mmap)
		return (vd->vd_driver->vd_fb_mmap(vd, offset, paddr, nprot,
		    memattr));

	return (ENXIO);
}

static int
vtterm_ioctl(struct terminal *tm, u_long cmd, caddr_t data,
    struct thread *td)
{
	struct vt_window *vw = tm->tm_softc;
	struct vt_device *vd = vw->vw_device;
	keyboard_t *kbd;
	int error, i, s;
#if defined(COMPAT_FREEBSD6) || defined(COMPAT_FREEBSD5) || \
    defined(COMPAT_FREEBSD4) || defined(COMPAT_43)
	int ival;

	switch (cmd) {
	case _IO('v', 4):
		cmd = VT_RELDISP;
		break;
	case _IO('v', 5):
		cmd = VT_ACTIVATE;
		break;
	case _IO('v', 6):
		cmd = VT_WAITACTIVE;
		break;
	case _IO('K', 20):
		cmd = KDSKBSTATE;
		break;
	case _IO('K', 67):
		cmd = KDSETRAD;
		break;
	case _IO('K', 7):
		cmd = KDSKBMODE;
		break;
	case _IO('K', 8):
		cmd = KDMKTONE;
		break;
	case _IO('K', 63):
		cmd = KIOCSOUND;
		break;
	case _IO('K', 66):
		cmd = KDSETLED;
		break;
	case _IO('c', 110):
		cmd = CONS_SETKBD;
		break;
	default:
		goto skip_thunk;
	}
	ival = IOCPARM_IVAL(data);
	data = (caddr_t)&ival;
skip_thunk:
#endif

	switch (cmd) {
	case KDSETRAD:		/* set keyboard repeat & delay rates (old) */
		if (*(int *)data & ~0x7f)
			return (EINVAL);
	case GIO_KEYMAP:
	case PIO_KEYMAP:
	case GIO_DEADKEYMAP:
	case PIO_DEADKEYMAP:
	case GETFKEY:
	case SETFKEY:
	case KDGKBINFO:
	case KDGKBTYPE:
	case KDSKBSTATE:	/* set keyboard state (locks) */
	case KDGKBSTATE:	/* get keyboard state (locks) */
	case KDGETREPEAT:	/* get keyboard repeat & delay rates */
	case KDSETREPEAT:	/* set keyboard repeat & delay rates (new) */
	case KDSETLED:		/* set keyboard LED status */
	case KDGETLED:		/* get keyboard LED status */
	case KBADDKBD:		/* add/remove keyboard to/from mux */
	case KBRELKBD: {
		error = 0;

		mtx_lock(&Giant);
		kbd = kbd_get_keyboard(vd->vd_keyboard);
		if (kbd != NULL)
			error = kbdd_ioctl(kbd, cmd, data);
		mtx_unlock(&Giant);
		if (error == ENOIOCTL) {
			if (cmd == KDGKBTYPE) {
				/* always return something? XXX */
				*(int *)data = 0;
			} else {
				return (ENODEV);
			}
		}
		return (error);
	}
	case KDGKBMODE: {
		int mode = -1;

		mtx_lock(&Giant);
		kbd = kbd_get_keyboard(vd->vd_keyboard);
		if (kbd != NULL) {
			kbdd_ioctl(kbd, KDGKBMODE, (void *)&mode);
		}
		mtx_unlock(&Giant);
		DPRINTF(20, "mode %d, vw_kbdmode %d\n", mode, vw->vw_kbdmode);
		*(int *)data = mode;
		return (0);
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
				error = 0;

				mtx_lock(&Giant);
				kbd = kbd_get_keyboard(vd->vd_keyboard);
				if (kbd != NULL) {
					error = kbdd_ioctl(kbd, KDSKBMODE,
					    (void *)&mode);
				}
				mtx_unlock(&Giant);
			}
			return (0);
		default:
			return (EINVAL);
		}
	}
	case FBIOGTYPE:
	case FBIO_GETWINORG:	/* get frame buffer window origin */
	case FBIO_GETDISPSTART:	/* get display start address */
	case FBIO_GETLINEWIDTH:	/* get scan line width in bytes */
	case FBIO_BLANK:	/* blank display */
		if (vd->vd_driver->vd_fb_ioctl)
			return (vd->vd_driver->vd_fb_ioctl(vd, cmd, data, td));
		break;
	case CONS_BLANKTIME:
		/* XXX */
		return (0);
	case CONS_GET:
		/* XXX */
		*(int *)data = M_CG640x480;
		return (0);
	case CONS_BELLTYPE: 	/* set bell type sound */
		if ((*(int *)data) & CONS_QUIET_BELL)
			vd->vd_flags |= VDF_QUIET_BELL;
		else
			vd->vd_flags &= ~VDF_QUIET_BELL;
		return (0);
	case CONS_GETINFO: {
		vid_info_t *vi = (vid_info_t *)data;

		vi->m_num = vd->vd_curwindow->vw_number + 1;
		/* XXX: other fields! */
		return (0);
	}
	case CONS_GETVERS: 
		*(int *)data = 0x200;
		return (0);
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
			vd->vd_flags &= ~VDF_MOUSECURSOR;
			return (0);
		case MOUSE_SHOW:
			vd->vd_mx = vd->vd_width / 2;
			vd->vd_my = vd->vd_height / 2;
			vd->vd_flags |= VDF_MOUSECURSOR;
			return (0);
		default:
			return (EINVAL);
		}
	}
	case PIO_VFONT: {
		struct vt_font *vf;

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
	case KDSETMODE:
		/* XXX */
		return (0);
	case KDENABIO:      	/* allow io operations */
		error = priv_check(td, PRIV_IO);
		if (error != 0)
			return (error);
		error = securelevel_gt(td->td_ucred, 0);
		if (error != 0)
			return (error);
#if defined(__i386__)
		td->td_frame->tf_eflags |= PSL_IOPL;
#elif defined(__amd64__)
		td->td_frame->tf_rflags |= PSL_IOPL;
#endif
		return (0);
	case KDDISABIO:     	/* disallow io operations (default) */
#if defined(__i386__)
		td->td_frame->tf_eflags &= ~PSL_IOPL;
#elif defined(__amd64__)
		td->td_frame->tf_rflags &= ~PSL_IOPL;
#endif
		return (0);
	case KDMKTONE:      	/* sound the bell */
		/* TODO */
		return (0);
	case KIOCSOUND:     	/* make tone (*data) hz */
		/* TODO */
		return (0);
	case CONS_SETKBD: 		/* set the new keyboard */
		mtx_lock(&Giant);
		error = 0;
		if (vd->vd_keyboard != *(int *)data) {
			kbd = kbd_get_keyboard(*(int *)data);
			if (kbd == NULL) {
				mtx_unlock(&Giant);
				return (EINVAL);
			}
			i = kbd_allocate(kbd->kb_name, kbd->kb_unit,
			    (void *)&vd->vd_keyboard, vt_kbdevent, vd);
			if (i >= 0) {
				if (vd->vd_keyboard != -1) {
					kbd_release(kbd,
					    (void *)&vd->vd_keyboard);
				}
				kbd = kbd_get_keyboard(i);
				vd->vd_keyboard = i;

				(void)kbdd_ioctl(kbd, KDSKBMODE,
				    (caddr_t)&vd->vd_curwindow->vw_kbdmode);
			} else {
				error = EPERM;	/* XXX */
			}
		}
		mtx_unlock(&Giant);
		return (error);
	case CONS_RELKBD: 		/* release the current keyboard */
		mtx_lock(&Giant);
		error = 0;
		if (vd->vd_keyboard != -1) {
			kbd = kbd_get_keyboard(vd->vd_keyboard);
			if (kbd == NULL) {
				mtx_unlock(&Giant);
				return (EINVAL);
			}
			error = kbd_release(kbd, (void *)&vd->vd_keyboard);
			if (error == 0) {
				vd->vd_keyboard = -1;
			}
		}
		mtx_unlock(&Giant);
		return (error);
	case VT_ACTIVATE: {
		int win;
		win = *(int *)data - 1;
		DPRINTF(5, "%s%d: VT_ACTIVATE ttyv%d ", SC_DRIVER_NAME,
		    VT_UNIT(vw), win);
		if ((win > VT_MAXWINDOWS) || (win < 0))
			return (EINVAL);
		return (vt_proc_window_switch(vd->vd_windows[win]));
	}
	case VT_GETACTIVE:
		*(int *)data = vd->vd_curwindow->vw_number + 1;
		return (0);
	case VT_GETINDEX:
		*(int *)data = vw->vw_number + 1;
		return (0);
	case VT_LOCKSWITCH:
		/* TODO: Check current state, switching can be in progress. */
		if ((*(int *)data) & 0x01)
			vw->vw_flags |= VWF_VTYLOCK;
		else
			vw->vw_flags &= ~VWF_VTYLOCK;
	case VT_OPENQRY:
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
	case VT_WAITACTIVE:
		error = 0;

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
	case VT_SETMODE: {    	/* set screen switcher mode */
		struct vt_mode *mode;
		struct proc *p1;

		mode = (struct vt_mode *)data;
		DPRINTF(5, "%s%d: VT_SETMODE ", SC_DRIVER_NAME, VT_UNIT(vw));
		if (vw->vw_smode.mode == VT_PROCESS) {
			p1 = pfind(vw->vw_pid);
			if (vw->vw_proc == p1 && vw->vw_proc != td->td_proc) {
				if (p1)
					PROC_UNLOCK(p1);
				DPRINTF(5, "error EPERM\n");
				return (EPERM);
			}
			if (p1)
				PROC_UNLOCK(p1);
		}
		if (mode->mode == VT_AUTO) {
			vw->vw_smode.mode = VT_AUTO;
			vw->vw_proc = NULL;
			vw->vw_pid = 0;
			DPRINTF(5, "VT_AUTO, ");
			if (vw == vw->vw_device->vd_windows[VT_CONSWINDOW])
				cnavailable(vw->vw_terminal->consdev, TRUE);
			/* were we in the middle of the vty switching process? */
			if (finish_vt_rel(vw, TRUE, &s) == 0)
				DPRINTF(5, "reset WAIT_REL, ");
			if (finish_vt_acq(vw) == 0)
				DPRINTF(5, "reset WAIT_ACQ, ");
			return (0);
		} else if (mode->mode == VT_PROCESS) {
			if (!ISSIGVALID(mode->relsig) ||
			    !ISSIGVALID(mode->acqsig) ||
			    !ISSIGVALID(mode->frsig)) {
				DPRINTF(5, "error EINVAL\n");
				return (EINVAL);
			}
			DPRINTF(5, "VT_PROCESS %d, ", td->td_proc->p_pid);
			bcopy(data, &vw->vw_smode, sizeof(struct vt_mode));
			vw->vw_proc = td->td_proc;
			vw->vw_pid = vw->vw_proc->p_pid;
			if (vw == vw->vw_device->vd_windows[VT_CONSWINDOW])
				cnavailable(vw->vw_terminal->consdev, FALSE);
		} else {
			DPRINTF(5, "VT_SETMODE failed, unknown mode %d\n",
			    mode->mode);
			return (EINVAL);
		}
		DPRINTF(5, "\n");
		return (0);
	}
	case VT_GETMODE:	/* get screen switcher mode */
		bcopy(&vw->vw_smode, data, sizeof(struct vt_mode));
		return (0);

	case VT_RELDISP:	/* screen switcher ioctl */
		/*
		 * This must be the current vty which is in the VT_PROCESS
		 * switching mode...
		 */
		if ((vw != vd->vd_curwindow) || (vw->vw_smode.mode !=
		    VT_PROCESS)) {
			return (EINVAL);
		}
		/* ...and this process is controlling it. */
		if (vw->vw_proc != td->td_proc) {
			return (EPERM);
		}
		error = EINVAL;
		switch(*(int *)data) {
		case VT_FALSE:	/* user refuses to release screen, abort */
			if ((error = finish_vt_rel(vw, FALSE, &s)) == 0)
				DPRINTF(5, "%s%d: VT_RELDISP: VT_FALSE\n",
				    SC_DRIVER_NAME, VT_UNIT(vw));
			break;
		case VT_TRUE:	/* user has released screen, go on */
			/* finish_vt_rel(..., TRUE, ...) should not be locked */
			if (vw->vw_flags & VWF_SWWAIT_REL) {
				if ((error = finish_vt_rel(vw, TRUE, &s)) == 0)
					DPRINTF(5, "%s%d: VT_RELDISP: VT_TRUE\n",
					    SC_DRIVER_NAME, VT_UNIT(vw));
			} else {
				error = EINVAL;
			}
			return (error);
		case VT_ACKACQ:	/* acquire acknowledged, switch completed */
			if ((error = finish_vt_acq(vw)) == 0)
				DPRINTF(5, "%s%d: VT_RELDISP: VT_ACKACQ\n",
				    SC_DRIVER_NAME, VT_UNIT(vw));
			break;
		default:
			break;
		}
		return (error);
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
	callout_init(&vw->vw_proc_dead_timer, 0);

	return (vw);
}

void
vt_upgrade(struct vt_device *vd)
{
	struct vt_window *vw;
	unsigned int i;

	/* Device didn't pass vd_init() or already upgraded. */
	if (vd->vd_flags & (VDF_ASYNC|VDF_DEAD))
		return;
	vd->vd_flags |= VDF_ASYNC;

	mtx_init(&vd->vd_lock, "vtdev", NULL, MTX_DEF);
	cv_init(&vd->vd_winswitch, "vtwswt");

	/* Init 25 Hz timer. */
	callout_init_mtx(&vd->vd_timer, &vd->vd_lock, 0);

	for (i = 0; i < VT_MAXWINDOWS; i++) {
		vw = vd->vd_windows[i];
		if (vw == NULL) {
			/* New window. */
			vw = vt_allocate_window(vd, i);
		}
		if (i == VT_CONSWINDOW) {
			/* Console window. */
			EVENTHANDLER_REGISTER(shutdown_pre_sync,
			    vt_window_switch, vw, SHUTDOWN_PRI_DEFAULT);
		}
		terminal_maketty(vw->vw_terminal, "v%r", VT_UNIT(vw));
	}
	if (vd->vd_curwindow == NULL)
		vd->vd_curwindow = vd->vd_windows[VT_CONSWINDOW];

	/* Attach keyboard. */
	vt_allocate_keyboard(vd);
	DPRINTF(20, "%s: vd_keyboard = %d\n", __func__, vd->vd_keyboard);

	/* Start timer when everything ready. */
	callout_reset(&vd->vd_timer, hz / VT_TIMERFREQ, vt_timer, vd);
}

static void
vt_resize(struct vt_device *vd)
{
	struct vt_window *vw;
	int i;

	for (i = 0; i < VT_MAXWINDOWS; i++) {
		vw = vd->vd_windows[i];
		/* Resize terminal windows */
		vt_change_font(vw, vw->vw_font);
	}
}

void
vt_allocate(struct vt_driver *drv, void *softc)
{
	struct vt_device *vd;
	struct winsize wsz;

	if (main_vd == NULL) {
		main_vd = malloc(sizeof *vd, M_VT, M_WAITOK|M_ZERO);
		printf("%s: VT initialize with new VT driver.\n", __func__);
	} else {
		/*
		 * Check if have rights to replace current driver. For example:
		 * it is bad idea to replace KMS driver with generic VGA one.
		 */
		if (drv->vd_priority <= main_vd->vd_driver->vd_priority) {
			printf("%s: Driver priority %d too low. Current %d\n ",
			    __func__, drv->vd_priority,
			    main_vd->vd_driver->vd_priority);
			return;
		}
		printf("%s: Replace existing VT driver.\n", __func__);
	}
	vd = main_vd;

	/* Stop vt_flush periodic task. */
	if (vd->vd_curwindow != NULL)
		callout_drain(&vd->vd_timer);

	vd->vd_driver = drv;
	vd->vd_softc = softc;
	vd->vd_driver->vd_init(vd);

	vt_upgrade(vd);

	/* Refill settings with new sizes. */
	vt_resize(vd);

#ifdef DEV_SPLASH
	if (vd->vd_flags & VDF_SPLASH)
		vtterm_splash(vd);
#endif

	if (vd->vd_curwindow != NULL)
		callout_schedule(&vd->vd_timer, hz / VT_TIMERFREQ);

	termcn_cnregister(vd->vd_windows[VT_CONSWINDOW]->vw_terminal);

	/* Update console window sizes to actual. */
	vt_winsize(vd, vd->vd_windows[VT_CONSWINDOW]->vw_font, &wsz);
	terminal_set_winsize(vd->vd_windows[VT_CONSWINDOW]->vw_terminal, &wsz);
}

void
vt_suspend()
{

	if (vt_suspendswitch == 0)
		return;
	/* Save current window. */
	main_vd->vd_savedwindow = main_vd->vd_curwindow;
	/* Ask holding process to free window and switch to console window */
	vt_proc_window_switch(main_vd->vd_windows[VT_CONSWINDOW]);
}

void
vt_resume()
{

	if (vt_suspendswitch == 0)
		return;
	/* Switch back to saved window */
	if (main_vd->vd_savedwindow != NULL)
		vt_proc_window_switch(main_vd->vd_savedwindow);
	main_vd->vd_savedwindow = NULL;
}
