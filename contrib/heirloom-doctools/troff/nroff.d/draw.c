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
#include <string.h>
#include "tdef.h"
#include "ext.h"
#include "bst.h"
#include "draw.h"
#include "pt.h"
#include "tw.h"

int ndraw;

extern int tlp, utf8;

#define BST_VAL(c)       ((union bst_val)(long)(c))
#define XY2KEY(row, col) ((union bst_val)(uint64_t)(((uint64_t)(row) << 32) \
    | (col)))
#define KEY2X(l)         (l & 0xffffffff)
#define KEY2Y(l)         ((l >> 32) & 0xffffffff)
#define UTF_TLP(u, a)    (utf8 ? setuc0(u) : a)
#define DASH             UTF_TLP(0x2500, '-')
#define BAR              UTF_TLP(0x2502, '|')
#define DOWN_RIGHT       "\\U'250C'"
#define DOWN_LEFT        "\\U'2510'"
#define DOWN_HOR         "\\U'252C'"
#define UP_RIGHT         "\\U'2514'"
#define UP_LEFT          "\\U'2518'"
#define UP_HOR           "\\U'2534'"

static void free_node(struct bst_node *);
static int coordcmp(union bst_val, union bst_val);
static void postproc(struct bst_node *);
static void chkcoord(uint64_t);
static void drawat(int, int, char *);

struct bst coords = {
	NULL,
	coordcmp
};

void
storechar(tchar c, int row, int col) {
	long s = cbits(c);
	if (s != DASH && s != BAR) return;
	if (bst_srch(&coords, XY2KEY(row, col), NULL))
		if (avl_add(&coords, XY2KEY(row, col), BST_VAL(c)))
			fprintf(stderr,
			    "BST: Unexpected internal error\n");
}

void
npic(int start) {
	ndraw = start && (tlp || utf8);
	if (coords.root) {
		postproc(coords.root);
		free_node(coords.root);
		coords.root = NULL;
	}
}

static void
free_node(struct bst_node *n) {
	if (n->left ) free_node(n->left );
	if (n->right) free_node(n->right);
	free(n);
}

static int
coordcmp(union bst_val a, union bst_val b) {
	return a.l < b.l ? -1 :
	       a.l > b.l ?  1 :
	                    0 ;
}

static void
postproc(struct bst_node *n) {
	if (n->left ) postproc(n->left );
	chkcoord(n->key.u64);
	if (n->right) postproc(n->right);
}

static void
chkcoord(uint64_t xy) {
	unsigned long e = 0;
	int i, c = 0;
	int x = KEY2X(xy);
	int y = KEY2Y(xy);
	for (i = 0; i < 9; i++) {
		struct bst_node *n;
		int xi = x + HOR  * (i % 3 - 1);
		int yi = y + VERT * (i / 3 - 1);
		if (i != 4) e <<= 4;
		if (xi >= 0 && yi >= 0 &&
		    !bst_srch(&coords, XY2KEY(yi, xi), &n)) {
			long s = cbits(n->data.l);
			if        (s == BAR ) {
				if (i == 4) c  = 1;
				else        e |= 1;
			} else if (s == DASH) {
				if (i == 4) c  = 2;
				else        e |= 2;
			} else {
				if (i == 4) return;
			}
		}
	}
	switch (e) {
	case 0x00002010: drawat(x, y, DOWN_RIGHT); break;
	case 0x00020010: drawat(x, y, DOWN_LEFT ); break;
	case 0x00022010: drawat(x, y, DOWN_HOR  ); break;
	case 0x11012220:
	case 0x22202000:
	case 0x01002200:
	case 0x01002220:
	case 0x01002000: drawat(x, y, UP_RIGHT  ); break;
	case 0x01121222:
	case 0x01020000: drawat(x, y, UP_LEFT   ); break;
	case 0x11022000:
	case 0x01122000:
	case 0x01022000: drawat(x, y, UP_HOR    ); break;
	case 0x00020001: /* empty upper right */
	case 0x02220001:
	case 0x00120001:
	case 0x01220001:
	case 0x11220001:
	case 0x10020001:
	case 0x10010001:
		if (c == 2) drawat(x + HOR, y, DOWN_LEFT);
		break;
	case 0x01020001:
		drawat(x      , y, UP_HOR   );
		drawat(x + HOR, y, DOWN_LEFT);
		break;
	case 0x00002100: /* empty upper left */
	case 0x00102100:
	case 0x00101100:
		if (c == 2) drawat(x - HOR, y, DOWN_RIGHT);
		break;
	}
}

static void
drawat(int x, int y, char *s) {
	size_t l = 0;
	char buf[100];
	x -= po + in + ne;
	y -= numtab[NL].val;
	cpushback(".sp -1\n");
	if (x) {
		snprintf(buf, sizeof(buf), "\\h'%du'", x);
		l = strlen(buf);
	}
	if (y) {
		snprintf(buf + l, sizeof(buf) - l, "\\v'%du'", y);
		l = strlen(buf);
	}
	snprintf(buf + l, sizeof(buf) - l, "%s\n", utf8 ? s : "+");
	cpushback(buf);
}
