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
static char copyright[] =
"@(#) Copyright (c) 1988, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
static char sccsid[] = "@(#)prt3270.c	8.1 (Berkeley) 6/6/93";
#endif /* not lint */

#if defined(unix)
#endif
#include <stdio.h>
#include <ctype.h>

#include "../general/general.h"

#include "../api/asc_ebc.h"
#include "../ctlr/hostctlr.h"
#include "../ctlr/screen.h"
#include "../ctlr/function.h"
#include "../api/astosc.h"
#include "../general/globals.h"

#include "../ctlr/kbd.out"


int NumberColumns = 80;

int direction;

int column = 1;
int indenting = 0;
int direction = '?';

unsigned char printBuffer[200], *print = printBuffer;

#define	ColsLeft()	(79-column)	/* A little room for error */


void
putSpace()
{
    extern void Column1();
    unsigned char *ourPrint = print;

    print = printBuffer;		/* For mutual calls */
    *ourPrint = 0;
    if (ColsLeft() < 0) {
	Column1();
    }
    if (column != (indenting*8+1)) {
	putchar(' ');
    } else {
	int i;

	putchar(direction);
	putchar(' ');
	for (i = 0; i < indenting; i++) {
	    putchar('\t');
	}
    }
    printf("%s", printBuffer);
    column += strlen(printBuffer);
}

void
Column1()
{
    if (print != printBuffer) {
	putSpace();
    }
    if (column != (indenting*8+1)) {
	putchar('\n');
	column = indenting*8+1;
    }
}

void
Indent()
{
    if ((column != (indenting*8+1)) || (print != printBuffer)) {
	Column1();
    }
    indenting++;
    column = indenting*8+1;
}

void
Undent()
{
    if ((column != (indenting*8+1)) || (print != printBuffer)) {
	Column1();
    }
    indenting--;
    if (indenting < 0) {
	fflush(stdout);
	fprintf(stderr, "INTERNAL ERROR: indenting < 0.\n");
	fflush(stderr);
    } else {
	column = indenting*8+1;
    }
}

void
putChar(character)
int	character;
{
    *print++ = character;
    column++;
}

void
putstr(s)
char *s;
{
    while (*s) {
	putChar(*s++);
    }
}

void
put2hex(i)
int i;
{
    char place[40];

    sprintf(place, "%02x", i);
    putstr(place);
}


void
putdecimal(i)
int i;
{
    char place[40];

    sprintf(place, "%d", i);
    putstr(place);
}

void
puthex(i)
int i;
{
    char place[40];

    sprintf(place, "%x", i);
    putstr(place);
}

void
putEChar(character)
int character;
{
    putChar(ebc_asc[character]);
    if (ColsLeft() < 10) {
	Column1();
    }
}

void
PrintAid(i)
int	i;
{
    struct astosc *this;

    for (this = &astosc[0]; this <= &astosc[highestof(astosc)]; this++) {
	if (this->function == FCN_AID) {
	    int j;

	    switch (this->shiftstate) {
	    case 0:
		j = 0;
		break;
	    case SHIFT_UPSHIFT:
		j = 1;
		break;
	    case SHIFT_ALT:
		j = 2;
		break;
	    case (SHIFT_UPSHIFT|SHIFT_ALT):
		j = 3;
		break;
	    default:
		fprintf(stderr, "Bad shiftstate 0x%x.\n", this->shiftstate);
		exit(1);
	    }
	    if (hits[this->scancode].hit[j].code == i) {
		putstr(this->name);
		return;
	    }
	}
    }

    putstr("Unknown AID 0x");
    put2hex(i);
}

void
PrintAddr(i)
int	i;
{
    if (ColsLeft() < 9) {
	Column1();
    }
    putChar('(');
    putdecimal(ScreenLine(i));
    putChar(',');
    putdecimal(ScreenLineOffset(i));
    putChar(')');
}


