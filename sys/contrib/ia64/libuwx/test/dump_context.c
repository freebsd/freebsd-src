/*
 * Copyright (c) 2002,2003 Hewlett-Packard Company
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#include <stdio.h>
#include <inttypes.h>

void dump_context(uint64_t *context)
{
    int i, j;
    uint64_t val;
    static char *names[] = {
	"ip", "sp", "bsp", "cfm",
	"rp", "psp", "pfs", "preds",
	"priunat", "bspstore", "rnat", "unat",
	"fpsr", "lc",
	"gr4", "gr5", "gr6", "gr7",
	"br1", "br2", "br3", "br4", "br5"
    };
    static int order[] = {
	0, 14,
	1, 15,
	2, 16,
	3, 17,
	7, 18,
	8, 19,
	10, 20,
	11, 21,
	12, 22,
	13, -1
    };

    for (i = 0; i < 20; i += 2) {
	j = order[i];
	if (j >= 0) {
	    val = context[j+1];
	    printf("  %-8s %08x %08x", names[j],
			(unsigned int)(val >> 32),
			(unsigned int)val);
	}
	else
	    printf("                            ");
	j = order[i+1];
	if (j >= 0) {
	    val = context[j+1];
	    printf("      %-8s %08x %08x", names[j],
			(unsigned int)(val >> 32),
			(unsigned int)val);
	}
	putchar('\n');
    }
}
