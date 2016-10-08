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
#include "char.h"
#include "main.h"
#include "bst.h"
#include "lib.h"
#include "tr_out.h"

int prevchar;

static struct bst namdat = { NULL, bst_scmp };
static struct bst chrdat = { NULL, bst_icmp };
static struct bst numdat = { NULL, bst_icmp };

void
char_open(void) {
	char *b;
	ssize_t s;
	if (!(b = file2ram(FNTDIR "/devhtml/CHAR", &s))) exit(1);
	while (1) {
		char *w, *l;
		size_t n;
		int t;
		if (!(s = lineskip(&b, s))) return;
		if (!(w = get_word(&b, &s, &n, &t))) return;
		if (!(l = get_line(&b, &s, NULL))) return;
		if (n == 1)
			avl_add(&chrdat, I2BST(*w), S2BST(l));
		else if (n != 2 && !(t & ~1))
			avl_add(&numdat, I2BST(atoi(w)), S2BST(l));
		else
			avl_add(&namdat, S2BST(w), S2BST(l));
	}
}

void
char_c(int c) {
	if (c == '`' || c == '\'') {
		if (!prevchar) prevchar = c;
		else if (c != prevchar) {
			chrout(prevchar);
			prevchar = c;
		} else {
			switch (c) {
			case '`' : fputs("&ldquo;", stdout); break;
			case '\'': fputs("&rdquo;", stdout); break;
			}
			prevchar = 0;
		}
	} else {
		clslig();
		chrout(c);
	}
	hdecr();
}

void
char_N(int i) {
	struct bst_node *n;
	clslig();
	if (!bst_srch(&numdat, I2BST(i), &n))
		fputs(n->data.p, stdout);
	else
		printf("&#%d;", i);
	hdecr();
}

void
char_C(char *s) {
	struct bst_node *n;
	clslig();
	if (!bst_srch(&namdat, S2BST(s), &n))
		fputs(n->data.p, stdout);
	else
		fprintf(stderr, "%s: Unknown character name \"%s\"\n",
		    progname, s);
	hdecr();
}

void
chrout(int c) {
	struct bst_node *n;
	if (!bst_srch(&chrdat, I2BST(c), &n))
		fputs(n->data.p, stdout);
	else
		putchar(c);
}
