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
#include <string.h>
#include <time.h>

int main()
{
	int output;
	unsigned int yy, mm, dd;
	struct bsddialog_conf conf;
	time_t cal;
	struct tm *localtm;

	time(&cal);
	localtm = localtime(&cal);
	yy = localtm->tm_year + 1900;
	mm = localtm->tm_mon + 1;
	dd = localtm->tm_mday;

	if (bsddialog_init() == BSDDIALOG_ERROR) {
		printf("Error: %s\n", bsddialog_geterror());
		return (1);
	}

	bsddialog_initconf(&conf);
	conf.title = "datebox";
	output = bsddialog_datebox(&conf, "Example", 9, 35, &yy, &mm, &dd);

	bsddialog_end();

	switch (output) {
	case BSDDIALOG_OK:
		printf("Date: %u/%u/%u", yy, mm, dd);
		break;
	case BSDDIALOG_CANCEL:
		printf("Cancel");
		break;
	case BSDDIALOG_ERROR:
		printf("Error: %s", bsddialog_geterror());
		break;
	}
	printf("\n");

	return (output);
}