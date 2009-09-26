/*-
 * Copyright (c) 2008-2009 Ed Schouten <ed@FreeBSD.org>
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
 *
 * $FreeBSD$
 */

#include <sys/cdefs.h>
#if defined(__FreeBSD__) && defined(_KERNEL)
#include <sys/param.h>
#include <sys/lock.h>
#include <sys/systm.h>
#define	teken_assert(x)		MPASS(x)
#define	teken_printf(x,...)
#else /* !(__FreeBSD__ && _KERNEL) */
#include <sys/types.h>
#include <assert.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#define	teken_assert(x)		assert(x)
#define	teken_printf(x,...)	do { \
	if (df != NULL) \
		fprintf(df, x, ## __VA_ARGS__); \
} while (0)
/* debug messages */
static FILE *df;
#endif /* __FreeBSD__ && _KERNEL */

/* Private flags for t_stateflags. */
#define	TS_FIRSTDIGIT	0x01	/* First numeric digit in escape sequence. */
#define	TS_INSERT	0x02	/* Insert mode. */
#define	TS_AUTOWRAP	0x04	/* Autowrap. */
#define	TS_ORIGIN	0x08	/* Origin mode. */
#define	TS_WRAPPED	0x10	/* Next character should be printed on col 0. */
#define	TS_8BIT		0x20	/* UTF-8 disabled. */
#define	TS_CONS25	0x40	/* cons25 emulation. */

/* Character that blanks a cell. */
#define	BLANK	' '

#include "teken.h"
#include "teken_wcwidth.h"
#include "teken_scs.h"

static teken_state_t	teken_state_init;

/*
 * Wrappers for hooks.
 */

static inline void
teken_funcs_bell(teken_t *t)
{

	t->t_funcs->tf_bell(t->t_softc);
}

static inline void
teken_funcs_cursor(teken_t *t)
{

	teken_assert(t->t_cursor.tp_row < t->t_winsize.tp_row);
	teken_assert(t->t_cursor.tp_col < t->t_winsize.tp_col);

	t->t_funcs->tf_cursor(t->t_softc, &t->t_cursor);
}

static inline void
teken_funcs_putchar(teken_t *t, const teken_pos_t *p, teken_char_t c,
    const teken_attr_t *a)
{

	teken_assert(p->tp_row < t->t_winsize.tp_row);
	teken_assert(p->tp_col < t->t_winsize.tp_col);

	t->t_funcs->tf_putchar(t->t_softc, p, c, a);
}

static inline void
teken_funcs_fill(teken_t *t, const teken_rect_t *r,
    const teken_char_t c, const teken_attr_t *a)
{

	teken_assert(r->tr_end.tp_row > r->tr_begin.tp_row);
	teken_assert(r->tr_end.tp_row <= t->t_winsize.tp_row);
	teken_assert(r->tr_end.tp_col > r->tr_begin.tp_col);
	teken_assert(r->tr_end.tp_col <= t->t_winsize.tp_col);

	t->t_funcs->tf_fill(t->t_softc, r, c, a);
}

static inline void
teken_funcs_copy(teken_t *t, const teken_rect_t *r, const teken_pos_t *p)
{

	teken_assert(r->tr_end.tp_row > r->tr_begin.tp_row);
	teken_assert(r->tr_end.tp_row <= t->t_winsize.tp_row);
	teken_assert(r->tr_end.tp_col > r->tr_begin.tp_col);
	teken_assert(r->tr_end.tp_col <= t->t_winsize.tp_col);
	teken_assert(p->tp_row + (r->tr_end.tp_row - r->tr_begin.tp_row) <= t->t_winsize.tp_row);
	teken_assert(p->tp_col + (r->tr_end.tp_col - r->tr_begin.tp_col) <= t->t_winsize.tp_col);

	t->t_funcs->tf_copy(t->t_softc, r, p);
}

static inline void
teken_funcs_param(teken_t *t, int cmd, unsigned int value)
{

	t->t_funcs->tf_param(t->t_softc, cmd, value);
}

static inline void
teken_funcs_respond(teken_t *t, const void *buf, size_t len)
{

	t->t_funcs->tf_respond(t->t_softc, buf, len);
}

#include "teken_subr.h"
#include "teken_subr_compat.h"

/*
 * Programming interface.
 */

