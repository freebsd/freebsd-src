/*-
 * Copyright (c) 1995-1998 Søren Schmidt
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
 *	$Id: syscons.h,v 1.49 1999/06/24 13:04:33 yokota Exp $
 */

#ifndef _DEV_SYSCONS_SYSCONS_H_
#define	_DEV_SYSCONS_SYSCONS_H_

/* machine-dependent part of the header */

#ifdef PC98
#include <pc98/pc98/sc_machdep.h>
#elif defined(__i386__)
/* nothing for the moment */
#elif defined(__alpha__)
/* nothing for the moment */
#endif

/* default values for configuration options */

#ifndef MAXCONS
#define MAXCONS		16
#endif

#ifdef SC_NO_SYSMOUSE
#undef SC_NO_CUTPASTE
#define SC_NO_CUTPASTE	1
#endif

#ifdef SC_NO_MODE_CHANGE
#undef SC_PIXEL_MODE
#endif

#ifndef SC_DEBUG_LEVEL
#define SC_DEBUG_LEVEL	0
#endif

#define DPRINTF(l, p)	if (SC_DEBUG_LEVEL >= (l)) printf p

#define SC_DRIVER_NAME	"sc"
#define SC_VTY(dev)	minor(dev)

/* printable chars */
#ifndef PRINTABLE
#define PRINTABLE(ch)	((ch) > 0x1b || ((ch) > 0x0d && (ch) < 0x1b) \
			 || (ch) < 0x07)
#endif

/* macros for "intelligent" screen update */
#define mark_for_update(scp, x)	{\
			  	    if ((x) < scp->start) scp->start = (x);\
				    else if ((x) > scp->end) scp->end = (x);\
				}
#define mark_all(scp)		{\
				    scp->start = 0;\
				    scp->end = scp->xsize * scp->ysize - 1;\
				}

/* vty status flags (scp->status) */
#define UNKNOWN_MODE	0x00010
#define SWITCH_WAIT_REL	0x00080
#define SWITCH_WAIT_ACQ	0x00100
#define BUFFER_SAVED	0x00200
#define CURSOR_ENABLED 	0x00400
#define MOUSE_MOVED	0x01000
#define MOUSE_CUTTING	0x02000
#define MOUSE_VISIBLE	0x04000
#define GRAPHICS_MODE	0x08000
#define PIXEL_MODE	0x10000
#define SAVER_RUNNING	0x20000
#define VR_CURSOR_BLINK	0x40000
#define VR_CURSOR_ON	0x80000

/* attribute flags */
#define NORMAL_ATTR             0x00
#define BLINK_ATTR              0x01
#define BOLD_ATTR               0x02
#define UNDERLINE_ATTR          0x04
#define REVERSE_ATTR            0x08
#define FOREGROUND_CHANGED      0x10
#define BACKGROUND_CHANGED      0x20

/* misc defines */
#define FALSE		0
#define TRUE		1
#define MAX_ESC_PAR 	5
#define	LOAD		1
#define SAVE		0
#define	COL		80
#define	ROW		25
#define CONSOLE_BUFSIZE 1024
#define PCBURST		128
#define FONT_NONE	1
#define FONT_8		2
#define FONT_14		4
#define FONT_16		8

#ifndef BELL_DURATION
#define BELL_DURATION	5
#define BELL_PITCH	800
#endif

/* special characters */
#define cntlc		0x03
#define cntld		0x04
#define bs		0x08
#define lf		0x0a
#define cr		0x0d
#define del		0x7f

#define DEAD_CHAR 	0x07			/* char used for cursor */

/* virtual terminal buffer */
typedef struct sc_vtb {
	int		vtb_flags;
#define VTB_VALID	(1 << 0)
	int		vtb_type;
#define VTB_INVALID	0
#define VTB_MEMORY	1
#define VTB_FRAMEBUFFER	2
#define VTB_RINGBUFFER	3
	int		vtb_cols;
	int		vtb_rows;
	int		vtb_size;
	vm_offset_t	vtb_buffer;
	int		vtb_tail;	/* valid for VTB_RINGBUFFER only */
} sc_vtb_t;

