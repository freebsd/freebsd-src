/*-
 * Copyright (c) 2000 Doug Rabson
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

#include <efi.h>
#include <efilib.h>
#include <teken.h>
#include <sys/reboot.h>

#include "bootstrap.h"

static EFI_GUID simple_input_ex_guid = EFI_SIMPLE_TEXT_INPUT_EX_PROTOCOL_GUID;
static SIMPLE_TEXT_OUTPUT_INTERFACE	*conout;
static SIMPLE_INPUT_INTERFACE		*conin;
static EFI_SIMPLE_TEXT_INPUT_EX_PROTOCOL *coninex;

static int mode;		/* Does ConOut have serial console? */

static uint32_t utf8_left;
static uint32_t utf8_partial;
#ifdef TERM_EMU
#define	DEFAULT_FGCOLOR EFI_LIGHTGRAY
#define	DEFAULT_BGCOLOR EFI_BLACK

#define	MAXARGS 8
static int args[MAXARGS], argc;
static int fg_c, bg_c, curx, cury;
static int esc;

void get_pos(int *x, int *y);
void curs_move(int *_x, int *_y, int x, int y);
static void CL(int);
void HO(void);
void end_term(void);
#endif

static tf_bell_t	efi_cons_bell;
static tf_cursor_t	efi_text_cursor;
static tf_putchar_t	efi_text_putchar;
static tf_fill_t	efi_text_fill;
static tf_copy_t	efi_text_copy;
static tf_param_t	efi_text_param;
static tf_respond_t	efi_cons_respond;

static teken_funcs_t tf = {
	.tf_bell	= efi_cons_bell,
	.tf_cursor	= efi_text_cursor,
	.tf_putchar	= efi_text_putchar,
	.tf_fill	= efi_text_fill,
	.tf_copy	= efi_text_copy,
	.tf_param	= efi_text_param,
	.tf_respond	= efi_cons_respond,
};

teken_t teken;
teken_pos_t tp;

struct text_pixel {
	teken_char_t c;
	teken_attr_t a;
};

static struct text_pixel *buffer;

#define	KEYBUFSZ 10
static unsigned keybuf[KEYBUFSZ];	/* keybuf for extended codes */
static int key_pending;

static const unsigned char teken_color_to_efi_color[16] = {
	EFI_BLACK,
	EFI_RED,
	EFI_GREEN,
	EFI_BROWN,
	EFI_BLUE,
	EFI_MAGENTA,
	EFI_CYAN,
	EFI_LIGHTGRAY,
	EFI_DARKGRAY,
	EFI_LIGHTRED,
	EFI_LIGHTGREEN,
	EFI_YELLOW,
	EFI_LIGHTBLUE,
	EFI_LIGHTMAGENTA,
	EFI_LIGHTCYAN,
	EFI_WHITE
};

static void efi_cons_probe(struct console *);
static int efi_cons_init(int);
void efi_cons_putchar(int);
int efi_cons_getchar(void);
void efi_cons_efiputchar(int);
int efi_cons_poll(void);

struct console efi_console = {
	"efi",
	"EFI console",
	C_WIDEOUT,
	efi_cons_probe,
	efi_cons_init,
	efi_cons_putchar,
	efi_cons_getchar,
	efi_cons_poll
};

/*
 * Not implemented.
 */
static void
efi_cons_bell(void *s __unused)
{
}

static void
efi_text_cursor(void *s __unused, const teken_pos_t *p)
{
	UINTN row, col;

	(void) conout->QueryMode(conout, conout->Mode->Mode, &col, &row);

	if (p->tp_col == col)
		col = p->tp_col - 1;
	else
		col = p->tp_col;

	if (p->tp_row == row)
		row = p->tp_row - 1;
	else
		row = p->tp_row;

	conout->SetCursorPosition(conout, col, row);
}

