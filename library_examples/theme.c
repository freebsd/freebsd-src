/*-
 * SPDX-License-Identifier: CC0-1.0
 *
 * Written in 2021 by Alfonso Sabato Siciliano.
 * To the extent possible under law, the author has dedicated all copyright
 * and related and neighboring rights to this software to the public domain
 * worldwide. This software is distributed without any warranty, see:
 *   <http://creativecommons.org/publicdomain/zero/1.0/>.
 */

#include <stdio.h>
#include <string.h>

#include <bsddialog.h>
#include <bsddialog_theme.h>

int main()
{
	int output;
	struct bsddialog_conf conf;
	enum bsddialog_default_theme theme;
	struct bsddialog_menuitem items[5] = {
	    {"", false, 0, "Dialog",    "Current dialog theme",  "BSDDIALOG_THEME_DIALOG" },
	    {"", false, 0, "BSDDialog", "Future default theme",  "BSDDIALOG_THEME_DEFAULT"},
	    {"", false, 0, "BlackWhite","Black and White theme", "BSDDIALOG_THEME_BLACKWHITE"},
	    {"", false, 0, "Magenta",   "Testing",               "BSDDIALOG_THEME_MAGENTA"},
	    {"", false, 0, "Quit",      "Exit",                  "Quit or Cancel to exit" }
	};

	bsddialog_initconf(&conf);
	conf.title = " Theme ";
	
	if (bsddialog_init() == BSDDIALOG_ERROR)
		return BSDDIALOG_ERROR;

	while (true) {
		bsddialog_backtitle(conf, "Theme Example");

		output = bsddialog_menu(conf, "Choose theme", 15, 40, 5, 5, items, NULL);

		if (output != BSDDIALOG_YESOK || items[4].on)
			break;

		if (items[0].on) {
			theme = BSDDIALOG_THEME_DIALOG;
			conf.menu.default_item = items[0].name;
		}
		else if (items[1].on) {
			theme = BSDDIALOG_THEME_BSDDIALOG;
			conf.menu.default_item = items[1].name;
		}
		else if (items[2].on) {
			theme = BSDDIALOG_THEME_BLACKWHITE;
			conf.menu.default_item = items[2].name;
		}
		else if (items[3].on) {
			theme = BSDDIALOG_THEME_MAGENTA;
			conf.menu.default_item = items[3].name;
		}

		bsddialog_set_default_theme(theme);
	}

	bsddialog_end();	

	return output;
}
