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
#include <stdio.h>

int main()
{
	int output;
	struct bsddialog_conf conf;

	if (bsddialog_init() == BSDDIALOG_ERROR) {
		printf("Error: %s\n", bsddialog_geterror());
		return (1);
	}
	bsddialog_initconf(&conf);
	conf.title = "msgbox";
	output = bsddialog_msgbox(&conf, "Example", 7, 20);
	bsddialog_end();

	switch (output) {
	case BSDDIALOG_ERROR:
		printf("Error %s\n", bsddialog_geterror());
		return (1);
	case BSDDIALOG_OK:
		printf("[OK]\n");
		break;
	}

	return (0);
}