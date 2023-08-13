#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <signal.h>
#include "lt_types.h"
#include "wchar.h"

static const char version[] = "lt_screen|v=1";

#define ERROR_CHAR       ' '
#define WIDESHADOW_CHAR  ((wchar)0)
static char const* osc8_start[] = { "\033]8;", NULL };
static char const* osc8_end[] = { "\033\\", "\7", NULL };

#define NUM_LASTCH 4 // must be >= strlen(osc8_*[*])
static wchar lastch[NUM_LASTCH];
static int lastch_curr = 0;

int usage(void) {
	fprintf(stderr, "usage: lt_screen [-w width] [-h height] [-qv]\n");
	return 0;
}

// ------------------------------------------------------------------

#define MAX_PARAMS         3

typedef struct ScreenChar {
	wchar ch;
	Attr attr;
	Color fg_color;
	Color bg_color;
} ScreenChar;

typedef struct ScreenState {
	ScreenChar* chars;
	int w;
	int h;
	int cx;
	int cy;
	Attr curr_attr;
	Color curr_fg_color;
	Color curr_bg_color;
	int param_top;
	int params[MAX_PARAMS+1];
	int in_esc;
	int in_osc8;
} ScreenState;

static ScreenState screen;
static int ttyin; // input text and control sequences
static int ttyout; // output for screen dump
static int quiet = 0;
static int verbose = 0;

// ------------------------------------------------------------------

// Initialize ScreenState.
static void screen_init(void) {
	screen.w = 80;
	screen.h = 24;
	screen.cx = 0;
	screen.cy = 0;
	screen.in_esc = 0;
	screen.in_osc8 = 0;
	screen.curr_attr = 0;
	screen.curr_fg_color = screen.curr_bg_color = NULL_COLOR;
	screen.param_top = -1;
	screen.params[0] = 0;
}

static int num_params(void) {
	return screen.param_top+1;
}

static void param_print(void) {
	int i;
	fprintf(stderr, "(");
	for (i = 0; i < num_params(); ++i)
		fprintf(stderr, "%d ", screen.params[i]);
	fprintf(stderr, ")");
}

static void param_clear(void) {
	screen.param_top = -1;
}

static void param_push(int v) {
	if (screen.param_top >= (int) countof(screen.params)-1) {
		param_clear();
		return;
	}
	screen.params[++screen.param_top] = v;
}

static int param_pop(void){
	if (num_params() == 0)
		return -1; // missing param 
	return screen.params[screen.param_top--];
}

static int screen_x(int x) {
	if (x < 0) x = 0;
	if (x >= screen.w) x = screen.w-1;
	return x;
}

static int screen_y(int y) {
	if (y < 0) y = 0;
	if (y >= screen.h) y = screen.h-1;
	return y;
}

// Return the char at a given screen position.
static ScreenChar* screen_char(int x, int y) {
	x = screen_x(x);
	y = screen_y(y);
	return &screen.chars[y * screen.w + x];
}

// Step the cursor after printing a char.
static int screen_incr(int* px, int* py) {
	if (++(*px) >= screen.w) {
		*px = 0;
		if (++(*py) >= screen.h) {
			*py = 0;
			return 0;
		}
	}
	return 1;
}

// Set the value, attributes and colors of a char on the screen.
static void screen_char_set(int x, int y, wchar ch, Attr attr, Color fg_color, Color bg_color) {
	ScreenChar* sc = screen_char(x, y);
	sc->ch = ch;
	sc->attr = attr;
	sc->fg_color = fg_color;
	sc->bg_color = bg_color;
}

static int screen_clear(int x, int y, int count) {
	while (count-- > 0) {
		screen_char_set(x, y, '_', 0, NULL_COLOR, NULL_COLOR);
		screen_incr(&x, &y);
	}
	return 1;
}

static void store_hex(byte** pp, int val) {
	char hexchar[] = "0123456789ABCDEF";
	*(*pp)++ = hexchar[(val >> 4) & 0xf];
	*(*pp)++ = hexchar[val & 0xf];
}

