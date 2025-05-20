/*
 * Copyright (C) 1984-2025  Mark Nudelman
 *
 * You may distribute under the terms of either the GNU General Public
 * License or the Less License, as specified in the README file.
 *
 * For more information, see the README file.
 */

/*
 * Routines to manipulate the "line buffer".
 * The line buffer holds a line of output as it is being built
 * in preparation for output to the screen.
 */

#include "less.h"
#include "charset.h"
#include "position.h"

#if MSDOS_COMPILER==WIN32C
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#define MAX_PFX_WIDTH (MAX_LINENUM_WIDTH + MAX_STATUSCOL_WIDTH + 1)
static struct {
	char *buf;    /* Buffer which holds the current output line */
	int *attr;    /* Parallel to buf, to hold attributes */
	size_t print; /* Index in buf of first printable char */
	size_t end;   /* Number of chars in buf */
	char pfx[MAX_PFX_WIDTH]; /* Holds status column and line number */
	int pfx_attr[MAX_PFX_WIDTH];
	size_t pfx_end;  /* Number of chars in pfx */
} linebuf;

/*
 * Buffer of ansi sequences which have been shifted off the left edge 
 * of the screen. 
 */
static struct xbuffer shifted_ansi;

/*
 * Ring buffer of last ansi sequences sent.
 * While sending a line, these will be resent at the end
 * of any highlighted string, to restore text modes.
 * {{ Not ideal, since we don't really know how many to resend. }}
 */
#define NUM_LAST_ANSIS 3
static struct xbuffer last_ansi;
static struct xbuffer last_ansis[NUM_LAST_ANSIS];
static int curr_last_ansi;

static size_t size_linebuf = 0; /* Size of line buffer (and attr buffer) */
static struct ansi_state *line_ansi = NULL;
static lbool ansi_in_line;
static int ff_starts_line;
static lbool hlink_in_line;
static int line_mark_attr;
static int cshift;   /* Current left-shift of output line buffer */
public int hshift;   /* Desired left-shift of output line buffer */
public int tabstops[TABSTOP_MAX] = { 0 }; /* Custom tabstops */
public int ntabstops = 1;        /* Number of tabstops */
public int tabdefault = 8;       /* Default repeated tabstops */
public POSITION highest_hilite;  /* Pos of last hilite in file found so far */
static POSITION line_pos;

static int end_column;  /* Printable length, accounting for backspaces, etc. */
static int right_curr;
static int right_column;
static int overstrike;  /* Next char should overstrike previous char */
static int last_overstrike = AT_NORMAL;
static lbool is_null_line;  /* There is no current line */
static LWCHAR pendc;
static POSITION pendpos;
static constant char *end_ansi_chars;
static constant char *mid_ansi_chars;
static constant char *osc_ansi_chars;
static int osc_ansi_allow_count;
static long *osc_ansi_allow;
static lbool in_hilite;
static lbool clear_after_line;

static int attr_swidth(int a);
static int attr_ewidth(int a);
static int do_append(LWCHAR ch, constant char *rep, POSITION pos);

extern int sigs;
extern int bs_mode;
extern int proc_backspace;
extern int proc_tab;
extern int proc_return;
extern int linenums;
extern int ctldisp;
extern int twiddle;
extern int status_col;
extern int status_col_width;
extern int linenum_width;
extern int auto_wrap, ignaw;
extern int bo_s_width, bo_e_width;
extern int ul_s_width, ul_e_width;
extern int bl_s_width, bl_e_width;
extern int so_s_width, so_e_width;
extern int sc_width, sc_height;
extern int utf_mode;
extern POSITION start_attnpos;
extern POSITION end_attnpos;
extern LWCHAR rscroll_char;
extern int rscroll_attr;
extern int use_color;
extern int status_line;

static char mbc_buf[MAX_UTF_CHAR_LEN];
static int mbc_buf_len = 0;
static int mbc_buf_index = 0;
static POSITION mbc_pos;
static size_t saved_line_end;
static int saved_end_column;

/* Configurable color map */
struct color_map { int attr; char color[12]; };
static struct color_map color_map[] = {
	{ AT_UNDERLINE,            "" },
	{ AT_BOLD,                 "" },
	{ AT_BLINK,                "" },
	{ AT_STANDOUT,             "" },
	{ AT_COLOR_ATTN,           "Wm" },
	{ AT_COLOR_BIN,            "kR" },
	{ AT_COLOR_CTRL,           "kR" },
	{ AT_COLOR_ERROR,          "kY" },
	{ AT_COLOR_LINENUM,        "c" },
	{ AT_COLOR_MARK,           "Wb" },
	{ AT_COLOR_PROMPT,         "kC" },
	{ AT_COLOR_RSCROLL,        "kc" },
	{ AT_COLOR_HEADER,         "" },
	{ AT_COLOR_SEARCH,         "kG" },
	{ AT_COLOR_SUBSEARCH(1),   "ky" },
	{ AT_COLOR_SUBSEARCH(2),   "wb" },
	{ AT_COLOR_SUBSEARCH(3),   "YM" },
	{ AT_COLOR_SUBSEARCH(4),   "Yr" },
	{ AT_COLOR_SUBSEARCH(5),   "Wc" },
};

/* State while processing an ANSI escape sequence */
struct ansi_state {
	osc8_state ostate; /* State while processing OSC8 sequence */
	unsigned int otype; /* OSC type number */
	unsigned int escs_in_seq;
};

/*
 * Initialize from environment variables.
 */
public void init_line(void)
{
	int ax;
	constant char *s;

	end_ansi_chars = lgetenv("LESSANSIENDCHARS");
	if (isnullenv(end_ansi_chars))
		end_ansi_chars = "m";

	mid_ansi_chars = lgetenv("LESSANSIMIDCHARS");
	if (isnullenv(mid_ansi_chars))
		mid_ansi_chars = "0123456789:;[?!\"'#%()*+ ";

	osc_ansi_chars = lgetenv("LESSANSIOSCCHARS");
	if (isnullenv(osc_ansi_chars))
		osc_ansi_chars = "";

	osc_ansi_allow_count = 0;
	s = lgetenv("LESSANSIOSCALLOW");
	if (!isnullenv(s))
	{
		struct xbuffer xbuf;
		xbuf_init(&xbuf);
		for (;;)
		{
			long num;
			s = skipspc(s);
			if (*s == '\0')
				break;
			num = lstrtoulc(s, &s, 10);
			s = skipspc(s);
			if (*s == ',')
				++s;
			xbuf_add_data(&xbuf, (constant void *) &num, sizeof(num));
			++osc_ansi_allow_count;
		}
		osc_ansi_allow = (long *) xbuf.data;
	}

	linebuf.buf = (char *) ecalloc(LINEBUF_SIZE, sizeof(char));
	linebuf.attr = (int *) ecalloc(LINEBUF_SIZE, sizeof(int));
	size_linebuf = LINEBUF_SIZE;
	xbuf_init(&shifted_ansi);
	xbuf_init(&last_ansi);
	for (ax = 0;  ax < NUM_LAST_ANSIS;  ax++)
		xbuf_init(&last_ansis[ax]);
	curr_last_ansi = 0;
}

/*
 * Expand the line buffer.
 */
