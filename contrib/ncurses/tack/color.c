/*
** Copyright (C) 1991, 1997 Free Software Foundation, Inc.
**
** This file is part of TACK.
**
** TACK is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation; either version 2, or (at your option)
** any later version.
**
** TACK is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with TACK; see the file COPYING.  If not, write to
** the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
** Boston, MA 02110-1301, USA
*/

#include <tack.h>

MODULE_ID("$Id: color.c,v 1.7 2006/11/26 00:14:25 tom Exp $")

/*
 * Color terminal tests.  Has only one entry point: test_color().
 */

static void color_check(struct test_list *, int *, int *);
static void color_setf(struct test_list *, int *, int *);
static void color_matrix(struct test_list *, int *, int *);
static void color_ncv(struct test_list *, int *, int *);
static void color_ccc(struct test_list *, int *, int *);
static void color_bce(struct test_list *, int *, int *);

struct test_list color_test_list[] = {
	{0, 0, 0, 0, "e) edit terminfo", 0, &edit_menu},
	{MENU_NEXT, 2, "colors) (pairs", 0, 0, color_check, 0},
	{MENU_NEXT, 12, "setf) (setb) (scp", 0, 0, color_setf, 0},
	{MENU_NEXT, 24, "op", 0, 0, color_matrix, 0},
	{MENU_NEXT, 16, "ncv", 0, 0, color_ncv, 0},
	{MENU_NEXT, 0, "bce", 0, 0, color_bce, 0},
	{MENU_NEXT | MENU_CLEAR, 0, "ccc) (initc) (initp", "hls op oc", 0, color_ccc, 0},
	{MENU_LAST, 0, 0, 0, 0, 0, 0}
};

#ifndef COLOR_BLACK
#define COLOR_BLACK     0
#define COLOR_BLUE      1
#define COLOR_GREEN     2
#define COLOR_CYAN      3
#define COLOR_RED       4
#define COLOR_MAGENTA   5
#define COLOR_YELLOW    6
#define COLOR_WHITE     7
#endif

struct color_table {
	const char *name;
	int index;
	int r, g, b;
	int h, l, s;
};

static struct color_table def_colors[8] = {
	{"black  ", COLOR_BLACK, 0, 0, 0, 0, 0, 0},
	{"blue   ", COLOR_BLUE, 0, 0, 1000, 330, 50, 100},
	{"green  ", COLOR_GREEN, 0, 1000, 0, 240, 50, 100},
	{"cyan   ", COLOR_CYAN, 0, 1000, 1000, 300, 50, 100},
	{"red    ", COLOR_RED, 1000, 0, 0, 120, 50, 100},
	{"magenta", COLOR_MAGENTA, 1000, 0, 1000, 60, 50, 100},
	{"yellow ", COLOR_YELLOW, 1000, 1000, 0, 180, 50, 100},
	{"white  ", COLOR_WHITE, 1000, 1000, 1000, 0, 100, 0}
};

#define MAX_PAIR	256
static int fg_color[MAX_PAIR] = {COLOR_BLACK, COLOR_BLUE, COLOR_GREEN,
COLOR_CYAN, COLOR_RED, COLOR_MAGENTA, COLOR_YELLOW, COLOR_WHITE};
static int bg_color[MAX_PAIR] = {COLOR_BLACK, COLOR_BLACK, COLOR_BLACK,
COLOR_BLACK, COLOR_BLACK, COLOR_BLACK, COLOR_BLACK, COLOR_BLACK};
static int pairs_used = 8;
static int a_bright_color, bright_value;
static int cookie_monster, color_step, colors_per_line;
static int R, G, B;

static void reset_colors(void)
{
	tc_putp(orig_colors);
	tc_putp(TPARM_0(orig_pair));
}

static int
color_trans(int c)
{				/* translate or load the color */
	int i;

	for (i = 0; i < pairs_used; i++) {
		if (fg_color[i] == c) {
			return i;
		}
	}
	if (!can_change) {
		return 0;
	}
	if (pairs_used > max_colors || pairs_used >= MAX_PAIR) {
		pairs_used = 0;
		ptextln("Ran out of colors");
	}
	fg_color[pairs_used] = c;
	bg_color[pairs_used] = c;
	if (hue_lightness_saturation) {
		tc_putp(TPARM_4(initialize_color, pairs_used,
			def_colors[c].h, def_colors[c].l, def_colors[c].s));
	} else {
		tc_putp(TPARM_4(initialize_color, pairs_used,
			def_colors[c].r, def_colors[c].g, def_colors[c].b));
	}
	return pairs_used++;
}