static void
efi_text_printchar(const teken_pos_t *p, bool autoscroll)
{
	UINTN a, attr;
	struct text_pixel *px;
	teken_color_t fg, bg, tmp;

	px = buffer + p->tp_col + p->tp_row * tp.tp_col;
	a = conout->Mode->Attribute;

	fg = teken_256to16(px->a.ta_fgcolor);
	bg = teken_256to16(px->a.ta_bgcolor);
	if (px->a.ta_format & TF_BOLD)
		fg |= TC_LIGHT;
	if (px->a.ta_format & TF_BLINK)
		bg |= TC_LIGHT;

	if (px->a.ta_format & TF_REVERSE) {
		tmp = fg;
		fg = bg;
		bg = tmp;
	}

	attr = EFI_TEXT_ATTR(teken_color_to_efi_color[fg],
	    teken_color_to_efi_color[bg] & 0x7);

	conout->SetCursorPosition(conout, p->tp_col, p->tp_row);

	/* to prvent autoscroll, skip print of lower right char */
	if (!autoscroll &&
	    p->tp_row == tp.tp_row - 1 &&
	    p->tp_col == tp.tp_col - 1)
		return;

	(void) conout->SetAttribute(conout, attr);
	efi_cons_efiputchar(px->c);
	(void) conout->SetAttribute(conout, a);
}

static void
efi_text_putchar(void *s __unused, const teken_pos_t *p, teken_char_t c,
    const teken_attr_t *a)
{
	EFI_STATUS status;
	int idx;

	idx = p->tp_col + p->tp_row * tp.tp_col;
	buffer[idx].c = c;
	buffer[idx].a = *a;
	efi_text_printchar(p, false);
}

static void
efi_text_fill(void *s, const teken_rect_t *r, teken_char_t c,
    const teken_attr_t *a)
{
	teken_pos_t p;
	UINTN row, col;

	(void) conout->QueryMode(conout, conout->Mode->Mode, &col, &row);

	conout->EnableCursor(conout, FALSE);
	for (p.tp_row = r->tr_begin.tp_row; p.tp_row < r->tr_end.tp_row;
	    p.tp_row++)
		for (p.tp_col = r->tr_begin.tp_col;
		    p.tp_col < r->tr_end.tp_col; p.tp_col++)
			efi_text_putchar(s, &p, c, a);
	conout->EnableCursor(conout, TRUE);
}

static bool
efi_same_pixel(struct text_pixel *px1, struct text_pixel *px2)
{
	if (px1->c != px2->c)
		return (false);

	if (px1->a.ta_format != px2->a.ta_format)
		return (false);
	if (px1->a.ta_fgcolor != px2->a.ta_fgcolor)
		return (false);
	if (px1->a.ta_bgcolor != px2->a.ta_bgcolor)
		return (false);

	return (true);
}