// Print an encoded image of the current screen to ttyout.
// The LTS_CHAR_* metachars encode changes of color and attribute.
static int screen_read(int x, int y, int count) {
	Attr attr = 0;
	int fg_color = NULL_COLOR;
	int bg_color = NULL_COLOR;
	while (count-- > 0) {
		byte buf[32];
		byte* bufp = buf;
		ScreenChar* sc = screen_char(x, y);
		if (sc->attr != attr) {
			attr = sc->attr;
			*bufp++ = LTS_CHAR_ATTR;
			store_hex(&bufp, attr);
		}
		if (sc->fg_color != fg_color) {
			fg_color = sc->fg_color;
			*bufp++ = LTS_CHAR_FG_COLOR;
			store_hex(&bufp, fg_color);
		}
		if (sc->bg_color != bg_color) {
			bg_color = sc->bg_color;
			*bufp++ = LTS_CHAR_BG_COLOR;
			store_hex(&bufp, bg_color);
		}
		if (x == screen.cx && y == screen.cy)
			*bufp++ = LTS_CHAR_CURSOR;
		if (sc->ch == '\\' || sc->ch == LTS_CHAR_ATTR || sc->ch == LTS_CHAR_FG_COLOR || sc->ch == LTS_CHAR_BG_COLOR || sc->ch == LTS_CHAR_CURSOR)
			*bufp++ = '\\';
		store_wchar(&bufp, sc->ch);
		write(ttyout, buf, bufp-buf);
		screen_incr(&x, &y);
	}
	write(ttyout, "\n", 1);
	return 1;
}

static int screen_move(int x, int y) {
	screen.cx = x;
	screen.cy = y;
	return 1;
}

static int screen_cr(void) {
	screen.cx = 0;
	return 1;
}

static int screen_bs(void) {
	if (screen.cx <= 0) return 0;
	--screen.cx;
	return 1;
}

static int screen_scroll(void) {
	int len = screen.w * (screen.h-1);
	memmove(screen_char(0,0), screen_char(0,1), len * sizeof(ScreenChar));
	screen_clear(0, screen.h-1, screen.w);
	return 1;
}

static int screen_rscroll(void) {
	int len = screen.w * (screen.h-1);
	memmove(screen_char(0,1), screen_char(0,0), len * sizeof(ScreenChar));
	screen_clear(0, 0, screen.w);
	return 1;
}

static int screen_set_attr(int attr) {
	screen.curr_attr |= attr;
	if (verbose) fprintf(stderr, "[%d,%d] set_attr(%d)=%d\n", screen.cx, screen.cy, attr, screen.curr_attr);
	return 1;
}

static int screen_clear_attr(int attr) {
	screen.curr_attr &= ~attr;
	if (verbose) fprintf(stderr, "[%d,%d] clr_attr(%d)=%d\n", screen.cx, screen.cy, attr, screen.curr_attr);
	return 1;
}

// ------------------------------------------------------------------ 
// lt_screen supports certain ANSI color values.
// This simplifies testing SGR sequences with less -R 
// compared to inventing custom color sequences.
static int screen_set_color(int color) {
	int ret = 0;
	switch (color) {
	case 1:  ret = screen_set_attr(ATTR_BOLD); break;
	case 4:  ret = screen_set_attr(ATTR_UNDERLINE); break;
	case 5:
	case 6:  ret = screen_set_attr(ATTR_BLINK); break;
	case 7:  ret = screen_set_attr(ATTR_STANDOUT); break;
	case 21:
	case 22: ret = screen_clear_attr(ATTR_BOLD); break;
	case 24: ret = screen_clear_attr(ATTR_UNDERLINE); break;
	case 25: ret = screen_clear_attr(ATTR_BLINK); break;
	case 27: ret = screen_clear_attr(ATTR_STANDOUT); break;
	// case 38: break;
	// case 48: break;
	default: 
		if (color <= 0) {
			screen.curr_fg_color = screen.curr_bg_color = NULL_COLOR;
			screen.curr_attr = 0;
			ret = 1;
		} else if ((color >= 30 && color <= 37) || (color >= 90 && color <= 97)) {
			screen.curr_fg_color = color;
			ret = 1;
		} else if ((color >= 40 && color <= 47) || (color >= 100 && color <= 107)) {
			screen.curr_bg_color = color;
			ret = 1;
		} else {
			fprintf(stderr, "[%d,%d] unrecognized color %d\n", screen.cx, screen.cy, color);
		}
		if (verbose) fprintf(stderr, "[%d,%d] set_color(%d)=%d/%d\n", screen.cx, screen.cy, color, screen.curr_fg_color, screen.curr_bg_color);
		break;
	}
	return ret;
}