static int expand_linebuf(void)
{
	/* Double the size of the line buffer. */
	size_t new_size = size_linebuf * 2;
	char *new_buf = (char *) calloc(new_size, sizeof(char));
	int *new_attr = (int *) calloc(new_size, sizeof(int));
	if (new_buf == NULL || new_attr == NULL)
	{
		if (new_attr != NULL)
			free(new_attr);
		if (new_buf != NULL)
			free(new_buf);
		return 1;
	}
	/*
	 * We just calloc'd the buffers; copy the old contents.
	 */
	memcpy(new_buf, linebuf.buf, size_linebuf * sizeof(char));
	memcpy(new_attr, linebuf.attr, size_linebuf * sizeof(int));
	free(linebuf.attr);
	free(linebuf.buf);
	linebuf.buf = new_buf;
	linebuf.attr = new_attr;
	size_linebuf = new_size;
	return 0;
}

/*
 * Is a character ASCII?
 */
public lbool is_ascii_char(LWCHAR ch)
{
	return (ch <= 0x7F);
}

/*
 */
static void inc_end_column(int w)
{
	if (end_column > right_column && w > 0)
	{
		right_column = end_column;
		right_curr = (int) linebuf.end;
	}
	end_column += w;
}

public POSITION line_position(void)
{
	return line_pos;
}

/*
 * Rewind the line buffer.
 */
public void prewind(void)
{
	int ax;

	linebuf.print = 6; /* big enough for longest UTF-8 sequence */
	linebuf.pfx_end = 0;
	for (linebuf.end = 0; linebuf.end < linebuf.print; linebuf.end++)
	{
		linebuf.buf[linebuf.end] = '\0';
		linebuf.attr[linebuf.end] = 0;
	}

	end_column = 0;
	right_curr = 0;
	right_column = 0;
	cshift = 0;
	overstrike = 0;
	last_overstrike = AT_NORMAL;
	mbc_buf_len = 0;
	is_null_line = FALSE;
	pendc = '\0';
	in_hilite = FALSE;
	ansi_in_line = FALSE;
	ff_starts_line = -1;
	hlink_in_line = FALSE;
	clear_after_line = FALSE;
	line_mark_attr = 0;
	line_pos = NULL_POSITION;
	xbuf_reset(&shifted_ansi);
	xbuf_reset(&last_ansi);
	for (ax = 0;  ax < NUM_LAST_ANSIS;  ax++)
		xbuf_reset(&last_ansis[ax]);
	curr_last_ansi = 0;
}

/*
 * Set a character in the line buffer.
 */
static void set_linebuf(size_t n, char ch, int attr)
{
	if (n >= size_linebuf)
	{
		/*
		 * Won't fit in line buffer.
		 * Try to expand it.
		 */
		if (expand_linebuf())
			return;
	}
	linebuf.buf[n] = ch;
	linebuf.attr[n] = attr;
}

/*
 * Append a character to the line buffer.
 */
static void add_linebuf(char ch, int attr, int w)
{
	set_linebuf(linebuf.end++, ch, attr);
	inc_end_column(w);
}

/*
 * Append a string to the line buffer.
 */
static void addstr_linebuf(constant char *s, int attr, int cw)
{
	for ( ;  *s != '\0';  s++)
		add_linebuf(*s, attr, cw);
}

/*
 * Set a character in the line prefix buffer.
 */
static void set_pfx(size_t n, char ch, int attr)
{
	linebuf.pfx[n] = ch;
	linebuf.pfx_attr[n] = attr;
}

/*
 * Append a character to the line prefix buffer.
 */
static void add_pfx(char ch, int attr)
{
	set_pfx(linebuf.pfx_end++, ch, attr);
}

/*
 * Insert the status column and line number into the line buffer.
 */
public void plinestart(POSITION pos)
{
	LINENUM linenum = 0;

	if (linenums == OPT_ONPLUS)
	{
		/*
		 * Get the line number and put it in the current line.
		 * {{ Note: since find_linenum calls forw_raw_line,
		 *    it may seek in the input file, requiring the caller 
		 *    of plinestart to re-seek if necessary. }}
		 * {{ Since forw_raw_line modifies linebuf, we must
		 *    do this first, before storing anything in linebuf. }}
		 */
		linenum = find_linenum(pos);
	}

	/*
	 * Display a status column if the -J option is set.
	 */
	if (status_col || status_line)
	{
		char c = posmark(pos);
		if (c != 0)
			line_mark_attr = AT_HILITE|AT_COLOR_MARK;
		else if (start_attnpos != NULL_POSITION &&
		         pos >= start_attnpos && pos <= end_attnpos)
			line_mark_attr = AT_HILITE|AT_COLOR_ATTN;
		if (status_col)
		{
			add_pfx(c ? c : ' ', line_mark_attr); /* column 0: status */
			while (linebuf.pfx_end < (size_t) status_col_width) /*{{type-issue}}*/
				add_pfx(' ', AT_NORMAL);
		}
	}

	/*
	 * Display the line number at the start of each line
	 * if the -N option is set.
	 */
	if (linenums == OPT_ONPLUS)
	{
		char buf[INT_STRLEN_BOUND(linenum) + 2];
		size_t len;
		size_t i;

		linenum = vlinenum(linenum);
		if (linenum == 0)
			len = 0;
		else
		{
			linenumtoa(linenum, buf, 10);
			len = strlen(buf);
		}
		for (i = 0; i + len < (size_t) linenum_width; i++)
			add_pfx(' ', AT_NORMAL);
		for (i = 0; i < len; i++)
			add_pfx(buf[i], AT_BOLD|AT_COLOR_LINENUM);
		add_pfx(' ', AT_NORMAL);
	}
	end_column = (int) linebuf.pfx_end; /*{{type-issue}}*/
}

/*
 * Return the width of the line prefix (status column and line number).
 * {{ Actual line number can be wider than linenum_width. }}
 */
public int line_pfx_width(void)
{
	int width = 0;
	if (status_col)
		width += status_col_width;
	if (linenums == OPT_ONPLUS)
		width += linenum_width + 1;
	return width;
}

/*
 * Shift line left so that the last char is just to the left
 * of the first visible column.
 */
public void pshift_all(void)
{
	size_t i;
	for (i = linebuf.print;  i < linebuf.end;  i++)
		if (linebuf.attr[i] == AT_ANSI)
			xbuf_add_char(&shifted_ansi, linebuf.buf[i]);
	linebuf.end = linebuf.print;
	end_column = (int) linebuf.pfx_end; /*{{type-issue}}*/
	line_pos = NULL_POSITION;
}

/*
 * Return the printing width of the start (enter) sequence
 * for a given character attribute.
 */
static int attr_swidth(int a)
{
	int w = 0;

	a = apply_at_specials(a);

	if (a & AT_UNDERLINE)
		w += ul_s_width;
	if (a & AT_BOLD)
		w += bo_s_width;
	if (a & AT_BLINK)
		w += bl_s_width;
	if (a & AT_STANDOUT)
		w += so_s_width;

	return w;
}

/*
 * Return the printing width of the end (exit) sequence
 * for a given character attribute.
 */
static int attr_ewidth(int a)
{
	int w = 0;

	a = apply_at_specials(a);

	if (a & AT_UNDERLINE)
		w += ul_e_width;
	if (a & AT_BOLD)
		w += bo_e_width;
	if (a & AT_BLINK)
		w += bl_e_width;
	if (a & AT_STANDOUT)
		w += so_e_width;

	return w;
}