static void
efi_text_copy(void *ptr __unused, const teken_rect_t *r, const teken_pos_t *p)
{
	int srow, drow;
	int nrow, ncol, x, y; /* Has to be signed - >= 0 comparison */
	teken_pos_t d, s;
	bool scroll = false;

	/*
	 * Copying is a little tricky. We must make sure we do it in
	 * correct order, to make sure we don't overwrite our own data.
	 */

	nrow = r->tr_end.tp_row - r->tr_begin.tp_row;
	ncol = r->tr_end.tp_col - r->tr_begin.tp_col;

	/*
	 * Check if we do copy whole screen.
	 */
	if (p->tp_row == 0 && p->tp_col == 0 &&
	    nrow == tp.tp_row - 2 && ncol == tp.tp_col - 2)
		scroll = true;

	conout->EnableCursor(conout, FALSE);
	if (p->tp_row < r->tr_begin.tp_row) {
		/* Copy from bottom to top. */
		for (y = 0; y < nrow; y++) {
			d.tp_row = p->tp_row + y;
			s.tp_row = r->tr_begin.tp_row + y;
			drow = d.tp_row * tp.tp_col;
			srow = s.tp_row * tp.tp_col;
			for (x = 0; x < ncol; x++) {
				d.tp_col = p->tp_col + x;
				s.tp_col = r->tr_begin.tp_col + x;

				if (!efi_same_pixel(
				    &buffer[d.tp_col + drow],
				    &buffer[s.tp_col + srow])) {
					buffer[d.tp_col + drow] =
					    buffer[s.tp_col + srow];
					if (!scroll)
						efi_text_printchar(&d, false);
				} else if (scroll) {
					/*
					 * Draw last char and trigger
					 * scroll.
					 */
					if (y == nrow - 1 &&
					    x == ncol - 1) {
						efi_text_printchar(&d, true);
					}
				}
			}
		}
	} else {
		/* Copy from top to bottom. */
		if (p->tp_col < r->tr_begin.tp_col) {
			/* Copy from right to left. */
			for (y = nrow - 1; y >= 0; y--) {
				d.tp_row = p->tp_row + y;
				s.tp_row = r->tr_begin.tp_row + y;
				drow = d.tp_row * tp.tp_col;
				srow = s.tp_row * tp.tp_col;
				for (x = 0; x < ncol; x++) {
					d.tp_col = p->tp_col + x;
					s.tp_col = r->tr_begin.tp_col + x;

					if (!efi_same_pixel(
					    &buffer[d.tp_col + drow],
					    &buffer[s.tp_col + srow])) {
						buffer[d.tp_col + drow] =
						    buffer[s.tp_col + srow];
						efi_text_printchar(&d, false);
					}
				}
			}
		} else {
			/* Copy from left to right. */
			for (y = nrow - 1; y >= 0; y--) {
				d.tp_row = p->tp_row + y;
				s.tp_row = r->tr_begin.tp_row + y;
				drow = d.tp_row * tp.tp_col;
				srow = s.tp_row * tp.tp_col;
				for (x = ncol - 1; x >= 0; x--) {
					d.tp_col = p->tp_col + x;
					s.tp_col = r->tr_begin.tp_col + x;

					if (!efi_same_pixel(
					    &buffer[d.tp_col + drow],
					    &buffer[s.tp_col + srow])) {
						buffer[d.tp_col + drow] =
						    buffer[s.tp_col + srow];
						efi_text_printchar(&d, false);
					}
				}
			}
		}
	}
	conout->EnableCursor(conout, TRUE);
}

static void
efi_text_param(void *s __unused, int cmd, unsigned int value)
{
	switch (cmd) {
	case TP_SETLOCALCURSOR:
		/*
		 * 0 means normal (usually block), 1 means hidden, and
		 * 2 means blinking (always block) for compatibility with
		 * syscons.  We don't support any changes except hiding,
		 * so must map 2 to 0.
		 */
		value = (value == 1) ? 0 : 1;
		/* FALLTHROUGH */
	case TP_SHOWCURSOR:
		if (value == 1)
			conout->EnableCursor(conout, TRUE);
		else
			conout->EnableCursor(conout, FALSE);
		break;
	default:
		/* Not yet implemented */
		break;
	}
}

/*
 * Not implemented.
 */
static void
efi_cons_respond(void *s __unused, const void *buf __unused,
    size_t len __unused)
{
}

static void
efi_cons_probe(struct console *cp)
{
	cp->c_flags |= C_PRESENTIN | C_PRESENTOUT;
}

static bool
color_name_to_teken(const char *name, int *val)
{
	if (strcasecmp(name, "black") == 0) {
		*val = TC_BLACK;
		return (true);
	}
	if (strcasecmp(name, "red") == 0) {
		*val = TC_RED;
		return (true);
	}
	if (strcasecmp(name, "green") == 0) {
		*val = TC_GREEN;
		return (true);
	}
	if (strcasecmp(name, "brown") == 0) {
		*val = TC_BROWN;
		return (true);
	}
	if (strcasecmp(name, "blue") == 0) {
		*val = TC_BLUE;
		return (true);
	}
	if (strcasecmp(name, "magenta") == 0) {
		*val = TC_MAGENTA;
		return (true);
	}
	if (strcasecmp(name, "cyan") == 0) {
		*val = TC_CYAN;
		return (true);
	}
	if (strcasecmp(name, "white") == 0) {
		*val = TC_WHITE;
		return (true);
	}
	return (false);
}

