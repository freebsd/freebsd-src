/*-
 * Copyright (c) 1999 FreeBSD(98) Porting Team.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer as
 *    the first lines of this file unmodified.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
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
 *
 * $FreeBSD$
 */

#include "opt_syscons.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/consio.h>

#include <machine/pc/display.h>

#include <dev/syscons/syscons.h>
#include <dev/syscons/sctermvar.h>

#ifndef SC_DUMB_TERMINAL

#define MAX_ESC_PAR	5

#ifdef KANJI
#define IS_KTYPE_ASCII_or_HANKAKU(A)	(!((A) & 0xee))
#define IS_KTYPE_KANA(A)		((A) & 0x11)
#define KTYPE_MASK_CTRL(A)		((A) &= 0xF0)
#endif /* KANJI */

/* attribute flags */
typedef struct {
	u_short		fg;			/* foreground color */
	u_short		bg;			/* background color */
} color_t;

typedef struct {
	int		flags;
#define SCTERM_BUSY	(1 << 0)
	int		esc;
	int		num_param;
	int		last_param;
	int		param[MAX_ESC_PAR];
	int		saved_xpos;
	int		saved_ypos;

#ifdef KANJI
	u_char		kanji_1st_char;
	u_char		kanji_type;
#define KTYPE_ASCII	0			/* ASCII */
#define KTYPE_KANA	1			/* HANKAKU */
#define KTYPE_JKANA	0x10			/* JIS HANKAKU */
#define KTYPE_7JIS	0x20			/* JIS */
#define KTYPE_SJIS	2			/* Shift JIS */
#define KTYPE_UJIS	4			/* UJIS */
#define KTYPE_SUKANA	3			/* Shift JIS or UJIS HANKAKU */
#define KTYPE_SUJIS	6			/* SHift JIS or UJIS */
#define KTYPE_KANIN	0x80			/* Kanji Invoke sequence */
#define KTYPE_ASCIN	0x40			/* ASCII Invoke sequence */
#endif /* KANJI */

	int		attr_mask;		/* current logical attr mask */
#define NORMAL_ATTR	0x00
#define BLINK_ATTR	0x01
#define BOLD_ATTR	0x02
#define UNDERLINE_ATTR	0x04
#define REVERSE_ATTR	0x08
#define FG_CHANGED	0x10
#define BG_CHANGED	0x20
	int		cur_attr;		/* current hardware attr word */
	color_t		cur_color;		/* current hardware color */
	color_t		std_color;		/* normal hardware color */
	color_t		rev_color;		/* reverse hardware color */
	color_t		dflt_std_color;		/* default normal color */
	color_t		dflt_rev_color;		/* default reverse color */
} term_stat;

static sc_term_init_t	scterm_init;
static sc_term_term_t	scterm_term;
static sc_term_puts_t	scterm_puts;
static sc_term_ioctl_t	scterm_ioctl;
static sc_term_reset_t	scterm_reset;
static sc_term_default_attr_t	scterm_default_attr;
static sc_term_clear_t	scterm_clear;
static sc_term_notify_t	scterm_notify;
static sc_term_input_t	scterm_input;

static sc_term_sw_t sc_term_sc = {
	{ NULL, NULL },
	"sck",					/* emulator name */
	"syscons kanji terminal",		/* description */
	"*",					/* matching renderer, any :-) */
	sizeof(term_stat),			/* softc size */
	0,
	scterm_init,
	scterm_term,
	scterm_puts,
	scterm_ioctl,
	scterm_reset,
	scterm_default_attr,
	scterm_clear,
	scterm_notify,
	scterm_input,
};

SCTERM_MODULE(sc, sc_term_sc);

static term_stat	reserved_term_stat;
static int		default_kanji = UJIS;
static void		scterm_scan_esc(scr_stat *scp, term_stat *tcp,
					u_char c);
static int		mask2attr(term_stat *tcp);

#ifdef KANJI
__inline static u_char
iskanji1(u_char mode, u_char c)
{
	if (c > 0x80) {
		if ((c >= 0xa1) && (c <= 0xdf)) {
			if (default_kanji == UJIS) {
				/* UJIS */
				return KTYPE_UJIS;
			}
			if (default_kanji == SJIS) {
				/* SJIS HANKAKU */
				return KTYPE_KANA;
			}
		}

		if (c <= 0x9f) {
			if (c == 0x8e) {
				/* SJIS or UJIS HANKAKU */
				return KTYPE_SUKANA;
			}

			/* SJIS */
			default_kanji = SJIS;
			return KTYPE_SJIS;
		}

		if ((c >= 0xe0) && (c <= 0xef)) {
			/* SJIS or UJIS */
			return KTYPE_SUJIS;
		}

		if ((c >= 0xf0) && (c <= 0xfe)) {
			/* UJIS */
			default_kanji = UJIS;
			return KTYPE_UJIS;
		}
	} else {
		if ((mode == KTYPE_7JIS) && (c >= 0x21) && (c <= 0x7e)) {
			/* JIS */
			default_kanji = UJIS;
			return KTYPE_7JIS;
		}

		if ((mode == KTYPE_JKANA) && (c >= 0x21) && (c <= 0x5f)) {
			/* JIS HANKAKU */
			default_kanji = UJIS;
			return KTYPE_JKANA;
		}
	}

	return KTYPE_ASCII;
}