/*
 * Return the printing width of a given character and attribute,
 * if the character were added after prev_ch.
 * Adding a character with a given attribute may cause an enter or exit
 * attribute sequence to be inserted, so this must be taken into account.
 */
public int pwidth(LWCHAR ch, int a, LWCHAR prev_ch, int prev_a)
{
	int w;

	if (ch == '\b')
	{
		/*
		 * Backspace moves backwards one or two positions.
		 */
		if (prev_a & (AT_ANSI|AT_BINARY))
			return (int) strlen(prchar('\b')); /*{{type-issue}}*/
		return (utf_mode && is_wide_char(prev_ch)) ? -2 : -1;
	}

	if (!utf_mode || is_ascii_char(ch))
	{
		if (control_char(ch))
		{
			/*
			 * Control characters do unpredictable things,
			 * so we don't even try to guess; say it doesn't move.
			 * This can only happen if the -r flag is in effect.
			 */
			return (0);
		}
	} else
	{
		if (is_composing_char(ch) || is_combining_char(prev_ch, ch))
		{
			/*
			 * Composing and combining chars take up no space.
			 *
			 * Some terminals, upon failure to compose a
			 * composing character with the character(s) that
			 * precede(s) it will actually take up one end_column
			 * for the composing character; there isn't much
			 * we could do short of testing the (complex)
			 * composition process ourselves and printing
			 * a binary representation when it fails.
			 */
			return (0);
		}
	}

	/*
	 * Other characters take one or two columns,
	 * plus the width of any attribute enter/exit sequence.
	 */
	w = 1;
	if (is_wide_char(ch))
		w++;
	if (linebuf.end > 0 && !is_at_equiv(linebuf.attr[linebuf.end-1], a))
		w += attr_ewidth(linebuf.attr[linebuf.end-1]);
	if (apply_at_specials(a) != AT_NORMAL &&
	    (linebuf.end == 0 || !is_at_equiv(linebuf.attr[linebuf.end-1], a)))
		w += attr_swidth(a);
	return (w);
}

/*
 * Delete to the previous base character in the line buffer.
 */
static int backc(void)
{
	LWCHAR ch;
	char *p;

	if (linebuf.end == 0)
		return (0);
	p = &linebuf.buf[linebuf.end];
	ch = step_char(&p, -1, linebuf.buf);
	/* Skip back to the next nonzero-width char. */
	while (p > linebuf.buf)
	{
		LWCHAR prev_ch;
		int width;
		linebuf.end = ptr_diff(p, linebuf.buf);
		prev_ch = step_char(&p, -1, linebuf.buf);
		width = pwidth(ch, linebuf.attr[linebuf.end], prev_ch, linebuf.attr[linebuf.end-1]);
		end_column -= width;
		/* {{ right_column? }} */
		if (width > 0)
			break;
		ch = prev_ch;
	}
	return (1);
}

/*
 * Preserve the current position in the line buffer (for word wrapping).
 */
public void savec(void)
{
	saved_line_end = linebuf.end;
	saved_end_column = end_column;
}

/*
 * Restore the position in the line buffer (start of line for word wrapping).
 */
public void loadc(void)
{
	linebuf.end = saved_line_end;
	end_column = saved_end_column;
}

/*
 * Is a character the end of an ANSI escape sequence?
 */
public lbool is_ansi_end(LWCHAR ch)
{
	if (!is_ascii_char(ch))
		return (FALSE);
	return (ch != 0 && strchr(end_ansi_chars, (char) ch) != NULL);
}

/*
 * Can a char appear in an ANSI escape sequence, before the end char?
 */
public lbool is_ansi_middle(LWCHAR ch)
{
	if (!is_ascii_char(ch))
		return (FALSE);
	if (is_ansi_end(ch))
		return (FALSE);
	return (ch != 0 && strchr(mid_ansi_chars, (char) ch) != NULL);
}

/*
 * Skip past an ANSI escape sequence.
 * pp is initially positioned just after the CSI_START char.
 */
public void skip_ansi(struct ansi_state *pansi, LWCHAR ch, constant char **pp, constant char *limit)
{
	ansi_step(pansi, ch);
	do {
		ch = step_charc(pp, +1, limit);
	} while (*pp < limit && ansi_step(pansi, ch) == ANSI_MID);
	/* Note that we discard final char, for which is_ansi_end is true. */
}

/*
 * Determine if a character starts an ANSI escape sequence.
 * If so, return an ansi_state struct; otherwise return NULL.
 */
public struct ansi_state * ansi_start(LWCHAR ch)
{
	struct ansi_state *pansi;

	if (!IS_CSI_START(ch))
		return NULL;
	pansi = ecalloc(1, sizeof(struct ansi_state));
	pansi->ostate = OSC_START;
	pansi->otype = 0;
	pansi->escs_in_seq = 0;
	return pansi;
}

/*
 * Is a character a valid intro char for an OSC sequence?
 * An intro char is the one immediately after the ESC, usually ']'.
 */
static lbool valid_osc_intro(char ch, lbool content)
{
	constant char *p = strchr(osc_ansi_chars, ch);
	if (p == NULL)
		return FALSE;
	return (!content || p[1] == '*');
}

/*
 * Is a given number a valid OSC type?
 */
static lbool valid_osc_type(int otype, lbool content)
{
	int i;
	if (!content)
		return TRUE;
	if (otype == 8)
		return TRUE;
	for (i = 0;  i < osc_ansi_allow_count;  i++)
		if (osc_ansi_allow[i] == otype)
			return TRUE;
	return FALSE;
}

/*
 * Helper function for ansi_step.
 */
static ansi_state osc_return(struct ansi_state *pansi, osc8_state ostate, ansi_state astate)
{
	pansi->ostate = ostate;
	return astate;
}

/*
 * Determine whether the next char in an ANSI escape sequence
 * ends the sequence.
 */
