/*-
 * Copyright (c) 1988, 1993
 *	The Regents of the University of California.  All rights reserved.
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
static char sccsid[] = "@(#)dohits.c	8.1 (Berkeley) 6/6/93";
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
#include "../general/general.h"
#include "../api/asc_ebc.h"
#include "../api/ebc_disp.h"
#include "../ctlr/function.h"

#include "dohits.h"

struct Hits Hits[256];		/* one for each of 0x00-0xff */

struct thing *table[100];

extern char *malloc();

unsigned int
dohash(seed, string)
unsigned int seed;
char *string;
{
    register unsigned int i = seed;
    register unsigned char c;

    while (c = *string++) {
	if (c >= 0x60) {
	    c -= (0x60+0x20);
	} else {
	    c -= 0x20;
	}
	i = (i>>26) + (i<<6) + (c&0x3f);
    }
    return i;
}

void
add(first, second, value)
char *first, *second;
int value;
{
    struct thing **item, *this;

    item = &firstentry(second);
    this = (struct thing *) malloc(sizeof *this);
    memset((void*)this,0,sizeof *this);
    this->next = *item;
    *item = this;
    this->value = value;
    strcpy(this->name, first);
    strcpy(this->name+strlen(this->name), second);
}

void
scanwhite(file, prefix)
char *file,		/* Name of file to scan for whitespace prefix */
     *prefix;		/* prefix of what should be picked up */
{
    FILE *ourfile;
    char compare[100];
    char what[100], value[100];
    char line[200];

    sprintf(compare, " %s%%[^,\t \n]", prefix);
    if ((ourfile = fopen(file, "r")) == NULL) {
	perror("fopen");
	exit(1);
    }
    while (!feof(ourfile)) {
	if (fscanf(ourfile, compare,  what) == 1) {
	    add(prefix, what, 0);
	}
	do {
	    if (fgets(line, sizeof line, ourfile) == NULL) {
		if (!feof(ourfile)) {
		    perror("fgets");
		}
		break;
	    }
	} while (line[strlen(line)-1] != '\n');
    }
}

void
scandefine(file, prefix)
char *file,		/* Name of file to scan for #define prefix */
     *prefix;		/* prefix of what should be picked up */
{
    FILE *ourfile;
    char compare[100];
    char what[100], value[100];
    char line[200];
    int whatitis;

    sprintf(compare, "#define %s%%s %%s", prefix);
    if ((ourfile = fopen(file, "r")) == NULL) {
	perror("fopen");
	exit(1);
    }
    while (!feof(ourfile)) {
	if (fscanf(ourfile, compare,  what, value) == 2) {
	    if (value[0] == '0') {
		if ((value[1] == 'x') || (value[1] == 'X')) {
		    sscanf(value, "0x%x", &whatitis);
		} else {
		    sscanf(value, "0%o", &whatitis);
		}
	    } else {
		sscanf(value, "%d", &whatitis);
	    }
	    add(prefix, what, whatitis);
	}
	do {
	    if (fgets(line, sizeof line, ourfile) == NULL) {
		if (!feof(ourfile)) {
		    perror("fgets");
		}
		break;
	    }
	} while (line[strlen(line)-1] != '\n');
    }
}

char *savechr(c)
unsigned char c;
{
    char *foo;

    foo = malloc(sizeof c);
    if (foo == 0) {
	fprintf(stderr, "No room for ascii characters!\n");
	exit(1);
    }
    *foo = c;
    return foo;
}

char *
doit(hit, type, hits)
struct hit *hit;
unsigned char *type;
struct Hits *hits;
{
    struct thing *this;

    hit->ctlrfcn = FCN_NULL;
    if (type[0] == 0) {
	return 0;
    }
    if (type[1] == 0) {		/* character */
	hit->ctlrfcn = FCN_CHARACTER;
	hit->code = ebc_disp[asc_ebc[type[0]]];
	return savechr(*type);		/* The character is the name */
    } else {
	for (this = firstentry(type); this; this = this->next) {
	    if ((type[0] == this->name[4])
				&& (strcmp(type, this->name+4) == 0)) {
		this->hits = hits;
		if (this->name[0] == 'F') {
		    hit->ctlrfcn = FCN_NULL;	/* XXX */
		} else {
		    hit->ctlrfcn = FCN_AID;
		}
		return this->name;
	    }
	}
	fprintf(stderr, "Error: Unknown type %s.\n", type);
	return 0;
    }
}


void
dohits(aidfile, fcnfile)
char	*aidfile, *fcnfile;
{
    unsigned char plain[100], shifted[100], alted[100], shiftalted[100];
    unsigned char line[200];
    int keynumber, scancode;
    int empty;
    int i;
    struct hit *hit;
    struct hits *ph;
    struct Hits *Ph;

    memset((char *)Hits, 0, sizeof Hits);

    /*
     * First, we read "host3270.h" to find the names/values of
     * various AID; then we read kbd3270.h to find the names/values
     * of various FCNs.
     */

    if (aidfile == 0) {
	aidfile = "../ctlr/hostctlr.h";
    }
    scandefine(aidfile, "AID_");
    if (fcnfile == 0) {
        fcnfile = "../ctlr/function.h";
    }
    scanwhite(fcnfile, "FCN_");

    while (fgets(line, sizeof line, stdin) != NULL) {
	if (!isdigit(line[0])) {
	    continue;
	}
	plain[0] = shifted[0] = alted[0] = shiftalted[0] = 0;
	keynumber = -1;
	scancode = -1;
	(void) sscanf(line, "%d %x %s %s %s %s", &keynumber,
		    &scancode, plain, shifted, alted, shiftalted);
	if ((keynumber == -1) || (scancode == -1)
		|| ((plain[0] == 0)
		    && (shifted[0] == 0)
		    && (alted[0] == 0)
		    && (shiftalted[0] == 0))) {
	    continue;
	}
	if (scancode >= 256) {
	    fprintf(stderr,
		"Error: scancode 0x%02x for keynumber %d\n", scancode,
		    keynumber);
	    break;
	}
	if (Hits[scancode].hits.hit[0].ctlrfcn != undefined) {
	    fprintf(stderr,
		"Error: duplicate scancode 0x%02x for keynumber %d\n",
		    scancode, keynumber);
	    break;
	}
	hit = Hits[scancode].hits.hit;
	Hits[scancode].hits.keynumber = keynumber;
	Hits[scancode].name[0] = doit(hit, plain, &Hits[scancode]);
	Hits[scancode].name[1] = doit(hit+1, shifted, &Hits[scancode]);
	Hits[scancode].name[2] = doit(hit+2, alted, &Hits[scancode]);
	Hits[scancode].name[3] = doit(hit+3, shiftalted, &Hits[scancode]);
    }
}
