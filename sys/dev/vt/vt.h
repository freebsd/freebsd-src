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
 *
 * $FreeBSD$
 */

#ifndef _DEV_VT_VT_H_
#define	_DEV_VT_VT_H_

#include <sys/param.h>
#include <sys/_lock.h>
#include <sys/_mutex.h>
#include <sys/callout.h>
#include <sys/condvar.h>
#include <sys/conf.h>
#include <sys/consio.h>
#include <sys/kbio.h>
#include <sys/mouse.h>
#include <sys/terminal.h>
#include <sys/sysctl.h>

#include "opt_syscons.h"
#include "opt_splash.h"

#ifndef	VT_MAXWINDOWS
#ifdef	MAXCONS
#define	VT_MAXWINDOWS	MAXCONS
#else
#define	VT_MAXWINDOWS	12
#endif
#endif

#ifndef VT_ALT_TO_ESC_HACK
#define	VT_ALT_TO_ESC_HACK	1
#endif

#define	VT_CONSWINDOW	0

#if defined(SC_TWOBUTTON_MOUSE) || defined(VT_TWOBUTTON_MOUSE)
#define VT_MOUSE_PASTEBUTTON	MOUSE_BUTTON3DOWN	/* right button */
#define VT_MOUSE_EXTENDBUTTON	MOUSE_BUTTON2DOWN	/* not really used */
#else
#define VT_MOUSE_PASTEBUTTON	MOUSE_BUTTON2DOWN	/* middle button */
#define VT_MOUSE_EXTENDBUTTON	MOUSE_BUTTON3DOWN	/* right button */
#endif /* defined(SC_TWOBUTTON_MOUSE) || defined(VT_TWOBUTTON_MOUSE) */

#define	SC_DRIVER_NAME	"vt"
#ifdef VT_DEBUG
#define	DPRINTF(_l, ...)	if (vt_debug > (_l)) printf( __VA_ARGS__ )
#define VT_CONSOLECTL_DEBUG
#define VT_SYSMOUSE_DEBUG
#else
#define	DPRINTF(_l, ...)	do {} while (0)
#endif
#define	ISSIGVALID(sig)	((sig) > 0 && (sig) < NSIG)

#define	VT_SYSCTL_INT(_name, _default, _descr)				\
static int vt_##_name = _default;					\
SYSCTL_INT(_kern_vt, OID_AUTO, _name, CTLFLAG_RW, &vt_##_name, _default,\
		_descr);						\