static void
new_color(
	int fg,
	int bg,
	int hungry)
{				/* change the color to fg and bg. */
	int i;

	if (hungry) {
		eat_cookie();
	}
	if (set_a_foreground) {
		/* set ANSI color (setaf) (setab) */
		tc_putp(TPARM_1(set_a_foreground, fg));
		tc_putp(TPARM_1(set_a_background, bg));
	} else if (set_foreground) {
		/* make sure black is zero */
		(void) color_trans(COLOR_BLACK);
		tc_putp(TPARM_1(set_foreground, color_trans(fg)));
		tc_putp(TPARM_1(set_background, color_trans(bg)));
	} else {	/* set color pair */
		for (i = 0; i < pairs_used; i++) {
			if (fg_color[i] == fg && bg_color[i] == bg) {
				tc_putp(TPARM_1(set_color_pair, i));
				if (hungry) {
					eat_cookie();
				}
				return;
			}
		}
		if (!can_change) {
			/* try to set just the foreground */
			for (i = pairs_used - 1; i; i--) {
				if (fg_color[i] == fg)
					break;
			}
			tc_putp(TPARM_1(set_color_pair, i));
			if (hungry) {
				eat_cookie();
			}
			return;
		}
		if (pairs_used > max_pairs || pairs_used >= MAX_PAIR) {
			pairs_used = 0;
			ptextln("Ran out of color pairs");
		}
		fg_color[pairs_used] = fg;
		bg_color[pairs_used] = bg;
		if (hue_lightness_saturation) {
			tc_putp(TPARM_7(initialize_pair, pairs_used,
				def_colors[fg].h, def_colors[fg].l, def_colors[fg].s,
				def_colors[bg].h, def_colors[bg].l, def_colors[bg].s));
		} else {
			tc_putp(TPARM_7(initialize_pair, pairs_used,
				def_colors[fg].r, def_colors[fg].g, def_colors[fg].b,
				def_colors[bg].r, def_colors[bg].g, def_colors[bg].b));
		}
		tc_putp(TPARM_1(set_color_pair, pairs_used));
		pairs_used++;
	}
	if (hungry) {
		eat_cookie();
	}
}


static void
set_color_step(void)
{				/* set the color_step for the (ccc) display */
	int i;

	for (i = 2; i < 1000; i++) {
		if ((i * i * i) >= max_colors) {
			break;
		}
	}
	color_step = 1000 / (i - 1);
}


static void
rgb_2_hls(int r, int g, int b, int *h, int *l, int *s)
{				/* convert RGB to HLS system */
	int min, max, t;

	if ((min = g < r ? g : r) > b) {
		min = b;
	}
	if ((max = g > r ? g : r) < b) {
		max = b;
	}

	/* calculate lightness */
	*l = (min + max) / 20;

	if (min == max) {	/* black, white and all shades of gray */
		*h = 0;
		*s = 0;
		return;
	}
	/* calculate saturation */
	if (*l < 50) {
		*s = ((max - min) * 100) / (max + min);
	} else {
		*s = ((max - min) * 100) / (2000 - max - min);
	}

	/* calculate hue */
	if (r == max) {
		t = 120 + ((g - b) * 60) / (max - min);
	} else if (g == max) {
		t = 240 + ((b - r) * 60) / (max - min);
	} else {
		t = 360 + ((r - g) * 60) / (max - min);
	}
	*h = t % 360;
}


static void
send_color(int p, int r, int g, int b)
{				/* send the initialize_color (initc) command */
	int h, l, s;

	if (hue_lightness_saturation) {
		rgb_2_hls(r, g, b, &h, &l, &s);
		tc_putp(TPARM_4(initialize_color, p, h, l, s));
	} else {
		tc_putp(TPARM_4(initialize_color, p, r, g, b));
	}
}


static void
send_pair(int p, int fr, int fg, int fb, int br, int bg, int bb)
{				/* send the initialize_pair (initp) command */
	int fh, fl, fs, bh, bl, bs;

	if (hue_lightness_saturation) {
		rgb_2_hls(fr, fg, fb, &fh, &fl, &fs);
		rgb_2_hls(br, bg, bb, &bh, &bl, &bs);
		tc_putp(TPARM_7(initialize_pair, p, fh, fl, fs, bh, bl, bs));
	} else {
		tc_putp(TPARM_7(initialize_pair, p, fr, fg, fb, bb, bg, bb));
	}
}


