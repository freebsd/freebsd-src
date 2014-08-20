/*
 * This file is part of RUfl
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license
 * Copyright 2006 James Bursa <james@semichrome.net>
 */

#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include "oslib/colourtrans.h"
#include "oslib/wimp.h"
#include "rufl.h"


unsigned int font = 0;
unsigned int weight = rufl_WEIGHT_400;
bool italic = false;


static rufl_code redraw(int x, int y, int y0, int y1);
static void try(rufl_code code, const char *context);
static void die(const char *error);


int main(void)
{
	unsigned int i;
	bool quit = false;
	const wimp_MESSAGE_LIST(2) messages = { { message_MODE_CHANGE,
			message_QUIT } };
	wimp_t task;
	wimp_menu *menu;
	struct wimp_window_base window = {
		{ 400, 400, 1700, 1200 },
		0, 0,
		wimp_TOP,
		wimp_WINDOW_MOVEABLE | wimp_WINDOW_BACK_ICON |
		wimp_WINDOW_CLOSE_ICON | wimp_WINDOW_TITLE_ICON |
		wimp_WINDOW_TOGGLE_ICON | wimp_WINDOW_VSCROLL |
		wimp_WINDOW_SIZE_ICON | wimp_WINDOW_NEW_FORMAT,
		wimp_COLOUR_BLACK, wimp_COLOUR_LIGHT_GREY,
		wimp_COLOUR_BLACK, wimp_COLOUR_WHITE,
		wimp_COLOUR_DARK_GREY, wimp_COLOUR_MID_LIGHT_GREY,
		wimp_COLOUR_CREAM,
		0,
		{ 0, -81928, 1300, 0 },
		wimp_ICON_TEXT | wimp_ICON_HCENTRED,
		0,
		0,
		2, 1,
		{ "RUfl Chars" },
		0 };
	wimp_w w;
	wimp_window_state state;
	wimp_block block;
	wimp_event_no event;
	wimp_pointer pointer;
	osbool more;
	os_error *error;
	rufl_code code = rufl_OK;

	error = xwimp_initialise(wimp_VERSION_RO3, "RUfl Chars",
			(const wimp_message_list *) (const void *) &messages,
			0, &task);
	if (error) {
		printf("error: xwimp_initialise: 0x%x: %s\n",
				error->errnum, error->errmess);
		exit(1);
	}

	try(rufl_init(), "rufl_init");

	menu = malloc(wimp_SIZEOF_MENU(10 + rufl_family_list_entries));
	if (!menu)
		die("Out of memory");
	strcpy(menu->title_data.text, "Fonts");
	menu->title_fg = wimp_COLOUR_BLACK;
	menu->title_bg = wimp_COLOUR_LIGHT_GREY;
	menu->work_fg = wimp_COLOUR_RED;
	menu->work_bg = wimp_COLOUR_WHITE;
	menu->width = 200;
	menu->height = wimp_MENU_ITEM_HEIGHT;
	menu->gap = wimp_MENU_ITEM_GAP;
	for (i = 0; i != 10; i++) {
		menu->entries[i].menu_flags = 0;
		menu->entries[i].sub_menu = wimp_NO_SUB_MENU;
		menu->entries[i].icon_flags = wimp_ICON_TEXT |
			      (wimp_COLOUR_BLACK << wimp_ICON_FG_COLOUR_SHIFT) |
			      (wimp_COLOUR_WHITE << wimp_ICON_BG_COLOUR_SHIFT);
		strcpy(menu->entries[i].data.text, "100");
		menu->entries[i].data.text[0] = '1' + i;
	}
	menu->entries[9].menu_flags = wimp_MENU_SEPARATE;
	menu->entries[9].sub_menu = wimp_NO_SUB_MENU;
	menu->entries[9].icon_flags = wimp_ICON_TEXT |
			(wimp_COLOUR_BLACK << wimp_ICON_FG_COLOUR_SHIFT) |
			(wimp_COLOUR_WHITE << wimp_ICON_BG_COLOUR_SHIFT);
	strcpy(menu->entries[9].data.text, "Italic");
	for (i = 0; i != rufl_family_list_entries; i++) {
		menu->entries[10 + i].menu_flags = 0;
		menu->entries[10 + i].sub_menu = wimp_NO_SUB_MENU;
		menu->entries[10 + i].icon_flags = wimp_ICON_TEXT |
				wimp_ICON_INDIRECTED |
			(wimp_COLOUR_BLACK << wimp_ICON_FG_COLOUR_SHIFT) |
			(wimp_COLOUR_WHITE << wimp_ICON_BG_COLOUR_SHIFT);
		menu->entries[10 + i].data.indirected_text.text =
				(char *) rufl_family_list[i];
		menu->entries[10 + i].data.indirected_text.validation =
				(char *) -1;
		menu->entries[10 + i].data.indirected_text.size =
				strlen(rufl_family_list[i]);
	}
	menu->entries[3].menu_flags |= wimp_MENU_TICKED;
	menu->entries[10].menu_flags |= wimp_MENU_TICKED;
	menu->entries[i + 9].menu_flags |= wimp_MENU_LAST;

	error = xwimp_create_window((wimp_window *) &window, &w);
	if (error)
		die(error->errmess);

	state.w = w;
	error = xwimp_get_window_state(&state);
	if (error)
		die(error->errmess);

	error = xwimp_open_window((wimp_open *) (void *) &state);
	if (error)
		die(error->errmess);

	while (!quit) {
		error = xwimp_poll(wimp_MASK_NULL, &block, 0, &event);
		if (error)
			die(error->errmess);

		switch (event) {
		case wimp_REDRAW_WINDOW_REQUEST:
			error = xwimp_redraw_window(&block.redraw, &more);
			if (error)
				die(error->errmess);
			xcolourtrans_set_font_colours(0, os_COLOUR_WHITE,
					os_COLOUR_BLACK, 14, 0, 0, 0);
			while (more) {
				code = redraw(block.redraw.box.x0 -
						block.redraw.xscroll,
						block.redraw.box.y1 -
						block.redraw.yscroll,
						block.redraw.box.y1 -
						block.redraw.yscroll -
						block.redraw.clip.y1,
						block.redraw.box.y1 -
						block.redraw.yscroll -
						block.redraw.clip.y0);
				error = xwimp_get_rectangle(&block.redraw,
						&more);
				if (error)
					die(error->errmess);
			}
			try(code, "redraw");
			break;

		case wimp_OPEN_WINDOW_REQUEST:
			error = xwimp_open_window(&block.open);
			if (error)
				die(error->errmess);
			break;

		case wimp_CLOSE_WINDOW_REQUEST:
			quit = true;
			break;

		case wimp_MOUSE_CLICK:
			if (block.pointer.buttons == wimp_CLICK_MENU) {
				error = xwimp_create_menu(menu,
						block.pointer.pos.x - 64,
						block.pointer.pos.y);
				if (error)
					die(error->errmess);
			}
			break;

		case wimp_MENU_SELECTION:
			error = xwimp_get_pointer_info(&pointer);
			if (error)
				die(error->errmess);
			if (block.selection.items[0] <= 8) {
				menu->entries[weight - 1].menu_flags ^=
						wimp_MENU_TICKED;
				weight = block.selection.items[0] + 1;
				menu->entries[weight - 1].menu_flags ^=
						wimp_MENU_TICKED;
			} else if (block.selection.items[0] == 9) {
				italic = !italic;
				menu->entries[9].menu_flags ^= wimp_MENU_TICKED;
			} else {
				menu->entries[10 + font].menu_flags ^=
						wimp_MENU_TICKED;
				font = block.selection.items[0] - 10;
				menu->entries[10 + font].menu_flags ^=
						wimp_MENU_TICKED;
			}
			error = xwimp_force_redraw(w,
					window.extent.x0, window.extent.y0,
					window.extent.x1, window.extent.y1);
			if (error)
				die(error->errmess);
			if (pointer.buttons == wimp_CLICK_ADJUST) {
				error = xwimp_create_menu(menu,
						pointer.pos.x - 64,
						pointer.pos.y);
				if (error)
					die(error->errmess);
			}
			break;

		case wimp_USER_MESSAGE:
		case wimp_USER_MESSAGE_RECORDED:
			switch (block.message.action) {
			case message_QUIT:
				quit = true;
				break;
			case message_MODE_CHANGE:
				rufl_invalidate_cache();
				break;
			}
			break;
		}
	}

/* 	try(rufl_paint("NewHall.Medium", 240, utf8_test, sizeof utf8_test - 1, */
/* 			1200, 1200), "rufl_paint"); */

	xwimp_close_down(task);

	rufl_quit();

	return 0;
}


