/*
 * Copyright 2005 Richard Wilson <info@tinct.net>
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
#include "css/css.h"
#include "utils/nsoption.h"
#include "desktop/plot_style.h"
#include "riscos/dialog.h"
#include "riscos/gui.h"
#include "riscos/menus.h"
#include "riscos/wimp.h"
#include "riscos/wimp_event.h"
#include "riscos/configure.h"
#include "riscos/configure/configure.h"
#include "utils/messages.h"
#include "utils/utils.h"


#define FONT_SANS_FIELD 3
#define FONT_SANS_MENU 4
#define FONT_SERIF_FIELD 6
#define FONT_SERIF_MENU 7
#define FONT_MONOSPACE_FIELD 9
#define FONT_MONOSPACE_MENU 10
#define FONT_CURSIVE_FIELD 12
#define FONT_CURSIVE_MENU 13
#define FONT_FANTASY_FIELD 15
#define FONT_FANTASY_MENU 16
#define FONT_DEFAULT_FIELD 18
#define FONT_DEFAULT_MENU 19
#define FONT_DEFAULT_SIZE 23
#define FONT_DEFAULT_DEC 24
#define FONT_DEFAULT_INC 25
#define FONT_MINIMUM_SIZE 28
#define FONT_MINIMUM_DEC 29
#define FONT_MINIMUM_INC 30
#define FONT_DEFAULT_BUTTON 32
#define FONT_CANCEL_BUTTON 33
#define FONT_OK_BUTTON 34

/* This menu only ever gets created once */
/** \todo The memory claimed for this menu should
 * probably be released at some point */
static wimp_menu *default_menu;

static const char *font_names[PLOT_FONT_FAMILY_COUNT] = {
	"Sans-serif",
	"Serif",
	"Monospace",
	"Cursive",
	"Fantasy"
};

static void ro_gui_options_fonts_default(wimp_pointer *pointer);
static bool ro_gui_options_fonts_ok(wimp_w w);
static bool ro_gui_options_fonts_init_menu(void);

bool ro_gui_options_fonts_initialise(wimp_w w)
{
	/* set the current values */
	ro_gui_set_icon_decimal(w, FONT_DEFAULT_SIZE, nsoption_int(font_size), 1);
	ro_gui_set_icon_decimal(w, FONT_MINIMUM_SIZE, nsoption_int(font_min_size), 1);
	ro_gui_set_icon_string(w, FONT_SANS_FIELD, nsoption_charp(font_sans), true);
	ro_gui_set_icon_string(w, FONT_SERIF_FIELD, nsoption_charp(font_serif), true);
	ro_gui_set_icon_string(w, FONT_MONOSPACE_FIELD, nsoption_charp(font_mono), true);
	ro_gui_set_icon_string(w, FONT_CURSIVE_FIELD, nsoption_charp(font_cursive), true);
	ro_gui_set_icon_string(w, FONT_FANTASY_FIELD, nsoption_charp(font_fantasy), true);
	ro_gui_set_icon_string(w, FONT_DEFAULT_FIELD,
			       font_names[nsoption_int(font_default)], true);

	if (!ro_gui_options_fonts_init_menu())
		return false;

	/* initialise all functions for a newly created window */
	ro_gui_wimp_event_register_menu_gright(w, FONT_SANS_FIELD,
			FONT_SANS_MENU, rufl_family_menu);
	ro_gui_wimp_event_register_menu_gright(w, FONT_SERIF_FIELD,
			FONT_SERIF_MENU, rufl_family_menu);
	ro_gui_wimp_event_register_menu_gright(w, FONT_MONOSPACE_FIELD,
			FONT_MONOSPACE_MENU, rufl_family_menu);
	ro_gui_wimp_event_register_menu_gright(w, FONT_CURSIVE_FIELD,
			FONT_CURSIVE_MENU, rufl_family_menu);
	ro_gui_wimp_event_register_menu_gright(w, FONT_FANTASY_FIELD,
			FONT_FANTASY_MENU, rufl_family_menu);
	ro_gui_wimp_event_register_menu_gright(w, FONT_DEFAULT_FIELD,
			FONT_DEFAULT_MENU, default_menu);
	ro_gui_wimp_event_register_numeric_field(w, FONT_DEFAULT_SIZE,
			FONT_DEFAULT_INC, FONT_DEFAULT_DEC, 50, 1000, 1, 1);
	ro_gui_wimp_event_register_numeric_field(w, FONT_MINIMUM_SIZE,
			FONT_MINIMUM_INC, FONT_MINIMUM_DEC, 10, 500, 1, 1);
	ro_gui_wimp_event_register_button(w, FONT_DEFAULT_BUTTON,
			ro_gui_options_fonts_default);
	ro_gui_wimp_event_register_cancel(w, FONT_CANCEL_BUTTON);
	ro_gui_wimp_event_register_ok(w, FONT_OK_BUTTON,
			ro_gui_options_fonts_ok);
	ro_gui_wimp_event_set_help_prefix(w, "HelpFontConfig");
	ro_gui_wimp_event_memorise(w);
	return true;

}