/* terminal status */
typedef struct term_stat {
	int 		esc;			/* processing escape sequence */
	int 		num_param;		/* # of parameters to ESC */
	int	 	last_param;		/* last parameter # */
	int 		param[MAX_ESC_PAR];	/* contains ESC parameters */
	int             cur_attr;               /* current hardware attr word */
	int             attr_mask;              /* current logical attr mask */
	int             cur_color;              /* current hardware color */
	int             std_color;              /* normal hardware color */
	int             rev_color;              /* reverse hardware color */
} term_stat;

/* softc */

struct keyboard;
struct video_adapter;
struct scr_stat;
struct tty;

typedef struct sc_softc {
	int		unit;			/* unit # */
	int		config;			/* configuration flags */
#define SC_VESA800X600	(1 << 7)
#define SC_AUTODETECT_KBD (1 << 8)
#define SC_KERNEL_CONSOLE (1 << 9)

	int		flags;			/* status flags */
#define SC_VISUAL_BELL	(1 << 0)
#define SC_QUIET_BELL	(1 << 1)
#define SC_BLINK_CURSOR	(1 << 2)
#define SC_CHAR_CURSOR	(1 << 3)
#define SC_MOUSE_ENABLED (1 << 4)
#define	SC_SCRN_IDLE	(1 << 5)
#define	SC_SCRN_BLANKED	(1 << 6)
#define	SC_SAVER_FAILED	(1 << 7)

#define	SC_INIT_DONE	(1 << 16)
#define	SC_SPLASH_SCRN	(1 << 17)

	int		keyboard;		/* -1 if unavailable */
	struct keyboard	*kbd;

	int		adapter;
	struct video_adapter *adp;
	int		initial_mode;		/* initial video mode */

	int		first_vty;
	int		vtys;
	struct tty	*tty;
	struct scr_stat	**console;
	struct scr_stat	*cur_scp;
	struct scr_stat	*new_scp;
	struct scr_stat	*old_scp;
	int     	delayed_next_scr;

	void		**devfs_token;

	char        	font_loading_in_progress;
	char        	switch_in_progress;
	char        	videoio_in_progress;
	char        	write_in_progress;
	char        	blink_in_progress;

	long		scrn_time_stamp;

	char		cursor_base;
	char		cursor_height;

	u_char      	scr_map[256];
	u_char      	scr_rmap[256];

#ifdef _SC_MD_SOFTC_DECLARED_
	sc_md_softc_t	md;			/* machine dependent vars */
#endif

#ifndef SC_NO_PALETTE_LOADING
	u_char        	palette[256*3];
#endif

#ifndef SC_NO_FONT_LOADING
	int     	fonts_loaded;
	u_char		*font_8;
	u_char		*font_14;
	u_char		*font_16;
#endif

} sc_softc_t;

