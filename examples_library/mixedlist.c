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
	int i, j, output;
	struct bsddialog_conf conf;
	struct bsddialog_menuitem item;
	struct bsddialog_menuitem check[5] = {
	    { "+", true,  0, "Name 1", "Desc 1", "Bottom Desc 1" },
	    { "" , false, 0, "Name 2", "Desc 2", "Bottom Desc 2" },
	    { "+", true,  0, "Name 3", "Desc 3", "Bottom Desc 3" },
	    { "" , false, 0, "Name 4", "Desc 4", "Bottom Desc 4" },
	    { "+", true,  0, "Name 5", "Desc 5", "Bottom Desc 5" }
	};
	struct bsddialog_menuitem sep[1] = {
	    { "", true, 0, "Radiolist", "(desc)", "" }
	};
	struct bsddialog_menuitem radio[5] = {
	    { "",  true,  0, "Name 1", "Desc 1", "Bottom Desc 1" },
	    { "+", false, 0, "Name 2", "Desc 2", "Bottom Desc 2" },
	    { "",  false, 0, "Name 3", "Desc 3", "Bottom Desc 3" },
	    { "+", false, 0, "Name 4", "Desc 4", "Bottom Desc 4" },
	    { "",  false, 0, "Name 5", "Desc 5", "Bottom Desc 5" }
	};
	struct bsddialog_menugroup group[3] = {
	    { BSDDIALOG_CHECKLIST, 5, check },
	    { BSDDIALOG_SEPARATOR, 1, sep   },
	    { BSDDIALOG_RADIOLIST, 5, radio }
	};

	bsddialog_initconf(&conf);
	conf.title = "mixedmenu";
	
	if (bsddialog_init() < 0)
		return -1;

	output = bsddialog_mixedlist(&conf, "dialog4ports", 20, 30, 11, 3, group,
	    NULL,NULL);

	bsddialog_end();

	printf("Mixedlist:\n");
	for (i=0; i<3; i++) {
		for (j=0; j<group[i].nitems; j++) {
			item = group[i].items[j];
			if (group[i].type == BSDDIALOG_SEPARATOR)
				printf("----- %s -----\n", item.name);
			else if (group[i].type == BSDDIALOG_RADIOLIST)
				printf(" (%c) %s\n", item.on ? '*' : ' ', item.name);
			else /* BSDDIALOG_PORTCHECKLIST */
				printf(" [%c] %s\n", item.on ? 'X' : ' ', item.name);
		}
	}
		
	
	return output;
}
