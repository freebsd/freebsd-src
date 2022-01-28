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
	int output;
	struct bsddialog_conf conf;

	if (bsddialog_init() == BSDDIALOG_ERROR) {
		printf("Error: %s\n", bsddialog_geterror());
		return (1);
	}

	bsddialog_initconf(&conf);
	conf.title = "yesno";
	output = bsddialog_yesno(&conf, "Example", 7, 25);

	bsddialog_end();

	switch (output) {
	case BSDDIALOG_ERROR:
		printf("Error %s\n", bsddialog_geterror());
		break;
	case BSDDIALOG_YES:
		printf("YES\n");
		break;
	case BSDDIALOG_NO:
		printf("NO\n");
		break;
	}

	return (output);
}