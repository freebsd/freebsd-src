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
	int value, output;
	struct bsddialog_conf conf;

	bsddialog_initconf(&conf);
	conf.title = "rangebox";
	
	if (bsddialog_init() < 0)
		return -1;

	value = 5;
	output = bsddialog_rangebox(&conf, "Example", 8, 50, 0, 10, &value);

	bsddialog_end();

	printf("Value: %d", value);

	return output;
}