static ansi_state ansi_step2(struct ansi_state *pansi, LWCHAR ch, lbool content)
{
	/*
	 * Pass thru OS commands. Assume OSC commands do not move the cursor.
	 * A "typed" OSC starts with ESC ] <integer> <semicolon>, followed by an
	 * arbitrary string, and ends with a String Terminator (ESC-backslash or BEL).
	 * An untyped OSC starts with ESC ] or ESC x where x is in osc_ansi_chars,
	 * and ends with ST.
	 * The only typed OSC we actually parse is OSC 8.
	 */
	switch (pansi->ostate)
	{
	case OSC_START:
		if (IS_CSI_START(ch))
			return osc_return(pansi, OSC_INTRO, ANSI_MID);
		break;
	case OSC_INTRO:
		if (ch == ']')
			return osc_return(pansi, OSC_TYPENUM, ANSI_MID);
		if (is_ascii_char(ch) && valid_osc_intro((char) ch, content))
			return osc_return(pansi, OSC_STRING, ANSI_MID);
		if (IS_CSI_START(ch))
			return osc_return(pansi, OSC_INTRO, ANSI_MID);
		/* ESC not followed by bracket; restart. */
		pansi->ostate = OSC_START;
		break;
	case OSC_TYPENUM:
		if (ch >= '0' && ch <= '9')
		{
			if (ckd_mul(&pansi->otype, pansi->otype, 10) ||
			    ckd_add(&pansi->otype, pansi->otype, ch - '0'))
				return osc_return(pansi, OSC_STRING, ANSI_MID);
			return osc_return(pansi, OSC_TYPENUM, ANSI_MID);
		}
		if (ch == ';')
			return osc_return(pansi, (pansi->otype == 8) ? OSC8_PARAMS : OSC_STRING, ANSI_MID);
		/* OSC is untyped */
		if (IS_CSI_START(ch))
			return osc_return(pansi, OSC_END_CSI, ANSI_MID);
		if (ch == '\7')
			return osc_return(pansi, OSC_END, ANSI_END);
		return osc_return(pansi, OSC_STRING, ANSI_MID);
	case OSC8_PARAMS:
		if (ch == ';')
			return osc_return(pansi, OSC8_URI, ANSI_MID);
		/* FALLTHRU */
	case OSC8_URI:
	case OSC_STRING:
		/* Look for ST. */
		if (ch == '\7')
			return osc_return(pansi, OSC_END, valid_osc_type(pansi->otype, content) ? ANSI_END : ANSI_ERR);
		if (IS_CSI_START(ch))
		{
			pansi->escs_in_seq++;
			return osc_return(pansi, OSC_END_CSI, ANSI_MID);
		}
		/* Stay in same ostate */
		return ANSI_MID;
	case OSC_END_CSI:
		/* Got ESC of ST, expect backslash next. */
		if (ch == '\\')
			return osc_return(pansi, OSC_END, valid_osc_type(pansi->otype, content) ? ANSI_END : ANSI_ERR);
		/* ESC not followed by backslash. */
		return osc_return(pansi, OSC_STRING, ANSI_MID);
	case OSC_END:
		return ANSI_END;
	case OSC8_NOT:
		/* cannot happen */
		break;
	}
	/* Check for SGR sequences */
	if (is_ansi_middle(ch))
		return ANSI_MID;
	if (is_ansi_end(ch))
		return ANSI_END;
	return ANSI_ERR;
}

public ansi_state ansi_step(struct ansi_state *pansi, LWCHAR ch)
{
	return ansi_step2(pansi, ch, TRUE);
}

/*
 * Return the current OSC8 parsing state.
 */
public osc8_state ansi_osc8_state(struct ansi_state *pansi)
{
	return pansi->ostate;
}

/*
 * Free an ansi_state structure.
 */
public void ansi_done(struct ansi_state *pansi)
{
	free(pansi);
}

/*
 * Will w characters in attribute a fit on the screen?
 */
static lbool fits_on_screen(int w, int a)
{
	if (ctldisp == OPT_ON)
		/* We're not counting, so say that everything fits. */
		return TRUE;
	return (end_column - cshift + w + attr_ewidth(a) <= sc_width);
}

/*
 * Append a character and attribute to the line buffer.
 */
#define STORE_CHAR(ch,a,rep,pos) \
	do { \
		if (store_char((ch),(a),(rep),(pos))) return (1); \
	} while (0)

static int store_char(LWCHAR ch, int a, constant char *rep, POSITION pos)
{
	int w;
	size_t i;
	size_t replen;
	char cs;
	int ov;

	ov = (a & (AT_UNDERLINE|AT_BOLD));
	if (ov != AT_NORMAL)
		last_overstrike = ov;

#if HILITE_SEARCH
	{
		int matches;
		int resend_last = 0;
		int hl_attr = 0;

		if (pos != NULL_POSITION && a != AT_ANSI)
		{
			hl_attr = is_hilited_attr(pos, pos+1, 0, &matches);
			if (hl_attr == 0 && status_line)
				hl_attr = line_mark_attr;
		}
		if (hl_attr)
		{
			/*
			 * This character should be highlighted.
			 * Override the attribute passed in.
			 */
			a |= hl_attr;
			if (highest_hilite != NULL_POSITION && pos != NULL_POSITION && pos > highest_hilite)
				highest_hilite = pos;
			in_hilite = TRUE;
		} else 
		{
			if (in_hilite)
			{
				/*
				 * This is the first non-hilited char after a hilite.
				 * Resend the last ANSI seq to restore color.
				 */
				resend_last = 1;
			}
			in_hilite = FALSE;
		}
		if (resend_last)
		{
			int ai;
			for (ai = 0;  ai < NUM_LAST_ANSIS;  ai++)
			{
				int ax = (curr_last_ansi + ai) % NUM_LAST_ANSIS;
				for (i = 0;  i < last_ansis[ax].end;  i++)
					STORE_CHAR(last_ansis[ax].data[i], AT_ANSI, NULL, pos);
			}
		}
	}
#endif

	if (a == AT_ANSI) {
		w = 0;
	} else {
		char *p = &linebuf.buf[linebuf.end];
		LWCHAR prev_ch = (linebuf.end > 0) ? step_char(&p, -1, linebuf.buf) : 0;
		int prev_a = (linebuf.end > 0) ? linebuf.attr[linebuf.end-1] : 0;
		w = pwidth(ch, a, prev_ch, prev_a);
	}

	if (!fits_on_screen(w, a))
		return (1);

	if (rep == NULL)
	{
		cs = (char) ch;
		rep = &cs;
		replen = 1;
	} else
	{
		replen = (size_t) utf_len(rep[0]); /*{{type-issue}}*/
	}

	if (cshift == hshift)
	{
		if (line_pos == NULL_POSITION)
			line_pos = pos;
		if (shifted_ansi.end > 0)
		{
			/* Copy shifted ANSI sequences to beginning of line. */
			for (i = 0;  i < shifted_ansi.end;  i++)
				add_linebuf((char) shifted_ansi.data[i], AT_ANSI, 0);
			xbuf_reset(&shifted_ansi);
		}
	}

	/* Add the char to the buf, even if we will left-shift it next. */
	inc_end_column(w);
	for (i = 0;  i < replen;  i++)
		add_linebuf(*rep++, a, 0);

	if (cshift < hshift)
	{
		/* We haven't left-shifted enough yet. */
		if (a == AT_ANSI)
			xbuf_add_char(&shifted_ansi, (char) ch); /* Save ANSI attributes */
		if (linebuf.end > linebuf.print)
		{
			/* Shift left enough to put last byte of this char at print-1. */
			size_t i;
			for (i = 0; i < linebuf.print; i++)
			{
				linebuf.buf[i] = linebuf.buf[i+replen];
				linebuf.attr[i] = linebuf.attr[i+replen];
			}
			linebuf.end -= replen;
			cshift += w;
			/*
			 * If the char we just left-shifted was double width,
			 * the 2 spaces we shifted may be too much.
			 * Represent the "half char" at start of line with a highlighted space.
			 */
			while (cshift > hshift)
			{
				add_linebuf(' ', rscroll_attr, 0);
				cshift--;
			}
		}
	}
	return (0);
}

#define STORE_STRING(s,a,pos) \
	do { if (store_string((s),(a),(pos))) return (1); } while (0)

static int store_string(constant char *s, int a, POSITION pos)
{
	if (!fits_on_screen((int) strlen(s), a))
		return 1;
	for ( ;  *s != 0;  s++)
		STORE_CHAR((LWCHAR)*s, a, NULL, pos);
	return 0;
}