__inline static u_char
iskanji2(u_char mode, u_char c)
{
	switch (mode) {
	case KTYPE_7JIS:
		if ((c >= 0x21) && (c <= 0x7e)) {
			/* JIS */
			return KTYPE_7JIS;
		}
		break;
	case KTYPE_SJIS:
		if ((c >= 0x40) && (c <= 0xfc) && (c != 0x7f)) {
			/* SJIS */
			return KTYPE_SJIS;
		}
		break;
	case KTYPE_UJIS:
		if ((c >= 0xa1) && (c <= 0xfe)) {
			/* UJIS */
			return KTYPE_UJIS;
		}
		break;
	case KTYPE_SUKANA:
		if ((c >= 0xa1) && (c <= 0xdf) && (default_kanji == UJIS)) {
			/* UJIS HANKAKU */
			return KTYPE_KANA;
		}
		if ((c >= 0x40) && (c <= 0xfc) && (c != 0x7f)) {
			/* SJIS */
			default_kanji = SJIS;
			return KTYPE_SJIS;
		}
		break;
	case KTYPE_SUJIS:
		if ((c >= 0x40) && (c <= 0xa0) && (c != 0x7f)) {
			/* SJIS */
			default_kanji = SJIS;
			return KTYPE_SJIS;
		}
		if ((c == 0xfd) || (c == 0xfe)) {
			/* UJIS */
			default_kanji = UJIS;
			return KTYPE_UJIS;
		}
		if ((c >= 0xa1) && (c <= 0xfc)) {
			if (default_kanji == SJIS)
				return KTYPE_SJIS;
			if (default_kanji == UJIS)
				return KTYPE_UJIS;
		}
		break;
	}

	return KTYPE_ASCII;
}

/*
 * JIS X0208-83 keisen conversion table
 */
static u_short keiConv[32] = {
	0x240c, 0x260c, 0x300c, 0x340c, 0x3c0c, 0x380c, 0x400c, 0x500c,
	0x480c, 0x580c, 0x600c, 0x250c, 0x270c, 0x330c, 0x370c, 0x3f0c,
	0x3b0c, 0x470c, 0x570c, 0x4f0c, 0x5f0c, 0x6f0c, 0x440c, 0x530c,
	0x4c0c, 0x5b0c, 0x630c, 0x410c, 0x540c, 0x490c, 0x5c0c, 0x660c
};

static u_short
kanji_convert(u_char mode, u_char h, u_char l)
{
	u_short tmp, high, low, c;

	high = (u_short) h;
	low  = (u_short) l;

	switch (mode) {
	case KTYPE_SJIS: /* SHIFT JIS */
		if (low >= 0xe0) {
			low -= 0x40;
		}
		low = (low - 0x81) * 2 + 0x21;
		if (high > 0x7f) {
			high--;
		}
		if (high > 0x9d) {
			low++;
			high -= 0x9e - 0x21;
		} else {
			high -= 0x40 - 0x21;
		}
		high &= 0x7F;
		low  &= 0x7F;
		tmp = ((high << 8) | low) - 0x20;
		break;
	case KTYPE_7JIS: /* JIS */
	case KTYPE_UJIS: /* UJIS */
		high &= 0x7F;
		low &= 0x7F;
		tmp = ((high << 8) | low) - 0x20;
		break;
	default:
		tmp = 0;
		break;
	}

	/* keisen */
	c = ((tmp & 0xff) << 8) | (tmp >> 8);
	/* 0x2821 .. 0x2840 */
	if (0x0821 <= c && c <= 0x0840)
		tmp = keiConv[c - 0x0821];

	return (tmp);
}
#endif /* KANJI */

static int
scterm_init(scr_stat *scp, void **softc, int code)
{
	term_stat *tcp;

	if (*softc == NULL) {
		if (reserved_term_stat.flags & SCTERM_BUSY)
			return EINVAL;
		*softc = &reserved_term_stat;
	}
	tcp = *softc;

	switch (code) {
	case SC_TE_COLD_INIT:
		bzero(tcp, sizeof(*tcp));
		tcp->flags = SCTERM_BUSY;
		tcp->esc = 0;
		tcp->saved_xpos = -1;
		tcp->saved_ypos = -1;
#ifdef KANJI
		tcp->kanji_1st_char = 0;
		tcp->kanji_type = KTYPE_ASCII;
#endif
		tcp->attr_mask = NORMAL_ATTR;
		/* XXX */
		tcp->dflt_std_color.fg = SC_NORM_ATTR & 0x0f;
		tcp->dflt_std_color.bg = (SC_NORM_ATTR >> 4) & 0x0f;
		tcp->dflt_rev_color.fg = SC_NORM_REV_ATTR & 0x0f;
		tcp->dflt_rev_color.bg = (SC_NORM_REV_ATTR >> 4) & 0x0f;
		tcp->std_color = tcp->dflt_std_color;
		tcp->rev_color = tcp->dflt_rev_color;
		tcp->cur_color = tcp->std_color;
		tcp->cur_attr = mask2attr(tcp);
		++sc_term_sc.te_refcount;
		break;

	case SC_TE_WARM_INIT:
		tcp->esc = 0;
		tcp->saved_xpos = -1;
		tcp->saved_ypos = -1;
#if 0
		tcp->std_color = tcp->dflt_std_color;
		tcp->rev_color = tcp->dflt_rev_color;
#endif
		tcp->cur_color = tcp->std_color;
		tcp->cur_attr = mask2attr(tcp);
		break;
	}

	return 0;
}