static int
efi_set_colors(struct env_var *ev, int flags, const void *value)
{
	int val = 0;
	char buf[2];
	const void *evalue;
	const teken_attr_t *ap;
	teken_attr_t a;

	if (value == NULL)
		return (CMD_OK);

	if (color_name_to_teken(value, &val)) {
		snprintf(buf, sizeof (buf), "%d", val);
		evalue = buf;
	} else {
		char *end;

		errno = 0;
		val = (int)strtol(value, &end, 0);
		if (errno != 0 || *end != '\0') {
			printf("Allowed values are either ansi color name or "
			    "number from range [0-7].\n");
			return (CMD_OK);
		}
		evalue = value;
	}

	ap = teken_get_defattr(&teken);
	a = *ap;
	if (strcmp(ev->ev_name, "teken.fg_color") == 0) {
		/* is it already set? */
		if (ap->ta_fgcolor == val)
			return (CMD_OK);
		a.ta_fgcolor = val;
	}
	if (strcmp(ev->ev_name, "teken.bg_color") == 0) {
		/* is it already set? */
		if (ap->ta_bgcolor == val)
			return (CMD_OK);
		a.ta_bgcolor = val;
	}
	env_setenv(ev->ev_name, flags | EV_NOHOOK, evalue, NULL, NULL);
	teken_set_defattr(&teken, &a);
	return (CMD_OK);
}

#ifdef TERM_EMU
/* Get cursor position. */
void
get_pos(int *x, int *y)
{
	*x = conout->Mode->CursorColumn;
	*y = conout->Mode->CursorRow;
}

/* Move cursor to x rows and y cols (0-based). */
void
curs_move(int *_x, int *_y, int x, int y)
{
	conout->SetCursorPosition(conout, x, y);
	if (_x != NULL)
		*_x = conout->Mode->CursorColumn;
	if (_y != NULL)
		*_y = conout->Mode->CursorRow;
}
 
/* Clear internal state of the terminal emulation code. */
void
end_term(void)
{
	esc = 0;
	argc = -1;
}
#endif

static void
efi_cons_rawputchar(int c)
{
	int i;
	UINTN x, y;
	conout->QueryMode(conout, conout->Mode->Mode, &x, &y);

	if (c == '\t') {
		int n;

		n = 8 - ((conout->Mode->CursorColumn + 8) % 8);
		for (i = 0; i < n; i++)
			efi_cons_rawputchar(' ');
	} else {
#ifndef TERM_EMU
		if (c == '\n')
			efi_cons_efiputchar('\r');
		efi_cons_efiputchar(c);
#else
		switch (c) {
		case '\r':
			curx = 0;
			efi_cons_efiputchar('\r');
			return;
		case '\n':
			efi_cons_efiputchar('\n');
			efi_cons_efiputchar('\r');
			cury++;
			if (cury >= y)
				cury--;
			curx = 0;
			return;
		case '\b':
			if (curx > 0) {
				efi_cons_efiputchar('\b');
				curx--;
			}
			return;
		default:
			efi_cons_efiputchar(c);
			curx++;
			if (curx > x-1) {
				curx = 0;
				cury++;
			}
			if (cury > y-1) {
				curx = 0;
				cury--;
			}
		}
#endif
	}
	conout->EnableCursor(conout, TRUE);
}

#ifdef TERM_EMU
/* Gracefully exit ESC-sequence processing in case of misunderstanding. */
static void
bail_out(int c)
{
	char buf[16], *ch;
	int i;

	if (esc) {
		efi_cons_rawputchar('\033');
		if (esc != '\033')
			efi_cons_rawputchar(esc);
		for (i = 0; i <= argc; ++i) {
			sprintf(buf, "%d", args[i]);
			ch = buf;
			while (*ch)
				efi_cons_rawputchar(*ch++);
		}
	}
	efi_cons_rawputchar(c);
	end_term();
}