rufl_code redraw(int x, int y, int y0, int y1)
{
	char s[10];
	unsigned int l;
	unsigned int u;
	rufl_code code;
	rufl_style style = weight | (italic ? rufl_SLANTED : 0);

	for (u = y0 / 40 * 32; (int) u != (y1 / 40 + 1) * 32; u++) {
		if (u <= 0x7f)
			s[0] = u, l = 1;
		else if (u <= 0x7ff)
			s[0] = 0xc0 | (u >> 6),
			s[1] = 0x80 | (u & 0x3f), l = 2;
		else if (u <= 0xffff)
			s[0] = 0xe0 | (u >> 12),
			s[1] = 0x80 | ((u >> 6) & 0x3f),
			s[2] = 0x80 | (u & 0x3f), l = 3;
		else
			break;
		s[l] = 0;

		code = rufl_paint(rufl_family_list[font], style, 240, s, l,
				x + 10 + 40 * (u % 32),
				y - 40 - 40 * (u / 32),
				0);
		if (code != rufl_OK)
			return code;
	}

	return rufl_OK;
}


void try(rufl_code code, const char *context)
{
	char s[200];
	if (code == rufl_OK)
		return;
	else if (code == rufl_OUT_OF_MEMORY)
		snprintf(s, sizeof s, "error: %s: out of memory\n", context);
	else if (code == rufl_FONT_MANAGER_ERROR)
		snprintf(s, sizeof s, "error: %s: Font Manager error %x %s\n",
				context, rufl_fm_error->errnum,
				rufl_fm_error->errmess);
	else if (code == rufl_FONT_NOT_FOUND)
		snprintf(s, sizeof s, "error: %s: font not found\n", context);
	else if (code == rufl_IO_ERROR)
		snprintf(s, sizeof s, "error: %s: io error: %i %s\n", context,
				errno, strerror(errno));
	else if (code == rufl_IO_EOF)
		snprintf(s, sizeof s, "error: %s: eof\n", context);
	else
		snprintf(s, sizeof s, "error: %s: unknown error\n", context);

	die(s);
}


void die(const char *error)
{
	os_error warn_error;

	warn_error.errnum = 1;
	strncpy(warn_error.errmess, error,
			sizeof warn_error.errmess  - 1);
	warn_error.errmess[sizeof warn_error.errmess  - 1] = '\0';
	xwimp_report_error(&warn_error,
			wimp_ERROR_BOX_OK_ICON,
			"RUfl Chars", 0);
	rufl_quit();
	exit(EXIT_FAILURE);
}
