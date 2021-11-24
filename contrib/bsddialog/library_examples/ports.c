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

/* Actually this is an example for mixedmenu to reproduce dialog4ports(1) */
int main()
{
	int i, j, output;
	struct bsddialog_conf conf;
	struct bsddialog_menuitem item;

	struct bsddialog_menuitem check1[5] = {
	    { "+", true,  0, "CSCOPE",        "cscope support", "" },
	    { "+", true,  0, "DEFAULT_VIMRC", "Install bundled vimrc as default setting", "" },
	    { "", false, 0, "MAKE_JOBS",     "Enable parallel build", "" },
	    { "", true,  0, "NLS",           "Native Language Support", "" },
	    { "+", false, 0, "XTERM_SAVE",    "Restore xterm screen after exit", "" }
	};
	struct bsddialog_menuitem sep1[1] = {
	    { "", true, 0, "Optional language bindings", "", "" }
	};
	struct bsddialog_menuitem check2[6] = {
	    { "",  false, 0, "LUA",    "Lua scripting language support", "" },
	    { "+",  true,  0, "PERL",   "Perl scripting language support", "" },
	    { "",  true,  0, "PYTHON", "Python bindings or support", "" },
	    { "+",  true,  0, "RUBY",   "Ruby bindings or support", "" },
	    { "",  false, 0, "SCHEME", "MzScheme (Racket) bindings", "" },
	    { "",  false, 0, "TCL",    "Tcl scripting language support", "" }
	};
	struct bsddialog_menuitem sep2[1] = {
	    { "", true, 0, "CTAGS", "", "" }
	};
	struct bsddialog_menuitem radio1[3] = {
	    { "+",  false, 0, "CTAGS_BASE",      "Use system ctags", "" },
	    { "",  true,  0, "CTAGS_EXUBERANT", "Use exctags instead of ctags", "" },
	    { "",  false, 0, "CTAGS_UNIVERSAL", "Use uctags instead of ctags", "" }
	};
	struct bsddialog_menuitem sep3[1] = {
	    { "", true, 0, "User interface", "", "" }
	};
	struct bsddialog_menuitem radio2[7] = {
	    { "",  false, 0, "ATHENA", "Athena GUI toolkit", "" },
	    { "",  false, 0, "CONSOLE","Console/terminal mode", "" },
	    { "",  false, 0, "GNOME",  "GNOME desktop environment support", "" },
	    { "",  false, 0, "GTK2",   "GTK+ 2 GUI toolkit support", "" },
	    { "",  true,  0, "GTK3",   "GTK+ 3 GUI toolkit support", "" },
	    { "",  false, 0, "MOTIF",  "Motif widget library support", "" },
	    { "",  false, 0, "X11",    "X11 (graphics) support", "" }
	};

	struct bsddialog_menugroup group[7] = {
	    { BSDDIALOG_CHECKLIST, 5, check1 },
	    { BSDDIALOG_SEPARATOR, 1, sep1   },
	    { BSDDIALOG_CHECKLIST, 6, check2 },
	    { BSDDIALOG_SEPARATOR, 1, sep2   },
	    { BSDDIALOG_RADIOLIST, 3, radio1  },
	    { BSDDIALOG_SEPARATOR, 1, sep3   },
	    { BSDDIALOG_RADIOLIST, 7, radio2  },
	};

	bsddialog_initconf(&conf);
	conf.title = "vim-8.2.2569";
	
	if (bsddialog_init() < 0)
		return -1;

	output = bsddialog_mixedlist(conf, "", 0, 0, 0, 7, group, NULL,NULL);

	bsddialog_end();

	printf("Options:\n");
	for (i=0; i<7; i++) {
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