TUNABLE_INT("kern.vt." #_name, &vt_##_name);

struct vt_driver;

void vt_allocate(struct vt_driver *, void *);
void vt_resume(void);
void vt_suspend(void);

typedef unsigned int 	vt_axis_t;

/*
 * List of locks
 * (d)	locked by vd_lock
 * (b)	locked by vb_lock
 * (G)	locked by Giant
 * (u)	unlocked, locked by higher levels
 * (c)	const until freeing
 * (?)	yet to be determined
 */

/*
 * Per-device datastructure.
 */

struct vt_device {
	struct vt_window	*vd_windows[VT_MAXWINDOWS]; /* (c) Windows. */
	struct vt_window	*vd_curwindow;	/* (d) Current window. */
	struct vt_window	*vd_savedwindow;/* (?) Saved for suspend. */
	struct vt_window	*vd_markedwin;	/* (?) Copy/paste buf owner. */
	const struct vt_driver	*vd_driver;	/* (c) Graphics driver. */
	void			*vd_softc;	/* (u) Driver data. */
	uint16_t		 vd_mx;		/* (?) Mouse X. */
	uint16_t		 vd_my;		/* (?) Mouse Y. */
	vt_axis_t		 vd_mdirtyx;	/* (?) Screen width. */
	vt_axis_t		 vd_mdirtyy;	/* (?) Screen height. */
	uint32_t		 vd_mstate;	/* (?) Mouse state. */
	term_pos_t		 vd_offset;	/* (?) Pixel offset. */
	vt_axis_t		 vd_width;	/* (?) Screen width. */
	vt_axis_t		 vd_height;	/* (?) Screen height. */
	struct mtx		 vd_lock;	/* Per-device lock. */
	struct cv		 vd_winswitch;	/* (d) Window switch notify. */
	struct callout		 vd_timer;	/* (d) Display timer. */
	int			 vd_flags;	/* (d) Device flags. */
#define	VDF_TEXTMODE	0x01	/* Do text mode rendering. */
#define	VDF_SPLASH	0x02	/* Splash screen active. */
#define	VDF_ASYNC	0x04	/* vt_timer() running. */
#define	VDF_INVALID	0x08	/* Entire screen should be re-rendered. */
#define	VDF_DEAD	0x10	/* Early probing found nothing. */
#define	VDF_INITIALIZED	0x20	/* vtterm_cnprobe already done. */
#define	VDF_MOUSECURSOR	0x40	/* Mouse cursor visible. */
#define	VDF_QUIET_BELL	0x80	/* Disable bell. */
	int			 vd_keyboard;	/* (G) Keyboard index. */
	unsigned int		 vd_kbstate;	/* (?) Device unit. */
	unsigned int		 vd_unit;	/* (c) Device unit. */
};

/*
 * Per-window terminal screen buffer.
 *
 * Because redrawing is performed asynchronously, the buffer keeps track
 * of a rectangle that needs to be redrawn (vb_dirtyrect).  Because this
 * approach seemed to cause suboptimal performance (when the top left
 * and the bottom right of the screen are modified), it also uses a set
 * of bitmasks to keep track of the rows and columns (mod 64) that have
 * been modified.
 */

struct vt_bufmask {
	uint64_t		 vbm_row, vbm_col;
#define	VBM_DIRTY		UINT64_MAX
};

struct vt_buf {
	struct mtx		 vb_lock;	/* Buffer lock. */
	term_pos_t		 vb_scr_size;	/* (b) Screen dimensions. */
	int			 vb_flags;	/* (b) Flags. */
#define	VBF_CURSOR	0x1	/* Cursor visible. */
#define	VBF_STATIC	0x2	/* Buffer is statically allocated. */
#define	VBF_MTX_INIT	0x4	/* Mutex initialized. */
#define	VBF_SCROLL	0x8	/* scroll locked mode. */
#define	VBF_HISTORY_FULL 0x10	/* All rows filled. */
	int			 vb_history_size;
#define	VBF_DEFAULT_HISTORY_SIZE	500
	int			 vb_roffset;	/* (b) History rows offset. */
	int			 vb_curroffset;	/* (b) Saved rows offset. */
	term_pos_t		 vb_cursor;	/* (u) Cursor position. */
	term_pos_t		 vb_mark_start;	/* (b) Copy region start. */
	term_pos_t		 vb_mark_end;	/* (b) Copy region end. */
	int			 vb_mark_last;	/* Last mouse event. */
	term_rect_t		 vb_dirtyrect;	/* (b) Dirty rectangle. */
	struct vt_bufmask	 vb_dirtymask;	/* (b) Dirty bitmasks. */
	term_char_t		*vb_buffer;	/* (u) Data buffer. */
	term_char_t		**vb_rows;	/* (u) Array of rows */
};

void vtbuf_copy(struct vt_buf *, const term_rect_t *, const term_pos_t *);
void vtbuf_fill_locked(struct vt_buf *, const term_rect_t *, term_char_t);
void vtbuf_init_early(struct vt_buf *);
void vtbuf_init(struct vt_buf *, const term_pos_t *);
void vtbuf_grow(struct vt_buf *, const term_pos_t *, int);
void vtbuf_putchar(struct vt_buf *, const term_pos_t *, term_char_t);
void vtbuf_cursor_position(struct vt_buf *, const term_pos_t *);
void vtbuf_scroll_mode(struct vt_buf *vb, int yes);
void vtbuf_undirty(struct vt_buf *, term_rect_t *, struct vt_bufmask *);
void vtbuf_sethistory_size(struct vt_buf *, int);
int vtbuf_iscursor(struct vt_buf *vb, int row, int col);
void vtbuf_cursor_visibility(struct vt_buf *, int);
#ifndef SC_NO_CUTPASTE
void vtbuf_mouse_cursor_position(struct vt_buf *vb, int col, int row);
int vtbuf_set_mark(struct vt_buf *vb, int type, int col, int row);
int vtbuf_get_marked_len(struct vt_buf *vb);
void vtbuf_extract_marked(struct vt_buf *vb, term_char_t *buf, int sz);
#endif

#define	VTB_MARK_NONE		0
#define	VTB_MARK_END		1
#define	VTB_MARK_START		2
#define	VTB_MARK_WORD		3
#define	VTB_MARK_ROW		4
#define	VTB_MARK_EXTEND		5
#define	VTB_MARK_MOVE		6

#define	VTBUF_SLCK_ENABLE(vb)	vtbuf_scroll_mode((vb), 1)
#define	VTBUF_SLCK_DISABLE(vb)	vtbuf_scroll_mode((vb), 0)

#define	VTBUF_MAX_HEIGHT(vb) \
	((vb)->vb_history_size)
#define	VTBUF_GET_ROW(vb, r) \
	((vb)->vb_rows[((vb)->vb_roffset + (r)) % VTBUF_MAX_HEIGHT(vb)])
#define	VTBUF_GET_FIELD(vb, r, c) \
	((vb)->vb_rows[((vb)->vb_roffset + (r)) % VTBUF_MAX_HEIGHT(vb)][(c)])
#define	VTBUF_FIELD(vb, r, c) \
	((vb)->vb_rows[((vb)->vb_curroffset + (r)) % VTBUF_MAX_HEIGHT(vb)][(c)])
#define	VTBUF_ISCURSOR(vb, r, c) \
	vtbuf_iscursor((vb), (r), (c))
#define	VTBUF_DIRTYROW(mask, row) \
	((mask)->vbm_row & ((uint64_t)1 << ((row) % 64)))
#define	VTBUF_DIRTYCOL(mask, col) \
	((mask)->vbm_col & ((uint64_t)1 << ((col) % 64)))
#define	VTBUF_SPACE_CHAR	(' ' | TC_WHITE << 26 | TC_BLACK << 29)

#define	VHS_SET	0
#define	VHS_CUR	1
#define	VHS_END	2
int vthistory_seek(struct vt_buf *, int offset, int whence);
void vthistory_addlines(struct vt_buf *vb, int offset);
void vthistory_getpos(const struct vt_buf *, unsigned int *offset);

/*
 * Per-window datastructure.
 */

struct vt_window {
	struct vt_device	*vw_device;	/* (c) Device. */
	struct terminal		*vw_terminal;	/* (c) Terminal. */
	struct vt_buf		 vw_buf;	/* (u) Screen buffer. */
	struct vt_font		*vw_font;	/* (d) Graphical font. */
	unsigned int		 vw_number;	/* (c) Window number. */
	int			 vw_kbdmode;	/* (?) Keyboard mode. */
	char			*vw_kbdsq;	/* Escape sequence queue*/
	unsigned int		 vw_flags;	/* (d) Per-window flags. */
	int			 vw_mouse_level;/* Mouse op mode. */
#define	VWF_BUSY	0x1	/* Busy reconfiguring device. */
#define	VWF_OPENED	0x2	/* TTY in use. */
#define	VWF_SCROLL	0x4	/* Keys influence scrollback. */
#define	VWF_CONSOLE	0x8	/* Kernel message console window. */
#define	VWF_VTYLOCK	0x10	/* Prevent window switch. */
#define	VWF_MOUSE_HIDE	0x20	/* Disable mouse events processing. */
#define	VWF_READY	0x40	/* Window fully initialized. */
#define	VWF_SWWAIT_REL	0x10000	/* Program wait for VT acquire is done. */
#define	VWF_SWWAIT_ACQ	0x20000	/* Program wait for VT release is done. */
	pid_t			 vw_pid;	/* Terminal holding process */
	struct proc		*vw_proc;
	struct vt_mode		 vw_smode;	/* switch mode */
	struct callout		 vw_proc_dead_timer;
	struct vt_window	*vw_switch_to;
};

#define	VT_AUTO		0		/* switching is automatic */
#define	VT_PROCESS	1		/* switching controlled by prog */
#define	VT_KERNEL	255		/* switching controlled in kernel */

#define	IS_VT_PROC_MODE(vw)	((vw)->vw_smode.mode == VT_PROCESS)

/*
 * Per-device driver routines.
 *
 * vd_bitbltchr is used when the driver operates in graphics mode, while
 * vd_putchar is used when the driver operates in text mode
 * (VDF_TEXTMODE).
 */

typedef int vd_init_t(struct vt_device *vd);
typedef int vd_probe_t(struct vt_device *vd);
typedef void vd_postswitch_t(struct vt_device *vd);
typedef void vd_blank_t(struct vt_device *vd, term_color_t color);
typedef void vd_bitbltchr_t(struct vt_device *vd, const uint8_t *src,
    const uint8_t *mask, int bpl, vt_axis_t top, vt_axis_t left,
    unsigned int width, unsigned int height, term_color_t fg, term_color_t bg);
typedef void vd_maskbitbltchr_t(struct vt_device *vd, const uint8_t *src,
    const uint8_t *mask, int bpl, vt_axis_t top, vt_axis_t left,
    unsigned int width, unsigned int height, term_color_t fg, term_color_t bg);
typedef void vd_putchar_t(struct vt_device *vd, term_char_t,
    vt_axis_t top, vt_axis_t left, term_color_t fg, term_color_t bg);
typedef int vd_fb_ioctl_t(struct vt_device *, u_long, caddr_t, struct thread *);
typedef int vd_fb_mmap_t(struct vt_device *, vm_ooffset_t, vm_paddr_t *, int,
    vm_memattr_t *);
typedef void vd_drawrect_t(struct vt_device *, int, int, int, int, int,
    term_color_t);
typedef void vd_setpixel_t(struct vt_device *, int, int, term_color_t);

struct vt_driver {
	char		 vd_name[16];
	/* Console attachment. */
	vd_probe_t	*vd_probe;
	vd_init_t	*vd_init;

	/* Drawing. */
	vd_blank_t	*vd_blank;
	vd_bitbltchr_t	*vd_bitbltchr;
	vd_maskbitbltchr_t *vd_maskbitbltchr;
	vd_drawrect_t	*vd_drawrect;
	vd_setpixel_t	*vd_setpixel;

	/* Framebuffer ioctls, if present. */
	vd_fb_ioctl_t	*vd_fb_ioctl;

	/* Framebuffer mmap, if present. */
	vd_fb_mmap_t	*vd_fb_mmap;

	/* Text mode operation. */
	vd_putchar_t	*vd_putchar;

	/* Update display setting on vt switch. */
	vd_postswitch_t	*vd_postswitch;

	/* Priority to know which one can override */
	int		vd_priority;
#define	VD_PRIORITY_DUMB	10
#define	VD_PRIORITY_GENERIC	100
#define	VD_PRIORITY_SPECIFIC	1000
};

/*
 * Console device madness.
 *
 * Utility macro to make early vt(4) instances work.
 */

extern const struct terminal_class vt_termclass;
void vt_upgrade(struct vt_device *vd);

#define	PIXEL_WIDTH(w)	((w) / 8)
#define	PIXEL_HEIGHT(h)	((h) / 16)

#ifndef VT_FB_DEFAULT_WIDTH
#define	VT_FB_DEFAULT_WIDTH	2048
#endif
#ifndef VT_FB_DEFAULT_HEIGHT
#define	VT_FB_DEFAULT_HEIGHT	1200
#endif

#define	VT_CONSDEV_DECLARE(driver, width, height, softc)		\
static struct terminal	driver ## _consterm;				\
static struct vt_window	driver ## _conswindow;				\
static struct vt_device	driver ## _consdev = {				\
	.vd_driver = &driver,						\
	.vd_softc = (softc),						\
	.vd_flags = VDF_INVALID,					\
	.vd_windows = { [VT_CONSWINDOW] =  &driver ## _conswindow, },	\
	.vd_curwindow = &driver ## _conswindow,				\
	.vd_markedwin = NULL,						\
	.vd_kbstate = 0,						\
};									\
static term_char_t	driver ## _constextbuf[(width) * 		\
	    (VBF_DEFAULT_HISTORY_SIZE)];				\
