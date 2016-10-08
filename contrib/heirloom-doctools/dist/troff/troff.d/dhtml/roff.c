#include <stdio.h>
#include "roff.h"

static struct req_in_data {
	int not_1st;
	int last_i;
} req_in_data;

void
req_in(int i) {
#if 0
	if (req_in_data.not_1st) {
		puts("\n</div>");
		req_in_data.not_1st = 0;
	}
	if (i) {
		printf("\n<div style=\"padding-left: %dpx;\">\n", i);
		req_in_data.not_1st = 1;
	}
#endif
	puts("<br>");
}

void
req_sp(int i) {
	printf("\n<div style=\"height: %dpx;\"></div>\n", (i*12)/40);
}
