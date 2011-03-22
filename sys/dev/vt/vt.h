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
#include <sys/consio.h>
#include <sys/kbio.h>
#include <sys/terminal.h>

#define	VT_MAXWINDOWS	12
#define	VT_CONSWINDOW	0

struct vt_driver;

void vt_allocate(struct vt_driver *, void *);

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
	const struct vt_driver	*vd_driver;	/* (c) Graphics driver. */
	void			*vd_softc;	/* (u) Driver data. */
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
	int			 vd_keyboard;	/* (G) Keyboard index. */
	unsigned int		 vd_unit;	/* (c) Device unit. */
	/* XXX: HACK */
	unsigned int		 vd_scrollpos;	/* (d) Last scroll position. */
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
	term_pos_t		 vb_size;	/* (b) Screen dimensions. */
	int			 vb_flags;	/* (b) Flags. */
#define	VBF_CURSOR	0x1	/* Cursor visible. */
#define	VBF_STATIC	0x2	/* Buffer is statically allocated. */
	term_pos_t		 vb_cursor;	/* (u) Cursor position. */
	term_rect_t		 vb_dirtyrect;	/* (b) Dirty rectangle. */
	struct vt_bufmask	 vb_dirtymask;	/* (b) Dirty bitmasks. */
	term_char_t		*vb_buffer;	/* (u) Data buffer. */
};

void vtbuf_copy(struct vt_buf *, const term_rect_t *, const term_pos_t *);
void vtbuf_fill(struct vt_buf *, const term_rect_t *, term_char_t);
void vtbuf_init_early(struct vt_buf *);
void vtbuf_init(struct vt_buf *, const term_pos_t *);
void vtbuf_grow(struct vt_buf *, const term_pos_t *);
void vtbuf_putchar(struct vt_buf *, const term_pos_t *, term_char_t);
void vtbuf_cursor_position(struct vt_buf *, const term_pos_t *);
void vtbuf_cursor_visibility(struct vt_buf *, int);
void vtbuf_undirty(struct vt_buf *, term_rect_t *, struct vt_bufmask *);
#define	VTBUF_FIELD(vb, r, c) \
	(vb)->vb_buffer[(r) * (vb)->vb_size.tp_col + (c)]
#define	VTBUF_ISCURSOR(vb, r, c) \
	((vb)->vb_flags & VBF_CURSOR && \
	(vb)->vb_cursor.tp_row == (r) && (vb)->vb_cursor.tp_col == (c))
#define	VTBUF_DIRTYROW(mask, row) \
	((mask)->vbm_row & ((uint64_t)1 << ((row) % 64)))
#define	VTBUF_DIRTYCOL(mask, col) \
	((mask)->vbm_col & ((uint64_t)1 << ((col) % 64)))

/*
 * Per-window history tracking.
 *
 * XXX: Unimplemented!
 */

struct vt_history {
	unsigned int	vh_offset;
};

void vthistory_add(struct vt_history *vh, struct vt_buf *vb,
    const term_rect_t *r);
#define	VHS_SET	0
#define	VHS_CUR	1
#define	VHS_END	2
void vthistory_seek(struct vt_history *vh, int offset, int whence);
void vthistory_getpos(const struct vt_history *vh, unsigned int *offset);
#define	VTHISTORY_FIELD(vh, r, c) \
	('?' | (TF_BOLD|TF_REVERSE) << 22 | TC_GREEN << 26 | TC_BLACK << 29)

/*
 * Per-window datastructure.
 */

struct vt_window {
	struct vt_device	*vw_device;	/* (c) Device. */
	struct terminal		*vw_terminal;	/* (c) Terminal. */
	struct vt_buf		 vw_buf;	/* (u) Screen buffer. */
	struct vt_history	 vw_history;	/* (?) History buffer. */
	struct vt_font		*vw_font;	/* (d) Graphical font. */
	unsigned int		 vw_number;	/* (c) Window number. */
	int			 vw_kbdmode;	/* (?) Keyboard mode. */
	unsigned int		 vw_flags;	/* (d) Per-window flags. */
#define	VWF_BUSY	0x1	/* Busy reconfiguring device. */
#define	VWF_OPENED	0x2	/* TTY in use. */
#define	VWF_SCROLL	0x4	/* Keys influence scrollback. */
#define	VWF_CONSOLE	0x8	/* Kernel message console window. */
};

/*
 * Per-device driver routines.
 *
 * vd_bitblt is used when the driver operates in graphics mode, while
 * vd_putchar is used when the driver operates in text mode
 * (VDF_TEXTMODE).
 */

typedef int vd_init_t(struct vt_device *vd);
typedef void vd_blank_t(struct vt_device *vd, term_color_t color);
typedef void vd_bitblt_t(struct vt_device *vd, const uint8_t *src,
    vt_axis_t top, vt_axis_t left, unsigned int width, unsigned int height,
    term_color_t fg, term_color_t bg);
typedef void vd_putchar_t(struct vt_device *vd, term_char_t,
    vt_axis_t top, vt_axis_t left, term_color_t fg, term_color_t bg);

struct vt_driver {
	/* Console attachment. */
	vd_init_t		*vd_init;

	/* Drawing. */
	vd_blank_t		*vd_blank;
	vd_bitblt_t		*vd_bitblt;

	/* Text mode operation. */
	vd_putchar_t		*vd_putchar;
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

#define	VT_CONSDEV_DECLARE(driver, width, height, softc)		\
static struct terminal	driver ## _consterm;				\
static struct vt_window	driver ## _conswindow;				\
static struct vt_device	driver ## _consdev = {				\
	.vd_driver = &driver,						\
	.vd_softc = (softc),						\
	.vd_flags = VDF_INVALID,					\
	.vd_windows = { [VT_CONSWINDOW] =  &driver ## _conswindow, },	\
	.vd_curwindow = &driver ## _conswindow,				\
};									\
static term_char_t	driver ## _constextbuf[(width) * (height)];	\
static struct vt_window	driver ## _conswindow = {			\
	.vw_number = VT_CONSWINDOW,					\
	.vw_flags = VWF_CONSOLE,					\
	.vw_buf = {							\
		.vb_buffer = driver ## _constextbuf,			\
		.vb_flags = VBF_STATIC,					\
		.vb_size = {						\
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
	struct vt_font_map	*vf_bold;
	struct vt_font_map	*vf_normal;
	uint8_t			*vf_bytes;
	unsigned int		 vf_height, vf_width,
				 vf_normal_length, vf_bold_length;
	unsigned int		 vf_refcount;
};

const uint8_t	*vtfont_lookup(const struct vt_font *vf, term_char_t c);
struct vt_font	*vtfont_ref(struct vt_font *vf);
void		 vtfont_unref(struct vt_font *vf);
int		 vtfont_load(vfnt_t *f, struct vt_font **ret);

/* Sysmouse. */
void sysmouse_process_event(mouse_info_t *mi);

#endif /* !_DEV_VT_VT_H_ */
