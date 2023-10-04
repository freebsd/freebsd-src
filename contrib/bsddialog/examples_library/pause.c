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
	unsigned int sec;
	struct bsddialog_conf conf;

	if (bsddialog_init() == BSDDIALOG_ERROR) {
		printf("Error: %s\n", bsddialog_geterror());
		return (1);
	}
	bsddialog_initconf(&conf);
	conf.title = "pause";
	sec = 10;
	output = bsddialog_pause(&conf, "Example", 8, 50, &sec);
	bsddialog_end();

	switch (output) {
	case BSDDIALOG_ERROR:
		printf("Error: %s\n", bsddialog_geterror());
		return (1);
	case BSDDIALOG_OK:
		printf("[OK] remaining time: %u\n", sec);
		break;
	case BSDDIALOG_CANCEL:
		printf("[Cancel] remaining time: %u\n", sec);
		break;
	case BSDDIALOG_TIMEOUT:
		printf("Timeout\n");
		break;
	}

	return (0);
}