/* Clear display from current position to end of screen. */
static void
CD(void)
{
	int i;
	UINTN x, y;

	get_pos(&curx, &cury);
	if (curx == 0 && cury == 0) {
		conout->ClearScreen(conout);
		end_term();
		return;
	}

	conout->QueryMode(conout, conout->Mode->Mode, &x, &y);
	CL(0);  /* clear current line from cursor to end */
	for (i = cury + 1; i < y-1; i++) {
		curs_move(NULL, NULL, 0, i);
		CL(0);
	}
	curs_move(NULL, NULL, curx, cury);
	end_term();
}

/*
 * Absolute cursor move to args[0] rows and args[1] columns
 * (the coordinates are 1-based).
 */
static void
CM(void)
{
	if (args[0] > 0)
		args[0]--;
	if (args[1] > 0)
		args[1]--;
	curs_move(&curx, &cury, args[1], args[0]);
	end_term();
}

/* Home cursor (left top corner), also called from mode command. */
void
HO(void)
{
	argc = 1;
	args[0] = args[1] = 1;
	CM();
}
 
/* Clear line from current position to end of line */
static void
CL(int direction)
{
	int i, len;
	UINTN x, y;
	CHAR16 *line;

	conout->QueryMode(conout, conout->Mode->Mode, &x, &y);
	switch (direction) {
	case 0:	/* from cursor to end */
		len = x - curx + 1;
		break;
	case 1:	/* from beginning to cursor */
		len = curx;
		break;
	case 2:	/* entire line */
		len = x;
		break;
	default:	/* NOTREACHED */
		__unreachable();
	}
 
	if (cury == y - 1)
		len--;

	line = malloc(len * sizeof (CHAR16));
	if (line == NULL) {
		printf("out of memory\n");
		return;
	}
	for (i = 0; i < len; i++)
		line[i] = ' ';
	line[len-1] = 0;

	if (direction != 0)
		curs_move(NULL, NULL, 0, cury);
 
	conout->OutputString(conout, line);
	/* restore cursor position */
	curs_move(NULL, NULL, curx, cury);
	free(line);
	end_term();
}

static void
get_arg(int c)
{
	if (argc < 0)
		argc = 0;
	args[argc] *= 10;
	args[argc] += c - '0';
}
#endif
 
/* Emulate basic capabilities of cons25 terminal */
static void
efi_term_emu(int c)
{
#ifdef TERM_EMU
	static int ansi_col[] = {
		0, 4, 2, 6, 1, 5, 3, 7
	};
	int t, i;
	EFI_STATUS status;
 
	switch (esc) {
	case 0:
		switch (c) {
		case '\033':
			esc = c;
			break;
		default:
			efi_cons_rawputchar(c);
			break;
		}
		break;
	case '\033':
		switch (c) {
		case '[':
			esc = c;
			args[0] = 0;
			argc = -1;
			break;
		default:
			bail_out(c);
			break;
		}
		break;
	case '[':
		switch (c) {
		case ';':
			if (argc < 0)
				argc = 0;
			else if (argc + 1 >= MAXARGS)
				bail_out(c);
			else
				args[++argc] = 0;
			break;
		case 'H':		/* ho = \E[H */
			if (argc < 0)
				HO();
			else if (argc == 1)
				CM();
			else
				bail_out(c);
			break;
		case 'J':		/* cd = \E[J */
			if (argc < 0)
				CD();
			else
				bail_out(c);
			break;
		case 'm':
			if (argc < 0) {
				fg_c = DEFAULT_FGCOLOR;
				bg_c = DEFAULT_BGCOLOR;
			}
			for (i = 0; i <= argc; ++i) {
				switch (args[i]) {
				case 0:		/* back to normal */
					fg_c = DEFAULT_FGCOLOR;
					bg_c = DEFAULT_BGCOLOR;
					break;
				case 1:		/* bold */
					fg_c |= 0x8;
					break;
				case 4:		/* underline */
				case 5:		/* blink */
					bg_c |= 0x8;
					break;
				case 7:		/* reverse */
					t = fg_c;
					fg_c = bg_c;
					bg_c = t;
					break;
				case 22:	/* normal intensity */
					fg_c &= ~0x8;
					break;
				case 24:	/* not underline */
				case 25:	/* not blinking */
					bg_c &= ~0x8;
					break;
				case 30: case 31: case 32: case 33:
				case 34: case 35: case 36: case 37:
					fg_c = ansi_col[args[i] - 30];
					break;
				case 39:	/* normal */
					fg_c = DEFAULT_FGCOLOR;
					break;
				case 40: case 41: case 42: case 43:
				case 44: case 45: case 46: case 47:
					bg_c = ansi_col[args[i] - 40];
					break;
				case 49:	/* normal */
					bg_c = DEFAULT_BGCOLOR;
					break;
				}
			}
			conout->SetAttribute(conout, EFI_TEXT_ATTR(fg_c, bg_c));
			end_term();
			break;
		default:
			if (isdigit(c))
				get_arg(c);
			else
				bail_out(c);
			break;
		}
		break;
	default:
		bail_out(c);
		break;
	}
#else
	efi_cons_rawputchar(c);
#endif
}

