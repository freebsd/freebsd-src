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
static char sccsid[] = "@(#)mkhits.c	4.2 (Berkeley) 4/26/91";
#endif /* not lint */

/*
 * This program scans a file which describes a keyboard.  The output
 * of the program is a series of 'C' declarations which describe a
 * mapping between (scancode, shiftstate, altstate) and 3270 functions,
 * characters, and AIDs.
 *
 * The format of the input file is as follows:
 *
 * keynumber [ scancode [ unshifted [ shifted [ alted [ shiftalted ] ] ] ] ]
 *
 * keynumber is in decimal, and starts in column 1.
 * scancode is hexadecimal.
 * unshifted, etc. - these are either a single ascii character,
 *			or the name of a function or an AID-generating key.
 *
 * all fields are separated by a single space.
 */

#include <stdio.h>
#if	defined(unix)
#include <strings.h>
#else	/* defined(unix) */
#include <string.h>
#endif	/* defined(unix) */
#include <ctype.h>
#include "../ctlr/function.h"

#include "dohits.h"


int
main(argc, argv)
int	argc;
char	*argv[];
{
    int scancode;
    int empty;
    int i;
    struct hits *ph;
    struct Hits *Ph;
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

    printf("struct hits hits[] = {\n");
    empty = 0;
    scancode = -1;
    for (Ph = Hits; Ph < Hits+(sizeof Hits/sizeof Hits[0]); Ph++) {
	ph = &Ph->hits;
	scancode++;
	if ((ph->hit[0].ctlrfcn == undefined)
		&& (ph->hit[1].ctlrfcn == undefined)
		&& (ph->hit[2].ctlrfcn == undefined)
		&& (ph->hit[3].ctlrfcn == undefined)) {
	    empty++;
	    continue;
	} else {
	    while (empty) {
		printf("\t{ 0, {  {undefined}, {undefined}");
		printf(", {undefined}, {undefined}  } },\n");
		empty--;
	    }
	}
	printf("\t{ %d, {\t/* 0x%02x */\n\t", ph->keynumber, scancode);
	for (i = 0; i < 4; i++) {
	    printf("\t{ ");
	    switch (ph->hit[i].ctlrfcn) {
	    case undefined:
		printf("undefined");
		break;
	    case FCN_CHARACTER:
		printf("FCN_CHARACTER, 0x%02x", ph->hit[i].code);
		break;
	    case FCN_AID:
		printf("FCN_AID, %s", Ph->name[i]);
		break;
	    case FCN_NULL:
	    default:
		if ((Ph->name[i] != 0)
				    && (strcmp(Ph->name[i], "FCN_NULL") != 0)) {
		    printf("%s", Ph->name[i]);
		} else {
		    printf("undefined");
		}
		break;
	    }
	    printf(" },\n\t");
	}
	printf("} },\n");
    }
    printf("};\n");
    return 0;
}