static int
load_palette(int n)
{				/* load the color palette */
	int rgb;

	for (;;) {
		if (pairs_used >= n) {
			return FALSE;
		}
		if (set_a_foreground || set_foreground) {
			if (pairs_used >= max_colors) {
				return FALSE;
			}
			send_color(pairs_used, R, G, B);
			rgb = R + G + B;
			if (rgb > bright_value) {
				bright_value = rgb;
				a_bright_color = pairs_used;
			}
		} else {
			if (pairs_used >= max_pairs) {
				return FALSE;
			}
			if (pairs_used == 0) {
				send_pair(pairs_used, 1000, 1000, 1000, R, G, B);
			} else {
				send_pair(pairs_used, R, G, B, R, G, B);
			}
		}
		pairs_used++;
		if ((B += color_step) > 1000) {
			B = 0;
			if ((G += color_step) > 1000) {
				G = 0;
				if ((R += color_step) > 1000) {
					return TRUE;
				}
			}
		}
	}
}


static int
rainbow(int n)
{				/* print the programmable color display */
	int i, c, d, palette_full, initial_pair;
	static const struct {
		const char *name;
		char ch;
	}  splat[] = {
		{"Bg normal", ' '},
		{"Fg normal", ' '},
		{0, 0}
	};

	if ((set_a_foreground || set_foreground)
	  ? pairs_used >= max_colors
	  : pairs_used >= max_pairs) {
		ptext("New palette: ");
		(void) wait_here();
		initial_pair = pairs_used = 1;
		bright_value = 0;
	} else if (line_count + 3 >= lines) {
		ptext("Go: ");
		(void) wait_here();
		put_clear();
		initial_pair = pairs_used = 1;
		bright_value = 0;
		n++;
	} else {
		initial_pair = pairs_used;
		n += initial_pair;
	}
	palette_full = load_palette(n);
	for (d = 0; splat[d].name; d++) {
		c = splat[d].ch;
		if (d == 1) {
			put_mode(enter_reverse_mode);
		}
		for (i = initial_pair; i < n; i++) {
			if (i >= pairs_used) {
				break;
			}
			if (set_a_foreground) {
				if (i >= max_colors) {
					break;
				}
				tc_putp(TPARM_1(set_a_foreground, i));
				tc_putp(TPARM_1(set_a_background, i));
			} else if (set_foreground) {
				if (i >= max_colors) {
					break;
				}
				tc_putp(TPARM_1(set_foreground, i));
				tc_putp(TPARM_1(set_background, i));
			} else {
				if (i >= max_pairs) {
					break;
				}
				tc_putp(TPARM_1(set_color_pair, i));
			}
			putchp(c);
		}
		if (d == 1) {
			put_mode(exit_attribute_mode);
		}
		if (set_a_foreground) {
			tc_putp(TPARM_1(set_a_foreground, a_bright_color));
			tc_putp(TPARM_1(set_a_background, 0));
		} else if (set_foreground) {
			tc_putp(TPARM_1(set_foreground, a_bright_color));
			tc_putp(TPARM_1(set_background, 0));
		} else {
			tc_putp(TPARM_1(set_color_pair, 0));
		}
		put_str("   ");
		put_str(splat[d].name);
		put_crlf();
	}
	return palette_full;
}


static void
ncv_display(int m)
{				/* print the no_color_video (ncv) test line */
	putchp('0' + m);
	putchp(' ');
	eat_cookie();
	set_attr(1 << m);
	sprintf(temp, "%-11s", alt_modes[m].name);
	put_str(temp);

	new_color(COLOR_BLUE, COLOR_BLACK, TRUE);
	put_str("blue");

	new_color(COLOR_BLACK, COLOR_GREEN, TRUE);
	put_str("green");

	new_color(COLOR_WHITE, COLOR_BLACK, TRUE);
	put_str(alt_modes[m].name);
	eat_cookie();
	set_attr(0);
	reset_colors();
	put_crlf();
}


static void
dump_colors(void)
{				/* display the colors in some esthetic
				   pattern */
	static int xmap[8] = {0, 3, 4, 7, 1, 2, 5, 6};
	int i, j, k, xi, xj, width, p, cs;
	int found_one;

	cs = color_step <= 125 ? 125 : color_step;
	width = (1000 / cs) + 1;
	for (xi = 0; xi < 16; xi++) {
		i = (xi & 8) ? xi ^ 15 : xi;
		R = i * cs;
		if (R <= 1000) {
			found_one = FALSE;
			for (xj = 0; xj < 32; xj++) {
				j = ((xj & 8) ? xj ^ 15 : xj) & 7;
				k = xmap[((xi >> 1) & 4) + (xj >> 3)];
				G = j * cs;
				B = k * cs;
				if (G <= 1000 && B <= 1000) {
					p = (k * width + j) * width + i;
					if (set_a_background) {
						if (p >= max_colors) {
							continue;
						}
						send_color(p, R, G, B);
						tc_putp(TPARM_1(set_a_background, p));
					} else if (set_background) {
						if (p >= max_colors) {
							continue;
						}
						send_color(p, R, G, B);
						tc_putp(TPARM_1(set_background, p));
					} else {
						if (p >= max_pairs) {
							continue;
						}
						send_pair(p, R, G, B, R, G, B);
						tc_putp(TPARM_1(set_color_pair, p));
					}
					found_one = TRUE;
					putchp(' ');
					putchp(' ');
				}
			}
			if (found_one) {
				put_crlf();
			}
		}
	}
}