/*
 * Return number of spaces from col to the next tab stop.
 */
static int tab_spaces(int col)
{
	int to_tab = col - (int) linebuf.pfx_end; /*{{type-issue}}*/

	if (ntabstops < 2 || to_tab >= tabstops[ntabstops-1])
		to_tab = tabdefault -
		     ((to_tab - tabstops[ntabstops-1]) % tabdefault);
	else
	{
		int i;
		for (i = ntabstops - 2;  i >= 0;  i--)
			if (to_tab >= tabstops[i])
				break;
		to_tab = tabstops[i+1] - to_tab;
	}
	return to_tab;
}

/*
 * Append a tab to the line buffer.
 * Store spaces to represent the tab.
 */
#define STORE_TAB(a,pos) \
	do { if (store_tab((a),(pos))) return (1); } while (0)

static int store_tab(int attr, POSITION pos)
{
	int to_tab = tab_spaces(end_column);
	do {
		STORE_CHAR(' ', attr, " ", pos);
	} while (--to_tab > 0);
	return 0;
}

#define STORE_PRCHAR(c, pos) \
	do { if (store_prchar((c), (pos))) return 1; } while (0)

static int store_prchar(LWCHAR c, POSITION pos)
{
	/*
	 * Convert to printable representation.
	 */
	STORE_STRING(prchar(c), AT_BINARY|AT_COLOR_CTRL, pos);
	return 0;
}

static int flush_mbc_buf(POSITION pos)
{
	int i;

	for (i = 0; i < mbc_buf_index; i++)
		if (store_prchar((LWCHAR) mbc_buf[i], pos))
			return mbc_buf_index - i;
	return 0;
}

/*
 * Append a character to the line buffer.
 * Expand tabs into spaces, handle underlining, boldfacing, etc.
 * Returns 0 if ok, 1 if couldn't fit in buffer.
 */
public int pappend_b(char c, POSITION pos, lbool before_pendc)
{
	LWCHAR ch = c & 0377;
	int r;

	if (pendc && !before_pendc)
	{
		if (ch == '\r' && pendc == '\r')
			return (0);
		if (do_append(pendc, NULL, pendpos))
			/*
			 * Oops.  We've probably lost the char which
			 * was in pendc, since caller won't back up.
			 */
			return (1);
		pendc = '\0';
	}

	if (ch == '\r' && (proc_return == OPT_ON || (bs_mode == BS_SPECIAL && proc_return == OPT_OFF)))
	{
		if (mbc_buf_len > 0)  /* utf_mode must be on. */
		{
			/* Flush incomplete (truncated) sequence. */
			r = flush_mbc_buf(mbc_pos);
			mbc_buf_index = r + 1;
			mbc_buf_len = 0;
			if (r)
				return (mbc_buf_index);
		}

		/*
		 * Don't put the CR into the buffer until we see 
		 * the next char.  If the next char is a newline,
		 * discard the CR.
		 */
		pendc = ch;
		pendpos = pos;
		return (0);
	}

	if (!utf_mode)
	{
		r = do_append(ch, NULL, pos);
	} else
	{
		/* Perform strict validation in all possible cases. */
		if (mbc_buf_len == 0)
		{
		retry:
			mbc_buf_index = 1;
			*mbc_buf = c;
			if (IS_ASCII_OCTET(c))
				r = do_append(ch, NULL, pos);
			else if (IS_UTF8_LEAD(c))
			{
				mbc_buf_len = utf_len(c);
				mbc_pos = pos;
				return (0);
			} else
				/* UTF8_INVALID or stray UTF8_TRAIL */
				r = flush_mbc_buf(pos);
		} else if (IS_UTF8_TRAIL(c))
		{
			mbc_buf[mbc_buf_index++] = c;
			if (mbc_buf_index < mbc_buf_len)
				return (0);
			if (is_utf8_well_formed(mbc_buf, mbc_buf_index))
				r = do_append(get_wchar(mbc_buf), mbc_buf, mbc_pos);
			else
				/* Complete, but not shortest form, sequence. */
				mbc_buf_index = r = flush_mbc_buf(mbc_pos);
			mbc_buf_len = 0;
		} else
		{
			/* Flush incomplete (truncated) sequence.  */
			r = flush_mbc_buf(mbc_pos);
			mbc_buf_index = r + 1;
			mbc_buf_len = 0;
			/* Handle new char.  */
			if (!r)
				goto retry;
		}
	}
	if (r)
	{
		/* How many chars should caller back up? */
		r = (!utf_mode) ? 1 : mbc_buf_index;
	}
	return (r);
}

public int pappend(char c, POSITION pos)
{
	if (ff_starts_line < 0)
		ff_starts_line = (c == CONTROL('L'));
	return pappend_b(c, pos, FALSE);
}

public lbool line_is_ff(void)
{
	return (ff_starts_line == 1);
}

static int store_control_char(LWCHAR ch, constant char *rep, POSITION pos)
{
	if (ctldisp == OPT_ON)
	{
		/* Output the character itself. */
		STORE_CHAR(ch, AT_NORMAL, rep, pos);
	} else 
	{
		/* Output a printable representation of the character. */
		STORE_PRCHAR(ch, pos);
	}
	return (0);
}

static int store_ansi(LWCHAR ch, constant char *rep, POSITION pos)
{
	switch (ansi_step2(line_ansi, ch, pos != NULL_POSITION))
	{
	case ANSI_MID:
		STORE_CHAR(ch, AT_ANSI, rep, pos);
		switch (ansi_osc8_state(line_ansi))
		{
		case OSC_TYPENUM: case OSC_STRING: hlink_in_line = TRUE; break;
		default: break;
		}
		xbuf_add_char(&last_ansi, (char) ch);
		break;
	case ANSI_END:
		STORE_CHAR(ch, AT_ANSI, rep, pos);
		ansi_done(line_ansi);
		line_ansi = NULL;
		xbuf_add_char(&last_ansi, (char) ch);
		xbuf_set(&last_ansis[curr_last_ansi], &last_ansi);
		xbuf_reset(&last_ansi);
		curr_last_ansi = (curr_last_ansi + 1) % NUM_LAST_ANSIS;
		break;
	case ANSI_ERR:
		{
			/* Remove whole unrecognized sequence.  */
			constant char *start = (cshift < hshift) ? xbuf_char_data(&shifted_ansi): linebuf.buf;
			size_t *end = (cshift < hshift) ? &shifted_ansi.end : &linebuf.end;
			constant char *p = start + *end;
			LWCHAR bch;
			do {
				bch = step_charc(&p, -1, start);
			} while (p > start && (!IS_CSI_START(bch) || line_ansi->escs_in_seq-- > 0));
			*end = ptr_diff(p, start);
		}
		xbuf_reset(&last_ansi);
		ansi_done(line_ansi);
		line_ansi = NULL;
		break;
	default:
		break;
	}
	return (0);
} 