static int
scterm_term(scr_stat *scp, void **softc)
{
	if (*softc == &reserved_term_stat) {
		*softc = NULL;
		bzero(&reserved_term_stat, sizeof(reserved_term_stat));
	}
	--sc_term_sc.te_refcount;
	return 0;
}

static void
scterm_scan_esc(scr_stat *scp, term_stat *tcp, u_char c)
{
	static u_char ansi_col[16] = {
		FG_BLACK,     FG_RED,          FG_GREEN,      FG_BROWN,
		FG_BLUE,      FG_MAGENTA,      FG_CYAN,       FG_LIGHTGREY,
		FG_DARKGREY,  FG_LIGHTRED,     FG_LIGHTGREEN, FG_YELLOW,
		FG_LIGHTBLUE, FG_LIGHTMAGENTA, FG_LIGHTCYAN,  FG_WHITE
	};
	static int cattrs[] = {
		0,					/* block */
		CONS_BLINK_CURSOR,			/* blinking block */
		CONS_CHAR_CURSOR,			/* underline */
		CONS_CHAR_CURSOR | CONS_BLINK_CURSOR,	/* blinking underline */
		CONS_RESET_CURSOR,			/* reset to default */
		CONS_HIDDEN_CURSOR,			/* hide cursor */
	};
	static int tcattrs[] = {
		CONS_RESET_CURSOR | CONS_LOCAL_CURSOR,	/* normal */
		CONS_HIDDEN_CURSOR | CONS_LOCAL_CURSOR,	/* invisible */
		CONS_BLINK_CURSOR | CONS_LOCAL_CURSOR,	/* very visible */
	};
	sc_softc_t *sc;
	int v0, v1, v2;
	int i, n;

	i = n = 0;
	sc = scp->sc; 
	if (tcp->esc == 1) {	/* seen ESC */
#ifdef KANJI
		switch (tcp->kanji_type) {
		case KTYPE_KANIN:	/* Kanji Invoke sequence */
			switch (c) {
			case 'B':
			case '@':
				tcp->kanji_type = KTYPE_7JIS;
				tcp->esc = 0;
				tcp->kanji_1st_char = 0;
				return;
			default:
				tcp->kanji_type = KTYPE_ASCII;
				tcp->esc = 0;
				break;
			}
			break;
		case KTYPE_ASCIN:	/* Ascii Invoke sequence */
			switch (c) {
			case 'J':
			case 'B':
			case 'H':
				tcp->kanji_type = KTYPE_ASCII;
				tcp->esc = 0;
				tcp->kanji_1st_char = 0;
				return;
			case 'I':
				tcp->kanji_type = KTYPE_JKANA;
				tcp->esc = 0;
				tcp->kanji_1st_char = 0;
				return;
			default:
				tcp->kanji_type = KTYPE_ASCII;
				tcp->esc = 0;
				break;
			}
			break;
		default:
			break;
		}
#endif
		switch (c) {

		case '7':	/* Save cursor position */
			tcp->saved_xpos = scp->xpos;
			tcp->saved_ypos = scp->ypos;
			break;

		case '8':	/* Restore saved cursor position */
			if (tcp->saved_xpos >= 0 && tcp->saved_ypos >= 0)
				sc_move_cursor(scp, tcp->saved_xpos,
					       tcp->saved_ypos);
			break;

		case '[':	/* Start ESC [ sequence */
			tcp->esc = 2;
			tcp->last_param = -1;
			for (i = tcp->num_param; i < MAX_ESC_PAR; i++)
				tcp->param[i] = 1;
			tcp->num_param = 0;
			return;

#ifdef KANJI
		case '$':	/* Kanji Invoke sequence */
			tcp->kanji_type = KTYPE_KANIN;
			return;
#endif

		case 'M':	/* Move cursor up 1 line, scroll if at top */
			sc_term_up_scroll(scp, 1, sc->scr_map[0x20],
					  tcp->cur_attr, 0, 0);
			break;
#ifdef notyet
		case 'Q':
			tcp->esc = 4;
			return;
#endif
		case 'c':       /* reset */
			tcp->attr_mask = NORMAL_ATTR;
			tcp->cur_color = tcp->std_color
				       = tcp->dflt_std_color;
			tcp->rev_color = tcp->dflt_rev_color;
			tcp->cur_attr = mask2attr(tcp);
			sc_change_cursor_shape(scp,
			    CONS_RESET_CURSOR | CONS_LOCAL_CURSOR, -1, -1);
			sc_clear_screen(scp);
			break;

		case '(':	/* iso-2022: designate 94 character set to G0 */
#ifdef KANJI
			tcp->kanji_type = KTYPE_ASCIN;
#else
			tcp->esc = 5;
#endif
			return;
		}
	} else if (tcp->esc == 2) {	/* seen ESC [ */
		if (c >= '0' && c <= '9') {
			if (tcp->num_param < MAX_ESC_PAR) {
				if (tcp->last_param != tcp->num_param) {
					tcp->last_param = tcp->num_param;
					tcp->param[tcp->num_param] = 0;
				} else {
					tcp->param[tcp->num_param] *= 10;
				}
				tcp->param[tcp->num_param] += c - '0';
				return;
			}
		}
		tcp->num_param = tcp->last_param + 1;
		switch (c) {

		case ';':
			if (tcp->num_param < MAX_ESC_PAR)
				return;
			break;

		case '=':
			tcp->esc = 3;
			tcp->last_param = -1;
			for (i = tcp->num_param; i < MAX_ESC_PAR; i++)
				tcp->param[i] = 1;
			tcp->num_param = 0;
			return;

		case 'A':	/* up n rows */
			sc_term_up(scp, tcp->param[0], 0);
			break;

		case 'B':	/* down n rows */
			sc_term_down(scp, tcp->param[0], 0);
			break;

		case 'C':	/* right n columns */
			sc_term_right(scp, tcp->param[0]);
			break;

		case 'D':	/* left n columns */
			sc_term_left(scp, tcp->param[0]);
			break;

		case 'E':	/* cursor to start of line n lines down */
			n = tcp->param[0];
			if (n < 1)
				n = 1;
			sc_move_cursor(scp, 0, scp->ypos + n);
			break;

		case 'F':	/* cursor to start of line n lines up */
			n = tcp->param[0];
			if (n < 1)
				n = 1;
			sc_move_cursor(scp, 0, scp->ypos - n);
			break;

		case 'f':	/* Cursor move */
		case 'H':
			if (tcp->num_param == 0)
				sc_move_cursor(scp, 0, 0);
			else if (tcp->num_param == 2)
				sc_move_cursor(scp, tcp->param[1] - 1,
					       tcp->param[0] - 1);
			break;

		case 'J':	/* Clear all or part of display */
			if (tcp->num_param == 0)
				n = 0;
			else
				n = tcp->param[0];
			sc_term_clr_eos(scp, n, sc->scr_map[0x20],
					tcp->cur_attr);
			break;

		case 'K':	/* Clear all or part of line */
			if (tcp->num_param == 0)
				n = 0;
			else
				n = tcp->param[0];
			sc_term_clr_eol(scp, n, sc->scr_map[0x20],
					tcp->cur_attr);
			break;

		case 'L':	/* Insert n lines */
			sc_term_ins_line(scp, scp->ypos, tcp->param[0],
					 sc->scr_map[0x20], tcp->cur_attr, 0);
			break;

		case 'M':	/* Delete n lines */
			sc_term_del_line(scp, scp->ypos, tcp->param[0],
					 sc->scr_map[0x20], tcp->cur_attr, 0);
			break;

		case 'P':	/* Delete n chars */
			sc_term_del_char(scp, tcp->param[0],
					 sc->scr_map[0x20], tcp->cur_attr);
			break;

		case '@':	/* Insert n chars */
			sc_term_ins_char(scp, tcp->param[0],
					 sc->scr_map[0x20], tcp->cur_attr);
			break;

		case 'S':	/* scroll up n lines */
			sc_term_del_line(scp, 0, tcp->param[0],
					 sc->scr_map[0x20], tcp->cur_attr, 0);
			break;

		case 'T':	/* scroll down n lines */
			sc_term_ins_line(scp, 0, tcp->param[0],
					 sc->scr_map[0x20], tcp->cur_attr, 0);
			break;

		case 'X':	/* erase n characters in line */
			n = tcp->param[0];
			if (n < 1)
				n = 1;
			if (n > scp->xsize - scp->xpos)
				n = scp->xsize - scp->xpos;
			sc_vtb_erase(&scp->vtb, scp->cursor_pos, n,
				     sc->scr_map[0x20], tcp->cur_attr);
			mark_for_update(scp, scp->cursor_pos);
			mark_for_update(scp, scp->cursor_pos + n - 1);
			break;

		case 'Z':	/* move n tabs backwards */
			sc_term_backtab(scp, tcp->param[0]);
			break;

		case '`':	/* move cursor to column n */
			sc_term_col(scp, tcp->param[0]);
			break;

		case 'a':	/* move cursor n columns to the right */
			sc_term_right(scp, tcp->param[0]);
			break;

		case 'd':	/* move cursor to row n */
			sc_term_row(scp, tcp->param[0]);
			break;

		case 'e':	/* move cursor n rows down */
			sc_term_down(scp, tcp->param[0], 0);
			break;

		case 'm':	/* change attribute */
			if (tcp->num_param == 0) {
				tcp->attr_mask = NORMAL_ATTR;
				tcp->cur_color = tcp->std_color;
				tcp->cur_attr = mask2attr(tcp);
				break;
			}
			for (i = 0; i < tcp->num_param; i++) {
				switch (n = tcp->param[i]) {
				case 0:	/* back to normal */
					tcp->attr_mask = NORMAL_ATTR;
					tcp->cur_color = tcp->std_color;
					tcp->cur_attr = mask2attr(tcp);
					break;
				case 1:	/* bold */
					tcp->attr_mask |= BOLD_ATTR;
					tcp->cur_attr = mask2attr(tcp);
					break;
				case 4:	/* underline */
					tcp->attr_mask |= UNDERLINE_ATTR;
					tcp->cur_attr = mask2attr(tcp);
					break;
				case 5:	/* blink */
					tcp->attr_mask |= BLINK_ATTR;
					tcp->cur_attr = mask2attr(tcp);
					break;
				case 7: /* reverse */
					tcp->attr_mask |= REVERSE_ATTR;
					tcp->cur_attr = mask2attr(tcp);
					break;
				case 22: /* remove bold (or dim) */
					tcp->attr_mask &= ~BOLD_ATTR;
					tcp->cur_attr = mask2attr(tcp);
					break;
				case 24: /* remove underline */
					tcp->attr_mask &= ~UNDERLINE_ATTR;
					tcp->cur_attr = mask2attr(tcp);
					break;
				case 25: /* remove blink */
					tcp->attr_mask &= ~BLINK_ATTR;
					tcp->cur_attr = mask2attr(tcp);
					break;
				case 27: /* remove reverse */
					tcp->attr_mask &= ~REVERSE_ATTR;
					tcp->cur_attr = mask2attr(tcp);
					break;
				case 30: case 31: /* set ansi fg color */
				case 32: case 33: case 34:
				case 35: case 36: case 37:
					tcp->attr_mask |= FG_CHANGED;
					tcp->cur_color.fg = ansi_col[n - 30];
					tcp->cur_attr = mask2attr(tcp);
					break;
				case 39: /* restore fg color back to normal */
					tcp->attr_mask &= ~(FG_CHANGED|BOLD_ATTR);
					tcp->cur_color.fg = tcp->std_color.fg;
					tcp->cur_attr = mask2attr(tcp);
					break;
				case 40: case 41: /* set ansi bg color */
				case 42: case 43: case 44:
				case 45: case 46: case 47:
					tcp->attr_mask |= BG_CHANGED;
					tcp->cur_color.bg = ansi_col[n - 40];
					tcp->cur_attr = mask2attr(tcp);
					break;
				case 49: /* restore bg color back to normal */
					tcp->attr_mask &= ~BG_CHANGED;
					tcp->cur_color.bg = tcp->std_color.bg;
					tcp->cur_attr = mask2attr(tcp);
					break;
				}
			}
			break;

		case 's':	/* Save cursor position */
			tcp->saved_xpos = scp->xpos;
			tcp->saved_ypos = scp->ypos;
			break;

		case 'u':	/* Restore saved cursor position */
			if (tcp->saved_xpos >= 0 && tcp->saved_ypos >= 0)
				sc_move_cursor(scp, tcp->saved_xpos,
					       tcp->saved_ypos);
			break;

		case 'x':
			if (tcp->num_param == 0)
				n = 0;
			else
				n = tcp->param[0];
			switch (n) {
			case 0: /* reset colors and attributes back to normal */
				tcp->attr_mask = NORMAL_ATTR;
				tcp->cur_color = tcp->std_color
					       = tcp->dflt_std_color;
				tcp->rev_color = tcp->dflt_rev_color;
				tcp->cur_attr = mask2attr(tcp);
				break;
			case 1:	/* set ansi background */
				tcp->attr_mask &= ~BG_CHANGED;
				tcp->cur_color.bg = tcp->std_color.bg
						  = ansi_col[tcp->param[1] & 0x0f];
				tcp->cur_attr = mask2attr(tcp);
				break;
			case 2:	/* set ansi foreground */
				tcp->attr_mask &= ~FG_CHANGED;
				tcp->cur_color.fg = tcp->std_color.fg
						  = ansi_col[tcp->param[1] & 0x0f];
				tcp->cur_attr = mask2attr(tcp);
				break;
			case 3: /* set adapter attribute directly */
				tcp->attr_mask &= ~(FG_CHANGED | BG_CHANGED);
				tcp->cur_color.fg = tcp->std_color.fg
						  = tcp->param[1] & 0x0f;
				tcp->cur_color.bg = tcp->std_color.bg
						  = (tcp->param[1] >> 4) & 0x0f;
				tcp->cur_attr = mask2attr(tcp);
				break;
			case 5: /* set ansi reverse background */
				tcp->rev_color.bg = ansi_col[tcp->param[1] & 0x0f];
				tcp->cur_attr = mask2attr(tcp);
				break;
			case 6: /* set ansi reverse foreground */
				tcp->rev_color.fg = ansi_col[tcp->param[1] & 0x0f];
				tcp->cur_attr = mask2attr(tcp);
				break;
			case 7: /* set adapter reverse attribute directly */
				tcp->rev_color.fg = tcp->param[1] & 0x0f;
				tcp->rev_color.bg = (tcp->param[1] >> 4) & 0x0f;
				tcp->cur_attr = mask2attr(tcp);
				break;
			}
			break;

		case 'z':	/* switch to (virtual) console n */
			if (tcp->num_param == 1)
				sc_switch_scr(sc, tcp->param[0]);
			break;
		}
	} else if (tcp->esc == 3) {	/* seen ESC [0-9]+ = */
		if (c >= '0' && c <= '9') {
			if (tcp->num_param < MAX_ESC_PAR) {
				if (tcp->last_param != tcp->num_param) {
					tcp->last_param = tcp->num_param;
					tcp->param[tcp->num_param] = 0;
				} else {
					tcp->param[tcp->num_param] *= 10;
				}
				tcp->param[tcp->num_param] += c - '0';
				return;
			}
		}
		tcp->num_param = tcp->last_param + 1;
		switch (c) {

		case ';':
			if (tcp->num_param < MAX_ESC_PAR)
				return;
			break;

		case 'A':   /* set display border color */
			if (tcp->num_param == 1) {
				scp->border=tcp->param[0] & 0xff;
				if (scp == sc->cur_scp)
					sc_set_border(scp, scp->border);
			}
			break;

		case 'B':   /* set bell pitch and duration */
			if (tcp->num_param == 2) {
				scp->bell_pitch = tcp->param[0];
				scp->bell_duration = 
				    (tcp->param[1] * hz + 99) / 100;
			}
			break;

		case 'C':   /* set global/parmanent cursor type & shape */
			i = spltty();
			n = tcp->num_param;
			v0 = tcp->param[0];
			v1 = tcp->param[1];
			v2 = tcp->param[2];
			switch (n) {
			case 1:	/* flags only */
				if (v0 < sizeof(cattrs)/sizeof(cattrs[0]))
					v0 = cattrs[v0];
				else	/* backward compatibility */
					v0 = cattrs[v0 & 0x3];
				sc_change_cursor_shape(scp, v0, -1, -1);
				break;
			case 2:
				v2 = 0;
				v0 &= 0x1f;	/* backward compatibility */
				v1 &= 0x1f;
				/* FALL THROUGH */
			case 3:	/* base and height */
				if (v2 == 0)	/* count from top */
					sc_change_cursor_shape(scp, -1,
					    scp->font_size - v1 - 1,
					    v1 - v0 + 1);
				else if (v2 == 1) /* count from bottom */
					sc_change_cursor_shape(scp, -1,
					    v0, v1 - v0 + 1);
				break;
			}
			splx(i);
			break;

		case 'F':   /* set adapter foreground */
			if (tcp->num_param == 1) {
				tcp->attr_mask &= ~FG_CHANGED;
				tcp->cur_color.fg = tcp->std_color.fg
						  = tcp->param[0] & 0x0f;
				tcp->cur_attr = mask2attr(tcp);
			}
			break;

		case 'G':   /* set adapter background */
			if (tcp->num_param == 1) {
				tcp->attr_mask &= ~BG_CHANGED;
				tcp->cur_color.bg = tcp->std_color.bg
						  = tcp->param[0] & 0x0f;
				tcp->cur_attr = mask2attr(tcp);
			}
			break;

		case 'H':   /* set adapter reverse foreground */
			if (tcp->num_param == 1) {
				tcp->rev_color.fg = tcp->param[0] & 0x0f;
				tcp->cur_attr = mask2attr(tcp);
			}
			break;

		case 'I':   /* set adapter reverse background */
			if (tcp->num_param == 1) {
				tcp->rev_color.bg = tcp->param[0] & 0x0f;
				tcp->cur_attr = mask2attr(tcp);
			}
			break;

		case 'S':   /* set local/temporary cursor type & shape */
			i = spltty();
			n = tcp->num_param;
			v0 = tcp->param[0];
			switch (n) {
			case 0:
				v0 = 0;
				/* FALL THROUGH */
			case 1:
				if (v0 < sizeof(tcattrs)/sizeof(tcattrs[0]))
					sc_change_cursor_shape(scp,
					    tcattrs[v0], -1, -1);
				break;
			}
			splx(i);
			break;
		}
#ifdef notyet
	} else if (tcp->esc == 4) {	/* seen ESC Q */
		/* to be filled */
#endif
	} else if (tcp->esc == 5) {	/* seen ESC ( */
		switch (c) {
		case 'B':   /* iso-2022: desginate ASCII into G0 */
			break;
		/* other items to be filled */
		default:
			break;
		}
	}
	tcp->esc = 0;
}