/*
**	color_check(test_list, status, ch)
**
**	test (colors) and (pairs)
*/
static void
color_check(
	struct test_list *t,
	int *state,
	int *ch)
{
	if (max_colors <= 0 && max_pairs <= 0) {
		ptext("This is not a color terminal; (colors) and (pairs) are missing.  ");
		*state |= MENU_STOP;
	} else {
		sprintf(temp, "This terminal can display %d colors and %d color pairs.  (colors) (pairs)",
			max_colors, max_pairs);
		ptextln(temp);
	}
	generic_done_message(t, state, ch);
}

/*
**	color_setf(test_list, status, ch)
**
**	test (setf) (setb) and (scp)
*/
static void
color_setf(
	struct test_list *t,
	int *state,
	int *ch)
{
	int i, j;

	if (max_colors <= 0 && max_pairs <= 0) {
		ptext("This is not a color terminal; (colors) and (pairs) are missing.  ");
		generic_done_message(t, state, ch);
		*state |= MENU_STOP;
		return;
	}
	if ((set_a_foreground == NULL || set_a_background == NULL)
	 && (set_foreground == NULL   || set_background == NULL)
	 && set_color_pair == NULL) {
		ptextln("Both set foreground (setaf/setf) and set color pair (scp) are not present.");
		if (!set_a_background || !set_background) {
			ptextln("(setab/setb) set background not present");
		}
		ptext("These must be defined for color testing.  ");
		generic_done_message(t, state, ch);
		*state |= MENU_STOP;
		return;
	}
	/* initialize the color palette */
	pairs_used = max_colors >= 8 ? 8 : max_colors;
	reset_colors();
	new_color(COLOR_WHITE, COLOR_BLACK, FALSE);

	ptextln("(setf) (setb) (scp) The following colors are predefined:");
	ptextln("\n   Foreground     Background");
	put_crlf();
	j = max_colors > 8 ? 8 : max_colors;
	/*
	 * the black on white test is the same as the white on black test.
	 */
	for (i = 1; i < j; i++) {
		putchp('0' + def_colors[i].index);
		putchp(' ');
		sprintf(temp, " %s ", def_colors[i].name);

		new_color(def_colors[i].index, COLOR_BLACK, TRUE);
		put_str(temp);

		new_color(COLOR_BLACK, COLOR_BLACK, TRUE);
		put_str("  ");

		new_color(COLOR_BLACK, def_colors[i].index, TRUE);
		put_str(temp);

		new_color(COLOR_WHITE, COLOR_BLACK, FALSE);
		put_crlf();
	}
	reset_colors();
	put_crlf();
	generic_done_message(t, state, ch);
}

/*
**	color_matrix(test_list, status, ch)
**
**	test (pairs) (op)
*/
static void
color_matrix(
	struct test_list *t,
	int *state,
	int *ch)
{
	int i, j, matrix_size, matrix_area, brightness;

	matrix_size = max_colors > 8 ? 8 : max_colors;

	sprintf(temp, "(pairs) There are %d color pairs.", max_pairs);
	ptextln(temp);

	for ( ; matrix_size; matrix_size--) {
		if (matrix_size * matrix_size <= max_pairs) {
			break;
		}
	}
	matrix_area = matrix_size * matrix_size;
	for (brightness = 0; brightness < 2; brightness++) {
		put_crlf();
		sprintf(temp,
			"%dx%d matrix of foreground/background colors, bright *o%s*",
			matrix_size, matrix_size, brightness ? "n" : "ff");
		put_str(temp);

		put_str("\n          ");
		for (i = 0; i < matrix_size; i++) {
			(void) sprintf(temp, "%-8s", def_colors[i].name);
			put_str(temp);
		}
		for (j = 0; j < matrix_area; j++) {
			if (j % matrix_size == 0) {
				reset_colors();
				put_crlf();
				if (brightness) {
					tc_putp(exit_attribute_mode);
				}
				(void) sprintf(temp, "%-8s", def_colors[j / matrix_size].name);
				put_str(temp);
				if (brightness) {
					put_mode(enter_bold_mode);
				}
			}
			new_color(def_colors[j % matrix_size].index,
				def_colors[j / matrix_size].index,
				FALSE);
			put_str("  Hello ");
		}
		reset_colors();
		if (brightness) {
			tc_putp(exit_attribute_mode);
		}
		put_crlf();
	}
	generic_done_message(t, state, ch);
}