/* virtual screen */
typedef struct scr_stat {
	int		index;			/* index of this vty */
	struct sc_softc *sc;			/* pointer to softc */
	struct sc_rndr_sw *rndr;		/* renderer */
	sc_vtb_t	scr;
	sc_vtb_t	vtb;
	int 		xpos;			/* current X position */
	int 		ypos;			/* current Y position */
	int             saved_xpos;             /* saved X position */
	int             saved_ypos;             /* saved Y position */
	int 		xsize;			/* X text size */
	int 		ysize;			/* Y text size */
	int 		xpixel;			/* X graphics size */
	int 		ypixel;			/* Y graphics size */
	int		xoff;			/* X offset in pixel mode */
	int		yoff;			/* Y offset in pixel mode */
	u_char		*font;			/* current font */
	int		font_size;		/* fontsize in Y direction */
	int		start;			/* modified area start */
	int		end;			/* modified area end */
	term_stat 	term;			/* terminal emulation stuff */
	int	 	status;			/* status (bitfield) */
	int		kbd_mode;		/* keyboard I/O mode */
	int		cursor_pos;		/* cursor buffer position */
	int		cursor_oldpos;		/* cursor old buffer position */
	u_short		cursor_saveunder_char;	/* saved char under cursor */
	u_short		cursor_saveunder_attr;	/* saved attr under cursor */
	char		cursor_base;		/* cursor base line # */
	char		cursor_height;		/* cursor height */
	int		mouse_pos;		/* mouse buffer position */
	int		mouse_oldpos;		/* mouse old buffer position */
	short		mouse_xpos;		/* mouse x coordinate */
	short		mouse_ypos;		/* mouse y coordinate */
	short		mouse_buttons;		/* mouse buttons */
	int		mouse_cut_start;	/* mouse cut start pos */
	int		mouse_cut_end;		/* mouse cut end pos */
	struct proc 	*mouse_proc;		/* proc* of controlling proc */
	pid_t 		mouse_pid;		/* pid of controlling proc */
	int		mouse_signal;		/* signal # to report with */
	u_short		bell_duration;
	u_short		bell_pitch;
	u_char		border;			/* border color */
	int	 	mode;			/* mode */
	pid_t 		pid;			/* pid of controlling proc */
	struct proc 	*proc;			/* proc* of controlling proc */
	struct vt_mode 	smode;			/* switch mode */
	sc_vtb_t	*history;		/* circular history buffer */
	int		history_pos;		/* position shown on screen */
	int		history_size;		/* size of history buffer */
	int		splash_save_mode;	/* saved mode for splash screen */
	int		splash_save_status;	/* saved status for splash screen */
#ifdef _SCR_MD_STAT_DECLARED_
	scr_md_stat_t	md;			/* machine dependent vars */
#endif
} scr_stat;

typedef struct default_attr {
	int             std_color;              /* normal hardware color */
	int             rev_color;              /* reverse hardware color */
} default_attr;

#ifndef SC_NORM_ATTR
#define SC_NORM_ATTR		(FG_LIGHTGREY | BG_BLACK)
#endif
#ifndef SC_NORM_REV_ATTR
#define SC_NORM_REV_ATTR	(FG_BLACK | BG_LIGHTGREY)
#endif
#ifndef SC_KERNEL_CONS_ATTR
#define SC_KERNEL_CONS_ATTR	(FG_WHITE | BG_BLACK)
#endif
#ifndef SC_KERNEL_CONS_REV_ATTR
#define SC_KERNEL_CONS_REV_ATTR	(FG_BLACK | BG_LIGHTGREY)
#endif

/* renderer function table */
typedef void	vr_clear_t(scr_stat *scp, int c, int attr);
typedef void	vr_draw_border_t(scr_stat *scp, int color);
typedef void	vr_draw_t(scr_stat *scp, int from, int count, int flip);
typedef void	vr_set_cursor_t(scr_stat *scp, int base, int height, int blink);
typedef void	vr_draw_cursor_t(scr_stat *scp, int at, int blink,
				 int on, int flip);
typedef void	vr_blink_cursor_t(scr_stat *scp, int at, int flip);
typedef void	vr_set_mouse_t(scr_stat *scp);
typedef void	vr_draw_mouse_t(scr_stat *scp, int x, int y, int on);

typedef struct sc_rndr_sw {
	vr_clear_t		*clear;
	vr_draw_border_t	*draw_border;
	vr_draw_t		*draw;
	vr_set_cursor_t		*set_cursor;
	vr_draw_cursor_t	*draw_cursor;
	vr_blink_cursor_t	*blink_cursor;
	vr_set_mouse_t		*set_mouse;
	vr_draw_mouse_t		*draw_mouse;
} sc_rndr_sw_t;

typedef struct sc_renderer {
	char		*name;
	int		mode;
	sc_rndr_sw_t	*rndrsw;
} sc_renderer_t;

#define RENDERER(name, mode, sw)				\
	static struct sc_renderer name##_##mode##_renderer = {	\
		#name, mode, &sw				\
	};							\
	DATA_SET(scrndr_set, name##_##mode##_renderer)

extern struct linker_set scrndr_set;

typedef struct {
	int		cursor_start;
	int		cursor_end;
	int		shift_state;
	int		bell_pitch;
} bios_values_t;

/* other macros */
#define ISTEXTSC(scp)	(!((scp)->status 				\
			  & (UNKNOWN_MODE | GRAPHICS_MODE | PIXEL_MODE)))
