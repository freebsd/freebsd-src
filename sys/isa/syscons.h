/*-
 * Copyright (c) 1995 Søren Schmidt
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in this position and unchanged.
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
 *	$Id: syscons.h,v 1.14 1995/12/10 13:39:20 phk Exp $
 */

#ifndef _I386_ISA_SYSCONS_H_
#define	_I386_ISA_SYSCONS_H_

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
				    scp->end = scp->xsize * scp->ysize;\
				}

/* status flags */
#define LOCK_KEY_MASK	0x0000F
#define LED_MASK	0x00007
#define UNKNOWN_MODE	0x00010
#define KBD_RAW_MODE	0x00020
#define SWITCH_WAIT_REL	0x00040
#define SWITCH_WAIT_ACQ	0x00080
#define BUFFER_SAVED	0x00100
#define CURSOR_ENABLED 	0x00200
#define CURSOR_SHOWN 	0x00400
#define MOUSE_ENABLED	0x00800
#define UPDATE_MOUSE	0x01000

/* configuration flags */
#define VISUAL_BELL	0x00001
#define BLINK_CURSOR	0x00002
#define CHAR_CURSOR	0x00004

/* attribute flags */
#define NORMAL_ATTR             0x00
#define BLINK_ATTR              0x01
#define BOLD_ATTR               0x02
#define UNDERLINE_ATTR          0x04
#define REVERSE_ATTR            0x08
#define FOREGROUND_CHANGED      0x10
#define BACKGROUND_CHANGED      0x20

/* video hardware memory addresses */
#define VIDEOMEM	0x000A0000

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
#define TIMER_FREQ	1193182			/* should be in isa.h */
#define CONSOLE_BUFSIZE 1024
#define PCBURST		128
#define FONT_8		0x001
#define FONT_14		0x002
#define FONT_16		0x004
#define HISTORY_SIZE	100*80

/* defines related to hardware addresses */
#define	MONO_BASE	0x3B4			/* crt controller base mono */
#define	COLOR_BASE	0x3D4			/* crt controller base color */
#define MISC		0x3C2			/* misc output register */
#define ATC		IO_VGA+0x00		/* attribute controller */
#define TSIDX		IO_VGA+0x04		/* timing sequencer idx */
#define TSREG		IO_VGA+0x05		/* timing sequencer data */
#define PIXMASK		IO_VGA+0x06		/* pixel write mask */
#define PALRADR		IO_VGA+0x07		/* palette read address */
#define PALWADR		IO_VGA+0x08		/* palette write address */
#define PALDATA		IO_VGA+0x09		/* palette data register */
#define GDCIDX		IO_VGA+0x0E		/* graph data controller idx */
#define GDCREG		IO_VGA+0x0F		/* graph data controller data */

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
	int             cur_attr;               /* current hardware attributes word */
	int             attr_mask;              /* current logical attributes mask */
	int             cur_color;              /* current hardware color */
	int             std_color;              /* normal hardware color */
	int             rev_color;              /* reverse hardware color */
} term_stat;

typedef struct scr_stat {
	u_short 	*scr_buf;		/* buffer when off screen */
	int 		xpos;			/* current X position */
	int 		ypos;			/* current Y position */
	int 		xsize;			/* X size */
	int 		ysize;			/* Y size */
	int		start;			/* modified area start */
	int		end;			/* modified area end */
	term_stat 	term;			/* terminal emulation stuff */
	int	 	status;			/* status (bitfield) */
	u_short 	*cursor_pos;		/* cursor buffer position */
	u_short		cursor_saveunder;	/* saved chars under cursor */
	char		cursor_start;		/* cursor start line # */
	char		cursor_end;		/* cursor end line # */
	u_short		*mouse_pos;		/* mouse buffer position */
	u_short		*mouse_oldpos;		/* mouse old buffer position */
	u_short		mouse_saveunder[4];	/* saved chars under mouse */
	short		mouse_xpos;		/* mouse x coordinate */
	short		mouse_ypos;		/* mouse y coordinate */
	u_char		mouse_cursor[128];	/* mouse cursor bitmap store */
	u_short		bell_duration;
	u_short		bell_pitch;
	u_char		border;			/* border color */
	u_char	 	mode;			/* mode */
	u_char		font;			/* font on this screen */
	pid_t 		pid;			/* pid of controlling proc */
	struct proc 	*proc;			/* proc* of controlling proc */
	struct vt_mode 	smode;			/* switch mode */
	u_short		*history;		/* circular history buffer */
	u_short		*history_head;		/* current head position */
	u_short		*history_pos;		/* position shown on screen */
	u_short		*history_save;		/* save area index */
	int		history_size;		/* size of history buffer */
	struct apmhook  r_hook;			/* reconfiguration support */
} scr_stat;

typedef struct default_attr {
	int             std_color;              /* normal hardware color */
	int             rev_color;              /* reverse hardware color */
} default_attr;

/* function prototypes */
static void scinit(void);
static u_int scgetc(int noblock);
static scr_stat *get_scr_stat(dev_t dev);
static scr_stat *alloc_scp(void);
static void init_scp(scr_stat *scp);
static int get_scr_num(void);
static void scrn_timer(void);
static void clear_screen(scr_stat *scp);
static int switch_scr(scr_stat *scp, u_int next_scr);
static void exchange_scr(void);
static inline void move_crsr(scr_stat *scp, int x, int y);
static void scan_esc(scr_stat *scp, u_char c);
static inline void draw_cursor(scr_stat *scp, int show);
static void ansi_put(scr_stat *scp, u_char *buf, int len);
static u_char *get_fstr(u_int c, u_int *len);
static void update_leds(int which);
static void history_to_screen(scr_stat *scp);
static int history_up_line(scr_stat *scp);
static int history_down_line(scr_stat *scp);
static void kbd_wait(void);
static void kbd_cmd(u_char command);
static void set_mode(scr_stat *scp);
       void set_border(int color);
static void set_vgaregs(char *modetable);
static void set_font_mode(void);
static void set_normal_mode(void);
static void copy_font(int operation, int font_type, char* font_image);
static void set_destructive_cursor(scr_stat *scp, int force);
static void draw_mouse_image(scr_stat *scp);
static void save_palette(void);
       void load_palette(void);
static void do_bell(scr_stat *scp, int pitch, int duration);
static void blink_screen(scr_stat *scp);

#endif /* !_I386_ISA_SYSCONS_H_ */