// ------------------------------------------------------------------ 

static void beep(void) {
	if (!quiet)
		fprintf(stderr, "\7");
}

// Execute an escape sequence ending with a given char.
static int exec_esc(wchar ch) {
	int x, y, count;
	if (verbose) {
		fprintf(stderr, "exec ESC-%c ", (char)ch);
		param_print();
		fprintf(stderr, "\n");
	}
	switch (ch) {
	case 'A': // clear all 
		return screen_clear(0, 0, screen.w * screen.h);
	case 'L': // clear from cursor to end of line 
		return screen_clear(screen.cx, screen.cy, screen.w - screen.cx);
	case 'S': // clear from cursor to end of screen 
		return screen_clear(screen.cx, screen.cy, 
			(screen.w - screen.cx) + (screen.h - screen.cy -1) * screen.w);
	case 'R': // read N3 chars starting at (N1,N2)
		count = param_pop();
		y = param_pop();
		x = param_pop();
		if (x < 0) x = 0;
		if (y < 0) y = 0;
		if (count < 0) count = 0;
		return screen_read(x, y, count);
	case 'j': // jump cursor to (N1,N2)
		y = param_pop();
		x = param_pop();
		if (x < 0) x = 0;
		if (y < 0) y = 0;
		return screen_move(x, y);
	case 'g': // visual bell 
		return 0;
	case 'h': // cursor home 
		return screen_move(0, 0);
	case 'l': // cursor lower left 
		return screen_move(0, screen.h-1);
	case 'r': // reverse scroll
		return screen_rscroll();
	case '<': // cursor left to start of line
		return screen_cr();
	case 'e': // exit bold
		return screen_clear_attr(ATTR_BOLD);
	case 'b': // enter blink
		return screen_set_attr(ATTR_BLINK);
	case 'c': // exit blink
		return screen_clear_attr(ATTR_BLINK);
	case 'm': // SGR (Select  Graphics Rendition)
		if (num_params() == 0) {
			screen_set_color(-1);
		} else {
			while (num_params() > 0)
				screen_set_color(param_pop());
		}
		return 0;
	case '?': // print version string
		write(ttyout, version, strlen(version));
		return 1;
	default:
		return 0;
	}
}

// Print a char on the screen.
// Handles cursor movement and scrolling.
static int add_char(wchar ch) {
	//if (verbose) fprintf(stderr, "add (%c) %lx at %d,%d\n", (char)ch, (long)ch, screen.cx, screen.cy);
	screen_char_set(screen.cx, screen.cy, ch, screen.curr_attr, screen.curr_fg_color, screen.curr_bg_color);
	int fits = 1;
	int zero_width = (is_composing_char(ch) || 
	    (screen.cx > 0 && is_combining_char(screen_char(screen.cx-1,screen.cy)->ch, ch)));
	if (!zero_width) {
		fits = screen_incr(&screen.cx, &screen.cy);
		if (fits) {
			if (is_wide_char(ch)) {
				// The "shadow" is the second column used by a wide char.
				screen_char_set(screen.cx, screen.cy, WIDESHADOW_CHAR, 0, NULL_COLOR, NULL_COLOR);
				fits = screen_incr(&screen.cx, &screen.cy);
			} else {
				ScreenChar* sc = screen_char(screen.cx, screen.cy);
				if (sc->ch == WIDESHADOW_CHAR) {
					// We overwrote the first half of a wide character.
					// Change the orphaned shadow to an error char.
					screen_char_set(screen.cx, screen.cy, ERROR_CHAR, screen.curr_attr, NULL_COLOR, NULL_COLOR);
				}
			}
		}
	}
	if (!fits) { // Wrap at bottom of screen = scroll
		screen.cx = 0;
		screen.cy = screen.h-1;
		return screen_scroll();
	}
	return 1;
}

// Remember the last few chars sent to the screen.
static void add_last(wchar ch) {
	lastch[lastch_curr++] = ch;
	if (lastch_curr >= NUM_LASTCH) lastch_curr = 0;
}