static term_char_t	*driver ## _constextbufrows[			\
	    VBF_DEFAULT_HISTORY_SIZE];					\
static struct vt_window	driver ## _conswindow = {			\
	.vw_number = VT_CONSWINDOW,					\
	.vw_flags = VWF_CONSOLE,					\
	.vw_buf = {							\
		.vb_buffer = driver ## _constextbuf,			\
		.vb_rows = driver ## _constextbufrows,			\
		.vb_history_size = VBF_DEFAULT_HISTORY_SIZE,		\
		.vb_curroffset = 0,					\
		.vb_roffset = 0,					\
		.vb_flags = VBF_STATIC,					\
		.vb_mark_start = {					\
			.tp_row = 0,					\
			.tp_col = 0,					\
		},							\
		.vb_mark_end = {					\
			.tp_row = 0,					\
			.tp_col = 0,					\
		},							\
		.vb_scr_size = {					\
			.tp_row = height,				\
			.tp_col = width,				\
		},							\
	},								\
	.vw_device = &driver ## _consdev,				\
	.vw_terminal = &driver ## _consterm,				\
	.vw_kbdmode = K_XLATE,						\
};									\
TERMINAL_DECLARE_EARLY(driver ## _consterm, vt_termclass,		\
    &driver ## _conswindow);						\
SYSINIT(vt_early_cons, SI_SUB_INT_CONFIG_HOOKS, SI_ORDER_ANY,		\
    vt_upgrade, &driver ## _consdev)

/* name argument is not used yet. */
#define VT_DRIVER_DECLARE(name, drv) DATA_SET(vt_drv_set, drv)

/*
 * Fonts.
 *
 * Remapping tables are used to map Unicode points to glyphs.  They need
 * to be sorted, because vtfont_lookup() performs a binary search.  Each
 * font has two remapping tables, for normal and bold.  When a character
 * is not present in bold, it uses a normal glyph.  When no glyph is
 * available, it uses glyph 0, which is normally equal to U+FFFD.
 */

struct vt_font_map {
	uint32_t		 vfm_src;
	uint16_t		 vfm_dst;
	uint16_t		 vfm_len;
};

struct vt_font {
	struct vt_font_map	*vf_map[VFNT_MAPS];
	uint8_t			*vf_bytes;
	unsigned int		 vf_height, vf_width;
	unsigned int		 vf_map_count[VFNT_MAPS];
	unsigned int		 vf_refcount;
};

#ifndef SC_NO_CUTPASTE
struct mouse_cursor {
	uint8_t map[64 * 64 / 8];
	uint8_t mask[64 * 64 / 8];
	uint8_t w;
	uint8_t h;
};
#endif

const uint8_t	*vtfont_lookup(const struct vt_font *vf, term_char_t c);
struct vt_font	*vtfont_ref(struct vt_font *vf);
void		 vtfont_unref(struct vt_font *vf);
int		 vtfont_load(vfnt_t *f, struct vt_font **ret);

/* Sysmouse. */
void sysmouse_process_event(mouse_info_t *mi);
#ifndef SC_NO_CUTPASTE
void vt_mouse_event(int type, int x, int y, int event, int cnt, int mlevel);
void vt_mouse_state(int show);
#endif
#define	VT_MOUSE_SHOW 1
#define	VT_MOUSE_HIDE 0

#endif /* !_DEV_VT_VT_H_ */