static void
scterm_puts(scr_stat *scp, u_char *buf, int len)
{
	term_stat *tcp;
	u_char *ptr;
#ifdef KANJI
	u_short kanji_code;
#endif

	tcp = scp->ts;
	ptr = buf;
outloop:
	scp->sc->write_in_progress++;

	if (tcp->esc) {
		scterm_scan_esc(scp, tcp, *ptr++);
		len--;
	} else if (PRINTABLE(*ptr)) {     /* Print only printables */
		vm_offset_t p;
		u_char *map;
		int attr;
		int i;
		int cnt;
#ifdef KANJI
		u_char c;
#endif

		p = sc_vtb_pointer(&scp->vtb, scp->cursor_pos);
		map = scp->sc->scr_map;
		attr = tcp->cur_attr;

#ifdef KANJI
		c = *ptr;
		if (tcp->kanji_1st_char == 0) {
		    tcp->kanji_type = iskanji1(tcp->kanji_type, c);
		    if (!IS_KTYPE_ASCII_or_HANKAKU(tcp->kanji_type)) {
			/* not Ascii & not HANKAKU */
			tcp->kanji_1st_char = c;
			goto kanji_end;
		    } else if (tcp->kanji_type == KTYPE_ASCII) {
			cnt = imin(len, scp->xsize - scp->xpos);
			i = cnt;
			do {
			    p = sc_vtb_putchar(&scp->vtb, p, map[c], attr);
			    c = *++ptr;
			    --i;
			} while (i > 0 && PRINTABLE(c) &&
				 iskanji1(tcp->kanji_type, c) == KTYPE_ASCII);

			len -= cnt - i;
			mark_for_update(scp, scp->cursor_pos);
			scp->cursor_pos += cnt - i;
			mark_for_update(scp, scp->cursor_pos - 1);
			scp->xpos += cnt - i;
			KTYPE_MASK_CTRL(tcp->kanji_type);
			goto ascii_end;
		    }
		} else {
		    if ((tcp->kanji_type =
			 iskanji2(tcp->kanji_type, c)) & 0xee) {
			/* print kanji on TEXT VRAM */
			kanji_code = kanji_convert(tcp->kanji_type, c,
						   tcp->kanji_1st_char);
			mark_for_update(scp, scp->cursor_pos);
			for (i = 0; i < 2; i++) {
			    /* *cursor_pos = (kanji_code | (i*0x80)); */
			    p = sc_vtb_putchar(&scp->vtb, p,
			       kanji_code | ((i == 0) ? 0x00 : 0x80), attr);
			    ++scp->cursor_pos;
			    if (++scp->xpos >= scp->xsize) {
				scp->xpos = 0;
				scp->ypos++;
			    }
			}
			mark_for_update(scp, scp->cursor_pos - 1);
			KTYPE_MASK_CTRL(tcp->kanji_type);
			tcp->kanji_1st_char = 0;
			goto kanji_end;
		    } else {
			tcp->kanji_1st_char = 0;
		    }
		}				
		if (IS_KTYPE_KANA(tcp->kanji_type))
		    c |= 0x80;
		KTYPE_MASK_CTRL(tcp->kanji_type);
		sc_vtb_putchar(&scp->vtb, p, map[c], attr);
		mark_for_update(scp, scp->cursor_pos);
		mark_for_update(scp, scp->cursor_pos);
		++scp->cursor_pos;
		++scp->xpos;
kanji_end:
		++ptr;
		--len;
ascii_end:
#else /* !KANJI */
		cnt = imin(len, scp->xsize - scp->xpos);
		i = cnt;
		do {
		    /*
		     * gcc-2.6.3 generates poor (un)sign extension code.
		     * Casting the pointers in the following to volatile should
		     * have no effect, but in fact speeds up this inner loop
		     * from 26 to 18 cycles (+ cache misses) on i486's.
		     */
#define	UCVP(ucp)	((u_char volatile *)(ucp))
		    p = sc_vtb_putchar(&scp->vtb, p, UCVP(map)[*UCVP(ptr)],
				       attr);
		    ++ptr;
		    --i;
		} while (i > 0 && PRINTABLE(*ptr));

		len -= cnt - i;
		mark_for_update(scp, scp->cursor_pos);
		scp->cursor_pos += cnt - i;
		mark_for_update(scp, scp->cursor_pos - 1);
		scp->xpos += cnt - i;
#endif /* !KANJI */

		if (scp->xpos >= scp->xsize) {
			scp->xpos = 0;
			scp->ypos++;
		}
	} else {
		switch (*ptr) {
		case 0x07:
			sc_bell(scp, scp->bell_pitch, scp->bell_duration);
			break;

		case 0x08:	/* non-destructive backspace */
			if (scp->cursor_pos > 0) {
				mark_for_update(scp, scp->cursor_pos);
				scp->cursor_pos--;
				mark_for_update(scp, scp->cursor_pos);
				if (scp->xpos > 0)
					scp->xpos--;
				else {
					scp->xpos += scp->xsize - 1;
					scp->ypos--;
				}
			}
			break;

		case 0x09:	/* non-destructive tab */
			mark_for_update(scp, scp->cursor_pos);
			scp->cursor_pos += (8 - scp->xpos % 8u);
			scp->xpos += (8 - scp->xpos % 8u);
			if (scp->xpos >= scp->xsize) {
				scp->xpos = 0;
				scp->ypos++;
				scp->cursor_pos = scp->xsize * scp->ypos;
			}
			mark_for_update(scp, scp->cursor_pos);
			break;

		case 0x0a:	/* newline, same pos */
			mark_for_update(scp, scp->cursor_pos);
			scp->cursor_pos += scp->xsize;
			mark_for_update(scp, scp->cursor_pos);
			scp->ypos++;
			break;

		case 0x0c:	/* form feed, clears screen */
			sc_clear_screen(scp);
			break;

		case 0x0d:	/* return, return to pos 0 */
			mark_for_update(scp, scp->cursor_pos);
			scp->cursor_pos -= scp->xpos;
			mark_for_update(scp, scp->cursor_pos);
			scp->xpos = 0;
			break;

		case 0x0e:	/* ^N */
			tcp->kanji_type = KTYPE_JKANA;
			tcp->esc = 0;
			tcp->kanji_1st_char = 0;
			break;

		case 0x0f:	/* ^O */
			tcp->kanji_type = KTYPE_ASCII;
			tcp->esc = 0;
			tcp->kanji_1st_char = 0;
			break;

		case 0x1b:	/* start escape sequence */
			tcp->esc = 1;
			tcp->num_param = 0;
			break;
		}
		ptr++;
		len--;
	}

	sc_term_gen_scroll(scp, scp->sc->scr_map[0x20], tcp->cur_attr);

	scp->sc->write_in_progress--;
	if (len)
		goto outloop;
}