void
teken_init(teken_t *t, const teken_funcs_t *tf, void *softc)
{
	teken_pos_t tp = { .tp_row = 24, .tp_col = 80 };

#if !(defined(__FreeBSD__) && defined(_KERNEL))
	df = fopen("teken.log", "w");
	if (df != NULL)
		setvbuf(df, NULL, _IOLBF, BUFSIZ);
#endif /* !(__FreeBSD__ && _KERNEL) */

	t->t_funcs = tf;
	t->t_softc = softc;

	t->t_nextstate = teken_state_init;
	t->t_stateflags = 0;
	t->t_utf8_left = 0;

	t->t_defattr.ta_format = 0;
	t->t_defattr.ta_fgcolor = TC_WHITE;
	t->t_defattr.ta_bgcolor = TC_BLACK;
	teken_subr_do_reset(t);

	teken_set_winsize(t, &tp);
}

static void
teken_input_char(teken_t *t, teken_char_t c)
{

	switch (c) {
	case '\0':
		break;
	case '\a':
		teken_subr_bell(t);
		break;
	case '\b':
		teken_subr_backspace(t);
		break;
	case '\n':
	case '\x0B':
		teken_subr_newline(t);
		break;
	case '\x0C':
		teken_subr_newpage(t);
		break;
	case '\x0E':
		if (t->t_stateflags & TS_CONS25)
			t->t_nextstate(t, c);
		else
			t->t_curscs = 1;
		break;
	case '\x0F':
		if (t->t_stateflags & TS_CONS25)
			t->t_nextstate(t, c);
		else
			t->t_curscs = 0;
		break;
	case '\r':
		teken_subr_carriage_return(t);
		break;
	case '\t':
		teken_subr_horizontal_tab(t);
		break;
	default:
		t->t_nextstate(t, c);
		break;
	}

	/* Post-processing assertions. */
	teken_assert(t->t_cursor.tp_row >= t->t_originreg.ts_begin);
	teken_assert(t->t_cursor.tp_row < t->t_originreg.ts_end);
	teken_assert(t->t_cursor.tp_row < t->t_winsize.tp_row);
	teken_assert(t->t_cursor.tp_col < t->t_winsize.tp_col);
	teken_assert(t->t_saved_cursor.tp_row < t->t_winsize.tp_row);
	teken_assert(t->t_saved_cursor.tp_col < t->t_winsize.tp_col);
	teken_assert(t->t_scrollreg.ts_end <= t->t_winsize.tp_row);
	teken_assert(t->t_scrollreg.ts_begin < t->t_scrollreg.ts_end);
	/* Origin region has to be window size or the same as scrollreg. */
	teken_assert((t->t_originreg.ts_begin == t->t_scrollreg.ts_begin &&
	    t->t_originreg.ts_end == t->t_scrollreg.ts_end) ||
	    (t->t_originreg.ts_begin == 0 &&
	    t->t_originreg.ts_end == t->t_winsize.tp_row));
}

static void
teken_input_byte(teken_t *t, unsigned char c)
{

	/*
	 * UTF-8 handling.
	 */
	if ((c & 0x80) == 0x00 || t->t_stateflags & TS_8BIT) {
		/* One-byte sequence. */
		t->t_utf8_left = 0;
		teken_input_char(t, c);
	} else if ((c & 0xe0) == 0xc0) {
		/* Two-byte sequence. */
		t->t_utf8_left = 1;
		t->t_utf8_partial = c & 0x1f;
	} else if ((c & 0xf0) == 0xe0) {
		/* Three-byte sequence. */
		t->t_utf8_left = 2;
		t->t_utf8_partial = c & 0x0f;
	} else if ((c & 0xf8) == 0xf0) {
		/* Four-byte sequence. */
		t->t_utf8_left = 3;
		t->t_utf8_partial = c & 0x07;
	} else if ((c & 0xc0) == 0x80) {
		if (t->t_utf8_left == 0)
			return;
		t->t_utf8_left--;
		t->t_utf8_partial = (t->t_utf8_partial << 6) | (c & 0x3f);
		if (t->t_utf8_left == 0) {
			teken_printf("Got UTF-8 char %x\n", t->t_utf8_partial);
			teken_input_char(t, t->t_utf8_partial);
		}
	}
}

void
teken_input(teken_t *t, const void *buf, size_t len)
{
	const char *c = buf;

	while (len-- > 0)
		teken_input_byte(t, *c++);
}