#define ISGRAPHSC(scp)	(((scp)->status 				\
			  & (UNKNOWN_MODE | GRAPHICS_MODE)))
#define ISPIXELSC(scp)	(((scp)->status 				\
			  & (UNKNOWN_MODE | GRAPHICS_MODE | PIXEL_MODE))\
			  == PIXEL_MODE)
#define ISUNKNOWNSC(scp) ((scp)->status & UNKNOWN_MODE)

#ifndef ISMOUSEAVAIL
#ifdef SC_ALT_MOUSE_IMAGE
#define ISMOUSEAVAIL(af) (1)
#else
#define ISMOUSEAVAIL(af) ((af) & V_ADP_FONT)
#endif /* SC_ALT_MOUSE_IMAGE */
#define ISFONTAVAIL(af)	((af) & V_ADP_FONT)
#define ISPALAVAIL(af)	((af) & V_ADP_PALETTE)
#endif /* ISMOUSEAVAIL */

#define ISSIGVALID(sig)	((sig) > 0 && (sig) < NSIG)

#define kbd_read_char(kbd, wait)					\
		(*kbdsw[(kbd)->kb_index]->read_char)((kbd), (wait))
#define kbd_check_char(kbd)						\
		(*kbdsw[(kbd)->kb_index]->check_char)((kbd))
#define kbd_enable(kbd)							\
		(*kbdsw[(kbd)->kb_index]->enable)((kbd))
#define kbd_disable(kbd)						\
		(*kbdsw[(kbd)->kb_index]->disable)((kbd))
#define kbd_lock(kbd, lockf)						\
		(*kbdsw[(kbd)->kb_index]->lock)((kbd), (lockf))
#define kbd_ioctl(kbd, cmd, arg)					\
	    (((kbd) == NULL) ?						\
		ENODEV : (*kbdsw[(kbd)->kb_index]->ioctl)((kbd), (cmd), (arg)))
#define kbd_clear_state(kbd)						\
		(*kbdsw[(kbd)->kb_index]->clear_state)((kbd))
#define kbd_get_fkeystr(kbd, fkey, len)					\
		(*kbdsw[(kbd)->kb_index]->get_fkeystr)((kbd), (fkey), (len))
#define kbd_poll(kbd, on)						\
		(*kbdsw[(kbd)->kb_index]->poll)((kbd), (on))

/* syscons.c */
extern int 	(*sc_user_ioctl)(dev_t dev, u_long cmd, caddr_t data,
				 int flag, struct proc *p);

int		sc_probe_unit(int unit, int flags);
int		sc_attach_unit(int unit, int flags);
int		sc_resume_unit(int unit);

int		set_mode(scr_stat *scp);
scr_stat	*sc_get_scr_stat(dev_t dev);

void		copy_font(scr_stat *scp, int operation, int font_size,
			  u_char *font_image);
void		set_border(scr_stat *scp, int color);

void		sc_touch_scrn_saver(void);
void		sc_clear_screen(scr_stat *scp);
void		sc_set_cursor_image(scr_stat *scp);
int		sc_clean_up(scr_stat *scp);
void		sc_alloc_scr_buffer(scr_stat *scp, int wait, int discard);
struct tty	*scdevtotty(dev_t dev);
#ifndef SC_NO_SYSMOUSE
struct tty	*sc_get_mouse_tty(void);
#endif /* SC_NO_SYSMOUSE */
#ifndef SC_NO_CUTPASTE
void		sc_paste(scr_stat *scp, u_char *p, int count);
#endif /* SC_NO_CUTPASTE */

/* schistory.c */
#ifndef SC_NO_HISTORY
int		sc_alloc_history_buffer(scr_stat *scp, int lines,
					int prev_ysize, int wait);
void		sc_free_history_buffer(scr_stat *scp, int prev_ysize);
void		sc_hist_save(scr_stat *scp);
#define		sc_hist_save_one_line(scp, from)	\
		sc_vtb_append(&(scp)->vtb, (from), (scp)->history, (scp)->xsize)