bool
efi_cons_update_mode(void)
{
	UINTN cols, rows;
	const teken_attr_t *a;
	EFI_STATUS status;
	char env[8];

	status = conout->QueryMode(conout, conout->Mode->Mode, &cols, &rows);
	if (EFI_ERROR(status)) {
		cols = 80;
		rows = 24;
	}

	/*
	 * When we have serial port listed in ConOut, use pre-teken emulator,
	 * if built with.
	 * The problem is, we can not output text on efi and comconsole when
	 * efi also has comconsole bound. But then again, we need to have
	 * terminal emulator for efi text mode to support the menu.
	 * While teken is too expensive to be used on serial console, the
	 * pre-teken emulator is light enough to be used on serial console.
	 */
	mode = parse_uefi_con_out();
	if ((mode & RB_SERIAL) == 0) {
		if (buffer != NULL) {
			if (tp.tp_row == rows && tp.tp_col == cols)
				return (true);
			free(buffer);
		} else {
			teken_init(&teken, &tf, NULL);
		}

		tp.tp_row = rows;
		tp.tp_col = cols;
		buffer = malloc(rows * cols * sizeof(*buffer));
		if (buffer == NULL)
			return (false);

		teken_set_winsize(&teken, &tp);
		a = teken_get_defattr(&teken);

		snprintf(env, sizeof(env), "%d", a->ta_fgcolor);
		env_setenv("teken.fg_color", EV_VOLATILE, env, efi_set_colors,
		    env_nounset);
		snprintf(env, sizeof(env), "%d", a->ta_bgcolor);
		env_setenv("teken.bg_color", EV_VOLATILE, env, efi_set_colors,
		    env_nounset);

		for (int row = 0; row < rows; row++) {
			for (int col = 0; col < cols; col++) {
				buffer[col + row * tp.tp_col].c = ' ';
				buffer[col + row * tp.tp_col].a = *a;
			}
		}
	} else {
#ifdef TERM_EMU
		conout->SetAttribute(conout, EFI_TEXT_ATTR(DEFAULT_FGCOLOR,
		    DEFAULT_BGCOLOR));
		end_term();
		get_pos(&curx, &cury);
		curs_move(&curx, &cury, curx, cury);
		fg_c = DEFAULT_FGCOLOR;
		bg_c = DEFAULT_BGCOLOR;
#endif
	}

	snprintf(env, sizeof (env), "%u", (unsigned)rows);
	setenv("LINES", env, 1);
	snprintf(env, sizeof (env), "%u", (unsigned)cols);
	setenv("COLUMNS", env, 1);

	return (true);
}

