/*-
 * SPDX-License-Identifier: CC0-1.0
 *
 * Written in 2021 by Alfonso Sabato Siciliano.
 * To the extent possible under law, the author has dedicated all copyright
 * and related and neighboring rights to this software to the public domain
 * worldwide. This software is distributed without any warranty, see:
 *   <http://creativecommons.org/publicdomain/zero/1.0/>.
 */

#include <bsddialog.h>
#include <bsddialog_theme.h>
#include <stdio.h>
#include <string.h>

int main()
{
	int output, focusitem;
	struct bsddialog_conf conf;
	enum bsddialog_default_theme theme;
	struct bsddialog_menuitem items[5] = {
		{"", false, 0, "Flat", "dialog-like",
		    "BSDDIALOG_THEME_FLAT" },
		{"", false, 0, "Dialog", "dialog clone",
		    "BSDDIALOG_THEME_DIALOG" },
		{"", false, 0, "BSDDialog", "new theme",
		    "BSDDIALOG_THEME_BSDDIALOG" },
		{"", false, 0, "BlackWhite","black and white",
		    "BSDDIALOG_THEME_BLACKWHITE" },
		{"", false, 0, "Quit", "Exit", "Quit or Cancel to exit" }
	};

	if (bsddialog_init() == BSDDIALOG_ERROR) {
		printf("Error: %s\n", bsddialog_geterror());
		return (1);
	}

	bsddialog_initconf(&conf);

	bsddialog_backtitle(&conf, "Theme Example");

	conf.title = " Theme ";
	focusitem = -1;
	while (true) {
		output = bsddialog_menu(&conf, "Choose theme", 15, 45, 5, 5,
		    items, &focusitem);

		if (output != BSDDIALOG_OK || items[4].on)
			break;

		if (items[0].on) {
			theme = BSDDIALOG_THEME_FLAT;
			focusitem = 0;
		}
		else if (items[1].on) {
			theme = BSDDIALOG_THEME_DIALOG;
			focusitem = 1;
		}
		else if (items[2].on) {
			theme = BSDDIALOG_THEME_BSDDIALOG;
			focusitem = 2;
		}
		else if (items[3].on) {
			theme = BSDDIALOG_THEME_BLACKWHITE;
			focusitem = 3;
		}

		bsddialog_set_default_theme(theme);
	}

	bsddialog_end();

	return (output);
}