/* returns the number of characters consumed */
int
DataFromNetwork(buffer, count, control)
register unsigned char	*buffer;		/* what the data is */
register int	count;				/* and how much there is */
int	control;				/* this buffer ended block? */
{
    int origCount;
    register int c;
    register int i;
    static int Command;
    static int Wcc;
    static int	LastWasTerminated = 1;	/* was "control" = 1 last time? */

    if (count == 0) {
	Column1();
	return 0;
    }

    origCount = count;

    if (LastWasTerminated) {

	if (count < 2) {
	    if (count == 0) {
		fflush(stdout);
		fprintf(stderr, "Short count received from host!\n");
		fflush(stderr);
		return(count);
	    }
	    Command = buffer[0];
	    switch (Command) {		/* This had better be a read command */
	    case CMD_READ_MODIFIED:
		putstr("read_modified command\n");
		break;
	    case CMD_SNA_READ_MODIFIED:
		putstr("sna_read_modified command\n");
		break;
	    case CMD_SNA_READ_MODIFIED_ALL:
		putstr("sna_read_modified_all command\n");
		break;
	    case CMD_READ_BUFFER:
		putstr("read_buffer command\n");
		break;
	    case CMD_SNA_READ_BUFFER:
		putstr("sna_read_buffer command\n");
		break;
	    default:
		break;
	    }
	    return(1);			/* We consumed everything */
	}
	Command = buffer[0];
	Wcc = buffer[1];
	switch (Command) {
	case CMD_ERASE_WRITE:
	    putstr("erase write command ");
	    break;
	case CMD_ERASE_WRITE_ALTERNATE:
	    putstr("erase write alternate command ");
	    break;
	case CMD_SNA_ERASE_WRITE:
	    putstr("sna erase write command ");
	    break;
	case CMD_SNA_ERASE_WRITE_ALTERNATE:
	    putstr("erase write alternate command ");
	    break;
	case CMD_ERASE_ALL_UNPROTECTED:
	    putstr("erase all unprotected command ");
	    break;
	case CMD_SNA_ERASE_ALL_UNPROTECTED:
	    putstr("sna erase write command ");
	    break;
	case CMD_WRITE:
	    putstr("write command ");
	    break;
	case CMD_SNA_WRITE:
	    putstr("sna write command ");
	    break;
	default:
	    putstr("Unexpected command code 0x");
	    puthex(Command);
	    putstr(" received.");
	    Column1();
	    break;
	}
	putstr("WCC is 0x");
	puthex(Wcc);
	Column1();

	count -= 2;			/* strip off command and wcc */
	buffer += 2;

    }
    LastWasTerminated = 0;		/* then, reset at end... */

    while (count) {
	count--;
	c = *buffer++;
	if (IsOrder(c)) {
	    /* handle an order */
	    switch (c) {
#		define Ensure(x)	if (count < x) { \
					    if (!control) { \
						return(origCount-(count+1)); \
					    } else { \
						/* XXX - should not occur */ \
						count = 0; \
						break; \
					    } \
					}
	    case ORDER_SF:
		Ensure(1);
		c = *buffer++;
		count--;
		putstr("SF (0x");
		put2hex(c);
		putstr(") ");
		break;
	    case ORDER_SBA:
		Ensure(2);
		i = buffer[0];
		c = buffer[1];
		buffer += 2;
		count -= 2;
		putstr("SBA to ");
		PrintAddr(Addr3270(i,c));
		putSpace();
		break;
	    case ORDER_IC:
		putstr("IC");
		putSpace();
		break;
	    case ORDER_PT:
		putstr("PT");
		putSpace();
		break;
	    case ORDER_RA:
		Ensure(3);
		i = Addr3270(buffer[0], buffer[1]);
		c = buffer[2];
		buffer += 3;
		count -= 3;
		putstr("RA to ");
		PrintAddr(i);
		putstr(" of 0x");
		put2hex(c);
		putSpace();
		break;
	    case ORDER_EUA:    /* (from [here,there), ie: half open interval] */
		Ensure(2);
		putstr("EUA to ");
		PrintAddr(Addr3270(buffer[0], buffer[1]));
		putSpace();
		buffer += 2;
		count -= 2;
		break;
	    case ORDER_YALE:		/* special YALE defined order */
		Ensure(2);	/* need at least two characters */
		putstr("YALE order");
		putSpace();
		break;
	    default:
		putstr("UNKNOWN ORDER: 0x");
		put2hex(c);
		putSpace();
		break;
	    }
	    if (count < 0) {
		count = 0;
	    }
	} else {
	    /* Data comes in large clumps - take it all */
	    putstr("DATA:");
	    Indent();
	    putEChar(c);
	    c = *buffer;
	    while (count && !IsOrder(c)) {
		putEChar(c);
		count--;
		buffer++;
		c = *buffer;
	    }
	    Undent();
	}
    }
    LastWasTerminated = control;
    return origCount - count;
}