void ro_gui_options_fonts_default(wimp_pointer *pointer)
{
	const char *fallback = nsfont_fallback_font();

	/* set the default values */
	ro_gui_set_icon_decimal(pointer->w, FONT_DEFAULT_SIZE, 128, 1);
	ro_gui_set_icon_decimal(pointer->w, FONT_MINIMUM_SIZE, 85, 1);
	ro_gui_set_icon_string(pointer->w, FONT_SANS_FIELD,
			nsfont_exists("Homerton") ? "Homerton" : fallback, true);
	ro_gui_set_icon_string(pointer->w, FONT_SERIF_FIELD,
			nsfont_exists("Trinity") ? "Trinity" : fallback, true);
	ro_gui_set_icon_string(pointer->w, FONT_MONOSPACE_FIELD,
			nsfont_exists("Corpus") ? "Corpus" : fallback, true);
	ro_gui_set_icon_string(pointer->w, FONT_CURSIVE_FIELD,
			nsfont_exists("Churchill") ? "Churchill" : fallback, true);
	ro_gui_set_icon_string(pointer->w, FONT_FANTASY_FIELD,
			nsfont_exists("Sassoon") ? "Sassoon" : fallback, true);
	ro_gui_set_icon_string(pointer->w, FONT_DEFAULT_FIELD,
			font_names[0], true);
}

bool ro_gui_options_fonts_ok(wimp_w w)
{
	unsigned int i;

	nsoption_set_int(font_size,
			 ro_gui_get_icon_decimal(w, FONT_DEFAULT_SIZE, 1));

	nsoption_set_int(font_min_size,
			 ro_gui_get_icon_decimal(w, FONT_MINIMUM_SIZE, 1));

	if (nsoption_int(font_size) < nsoption_int(font_min_size)) {
		nsoption_set_int(font_size, nsoption_int(font_min_size));
		ro_gui_set_icon_decimal(w, FONT_DEFAULT_SIZE, nsoption_int(font_size), 1);
	
}

	nsoption_set_charp(font_sans,
			   strdup(ro_gui_get_icon_string(w, FONT_SANS_FIELD)));

	nsoption_set_charp(font_serif,
			   strdup(ro_gui_get_icon_string(w, FONT_SERIF_FIELD)));

	nsoption_set_charp(font_mono,
			   strdup(ro_gui_get_icon_string(w, FONT_MONOSPACE_FIELD)));

	nsoption_set_charp(font_cursive,
			   strdup(ro_gui_get_icon_string(w, FONT_CURSIVE_FIELD)));

	nsoption_set_charp(font_fantasy,
			   strdup(ro_gui_get_icon_string(w, FONT_FANTASY_FIELD)));

	for (i = 0; i != 5; i++) {
		if (!strcmp(font_names[i], ro_gui_get_icon_string(w,
				FONT_DEFAULT_FIELD)))
			break;
	}
	if (i == 5)
		/* this should never happen, but still */
		i = 0;

	nsoption_set_int(font_default, i);

	ro_gui_save_options();
	return true;
}

bool ro_gui_options_fonts_init_menu(void)
{
	unsigned int i;

	if (default_menu)
		/* Already exists */
		return true;

	default_menu = malloc(wimp_SIZEOF_MENU(5));
	if (!default_menu) {
		warn_user("NoMemory", 0);
		return false;
	}
	default_menu->title_data.indirected_text.text =
			(char *) messages_get("DefaultFonts");
	ro_gui_menu_init_structure(default_menu, 5);
	for (i = 0; i < 5; i++) {
		default_menu->entries[i].data.indirected_text.text =
				(char *) font_names[i];
		default_menu->entries[i].data.indirected_text.size =
				strlen(font_names[i]);
	}
	return true;
}