static int
scterm_ioctl(scr_stat *scp, struct tty *tp, u_long cmd, caddr_t data,
	     int flag, struct thread *td)
{
	term_stat *tcp = scp->ts;
	vid_info_t *vi;

	switch (cmd) {
	case GIO_ATTR:      	/* get current attributes */
		/* FIXME: */
		*(int*)data = (tcp->cur_attr >> 8) & 0xff;
		return 0;
	case CONS_GETINFO:  	/* get current (virtual) console info */
		vi = (vid_info_t *)data;
		if (vi->size != sizeof(struct vid_info))
			return EINVAL;
		vi->mv_norm.fore = tcp->std_color.fg;
		vi->mv_norm.back = tcp->std_color.bg;
		vi->mv_rev.fore = tcp->rev_color.fg;
		vi->mv_rev.back = tcp->rev_color.bg;
		/*
		 * The other fields are filled by the upper routine. XXX
		 */
		return ENOIOCTL;
	}
	return ENOIOCTL;
}

static int
scterm_reset(scr_stat *scp, int code)
{
	/* FIXME */
	return 0;
}

static void
scterm_default_attr(scr_stat *scp, int color, int rev_color)
{
	term_stat *tcp = scp->ts;

	tcp->dflt_std_color.fg = color & 0x0f;
	tcp->dflt_std_color.bg = (color >> 4) & 0x0f;
	tcp->dflt_rev_color.fg = rev_color & 0x0f;
	tcp->dflt_rev_color.bg = (rev_color >> 4) & 0x0f;
	tcp->std_color = tcp->dflt_std_color;
	tcp->rev_color = tcp->dflt_rev_color;
	tcp->cur_color = tcp->std_color;
	tcp->cur_attr = mask2attr(tcp);
}