// Do the last entered characters match a string?
static int last_matches_str(char const* str) {
	int ci = lastch_curr;
	int si;
	for (si = strlen(str)-1; si >= 0; --si) {
		ci = (ci > 0) ? ci-1 : NUM_LASTCH-1;
		if (str[si] != lastch[ci]) return 0;
	}
	return 1;
}

// Do the last entered characters match any one of a list of strings?
static int last_matches(const char* const* tbl) {
	int ti;
	for (ti = 0; tbl[ti] != NULL; ++ti) {
		if (last_matches_str(tbl[ti]))
			return 1;
	}
	return 0;
}

// Handle a char sent to the screen while it is receiving an escape sequence.
static int process_esc(wchar ch) {
	int ok = 1;
	if (screen.in_osc8) {
		if (last_matches(osc8_end)) {
			screen.in_osc8 = screen.in_esc = 0;
		} else {
			// Discard everything between osc8_start and osc8_end.
		}
	} else if (last_matches(osc8_start)) {
		param_pop(); // pop the '8'
		screen.in_osc8 = 1;
	} else if (ch >= '0' && ch <= '9') {
		int d = (num_params() == 0) ? 0 : screen.params[screen.param_top--];
		param_push(10 * d + ch - '0');
	} else if (ch == ';') {
		param_push(0);
	} else if (ch == '[' || ch == ']') {
		; // Ignore ANSI marker
	} else { // end of escape sequence
		screen.in_esc = 0;
		ok = exec_esc(ch);
		param_clear();
	}
	return ok;
}

// Handle a char sent to the screen.
// Normally it is just printed, but some control chars are handled specially.
static int process_char(wchar ch) {
	int ok = 1;
	add_last(ch);
	if (screen.in_esc) {
		ok = process_esc(ch);
	} else if (ch == ESC) {
		screen.in_esc = 1;
	} else if (ch == '\r') {
		screen_cr();
	} else if (ch == '\b') {
		screen_bs();
	} else if (ch == '\n') {
		if (screen.cy < screen.h-1)
			++screen.cy;
		else 
			screen_scroll();
		screen_cr(); // auto CR
	} else if (ch == '\7') {
		beep();
	} else if (ch == '\t') {
		ok = add_char(' '); // hardware tabs not supported
	} else if (ch >= '\40') { // printable char
		ok = add_char(ch);
	}
	return ok;
}

// ------------------------------------------------------------------ 

static int setup(int argc, char** argv) {
	int ch;
	screen_init();
	while ((ch = getopt(argc, argv, "h:qvw:")) != -1) {
		switch (ch) {
		case 'h':
			screen.h = atoi(optarg);
			break;
		case 'q':
			quiet = 1;
			break;
		case 'v':
			++verbose;
			break;
		case 'w':
			screen.w = atoi(optarg);
			break;
		default:
			return usage();
		}
	}
	int len = screen.w * screen.h;
	screen.chars = malloc(len * sizeof(ScreenChar));
	screen_clear(0, 0, len);
	if (optind >= argc) {
		ttyin = 0;
		ttyout = 1;
	} else {
		ttyin = ttyout = open(argv[optind], O_RDWR);
		if (ttyin < 0) {
			fprintf(stderr, "cannot open %s\n", argv[optind]);
			return 0;
		}
	}
	return 1;
}

static void set_signal(int signum, void (*handler)(int)) {
	struct sigaction sa;
	sa.sa_handler = handler;
	sa.sa_flags = 0;
	sigemptyset(&sa.sa_mask);
	sigaction(signum, &sa, NULL);
}

int main(int argc, char** argv) {
	set_signal(SIGINT,  SIG_IGN);
	set_signal(SIGQUIT, SIG_IGN);
	set_signal(SIGKILL, SIG_IGN);
	if (!setup(argc, argv))
		return RUN_ERR;
	for (;;) {
		wchar ch = read_wchar(ttyin);
		//if (verbose) fprintf(stderr, "screen read %c (%lx)\n", pr_ascii(ch), ch);
		if (ch == 0)
			break;
		if (!process_char(ch))
			beep();
	}
	return RUN_OK;
}