static int store_bs(LWCHAR ch, constant char *rep, POSITION pos)
{
	if (proc_backspace == OPT_ONPLUS || (bs_mode == BS_CONTROL && proc_backspace == OPT_OFF))
		return store_control_char(ch, rep, pos);
	if (linebuf.end > 0 &&
		((linebuf.end <= linebuf.print && linebuf.buf[linebuf.end-1] == '\0') ||
	     (linebuf.end > 0 && linebuf.attr[linebuf.end - 1] & (AT_ANSI|AT_BINARY))))
		STORE_PRCHAR('\b', pos);
	else if (proc_backspace == OPT_OFF && bs_mode == BS_NORMAL)
		STORE_CHAR(ch, AT_NORMAL, NULL, pos);
	else if (proc_backspace == OPT_ON || (bs_mode == BS_SPECIAL && proc_backspace == OPT_OFF))
		overstrike = backc();
	return 0;
}

static int do_append(LWCHAR ch, constant char *rep, POSITION pos)
{
	int a = AT_NORMAL;
	int in_overstrike = overstrike;

	if ((ctldisp == OPT_ONPLUS || pos == NULL_POSITION) && line_ansi == NULL)
	{
		line_ansi = ansi_start(ch);
		if (line_ansi != NULL)
			ansi_in_line = TRUE;
	}

	overstrike = 0;
	if (line_ansi != NULL)
		return store_ansi(ch, rep, pos);

	if (ch == '\b')
		return store_bs(ch, rep, pos);

	if (in_overstrike > 0)
	{
		/*
		 * Overstrike the character at the current position
		 * in the line buffer.  This will cause either 
		 * underline (if a "_" is overstruck), 
		 * bold (if an identical character is overstruck),
		 * or just replacing the character in the buffer.
		 */
		LWCHAR prev_ch;
		overstrike = utf_mode ? -1 : 0;
		if (utf_mode)
		{
			/* To be correct, this must be a base character.  */
			prev_ch = get_wchar(&linebuf.buf[linebuf.end]);
		} else
		{
			prev_ch = (unsigned char) linebuf.buf[linebuf.end];
		}
		a = linebuf.attr[linebuf.end];
		if (ch == prev_ch)
		{
			/*
			 * Overstriking a char with itself means make it bold.
			 * But overstriking an underscore with itself is
			 * ambiguous.  It could mean make it bold, or
			 * it could mean make it underlined.
			 * Use the previous overstrike to resolve it.
			 */
			if (ch == '_')
			{
				if ((a & (AT_BOLD|AT_UNDERLINE)) != AT_NORMAL)
					a |= (AT_BOLD|AT_UNDERLINE);
				else if (last_overstrike != AT_NORMAL)
					a |= last_overstrike;
				else
					a |= AT_BOLD;
			} else
				a |= AT_BOLD;
		} else if (ch == '_')
		{
			a |= AT_UNDERLINE;
			ch = prev_ch;
			rep = &linebuf.buf[linebuf.end];
		} else if (prev_ch == '_')
		{
			a |= AT_UNDERLINE;
		}
		/* Else we replace prev_ch, but we keep its attributes.  */
	} else if (in_overstrike < 0)
	{
		if (   is_composing_char(ch)
		    || is_combining_char(get_wchar(&linebuf.buf[linebuf.end]), ch))
			/* Continuation of the same overstrike.  */
			a = last_overstrike;
		else
			overstrike = 0;
	}

	if (ch == '\t')
	{
		/*
		 * Expand a tab into spaces.
		 */
		if (proc_tab == OPT_ONPLUS || (bs_mode == BS_CONTROL && proc_tab == OPT_OFF))
			return store_control_char(ch, rep, pos);
		STORE_TAB(a, pos);
		return (0);
	}
	if ((!utf_mode || is_ascii_char(ch)) && control_char(ch))
	{
		return store_control_char(ch, rep, pos);
	} else if (utf_mode && ctldisp != OPT_ON && is_ubin_char(ch))
	{
		STORE_STRING(prutfchar(ch), AT_BINARY, pos);
	} else
	{
		STORE_CHAR(ch, a, rep, pos);
	}
	return (0);
}

/*
 *
 */
public int pflushmbc(void)
{
	int r = 0;

	if (mbc_buf_len > 0)
	{
		/* Flush incomplete (truncated) sequence.  */
		r = flush_mbc_buf(mbc_pos);
		mbc_buf_len = 0;
	}
	return r;
}

/*
 * Switch to normal attribute at end of line.
 */
static void add_attr_normal(void)
{
	if (line_ansi != NULL)
	{
		switch (line_ansi->ostate)
		{
		case OSC_TYPENUM:
		case OSC8_PARAMS:
		case OSC8_URI:
		case OSC_STRING:
			addstr_linebuf("\033\\", AT_ANSI, 0);
			break;
		default:
			break;
		}
		ansi_done(line_ansi);
		line_ansi = NULL;
	}
	if (ctldisp != OPT_ONPLUS || !is_ansi_end('m'))
		return;
	addstr_linebuf("\033[m", AT_ANSI, 0);
	if (hlink_in_line) /* Don't send hyperlink clear if we know we don't need to. */
		addstr_linebuf("\033]8;;\033\\", AT_ANSI, 0);
}

/*
 * Terminate the line in the line buffer.
 */
public void pdone(lbool endline, lbool chopped, lbool forw)
{
	(void) pflushmbc();

	if (pendc && (pendc != '\r' || !endline))
		/*
		 * If we had a pending character, put it in the buffer.
		 * But discard a pending CR if we are at end of line
		 * (that is, discard the CR in a CR/LF sequence).
		 */
		(void) do_append(pendc, NULL, pendpos);

	if (chopped && rscroll_char)
	{
		char rscroll_utf8[MAX_UTF_CHAR_LEN+1];
		char *up = rscroll_utf8;

		/*
		 * Display the right scrolling char.
		 * If we've already filled the rightmost screen char 
		 * (in the buffer), overwrite it.
		 */
		if (end_column >= sc_width + cshift)
		{
			/* We've already written in the rightmost char. */
			end_column = right_column;
			linebuf.end = (size_t) right_curr;
		}
		add_attr_normal();
		while (end_column < sc_width-1 + cshift) 
		{
			/*
			 * Space to last (rightmost) char on screen.
			 * This may be necessary if the char we overwrote
			 * was double-width.
			 */
			add_linebuf(' ', 0, 1);
		}
		/* Print rscroll char. */
		put_wchar(&up, rscroll_char);
		*up = '\0';
		addstr_linebuf(rscroll_utf8, rscroll_attr, 0);
		inc_end_column(1); /* assume rscroll_char is single-width */
	} else
	{
		add_attr_normal();
	}

	/*
	 * If we're coloring a status line, fill out the line with spaces.
	 */
	if (status_line && line_mark_attr != 0) {
		while (end_column +1 < sc_width + cshift)
			add_linebuf(' ', line_mark_attr, 1);
	}

	/*
	 * Add a newline if necessary,
	 * and append a '\0' to the end of the line.
	 * We output a newline if we're not at the right edge of the screen,
	 * or if the terminal doesn't auto wrap,
	 * or if this is really the end of the line AND the terminal ignores
	 * a newline at the right edge.
	 * (In the last case we don't want to output a newline if the terminal 
	 * doesn't ignore it since that would produce an extra blank line.
	 * But we do want to output a newline if the terminal ignores it in case
	 * the next line is blank.  In that case the single newline output for
	 * that blank line would be ignored!)
	 */
	if (end_column < sc_width + cshift || !auto_wrap || (endline && ignaw) || ctldisp == OPT_ON)
	{
		add_linebuf('\n', AT_NORMAL, 0);
	} 
	else if (ignaw && end_column >= sc_width + cshift && forw)
	{
		/*
		 * Terminals with "ignaw" don't wrap until they *really* need
		 * to, i.e. when the character *after* the last one to fit on a
		 * line is output. But they are too hard to deal with when they
		 * get in the state where a full screen width of characters
		 * have been output but the cursor is sitting on the right edge
		 * instead of at the start of the next line.
		 * So we nudge them into wrapping by outputting a space 
		 * character plus a backspace.  But do this only if moving 
		 * forward; if we're moving backward and drawing this line at
		 * the top of the screen, the space would overwrite the first
		 * char on the next line.  We don't need to do this "nudge" 
		 * at the top of the screen anyway.
		 */
		add_linebuf(' ', AT_NORMAL, 1);
		add_linebuf('\b', AT_NORMAL, -1);
	}
	/*
	 * If a terminal moves the cursor to the next line immediately after
	 * writing into the last char of a line, the following line may get
	 * colored with the last char's background color before the color
	 * reset sequence is sent. Clear the line to reset the background color.
	 */
	if (auto_wrap && !ignaw && end_column >= sc_width + cshift)
		clear_after_line = TRUE;
	set_linebuf(linebuf.end, '\0', AT_NORMAL);
}

