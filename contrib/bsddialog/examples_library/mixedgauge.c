/*-
 * SPDX-License-Identifier: CC0-1.0
 *
 * Written in 2023 by Alfonso Sabato Siciliano.
 * To the extent possible under law, the author has dedicated all copyright
 * and related and neighboring rights to this software to the public domain
 * worldwide. This software is distributed without any warranty, see:
 *   <http://creativecommons.org/publicdomain/zero/1.0/>.
 */

#include <bsddialog.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#define NMINIBAR    13

static const char *minilabels[NMINIBAR] = {
	"Label   1",
	"Label   2",
	"Label   3",
	"Label   4",
	"Label   5",
	"Label   6",
	"Label   7",
	"Label   8",
	"Label   9",
	"Label  10",
	"Label  11",
	"Label   X",
	"Label   Y",
};

static int minipercs[NMINIBAR] = {
	BSDDIALOG_MG_SUCCEEDED,
	BSDDIALOG_MG_FAILED,
	BSDDIALOG_MG_PASSED,
	BSDDIALOG_MG_COMPLETED,
	BSDDIALOG_MG_CHECKED,
	BSDDIALOG_MG_DONE,
	BSDDIALOG_MG_SKIPPED,
	BSDDIALOG_MG_INPROGRESS,
	BSDDIALOG_MG_BLANK,
	BSDDIALOG_MG_NA,
	BSDDIALOG_MG_PENDING,
	67,
	0,
};

static void exit_error()
{
	if (bsddialog_inmode())
		bsddialog_end();
	printf("Error: %s\n", bsddialog_geterror());
	exit (1);
}

int main()
{
	int retval, i;
	struct bsddialog_conf conf;

	if (bsddialog_init() == BSDDIALOG_ERROR)
		exit_error();
	bsddialog_initconf(&conf);
	conf.title = "mixedgauge";
	for (i = 0; i <= 10; i++) {
		minipercs[11] += 3;
		minipercs[12] = i * 10;
		retval= bsddialog_mixedgauge(&conf, "Example", 20, 40,
		    50 + i * 5, NMINIBAR, minilabels, minipercs);
    		if(retval == BSDDIALOG_ERROR)
			exit_error();
		sleep(1);
	}
	bsddialog_end();

	return (0);
}