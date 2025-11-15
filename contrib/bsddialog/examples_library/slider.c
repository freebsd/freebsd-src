/*-
 * SPDX-License-Identifier: CC0-1.0
 *
 * Written in 2025 by Alfonso Sabato Siciliano.
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
	unsigned long start, end, blocks[2][2];
	struct bsddialog_conf conf;

	start = 20;
	end = 70;
	blocks[0][0] = 5;
	blocks[0][1] = 10;
	blocks[1][0] = 80;
	blocks[1][1] = 90;

	if (bsddialog_init() == BSDDIALOG_ERROR) {
		printf("Error: %s\n", bsddialog_geterror());
		return (1);
	}
	bsddialog_initconf(&conf);
	conf.title = "slider";

	output = bsddialog_slider(&conf, "Example", 0, 0, "GiB", 100, &start,
	    &end, false, 2, blocks);
	bsddialog_end();
	if (output == BSDDIALOG_ERROR) {
		printf("Error: %s\n", bsddialog_geterror());
		return (1);
	}
	printf("Start: %lu, End: %lu\n", start, end);

	return (0);
}