static void
scterm_clear(scr_stat *scp)
{
	term_stat *tcp = scp->ts;

	sc_move_cursor(scp, 0, 0);
	sc_vtb_clear(&scp->vtb, scp->sc->scr_map[0x20], tcp->cur_attr);
	mark_all(scp);
}

static void
scterm_notify(scr_stat *scp, int event)
{
	switch (event) {
	case SC_TE_NOTIFY_VTSWITCH_IN:
		break;
	case SC_TE_NOTIFY_VTSWITCH_OUT:
		break;
	}
}

static int
scterm_input(scr_stat *scp, int c, struct tty *tp)
{
	return FALSE;
}

/*
 * Calculate hardware attributes word using logical attributes mask and
 * hardware colors
 */

/* FIXME */
static int
mask2attr(term_stat *tcp)
{
	int attr, mask = tcp->attr_mask;

	if (mask & REVERSE_ATTR) {
		attr = ((mask & FG_CHANGED) ?
			tcp->cur_color.bg : tcp->rev_color.fg) |
			(((mask & BG_CHANGED) ?
			tcp->cur_color.fg : tcp->rev_color.bg) << 4);
	} else
		attr = tcp->cur_color.fg | (tcp->cur_color.bg << 4);

	/* XXX: underline mapping for Hercules adapter can be better */
	if (mask & (BOLD_ATTR | UNDERLINE_ATTR))
		attr ^= 0x08;
	if (mask & BLINK_ATTR)
		attr ^= 0x80;

	return (attr << 8);
}

#endif /* SC_DUMB_TERMINAL */
