/*
 * Copyright 2010 Vincent Sanders <vince@simtec.co.uk>
 *
 * Framebuffer windowing toolkit on screen keyboard.
 *
 * This file is part of NetSurf, http://www.netsurf-browser.org/
 *
 * NetSurf is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * NetSurf is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdbool.h>
#include <limits.h>

#include <libnsfb.h>
#include <libnsfb_plot.h>
#include <libnsfb_event.h>
#include <libnsfb_cursor.h>

#include "utils/log.h"
#include "desktop/browser.h"

#include "framebuffer/gui.h"
#include "framebuffer/fbtk.h"
#include "framebuffer/image_data.h"

#include "widget.h"

struct kbd_button_s {
	int x;
	int y;
	int w;
	int h;
	const char *t;
	enum nsfb_key_code_e keycode;
};

#define KEYCOUNT 58

static struct kbd_button_s kbdbase[KEYCOUNT] = {
	{   0,   0,  20,  15, "`", NSFB_KEY_BACKQUOTE},
	{  20,   0,  20,  15, "1", NSFB_KEY_1},
	{  40,   0,  20,  15, "2", NSFB_KEY_2},
	{  60,   0,  20,  15, "3", NSFB_KEY_3},
	{  80,   0,  20,  15, "4", NSFB_KEY_4},
	{ 100,   0,  20,  15, "5", NSFB_KEY_5},
	{ 120,   0,  20,  15, "6", NSFB_KEY_6},
	{ 140,   0,  20,  15, "7", NSFB_KEY_7},
	{ 160,   0,  20,  15, "8", NSFB_KEY_8},
	{ 180,   0,  20,  15, "9", NSFB_KEY_9},
	{ 200,   0,  20,  15, "0", NSFB_KEY_0},
	{ 220,   0,  20,  15, "-", NSFB_KEY_MINUS},
	{ 240,   0,  20,  15, "=", NSFB_KEY_EQUALS},
	{ 260,   0,  40,  15, "\xe2\x8c\xab", NSFB_KEY_BACKSPACE},
	{   0,  15,  30,  15, "\xe2\x86\xb9", NSFB_KEY_TAB},
	{  30,  15,  20,  15, "q", NSFB_KEY_q},
	{  50,  15,  20,  15, "w", NSFB_KEY_w},
	{  70,  15,  20,  15, "e", NSFB_KEY_e},
	{  90,  15,  20,  15, "r", NSFB_KEY_r},
	{ 110,  15,  20,  15, "t", NSFB_KEY_t},
	{ 130,  15,  20,  15, "y", NSFB_KEY_y},
	{ 150,  15,  20,  15, "u", NSFB_KEY_u},
	{ 170,  15,  20,  15, "i", NSFB_KEY_i},
	{ 190,  15,  20,  15, "o", NSFB_KEY_o},
	{ 210,  15,  20,  15, "p", NSFB_KEY_p},
	{ 230,  15,  20,  15, "[", NSFB_KEY_LEFTBRACKET},
	{ 250,  15,  20,  15, "]", NSFB_KEY_RIGHTBRACKET},
	{ 275,  15,  25,  30, "\xe2\x8f\x8e", NSFB_KEY_RETURN},
	{  35,  30,  20,  15, "a", NSFB_KEY_a},
	{  55,  30,  20,  15, "s", NSFB_KEY_s},
	{  75,  30,  20,  15, "d", NSFB_KEY_d},
	{  95,  30,  20,  15, "f", NSFB_KEY_f},
	{ 115,  30,  20,  15, "g", NSFB_KEY_g},
	{ 135,  30,  20,  15, "h", NSFB_KEY_h},
	{ 155,  30,  20,  15, "j", NSFB_KEY_j},
	{ 175,  30,  20,  15, "k", NSFB_KEY_k},
	{ 195,  30,  20,  15, "l", NSFB_KEY_l},
	{ 215,  30,  20,  15, ";", NSFB_KEY_SEMICOLON},
	{ 235,  30,  20,  15, "'", NSFB_KEY_l},
	{ 255,  30,  20,  15, "#", NSFB_KEY_HASH},
	{   0,  45,  25,  15, "\xe2\x87\xa7", NSFB_KEY_LSHIFT},
	{  25,  45,  20,  15, "\\", NSFB_KEY_SLASH},
	{  45,  45,  20,  15, "z", NSFB_KEY_z},
	{  65,  45,  20,  15, "x", NSFB_KEY_x},
	{  85,  45,  20,  15, "c", NSFB_KEY_c},
	{ 105,  45,  20,  15, "v", NSFB_KEY_v},
	{ 125,  45,  20,  15, "b", NSFB_KEY_b},
	{ 145,  45,  20,  15, "n", NSFB_KEY_n},
	{ 165,  45,  20,  15, "m", NSFB_KEY_m},
	{ 185,  45,  20,  15, ",", NSFB_KEY_COMMA},
	{ 205,  45,  20,  15, ".", NSFB_KEY_PERIOD},
	{ 225,  45,  20,  15, "/", NSFB_KEY_BACKSLASH},
	{ 245,  45,  55,  15, "\xe2\x87\xa7", NSFB_KEY_RSHIFT},
	{  40,  67, 185,  15, "", NSFB_KEY_SPACE},
	{ 250,  60,  20,  15, "\xe2\x96\xb2", NSFB_KEY_UP},
	{ 230,  67,  20,  15, "\xe2\x97\x80", NSFB_KEY_LEFT},
	{ 270,  67,  20,  15, "\xe2\x96\xb6", NSFB_KEY_RIGHT},
	{ 250,  75,  20,  15, "\xe2\x96\xbc", NSFB_KEY_DOWN},
};

static fbtk_widget_t *osk;

static int
osk_close(fbtk_widget_t *widget, fbtk_callback_info *cbi)
{
	if (cbi->event->type != NSFB_EVENT_KEY_UP)
		return 0;

	fbtk_set_mapping(osk, false);

	return 0;
}

static int
osk_click(fbtk_widget_t *widget, fbtk_callback_info *cbi)
{
	nsfb_event_t event;
	struct kbd_button_s *kbd_button = cbi->context;

	event.type = cbi->event->type;
	event.value.keycode = kbd_button->keycode;
	fbtk_input(widget, &event);

	return 0;
}

/* exported function documented in fbtk.h */
void 
fbtk_enable_oskb(fbtk_widget_t *fbtk)
{
	fbtk_widget_t *widget;
	int kloop;
	int maxx = 0;
	int maxy = 0;
	int ww;
	int wh;
	fbtk_widget_t *root = fbtk_get_root_widget(fbtk);
	int furniture_width = 18;

	for (kloop=0; kloop < KEYCOUNT; kloop++) {
		if ((kbdbase[kloop].x + kbdbase[kloop].w) > maxx)
			maxx=kbdbase[kloop].x + kbdbase[kloop].w;
		if ((kbdbase[kloop].y + kbdbase[kloop].h) > maxy)
			maxy=kbdbase[kloop].y + kbdbase[kloop].h;
	}

	ww = fbtk_get_width(root);

	/* scale window height apropriately */
	wh = (maxy * ww) / maxx;

	osk = fbtk_create_window(root, 0, fbtk_get_height(root) - wh, 0, wh, 0xff202020);

	for (kloop=0; kloop < KEYCOUNT; kloop++) {
		widget = fbtk_create_text_button(osk,
						 (kbdbase[kloop].x * ww) / maxx,
						 (kbdbase[kloop].y * ww) / maxx,
						 (kbdbase[kloop].w * ww) / maxx,
						 (kbdbase[kloop].h *ww) / maxx,
						 FB_FRAME_COLOUR,
						 FB_COLOUR_BLACK,
						 osk_click,
						 &kbdbase[kloop]);
		fbtk_set_text(widget, kbdbase[kloop].t);
	}

	widget = fbtk_create_button(osk,
			fbtk_get_width(osk) - furniture_width,
			fbtk_get_height(osk) - furniture_width,
			furniture_width,
			furniture_width,
			FB_FRAME_COLOUR,
			&osk_image,
			osk_close,
			NULL);
}

/* exported function documented in fbtk.h */
void 
map_osk(void)
{
	fbtk_set_zorder(osk, INT_MIN);
	fbtk_set_mapping(osk, true);
}

/*
 * Local Variables:
 * c-basic-offset:8
 * End:
 */
