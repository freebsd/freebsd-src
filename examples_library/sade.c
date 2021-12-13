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

/* Figure 15 - https://docs.freebsd.org/en/books/handbook/bsdinstall/ */
int main()
{
	int i, output;
	struct bsddialog_conf conf;
	struct bsddialog_menuitem items[5] = {
	    {"", false, 0, "ada0",   "16 GB GPT", ""},
	    {"", false, 1, "ada0p1", "512 KB freebsd-boot", ""},
	    {"", false, 1, "ada0p2", "15 GB freebsd-ufs", ""},
	    {"", false, 1, "ada0p3", "819 MB freebsd-swap none", ""},
	    {"", false, 0, "ada1",   "16 GB", ""}
	};

	bsddialog_initconf(&conf);
	conf.title = "Partition Editor";
	char *text = "Please review the disk setup. When complete, press the "\
	    "Finish button";

	conf.menu.align_left = true;

	conf.button.ok_label = "Create";
	
	conf.button.with_extra = true;
	conf.button.extra_label = "Delete";
	
	conf.button.cancel_label = "Cancel";
	
	conf.button.with_help = true;
	conf.button.help_label = "Revert";
	
	conf.button.generic1_label = "Auto";
	conf.button.generic2_label = "Finish";
	
	conf.button.default_label= "Finish";
	
	if (bsddialog_init() < 0)
		return -1;

	output = bsddialog_menu(&conf, text, 20, 0, 10, 5, items, NULL);

	bsddialog_end();

	printf("Menu:\n");
	for (i=0; i<5; i++)
		printf(" [%c] %s\n", items[i].on ? 'X' : ' ', items[i].name);
		
	
	return output;
}
