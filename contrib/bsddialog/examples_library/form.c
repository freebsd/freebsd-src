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
#include <stdlib.h>
#include <string.h>

#include <bsddialog.h>

#define H   BSDDIALOG_FIELDHIDDEN
#define RO  BSDDIALOG_FIELDREADONLY

int main()
{
	int i, output;
	struct bsddialog_conf conf;
	struct bsddialog_formitem items[3] = {
	    {"Input:",    1, 1, "value",     1, 11, 30, 50, NULL, 0,  "desc 1"},
	    {"Input:",    2, 1, "read only", 2, 11, 30, 50, NULL, RO, "desc 2"},
	    {"Password:", 3, 1, "",          3, 11, 30, 50, NULL, H,  "desc 3"}
	};

	if (bsddialog_init() == BSDDIALOG_ERROR) {
		printf("Error: %s\n", bsddialog_geterror());
		return (1);
	}

	bsddialog_initconf(&conf);
	conf.title = "form";
	conf.form.securech = '*';
	output = bsddialog_form(&conf, "Example", 10, 50, 3, 3, items);

	bsddialog_end();

	if (output == BSDDIALOG_ERROR) {
		printf("Error: %s", bsddialog_geterror());
		return (1);
	}

	if (output == BSDDIALOG_CANCEL) {
		printf("Cancel\n");
		return (0);
	}

	for (i = 0; i < 3; i++) {
		printf("%s \"%s\"\n", items[i].label, items[i].value);
		free(items[i].value);
	}

	return (output);
}