const teken_pos_t *
teken_get_cursor(teken_t *t)
{

	return (&t->t_cursor);
}

void
teken_set_cursor(teken_t *t, const teken_pos_t *p)
{

	/* XXX: bounds checking with originreg! */
	teken_assert(p->tp_row < t->t_winsize.tp_row);
	teken_assert(p->tp_col < t->t_winsize.tp_col);

	t->t_cursor = *p;
}

const teken_attr_t *
teken_get_curattr(teken_t *t)
{

	return (&t->t_curattr);
}

void
teken_set_curattr(teken_t *t, const teken_attr_t *a)
{

	t->t_curattr = *a;
}

const teken_attr_t *
teken_get_defattr(teken_t *t)
{

	return (&t->t_defattr);
}

void
teken_set_defattr(teken_t *t, const teken_attr_t *a)
{

	t->t_curattr = t->t_saved_curattr = t->t_defattr = *a;
}

const teken_pos_t *
teken_get_winsize(teken_t *t)
{

	return (&t->t_winsize);
}

void
teken_set_winsize(teken_t *t, const teken_pos_t *p)
{

	t->t_winsize = *p;
	teken_subr_do_reset(t);
}

void
teken_set_8bit(teken_t *t)
{

	t->t_stateflags |= TS_8BIT;
}

void
teken_set_cons25(teken_t *t)
{

	t->t_stateflags |= TS_CONS25;
}

/*
 * State machine.
 */

static void
teken_state_switch(teken_t *t, teken_state_t *s)
{

	t->t_nextstate = s;
	t->t_curnum = 0;
	t->t_stateflags |= TS_FIRSTDIGIT;
}

static int
teken_state_numbers(teken_t *t, teken_char_t c)
{

	teken_assert(t->t_curnum < T_NUMSIZE);

	if (c >= '0' && c <= '9') {
		/*
		 * Don't do math with the default value of 1 when a
		 * custom number is inserted.
		 */
		if (t->t_stateflags & TS_FIRSTDIGIT) {
			t->t_stateflags &= ~TS_FIRSTDIGIT;
			t->t_nums[t->t_curnum] = 0;
		} else {
			t->t_nums[t->t_curnum] *= 10;
		}

		t->t_nums[t->t_curnum] += c - '0';
		return (1);
	} else if (c == ';') {
		if (t->t_stateflags & TS_FIRSTDIGIT)
			t->t_nums[t->t_curnum] = 0;

		/* Only allow a limited set of arguments. */
		if (++t->t_curnum == T_NUMSIZE) {
			teken_state_switch(t, teken_state_init);
			return (1);
		}

		t->t_stateflags |= TS_FIRSTDIGIT;
		return (1);
	} else {
		if (t->t_stateflags & TS_FIRSTDIGIT && t->t_curnum > 0) {
			/* Finish off the last empty argument. */
			t->t_nums[t->t_curnum] = 0;
			t->t_curnum++;
		} else if ((t->t_stateflags & TS_FIRSTDIGIT) == 0) {
			/* Also count the last argument. */
			t->t_curnum++;
		}
	}

	return (0);
}

teken_color_t
teken_256to8(teken_color_t c)
{
	unsigned int r, g, b;

	if (c < 16) {
		/* Traditional color indices. */
		return (c % 8);
	} else if (c >= 244) {
		/* Upper grayscale colors. */
		return (TC_WHITE);
	} else if (c >= 232) {
		/* Lower grayscale colors. */
		return (TC_BLACK);
	}

	/* Convert to RGB. */
	c -= 16;
	b = c % 6;
	g = (c / 6) % 6;
	r = c / 36;

	if (r < g) {
		/* Possibly green. */
		if (g < b)
			return (TC_BLUE);
		else if (g > b)
			return (TC_GREEN);
		else
			return (TC_CYAN);
	} else if (r > g) {
		/* Possibly red. */
		if (r < b)
			return (TC_BLUE);
		else if (r > b)
			return (TC_RED);
		else
			return (TC_MAGENTA);
	} else {
		/* Possibly brown. */
		if (g < b)
			return (TC_BLUE);
		else if (g > b)
			return (TC_BROWN);
		else if (r < 3)
			return (TC_BLACK);
		else
			return (TC_WHITE);
	}
}

#include "teken_state.h"
