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

int main()
{
	int i, output;
	struct bsddialog_conf conf;
	struct bsddialog_menuitem items[5] = {
	    {"I",   true,  0, "Name 1", "Desc 1", "Bottom Desc 1"},
	    {"II",  false, 0, "Name 2", "Desc 2", "Bottom Desc 2"},
	    {"III", true,  0, "Name 3", "Desc 3", "Bottom Desc 3"},
	    {"IV",  false, 0, "Name 4", "Desc 4", "Bottom Desc 4"},
	    {"V",   true,  0, "Name 5", "Desc 5", "Bottom Desc 5"}
	};

	if (bsddialog_init() == BSDDIALOG_ERROR) {
		printf("Error: %s\n", bsddialog_geterror());
		return (1);
	}

	bsddialog_initconf(&conf);
	conf.title = "radiolist";
	output = bsddialog_radiolist(&conf, "Example", 15, 30, 5, 5, items, NULL);

	bsddialog_end();

	if (output == BSDDIALOG_ERROR) {
		printf("Error: %s\n", bsddialog_geterror());
		return (1);
	}

	if (output == BSDDIALOG_CANCEL) {
		printf("Cancel\n");
		return (0);
	}

	printf("Radiolist:\n");
	for (i = 0; i < 5; i++)
		printf(" (%c) %s\n", items[i].on ? '*' : ' ', items[i].name);

	return (output);
}