/*
**	color_ncv(test_list, status, ch)
**
**	test (ncv)
*/
static void
color_ncv(
	struct test_list *t,
	int *state,
	int *ch)
{
	int i;

	if (no_color_video == -1) {
		/* I have no idea what this means */
		return;
	}
	sprintf(temp, "According to no_color_video (ncv) which is %d, the following attributes should work correctly with color.", no_color_video);
	ptextln(temp);
	put_crlf();
	set_attr(0);
	ncv_display(0);
	for (i = 1; i <= 9; i++) {
		if (((no_color_video >> (mode_map[i] - 1)) & 1) == 0) {
			ncv_display(mode_map[i]);
		}
	}
	if (no_color_video & 0x3ff) {
		ptextln("\nThe following attributes should not work correctly with color. (ncv)\n");
		for (i = 1; i <= 9; i++) {
			if ((no_color_video >> (mode_map[i] - 1)) & 1) {
				ncv_display(mode_map[i]);
			}
		}
	}
	reset_colors();
	put_crlf();
	generic_done_message(t, state, ch);
}

/*
**	color_bce(test_list, status, ch)
**
**	test (bce) background color erase
*/
static void
color_bce(
	struct test_list *t,
	int *state,
	int *ch)
{
	new_color(COLOR_CYAN, COLOR_BLUE, FALSE);
	put_clear();
	put_newlines(2);
	reset_colors();
	ptextln("If the two lines above are blue then back_color_erase (bce) should be true.");
	sprintf(temp, "(bce) is %s in the data base.", back_color_erase ? "true" : "false");
	ptextln(temp);
	generic_done_message(t, state, ch);
}

/*
**	color_ccc(test_list, status, ch)
**
**	test (ccc) color palette test (oc) (op) (initc) (initp)
*/
static void
color_ccc(
	struct test_list *t,
	int *state,
	int *ch)
{
	int i, j;

	if (!can_change) {
		ptextln("Terminal can not change colors (ccc)");
		generic_done_message(t, state, ch);
		return;
	}
	reset_colors();
	pairs_used = 0;
	new_color(COLOR_WHITE, COLOR_BLACK, FALSE);
	sprintf(temp, "Reloading colors (init%c) using %s method",
		set_foreground ? 'c' : 'p',
		hue_lightness_saturation ? "HLS" : "RGB");
	ptextln(temp);
	put_crlf();
	j = max_colors > 7 ? 7 : max_colors;
	/* redisplay the above test with reinitialized colors */
	/* If these colors don't look right to you... */
	for (i = 0; i < j; i++) {
		sprintf(temp, " %s ", def_colors[i ^ 7].name);

		new_color(i ^ 7, COLOR_BLACK, TRUE);
		put_str(temp);

		new_color(COLOR_BLACK, COLOR_BLACK, TRUE);
		put_str("  ");

		new_color(COLOR_BLACK, i ^ 7, TRUE);
		put_str(temp);

		new_color(COLOR_WHITE, COLOR_BLACK, FALSE);
		put_crlf();
	}
	generic_done_message(t, state, ch);
	if (*ch != 0 && *ch != 'n') {
		reset_colors();
		return;
	}

	pairs_used = 0;
	cookie_monster = 0;
	if (magic_cookie_glitch > 0) {
		cookie_monster =
			((set_a_foreground || set_foreground)
				? magic_cookie_glitch : 0) +
			((set_a_background || set_background)
				? magic_cookie_glitch : 0) +
			(set_color_pair ? magic_cookie_glitch : 0);
	}
	set_color_step();
	colors_per_line = max_colors > max_pairs
		? max_pairs : max_colors;
	j = (columns - 14) / (cookie_monster + 1);
	if (colors_per_line > j) {
		colors_per_line = (j / i) * i;
	}
	sprintf(temp, "RGB color step %d, cookies %d", color_step,
		cookie_monster);
	ptextln(temp);

	R = G = B = 0;
	pairs_used = 0;
	for (;;) {
		if (rainbow(colors_per_line)) {
			break;
		}
	}
	generic_done_message(t, state, ch);
	if (*ch != 0 && *ch != 'n') {
		reset_colors();
		return;
	}
	dump_colors();
	reset_colors();
	generic_done_message(t, state, ch);
}
