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
 *	$Id: syscons.h,v 1.46 1999/01/19 11:31:19 yokota Exp $
 */

#ifndef _DEV_SYSCONS_SYSCONS_H_
#define	_DEV_SYSCONS_SYSCONS_H_

/* vm things */
#define	ISMAPPED(pa, width) \
	(((pa) <= (u_long)0x1000 - (width)) \
	 || ((pa) >= 0xa0000 && (pa) <= 0x100000 - (width)))
#define	pa_to_va(pa)	(KERNBASE + (pa))	/* works if ISMAPPED(pa...) */

/* printable chars */
#define PRINTABLE(ch)	((ch) > 0x1b || ((ch) > 0x0d && (ch) < 0x1b) \
			 || (ch) < 0x07)

/* macros for "intelligent" screen update */
#define mark_for_update(scp, x)	{\
			  	    if ((x) < scp->start) scp->start = (x);\
				    else if ((x) > scp->end) scp->end = (x);\
				}
#define mark_all(scp)		{\
				    scp->start = 0;\
				    scp->end = scp->xsize * scp->ysize - 1;\
				}

/* status flags */
#define UNKNOWN_MODE	0x00010
#define SWITCH_WAIT_REL	0x00080
#define SWITCH_WAIT_ACQ	0x00100
#define BUFFER_SAVED	0x00200
#define CURSOR_ENABLED 	0x00400
#define MOUSE_ENABLED	0x00800
#define MOUSE_MOVED	0x01000
#define MOUSE_CUTTING	0x02000
#define MOUSE_VISIBLE	0x04000
#define GRAPHICS_MODE	0x08000
#define PIXEL_MODE	0x10000
#define SAVER_RUNNING	0x20000

/* configuration flags */
#define VISUAL_BELL	0x00001
#define BLINK_CURSOR	0x00002
#define CHAR_CURSOR	0x00004
/* these options are now obsolete; use corresponding options for kbd driver */
#if 0
#define DETECT_KBD	0x00008
#define XT_KEYBD	0x00010
#define KBD_NORESET	0x00020
#endif
#define QUIET_BELL	0x00040
#define VESA800X600	0x00080
#define AUTODETECT_KBD	0x00100

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
#define BELL_DURATION	5
#define BELL_PITCH	800
#define CONSOLE_BUFSIZE 1024
#define PCBURST		128
#define FONT_NONE	1
#define FONT_8		2
#define FONT_14		4
#define FONT_16		8

/* special characters */
#define cntlc		0x03
#define cntld		0x04
#define bs		0x08
#define lf		0x0a
#define cr		0x0d
#define del		0x7f

#define DEAD_CHAR 	0x07			/* char used for cursor */

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

typedef struct scr_stat {
	int		ad;			/* video adapter index */
	video_adapter_t	*adp;			/* video adapter structure */
	u_short 	*scr_buf;		/* buffer when off screen */
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
	int		font_size;		/* fontsize in Y direction */
	int		start;			/* modified area start */
	int		end;			/* modified area end */
	term_stat 	term;			/* terminal emulation stuff */
	int	 	status;			/* status (bitfield) */
	int		kbd_mode;		/* keyboard I/O mode */
	u_short 	*cursor_pos;		/* cursor buffer position */
	u_short 	*cursor_oldpos;		/* cursor old buffer position */
	u_short		cursor_saveunder;	/* saved chars under cursor */
	char		cursor_start;		/* cursor start line # */
	char		cursor_end;		/* cursor end line # */
	u_short		*mouse_pos;		/* mouse buffer position */
	u_short		*mouse_oldpos;		/* mouse old buffer position */
	short		mouse_xpos;		/* mouse x coordinate */
	short		mouse_ypos;		/* mouse y coordinate */
	short		mouse_buttons;		/* mouse buttons */
	u_char		mouse_cursor[128];	/* mouse cursor bitmap store */
	u_short		*mouse_cut_start;	/* mouse cut start pos */
	u_short		*mouse_cut_end;		/* mouse cut end pos */
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
	u_short		*history;		/* circular history buffer */
	u_short		*history_head;		/* current head position */
	u_short		*history_pos;		/* position shown on screen */
	u_short		*history_save;		/* save area index */
	int		history_size;		/* size of history buffer */
#ifdef __i386__
	struct apmhook  r_hook;			/* reconfiguration support */
#endif
	int		splash_save_mode;	/* saved mode for splash screen */
	int		splash_save_status;	/* saved status for splash screen */
} scr_stat;

typedef struct default_attr {
	int             std_color;              /* normal hardware color */
	int             rev_color;              /* reverse hardware color */
} default_attr;


#define ISTEXTSC(scp)	(!((scp)->status 				\
			  & (UNKNOWN_MODE | GRAPHICS_MODE | PIXEL_MODE)))
#define ISGRAPHSC(scp)	(((scp)->status 				\
			  & (UNKNOWN_MODE | GRAPHICS_MODE)))
#define ISPIXELSC(scp)	(((scp)->status 				\
			  & (UNKNOWN_MODE | GRAPHICS_MODE | PIXEL_MODE))\
			  == PIXEL_MODE)
#define ISUNKNOWNSC(scp) ((scp)->status & UNKNOWN_MODE)

#define ISFONTAVAIL(af)	((af) & V_ADP_FONT)
#define ISMOUSEAVAIL(af) ((af) & V_ADP_FONT)
#define ISPALAVAIL(af)	((af) & V_ADP_PALETTE)

/* misc prototypes used by different syscons related LKM's */

/* syscons.c */
int sc_probe_unit(int unit, int flags);
int sc_attach_unit(int unit, int flags);

extern int (*sc_user_ioctl)(dev_t dev, u_long cmd, caddr_t data, int flag, 
			    struct proc *p);

int set_mode(scr_stat *scp);
scr_stat *sc_get_scr_stat(dev_t dev);

void copy_font(scr_stat *scp, int operation, int font_size, u_char *font_image);
void set_border(scr_stat *scp, int color);

void sc_touch_scrn_saver(void);
void sc_clear_screen(scr_stat *scp);
void sc_move_mouse(scr_stat *scp, int x, int y);
int sc_clean_up(scr_stat *scp);
void sc_alloc_scr_buffer(scr_stat *scp, int wait, int clear);
void sc_alloc_cut_buffer(scr_stat *scp, int wait);
void sc_alloc_history_buffer(scr_stat *scp, int lines, int extra, int wait);
struct tty *scdevtotty(dev_t dev);

/* scvidctl.c */
int sc_set_text_mode(scr_stat *scp, struct tty *tp, int mode,
		     int xsize, int ysize, int fontsize);
int sc_set_graphics_mode(scr_stat *scp, struct tty *tp, int mode);
int sc_set_pixel_mode(scr_stat *scp, struct tty *tp,
		      int xsize, int ysize, int fontsize);
int sc_vid_ioctl(struct tty *tp, u_long cmd, caddr_t data, int flag, 
		 struct proc *p);

#endif /* !_DEV_SYSCONS_SYSCONS_H_ */