static int
efi_cons_init(int arg)
{
	EFI_STATUS status;

	if (conin != NULL)
		return (0);

	conout = ST->ConOut;
	conin = ST->ConIn;

	conout->EnableCursor(conout, TRUE);
	status = BS->OpenProtocol(ST->ConsoleInHandle, &simple_input_ex_guid,
	    (void **)&coninex, IH, NULL, EFI_OPEN_PROTOCOL_GET_PROTOCOL);
	if (status != EFI_SUCCESS)
		coninex = NULL;

	if (efi_cons_update_mode())
		return (0);

	return (1);
}

static void
input_partial(void)
{
	unsigned i;
	uint32_t c;

	if (utf8_left == 0)
		return;

	for (i = 0; i < sizeof(utf8_partial); i++) {
		c = (utf8_partial >> (24 - (i << 3))) & 0xff;
		if (c != 0)
			efi_term_emu(c);
	}
	utf8_left = 0;
	utf8_partial = 0;
}

static void
input_byte(uint8_t c)
{
	if ((c & 0x80) == 0x00) {
		/* One-byte sequence. */
		input_partial();
		efi_term_emu(c);
		return;
	}
	if ((c & 0xe0) == 0xc0) {
		/* Two-byte sequence. */
		input_partial();
		utf8_left = 1;
		utf8_partial = c;
		return;
	}
	if ((c & 0xf0) == 0xe0) {
		/* Three-byte sequence. */
		input_partial();
		utf8_left = 2;
		utf8_partial = c;
		return;
	}
	if ((c & 0xf8) == 0xf0) {
		/* Four-byte sequence. */
		input_partial();
		utf8_left = 3;
		utf8_partial = c;
		return;
	}
	if ((c & 0xc0) == 0x80) {
		/* Invalid state? */
		if (utf8_left == 0) {
			efi_term_emu(c);
			return;
		}
		utf8_left--;
		utf8_partial = (utf8_partial << 8) | c;
		if (utf8_left == 0) {
			uint32_t v, u;
			uint8_t b;

			v = 0;
			u = utf8_partial;
			b = (u >> 24) & 0xff;
			if (b != 0) {		/* Four-byte sequence */
				v = b & 0x07;
				b = (u >> 16) & 0xff;
				v = (v << 6) | (b & 0x3f);
				b = (u >> 8) & 0xff;
				v = (v << 6) | (b & 0x3f);
				b = u & 0xff;
				v = (v << 6) | (b & 0x3f);
			} else if ((b = (u >> 16) & 0xff) != 0) {
				v = b & 0x0f;	/* Three-byte sequence */
				b = (u >> 8) & 0xff;
				v = (v << 6) | (b & 0x3f);
				b = u & 0xff;
				v = (v << 6) | (b & 0x3f);
			} else if ((b = (u >> 8) & 0xff) != 0) {
				v = b & 0x1f;	/* Two-byte sequence */
				b = u & 0xff;
				v = (v << 6) | (b & 0x3f);
			}
			/* Send unicode char directly to console. */
			efi_cons_efiputchar(v);
			utf8_partial = 0;
		}
		return;
	}
	/* Anything left is illegal in UTF-8 sequence. */
	input_partial();
	efi_term_emu(c);
}

void
efi_cons_putchar(int c)
{
	unsigned char ch = c;

	if ((mode & RB_SERIAL) != 0) {
		input_byte(ch);
		return;
	}

	if (buffer != NULL)
		teken_input(&teken, &ch, sizeof (ch));
	else
		efi_cons_efiputchar(c);
}

static int
keybuf_getchar(void)
{
	int i, c = 0;

	for (i = 0; i < KEYBUFSZ; i++) {
		if (keybuf[i] != 0) {
			c = keybuf[i];
			keybuf[i] = 0;
			break;
		}
	}

	return (c);
}

static bool
keybuf_ischar(void)
{
	int i;

	for (i = 0; i < KEYBUFSZ; i++) {
		if (keybuf[i] != 0)
			return (true);
	}
	return (false);
}

/*
 * We are not reading input before keybuf is empty, so we are safe
 * just to fill keybuf from the beginning.
 */
