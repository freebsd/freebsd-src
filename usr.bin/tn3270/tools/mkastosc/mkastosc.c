/*-
 * Copyright (c) 1988 The Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef lint
char copyright[] =
"@(#) Copyright (c) 1988 The Regents of the University of California.\n\
 All rights reserved.\n";
#endif /* not lint */

#ifndef lint
static char sccsid[] = "@(#)mkastosc.c	4.2 (Berkeley) 4/26/91";
#endif /* not lint */

#include <stdio.h>
#if	defined(unix)
#include <strings.h>
#else	/* defined(unix) */
#include <string.h>
#endif	/* defined(unix) */
#include <ctype.h>

#include "../general/general.h"
#include "../ctlr/function.h"

#include "dohits.h"

static struct tbl {
    unsigned char
	scancode,
	used;
    char
	*shiftstate;
} tbl[128];

int
main(argc, argv)
int	argc;
char	*argv[];
{
    int scancode;
    int asciicode;
    int empty;
    int i;
    int c;
    int found;
    struct hits *ph;
    struct Hits *Ph;
    struct thing *this;
    struct thing **attable;
    struct tbl *Pt;
    static char *shiftof[] =
	    { "0", "SHIFT_UPSHIFT", "SHIFT_ALT", "SHIFT_ALT|SHIFT_UPSHIFT" };
    char *aidfile = 0, *fcnfile = 0;

    if (argc > 1) {
	if (argv[1][0] != '-') {
	    aidfile = argv[1];
	}
    }
    if (argc > 2) {
	if (argv[2][0] != '-') {
	    fcnfile = argv[2];
	}
    }

    dohits(aidfile, fcnfile);		/* Set up "Hits" */

    printf("/*\n");
    printf(" * Ascii to scancode conversion table.  First\n");
    printf(" * 128 bytes (0-127) correspond with actual Ascii\n");
    printf(" * characters; the rest are functions from ctrl/function.h\n");
    printf(" */\n");
    /* Build the ascii part of the table. */
    for (Ph = Hits, scancode = 0; Ph <= Hits+highestof(Hits);
							Ph++, scancode++) {
	ph = &Ph->hits;
	for (i = 0; i < 4; i++) {
	    if (ph->hit[i].ctlrfcn == FCN_CHARACTER) {
		c = Ph->name[i][0];	/* "name" of this one */
		if (tbl[c].used == 0) {
		    tbl[c].used = 1;
		    tbl[c].shiftstate = shiftof[i];
		    tbl[c].scancode = scancode;
		}
	    }
	}
    }
    /* Now, output the table */
    for (Pt = tbl, asciicode = 0; Pt <= tbl+highestof(tbl); Pt++, asciicode++) {
	if (Pt->used == 0) {
	    if (isprint(asciicode) && (asciicode != ' ')) {
		fprintf(stderr, "Unable to produce scancode sequence for");
		fprintf(stderr, " ASCII character [%c]!\n", asciicode);
	    }
	    printf("\t{ 0, 0, undefined, 0 },\t");
	} else {
	    printf("\t{ 0x%02x, %s, FCN_CHARACTER, 0 },",
					Pt->scancode, Pt->shiftstate);
	}
	printf("\t/* 0x%x", asciicode);
	if (isprint(asciicode)) {
	    printf(" [%c]", asciicode);
	}
	printf(" */\n");
    }
		

    for (attable = &table[0]; attable <= &table[highestof(table)]; attable++) {
	for (this = *attable; this; this = this->next) {
	    Ph = this->hits;
	    if (Ph == 0) {
		continue;
	    }
	    for (i = 0; i < 4; i++) {
		if ((Ph->name[i] != 0) &&
			(Ph->name[i][0] == this->name[0]) &&
			(strcmp(Ph->name[i], this->name) == 0)) {
		    printf("\t{ 0x%02x, %s, ",
				Ph-Hits, shiftof[i]);
		    if (memcmp("AID_", this->name, 4) == 0) {	/* AID key */
			printf("FCN_AID, ");
		    } else {
			printf("%s, ", Ph->name[i]);
		    }
		    if (memcmp("PF", this->name+4, 2) == 0) {
			printf("\"PFK%s\" },\n", Ph->name[i]+4+2);
		    } else {
			printf("\"%s\" },\n", Ph->name[i]+4);
		    }
		}
	    }
	}
    }

    return 0;
}