/*
 * Return the column number (screen position) of a given file position in its line.
 * linepos = position of first char in line
 * spos = position of char being queried
 * saved_pos = position of a known column, or NULL_POSITION if no known column
 * saved_col = column number of a known column, or -1 if no known column
 *
 * This attempts to mimic the logic in pappend() and the store_*() functions.
 * Duplicating this complicated logic is not a good design.
 */

struct col_pos { int col; POSITION pos; };

static void col_vs_pos(POSITION linepos, mutable struct col_pos *cp, POSITION saved_pos, int saved_col)
{
	int col = (saved_col < 0) ? 0 : saved_col;
	LWCHAR prev_ch = 0;
	struct ansi_state *pansi = NULL;
	char utf8_buf[MAX_UTF_CHAR_LEN];
	int utf8_len = 0;
	POSITION chpos;

	if (ch_seek(saved_pos != NULL_POSITION ? saved_pos : linepos))
		return;
	for (;;)
	{
		int ich;
		char ch;
		int cw = 0;

		chpos = ch_tell();
		ich = ch_forw_get();
		ch = (char) ich;
		if (ich == EOI || ch == '\n')
			break;
		if (pansi != NULL)
		{
			if (ansi_step(pansi, ch) != ANSI_MID)
			{
				ansi_done(pansi);
				pansi = NULL;
			}
		} else if (ctldisp == OPT_ONPLUS && (pansi = ansi_start(ch)) != NULL)
		{
			/* start of ansi sequence */
			(void) ansi_step(pansi, ch);
		} else if (ch == '\b')
		{
			if (proc_backspace == OPT_ONPLUS || (bs_mode == BS_CONTROL && proc_backspace == OPT_OFF))
				cw = (int) strlen(prchar(ch));
			else
				cw = (utf_mode && is_wide_char(prev_ch)) ? -2 : -1;
		} else if (ch == '\t')
		{
			if (proc_tab == OPT_ONPLUS || (bs_mode == BS_CONTROL && proc_tab == OPT_OFF))
				cw = (int) strlen(prchar(ch));
			else
				cw = tab_spaces(col);
		} else if ((!utf_mode || is_ascii_char(ch)) && control_char(ch))
		{
			cw = (int) strlen(prchar(ch));
		} else if (utf8_len < MAX_UTF_CHAR_LEN)
		{
			utf8_buf[utf8_len++] = ch;
			if (is_utf8_well_formed(utf8_buf, utf8_len))
			{
				LWCHAR wch = get_wchar(utf8_buf);
				int attr = 0; /* {{ ignoring attribute is not correct for magic cookie terminals }} */
				utf8_len = 0;
				if (utf_mode && ctldisp != OPT_ON && is_ubin_char(wch))
					cw = (int) strlen(prutfchar(wch));
				else
					cw = pwidth(wch, attr, prev_ch, attr);
				prev_ch = wch;
			}
		} else
		{
			utf8_len = 0; /* flush invalid UTF-8 */
		}

		if (cp->pos != NULL_POSITION && chpos == cp->pos) /* found the position we want */
			break;
		if (cp->col >= 0 && col >= cp->col && cw > 0) /* found the column we want */
			break;
		col += cw;
		prev_ch = ch;
	}
	cp->col = col;
	cp->pos = chpos;
}

public int col_from_pos(POSITION linepos, POSITION spos, POSITION saved_pos, int saved_col)
{
	struct col_pos cp;
	cp.pos = spos;
	cp.col = -1;
	col_vs_pos(linepos, &cp, saved_pos, saved_col);
	return cp.col;
}

public POSITION pos_from_col(POSITION linepos, int col, POSITION saved_pos, int saved_col)
{
	struct col_pos cp;
	cp.col = col + hshift - line_pfx_width();
	cp.pos = NULL_POSITION;
	col_vs_pos(linepos, &cp, saved_pos, saved_col);
	return cp.pos;
}

/*
 * Set an attribute on each char of the line in the line buffer.
 */
public void set_attr_line(int a)
{
	size_t i;

	for (i = linebuf.print;  i < linebuf.end;  i++)
		if ((linebuf.attr[i] & AT_COLOR) == 0 || (a & AT_COLOR) == 0)
			linebuf.attr[i] |= a;
}

/*
 * Set the char to be displayed in the status column.
 */
public void set_status_col(char c, int attr)
{
	set_pfx(0, c, attr);
}

/*
 * Get a character from the current line.
 * Return the character as the function return value,
 * and the character attribute in *ap.
 */
public int gline(size_t i, int *ap)
{
	if (is_null_line)
	{
		/*
		 * If there is no current line, we pretend the line is
		 * either "~" or "", depending on the "twiddle" flag.
		 */
		if (twiddle)
		{
			if (i == 0)
			{
				*ap = AT_BOLD;
				return '~';
			}
			--i;
		}
		/* Make sure we're back to AT_NORMAL before the '\n'.  */
		*ap = AT_NORMAL;
		return i ? '\0' : '\n';
	}

	if (i < linebuf.pfx_end)
	{
		*ap = linebuf.pfx_attr[i];
		return linebuf.pfx[i];
	}
	i += linebuf.print - linebuf.pfx_end;
	*ap = linebuf.attr[i];
	return (linebuf.buf[i] & 0xFF);
}

/*
 * Should we clear to end of line after printing this line?
 */
public lbool should_clear_after_line(void)
{
	return clear_after_line;
}

/*
 * Indicate that there is no current line.
 */
public void null_line(void)
{
	is_null_line = TRUE;
	cshift = 0;
}

/*
 * Analogous to forw_line(), but deals with "raw lines":
 * lines which are not split for screen width.
 * {{ This is supposed to be more efficient than forw_line(). }}
 */
