/*-
 * Copyright (c) 1995 Sen Schmidt
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
 *	$FreeBSD$
 */

#ifndef _PC98_PC98_SYSCONS_H_
#define	_PC98_PC98_SYSCONS_H_

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
#define MOUSE_ENABLED	0x00400
#define MOUSE_MOVED	0x00800
#define MOUSE_CUTTING	0x01000
#define MOUSE_VISIBLE	0x02000

/* configuration flags */
#define VISUAL_BELL	0x00001
#define BLINK_CURSOR	0x00002
#define CHAR_CURSOR	0x00004
#define DETECT_KBD	0x00008
#define XT_KEYBD	0x00010

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
#ifdef PC98
#define	UJIS	0
#define SJIS	1
#ifndef AUTO_CLOCK
#ifndef PC98_8M
#define BELL_PITCH	1678
#else
#define BELL_PITCH	1339
#endif
#else /* AUTO_CLOCK */
static unsigned int BELL_PITCH = 1678;
#endif /* AUTO_CLOCK */
#else /* IBM-PC */
#define BELL_PITCH	800
#define TIMER_FREQ	1193182			/* should be in isa.h */
#endif

#define CONSOLE_BUFSIZE 1024
#define PCBURST		128
#define FONT_NONE	1
#define FONT_8		8
#define FONT_14		14
#define FONT_16		16
#define HISTORY_SIZE	100*80

/* defines related to hardware addresses */
#ifdef PC98
#define TEXT_GDC	IO_GDC1	/* 0x60 */
#define TEXT_VRAM	(KERNBASE+0xA0000)
#define ATTR_OFFSET	0x1000
#else /* IBM */
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
#endif

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
	u_short 	*scr_buf;		/* buffer when off screen */
#ifdef PC98
	u_short 	*atr_buf;		/* buffer when off screen */
	u_short 	*cursor_atr;		/* cursor address (attribute)*/
#endif
	int 		xpos;			/* current X position */
	int 		ypos;			/* current Y position */
	int 		xsize;			/* X text size */
	int 		ysize;			/* Y text size */
	int 		xpixel;			/* X graphics size */
	int 		ypixel;			/* Y graphics size */
	int		font_size;		/* fontsize in Y direction */
	int		start;			/* modified area start */
	int		end;			/* modified area end */
	term_stat 	term;			/* terminal emulation stuff */
	int	 	status;			/* status (bitfield) */
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
	u_char	 	mode;			/* mode */
	pid_t 		pid;			/* pid of controlling proc */
	struct proc 	*proc;			/* proc* of controlling proc */
	struct vt_mode 	smode;			/* switch mode */
	u_short		*history;		/* circular history buffer */
	u_short		*history_head;		/* current head position */
	u_short		*history_pos;		/* position shown on screen */
	u_short		*history_save;		/* save area index */
#ifdef PC98
	u_short		*his_atr;		/* history buffer (attribute)*/
	u_short		*his_atr_head;		/* current head position */
	u_short		*his_atr_pos;		/* position shown on screen */
	u_short		*his_atr_save;		/* save area index */
#endif
	int		history_size;		/* size of history buffer */
	struct apmhook  r_hook;			/* reconfiguration support */
#ifdef KANJI
	u_char  kanji_1st_char;
	u_char 	kanji_type;	/* 0: ASCII CODE	1: HANKAKU ?	*/
				/* 2: SHIFT JIS 	4: EUC		*/
				/* 0x10: JIS HANKAKU	0x20: JIS	*/
#endif
} scr_stat;

typedef struct default_attr {
	int             std_color;              /* normal hardware color */
	int             rev_color;              /* reverse hardware color */
} default_attr;

/* misc prototypes used by different syscons related LKM's */
void set_border(u_char color);
void set_mode(scr_stat *scp);
void copy_font(int operation, int font_type, char* font_image);
void load_palette(char *palette);

#ifdef PC98
unsigned int at2pc98(unsigned int attr);
#endif

#endif /* !_I386_ISA_SYSCONS_H_ */
