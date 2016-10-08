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
#include <string.h>
#include <stdlib.h>
#include "tr_out.h"
#include "main.h"
#include "bst.h"
#include "lib.h"
#include "char.h"

int fontsize;
int hdec;

static void closesize(void);

static int lineheight;
static int totalh;
static char *size_end;
static struct bst fonts = { NULL, bst_icmp };

void
out_x_T(char *dev) {
	if (!strcmp(dev, "html")) return;
	fprintf(stderr, "%s: Invalid troff output device \"%s\"."
	    "Please use troff with option \"-Thtml\".\n", progname, dev);
}

void
out_f(int num) {
	static char *prevfont  = "R";
	static char *closefont;
	struct bst_node *n;
	char *nam;
	clslig();
	if (!bst_srch(&fonts, I2BST(num), &n)) {
		nam = n->data.p;
	} else {
		nam = "R";
		if (num != 1)
			fprintf(stderr, "%s: Unknown font %d\n", progname,
			    num);
	}
	if (!strcmp(nam, prevfont)) return;
	closesize();
	if (closefont) {
		fputs(closefont, stdout);
		closefont = NULL;
	}
	prevfont = nam;
	if (!strcmp(nam, "R") ||
	    !strcmp(nam, "S")) return;
	else
	if (!strcmp(nam, "I")) {
		fputs("<span style=\"font-style: italic\">", stdout);
		closefont = "</span>";
	} else
	if (!strcmp(nam, "B")) {
		fputs("<span style=\"font-weight: bold\">", stdout);
		closefont = "</span>";
	} else
	if (!strcmp(nam, "BI")) {
		fputs("<span style=\"font-weight: bold;"
		    " font-style: italic\">", stdout);
		closefont = "</span>";
	} else
	if (!strcmp(nam, "CW") ||
	    !strcmp(nam, "CR") ||
	    !strcmp(nam, "C")) {
		fputs("<span style=\"font-family: monospace\">", stdout);
		closefont = "</span>";
	} else
	if (!strcmp(nam, "CI")) {
		fputs("<span style=\"font-family: monospace;"
		    " font-style: italic\">", stdout);
		closefont = "</span>";
	} else
	if (!strcmp(nam, "CB")) {
		fputs("<span style=\"font-family: monospace;"
		    " font-weight: bold\">", stdout);
		closefont = "</span>";
	} else
	if (!strcmp(nam, "H")) {
		fputs("<span style=\"font-family: sans-serif\">", stdout);
		closefont = "</span>";
	} else
	if (!strcmp(nam, "HI")) {
		fputs("<span style=\"font-family: sans-serif;"
		    " font-style: italic\">", stdout);
		closefont = "</span>";
	} else
	if (!strcmp(nam, "HB")) {
		fputs("<span style=\"font-family: sans-serif;"
		    " font-weight: bold\">", stdout);
		closefont = "</span>";
	} else
		fprintf(stderr, "%s: Unknown font name \"%s\"\n",
		    progname, nam);
	out_s(fontsize);
}

void
out_s(int i) {
	fontsize = i;
	clslig();
	closesize();
	if (fontsize == 10) return;
	printf("<span style=\"font-size: %gem\">", fontsize / 10.0);
	size_end = "</span>";
}

static void
closesize(void) {
	if (size_end) {
		fputs(size_end, stdout);
		size_end = NULL;
	}
}

void
out_n(int i) {
	clslig();
	lineheight = i;
}

void
out_V(int i) {
	static int prevv;
	int d;
	clslig();
	if (i < 0) return;
	if (i < prevv) {
		prevv = i;
		return;
	}
	if (!lineheight) return;
	prevv += lineheight;
	if ((d = i - prevv) > 0) {
		printf("<p style=\"heigh: %dpx\"></p>\n", d);
		prevv = i;
	} else {
		puts("<br>");
	}
	lineheight = 0;
	totalh = 0;
	hdec = 0;
}

void
out_h(int i) {
	if (lineheight) i -= totalh;
	else totalh += i;
	i -= hdec;
	hdec = 0;
	if (i <= 0) return;
	printf("<span style=\"display: inline-block; width: %gpt\"></span>",
	    i / 20.0);
}

void
out_w(void) {
	clslig();
	putchar(' ');
	hdec += 10 * fontsize;
}

void
out_x_f(int num, char *nam) {
	struct bst_node *n;
	if (bst_srch(&fonts, I2BST(num), &n)) {
		avl_add(&fonts, I2BST(num), S2BST(strdup(nam)));
	} else {
		free(n->data.p);
		n->data.p = strdup(nam);
	}
}

void
out_begin_link(char *l) {
	printf("<a href=\"#%s\">", l);
}

void
out_begin_ulink(char *l) {
	printf("<a href=\"%s\">", l);
}

void
out_end_link(void) {
	fputs("</a>", stdout);
}

void
out_anchor(char *a) {
	printf("<span id=\"%s\"></span>", a);
}