static void
keybuf_inschar(EFI_INPUT_KEY *key)
{

	switch (key->ScanCode) {
	case SCAN_UP: /* UP */
		keybuf[0] = 0x1b;	/* esc */
		keybuf[1] = '[';
		keybuf[2] = 'A';
		break;
	case SCAN_DOWN: /* DOWN */
		keybuf[0] = 0x1b;	/* esc */
		keybuf[1] = '[';
		keybuf[2] = 'B';
		break;
	case SCAN_RIGHT: /* RIGHT */
		keybuf[0] = 0x1b;	/* esc */
		keybuf[1] = '[';
		keybuf[2] = 'C';
		break;
	case SCAN_LEFT: /* LEFT */
		keybuf[0] = 0x1b;	/* esc */
		keybuf[1] = '[';
		keybuf[2] = 'D';
		break;
	case SCAN_DELETE:
		keybuf[0] = CHAR_BACKSPACE;
		break;
	case SCAN_ESC:
		keybuf[0] = 0x1b;	/* esc */
		break;
	default:
		keybuf[0] = key->UnicodeChar;
		break;
	}
}

static bool
efi_readkey(void)
{
	EFI_STATUS status;
	EFI_INPUT_KEY key;

	status = conin->ReadKeyStroke(conin, &key);
	if (status == EFI_SUCCESS) {
		keybuf_inschar(&key);
		return (true);
	}
	return (false);
}

static bool
efi_readkey_ex(void)
{
	EFI_STATUS status;
	EFI_INPUT_KEY *kp;
	EFI_KEY_DATA  key_data;
	uint32_t kss;

	status = coninex->ReadKeyStrokeEx(coninex, &key_data);
	if (status == EFI_SUCCESS) {
		kss = key_data.KeyState.KeyShiftState;
		kp = &key_data.Key;
		if (kss & EFI_SHIFT_STATE_VALID) {

			/*
			 * quick mapping to control chars, replace with
			 * map lookup later.
			 */
			if (kss & EFI_RIGHT_CONTROL_PRESSED ||
			    kss & EFI_LEFT_CONTROL_PRESSED) {
				if (kp->UnicodeChar >= 'a' &&
				    kp->UnicodeChar <= 'z') {
					kp->UnicodeChar -= 'a';
					kp->UnicodeChar++;
				}
			}
		}

		keybuf_inschar(kp);
		return (true);
	}
	return (false);
}

int
efi_cons_getchar(void)
{
	int c;

	if ((c = keybuf_getchar()) != 0)
		return (c);

	key_pending = 0;

	if (coninex == NULL) {
		if (efi_readkey())
			return (keybuf_getchar());
	} else {
		if (efi_readkey_ex())
			return (keybuf_getchar());
	}

	return (-1);
}

int
efi_cons_poll(void)
{
	EFI_STATUS status;

	if (keybuf_ischar() || key_pending)
		return (1);

	/*
	 * Some EFI implementation (u-boot for example) do not support
	 * WaitForKey().
	 * CheckEvent() can clear the signaled state.
	 */
	if (coninex != NULL) {
		if (coninex->WaitForKeyEx == NULL) {
			key_pending = efi_readkey_ex();
		} else {
			status = BS->CheckEvent(coninex->WaitForKeyEx);
			key_pending = status == EFI_SUCCESS;
		}
	} else {
		if (conin->WaitForKey == NULL) {
			key_pending = efi_readkey();
		} else {
			status = BS->CheckEvent(conin->WaitForKey);
			key_pending = status == EFI_SUCCESS;
		}
	}

	return (key_pending);
}

/* Plain direct access to EFI OutputString(). */
void
efi_cons_efiputchar(int c)
{
	CHAR16 buf[2];
	EFI_STATUS status;

	buf[0] = c;
        buf[1] = 0;     /* terminate string */

	status = conout->TestString(conout, buf);
	if (EFI_ERROR(status))
		buf[0] = '?';
	conout->OutputString(conout, buf);
}