public POSITION forw_raw_line_len(POSITION curr_pos, size_t read_len, constant char **linep, size_t *line_lenp)
{
	size_t n;
	int c;
	POSITION new_pos;

	if (curr_pos == NULL_POSITION || ch_seek(curr_pos) ||
		(c = ch_forw_get()) == EOI)
		return (NULL_POSITION);

	n = 0;
	for (;;)
	{
		if (c == '\n' || c == EOI || ABORT_SIGS())
		{
			new_pos = ch_tell();
			break;
		}
		if (n >= size_linebuf-1)
		{
			if (expand_linebuf())
			{
				/*
				 * Overflowed the input buffer.
				 * Pretend the line ended here.
				 */
				new_pos = ch_tell() - 1;
				break;
			}
		}
		linebuf.buf[n++] = (char) c;
		if (read_len != size_t_null && read_len > 0 && n >= read_len)
		{
			new_pos = ch_tell();
			break;
		}
		c = ch_forw_get();
	}
	linebuf.buf[n] = '\0';
	if (linep != NULL)
		*linep = linebuf.buf;
	if (line_lenp != NULL)
		*line_lenp = n;
	return (new_pos);
}

public POSITION forw_raw_line(POSITION curr_pos, constant char **linep, size_t *line_lenp)
{
	return forw_raw_line_len(curr_pos, size_t_null, linep, line_lenp);
}

/*
 * Analogous to back_line(), but deals with "raw lines".
 * {{ This is supposed to be more efficient than back_line(). }}
 */
public POSITION back_raw_line(POSITION curr_pos, constant char **linep, size_t *line_lenp)
{
	size_t n;
	int c;
	POSITION new_pos;

	if (curr_pos == NULL_POSITION || curr_pos <= ch_zero() ||
		ch_seek(curr_pos-1))
		return (NULL_POSITION);

	n = size_linebuf;
	linebuf.buf[--n] = '\0';
	for (;;)
	{
		c = ch_back_get();
		if (c == '\n' || ABORT_SIGS())
		{
			/*
			 * This is the newline ending the previous line.
			 * We have hit the beginning of the line.
			 */
			new_pos = ch_tell() + 1;
			break;
		}
		if (c == EOI)
		{
			/*
			 * We have hit the beginning of the file.
			 * This must be the first line in the file.
			 * This must, of course, be the beginning of the line.
			 */
			new_pos = ch_zero();
			break;
		}
		if (n <= 0)
		{
			size_t old_size_linebuf = size_linebuf;
			char *fm;
			char *to;
			if (expand_linebuf())
			{
				/*
				 * Overflowed the input buffer.
				 * Pretend the line ended here.
				 */
				new_pos = ch_tell() + 1;
				break;
			}
			/*
			 * Shift the data to the end of the new linebuf.
			 */
			for (fm = linebuf.buf + old_size_linebuf - 1,
			      to = linebuf.buf + size_linebuf - 1;
			     fm >= linebuf.buf;  fm--, to--)
				*to = *fm;
			n = size_linebuf - old_size_linebuf;
		}
		linebuf.buf[--n] = (char) c;
	}
	if (linep != NULL)
		*linep = &linebuf.buf[n];
	if (line_lenp != NULL)
		*line_lenp = size_linebuf - 1 - n;
	return (new_pos);
}

/*
 * Skip cols printable columns at the start of line.
 * Return number of bytes skipped.
 */
public int skip_columns(int cols, constant char **linep, size_t *line_lenp)
{
	constant char *line = *linep;
	constant char *eline = line + *line_lenp;
	LWCHAR pch = 0;
	size_t bytes;

	while (cols > 0 && line < eline)
	{
		LWCHAR ch = step_charc(&line, +1, eline);
		struct ansi_state *pansi = ansi_start(ch);
		if (pansi != NULL)
		{
			skip_ansi(pansi, ch, &line, eline);
			ansi_done(pansi);
			pch = 0;
		} else
		{
			int w = pwidth(ch, 0, pch, 0);
			cols -= w;
			pch = ch;
		}
	}
	bytes = ptr_diff(line, *linep);
	*linep = line;
	*line_lenp -= bytes;
	return (int) bytes; /*{{type-issue}}*/
}

/*
 * Append a string to the line buffer.
 */
static int pappstr(constant char *str)
{
	while (*str != '\0')
	{
		if (pappend(*str++, NULL_POSITION))
			/* Doesn't fit on screen. */
			return 1;
	}
	return 0;
}

/*
 * Load a string into the line buffer.
 * If the string is too long to fit on the screen,
 * truncate the beginning of the string to fit.
 */
public void load_line(constant char *str)
{
	int save_hshift = hshift;

	hshift = 0;
	for (;;)
	{
		prewind();
		if (pappstr(str) == 0)
			break;
		/*
		 * Didn't fit on screen; increase left shift by one.
		 * {{ This gets very inefficient if the string
		 * is much longer than the screen width. }}
		 */
		hshift += 1;
	}
	set_linebuf(linebuf.end, '\0', AT_NORMAL);

	/* Color the prompt unless it has ansi sequences in it. */
	if (!ansi_in_line)
	{
		size_t i;
		for (i = linebuf.print;  i < linebuf.end;  i++)
			set_linebuf(i, linebuf.buf[i], AT_STANDOUT|AT_COLOR_PROMPT);
	}
	hshift = save_hshift;
}

/*
 * Find the shift necessary to show the end of the longest displayed line.
 */
public int rrshift(void)
{
	POSITION pos;
	int save_width;
	int sline;
	int longest = 0;

	save_width = sc_width;
	sc_width = INT_MAX; /* so forw_line() won't chop */
	for (sline = TOP; sline < sc_height; sline++)
		if ((pos = position(sline)) != NULL_POSITION)
			break;
	for (; sline < sc_height && pos != NULL_POSITION; sline++)
	{
		pos = forw_line(pos, NULL, NULL);
		if (end_column > longest)
			longest = end_column;
	}
	sc_width = save_width;
	if (longest < sc_width)
		return 0;
	return longest - sc_width;
}

/*
 * Get the color_map index associated with a given attribute.
 */
static int lookup_color_index(int attr)
{
	int cx;
	for (cx = 0;  cx < countof(color_map);  cx++)
		if (color_map[cx].attr == attr)
			return cx;
	return -1;
}

static int color_index(int attr)
{
	if (use_color && (attr & AT_COLOR))
		return lookup_color_index(attr & AT_COLOR);
	if (attr & AT_UNDERLINE)
		return lookup_color_index(AT_UNDERLINE);
	if (attr & AT_BOLD)
		return lookup_color_index(AT_BOLD);
	if (attr & AT_BLINK)
		return lookup_color_index(AT_BLINK);
	if (attr & AT_STANDOUT)
		return lookup_color_index(AT_STANDOUT);
	return -1;
}

/*
 * Set the color string to use for a given attribute.
 */
public int set_color_map(int attr, constant char *colorstr)
{
	int cx = color_index(attr);
	if (cx < 0)
		return -1;
	if (strlen(colorstr)+1 > sizeof(color_map[cx].color))
		return -1;
	if (*colorstr != '\0' && parse_color(colorstr, NULL, NULL, NULL) == CT_NULL)
		return -1;
	strcpy(color_map[cx].color, colorstr);
	return 0;
}

/*
 * Get the color string to use for a given attribute.
 */
public constant char * get_color_map(int attr)
{
	int cx = color_index(attr);
	if (cx < 0)
		return NULL;
	return color_map[cx].color;
}
