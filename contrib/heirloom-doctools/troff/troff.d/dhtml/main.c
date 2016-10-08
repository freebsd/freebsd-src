/*
 * Copyright (c) 2015, Carsten Kunze
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "dhtml.h"
#include "char.h"

char *progname;

static void begin_html(void);
static void end_html(void);

static char *title;

int
main(int argc, char **argv) {
	int opt;
	progname = argv[0];
	while ((opt = getopt(argc, argv, "t:")) != -1) {
		switch (opt) {
		case 't':
			title = optarg;
			break;
		default:
			fprintf(stderr, "Usage: %s [-t title] < "
			    "ditroff_output > html_output\n", progname);
			exit(EXIT_FAILURE);
		}
	}
	begin_html();
	char_open();
	run_lex();
	end_html();
	return 0;
}

static void
begin_html(void) {
	puts("<!DOCTYPE html>");
	puts("<html lang=\"en\">");
	puts("  <head>");
	puts("    <meta charset=\"utf-8\">");
	printf("    <title>%s</title>", title ? title : "");
	puts("  </head>");
	puts("  <body>");
}

static void
end_html(void) {
	puts("");
	puts("  </body>");
	puts("</html>");
}