int
DataToNetwork(buffer, count, control)
unsigned char *buffer;
int count;
int control;
{
#define	NEED_AID	0
#define	JUST_GOT_AID	1
#define	DATA		2
#define	DATA_CONTINUE	3
    static int state = NEED_AID;
    static int aid;
    int origCount = count;

    if (count == 0) {
	if (control) {
	    state = NEED_AID;
	}
	Column1();
	return 0;
    }

    switch (state) {
    case NEED_AID:
	aid = buffer[0];
	buffer++;
	count--;
	PrintAid(aid);
	putSpace();
	if (aid == AID_TREQ) {
	    state = DATA;
	} else {
	    state = JUST_GOT_AID;
	}
	return origCount - count + DataToNetwork(buffer, count, control);
    case JUST_GOT_AID:
	Ensure(2);
	PrintAddr(Addr3270(buffer[0], buffer[1]));
	putSpace();
	buffer += 2;
	count -= 2;
	state = DATA;
	return origCount - count + DataToNetwork(buffer, count, control);
    case DATA:
    case DATA_CONTINUE:
	while (count) {
	    if (*buffer == ORDER_SBA) {
		if (state == DATA_CONTINUE) {
		    Undent();
		    state = DATA;
		}
		putstr("SBA ");
		PrintAddr(Addr3270(buffer[1], buffer[2]));
		putSpace();
		buffer += 3;
		count -= 3;
	    } else {
		if (state == DATA) {
		    putstr("DATA:");
		    Indent();
		    state = DATA_CONTINUE;
		}
		putEChar(*buffer);
		buffer++;
		count--;
	    }
	}
	if (control) {
	    if (state == DATA_CONTINUE) {
		Undent();
	    }
	    state = NEED_AID;
	}
	return origCount-count;
    }
}

int
GetXValue(c)
int	c;
{
    if (!isascii(c)) {
	fflush(stdout);
	fprintf(stderr, "Non-hex digit 0x%x.\n");
	fflush(stderr);
	return 0;
    } else {
	if (islower(c)) {
	    return (c-'a')+10;
	} else if (isupper(c)) {
	    return (c-'A')+10;
	} else {
	    return c-'0';
	}
    }
}

unsigned char outbound[8192], inbound[8192],
	*outnext = outbound, *innext = inbound, *p = 0;

void
termblock(old, new, control)
int old,
	new;		/* old and new directions */
{
    int count;

    if (p) {
	if (old == '<') {
	    outnext = p;
	    count = DataFromNetwork(outbound, outnext-outbound, control);
	    if (outbound+count == outnext) {
		outnext = outbound;
	    } else {
		memcpy(outbound, outbound+count, outnext-(outbound+count));
		outnext = outbound+count;
	    }
	} else {
	    innext = p;
	    count = DataToNetwork(inbound, innext-inbound, control);
	    if (inbound+count == innext) {
		innext = inbound;
	    } else {
		memcpy(inbound, inbound+count, innext-(inbound+count));
		innext = inbound+count;
	    }
	}
    }
    if (new == '<') {
	p = outnext;
    } else if (new == '>') {
	p = innext;
    } else {
	fprintf(stderr, "Bad direction character '%c'.\n", new);
	exit(1);
    }
}

main()
{
    int location;
    char new;
    int c, c1;

    memset(Orders, 0, sizeof Orders);
    Orders[ORDER_SF] = Orders[ORDER_SBA] = Orders[ORDER_IC]
	    = Orders[ORDER_PT] = Orders[ORDER_RA] = Orders[ORDER_EUA]
	    = Orders[ORDER_YALE] = 1;

    while (scanf("%c 0x%x\t", &new, &location) != EOF) {
	if (new != direction) {
	    termblock(direction, new, 0);
	    direction = new;
	}
	while (((c = getchar()) != EOF) && (c != '\n') && (isxdigit(c))) {
#define	NORMAL	0
#define	GOT0XFF	0xff
	    static int state = NORMAL;

	    c1 = getchar();
	    c = (GetXValue(c) << 4) + GetXValue(c1);
	    switch (state) {
	    case NORMAL:
		if (c == 0xff) {
		    state = GOT0XFF;
		} else {
		    *p++ = c;
		}
		break;
	    case GOT0XFF:
		if (c == 0xef) {
		    termblock(direction, direction, 1);
		} else {
		    *p++ = 0xff;
		    *p++ = c;
		}
		state = NORMAL;
	    }
	}
    }
    return 0;
}