int		sc_hist_restore(scr_stat *scp);
void		sc_hist_home(scr_stat *scp);
void		sc_hist_end(scr_stat *scp);
int		sc_hist_up_line(scr_stat *scp);
int		sc_hist_down_line(scr_stat *scp);
int		sc_hist_ioctl(struct tty *tp, u_long cmd, caddr_t data,
			      int flag, struct proc *p);
#endif /* SC_NO_HISTORY */

/* scmouse.c */
#ifndef SC_NO_CUTPASTE
void		sc_alloc_cut_buffer(scr_stat *scp, int wait);
void		sc_draw_mouse_image(scr_stat *scp); 
void		sc_remove_mouse_image(scr_stat *scp); 
int		sc_inside_cutmark(scr_stat *scp, int pos);
void		sc_remove_cutmarking(scr_stat *scp);
void		sc_remove_all_cutmarkings(sc_softc_t *scp);
void		sc_remove_all_mouse(sc_softc_t *scp);
#else
#define		sc_inside_cutmark(scp, pos)	FALSE
#define		sc_remove_cutmarking(scp)
#endif /* SC_NO_CUTPASTE */
#ifndef SC_NO_SYSMOUSE
void		sc_mouse_set_level(int level);
void		sc_mouse_move(scr_stat *scp, int x, int y);
int		sc_mouse_ioctl(struct tty *tp, u_long cmd, caddr_t data,
			       int flag, struct proc *p);
#endif /* SC_NO_SYSMOUSE */

/* scvidctl.c */
int		sc_set_text_mode(scr_stat *scp, struct tty *tp, int mode,
				 int xsize, int ysize, int fontsize);
int		sc_set_graphics_mode(scr_stat *scp, struct tty *tp, int mode);
int		sc_set_pixel_mode(scr_stat *scp, struct tty *tp,
				  int xsize, int ysize, int fontsize);
sc_rndr_sw_t	*sc_render_match(scr_stat *scp, video_adapter_t *adp, int mode);
int		sc_vid_ioctl(struct tty *tp, u_long cmd, caddr_t data, int flag,
			     struct proc *p);

/* scvtb.c */
void		sc_vtb_init(sc_vtb_t *vtb, int type, int cols, int rows, 
			    void *buffer, int wait);
void		sc_vtb_destroy(sc_vtb_t *vtb);
size_t		sc_vtb_size(int cols, int rows);
void		sc_vtb_clear(sc_vtb_t *vtb, int c, int attr);

int		sc_vtb_getc(sc_vtb_t *vtb, int at);
int		sc_vtb_geta(sc_vtb_t *vtb, int at);
void		sc_vtb_putc(sc_vtb_t *vtb, int at, int c, int a);
vm_offset_t	sc_vtb_putchar(sc_vtb_t *vtb, vm_offset_t p, int c, int a);
vm_offset_t	sc_vtb_pointer(sc_vtb_t *vtb, int at);
int		sc_vtb_pos(sc_vtb_t *vtb, int pos, int offset);

#define		sc_vtb_tail(vtb)	((vtb)->vtb_tail)
#define		sc_vtb_rows(vtb)	((vtb)->vtb_rows)

void		sc_vtb_copy(sc_vtb_t *vtb1, int from, sc_vtb_t *vtb2, int to,
			    int count);
void		sc_vtb_append(sc_vtb_t *vtb1, int from, sc_vtb_t *vtb2,
			      int count);
void		sc_vtb_seek(sc_vtb_t *vtb, int pos);
void		sc_vtb_erase(sc_vtb_t *vtb, int at, int count, int c, int attr);
void		sc_vtb_move(sc_vtb_t *vtb, int from, int to, int count);
void		sc_vtb_delete(sc_vtb_t *vtb, int at, int count, int c, int attr);
void		sc_vtb_ins(sc_vtb_t *vtb, int at, int count, int c, int attr);

/* machine dependent functions */
int		sc_max_unit(void);
sc_softc_t	*sc_get_softc(int unit, int flags);
sc_softc_t	*sc_find_softc(struct video_adapter *adp, struct keyboard *kbd);
int		sc_get_cons_priority(int *unit, int *flags);
void		sc_get_bios_values(bios_values_t *values);
int		sc_tone(int herz);

#endif /* !_DEV_SYSCONS_SYSCONS_H_ */
