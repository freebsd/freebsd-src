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
static char sccsid[] = "@(#)outbound.c	8.1 (Berkeley) 6/6/93";
#endif /* not lint */

#include <stdio.h>

#include "../general/general.h"

#include "hostctlr.h"
#include "oia.h"
#include "screen.h"
#include "../api/ebc_disp.h"

#include "../general/globals.h"
#include "externs.h"
#include "declare.h"

#define SetHighestLowest(position) { \
					if (position < Lowest) { \
					    Lowest = position; \
					} \
					if (position > Highest) { \
					    Highest = position; \
					} \
				    }


static int	LastWasTerminated = 1;	/* was "control" = 1 last time? */

/* some globals */

#if	!defined(PURE3274)
int	OutputClock;		/* what time it is */
int	TransparentClock;		/* time we were last in transparent */
#endif	/* !defined(PURE3274) */

char CIABuffer[64] = {
    0x40, 0xc1, 0xc2, 0xc3, 0xc4, 0xc5, 0xc6, 0xc7,
    0xc8, 0xc9, 0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f,
    0x50, 0xd1, 0xd2, 0xd3, 0xd4, 0xd5, 0xd6, 0xd7,
    0xd8, 0xd9, 0x5a, 0x5b, 0x5c, 0x5d, 0x5e, 0x5f,
    0x60, 0x61, 0xe2, 0xe3, 0xe4, 0xe5, 0xe6, 0xe7,
    0xe8, 0xe9, 0x6a, 0x6b, 0x6c, 0x6d, 0x6e, 0x6f,
    0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7,
    0xf8, 0xf9, 0x7a, 0x7b, 0x7c, 0x7d, 0x7e, 0x7f
};

static struct orders_def orders_def[] = ORDERS_DEF;

/*
 * init_ctlr()
 *
 *	Initialize all data from the 'data' portion to their startup values.
 */

void
init_ctlr()
{
    LastWasTerminated = 1;
    init_inbound();
    init_oia();
}


FieldInc(position)
register int	position;		/* Position in previous field */
{
    register ScreenImage *ptr;

    ptr = (ScreenImage *)memNSchr((char *)Host+position+1, ATTR_MASK,
			HighestScreen()-position, ATTR_MASK, sizeof Host[0]);
    if (ptr == 0) {
	ptr = (ScreenImage *)memNSchr((char *)Host+LowestScreen(), ATTR_MASK,
			position-LowestScreen(), ATTR_MASK, sizeof Host[0]);
	if (ptr == 0) {
	    return LowestScreen();
	}
    }
    return ptr-Host;
}

FieldDec(position)
int	position;
{
    register ScreenImage *ptr;

    ptr = (ScreenImage *)memNSchr((char *)(Host+position)-1, ATTR_MASK,
			position-LowestScreen(), ATTR_MASK, -sizeof Host[0]);
    if (ptr == 0) {
	ptr = (ScreenImage *)memNSchr((char *)Host+HighestScreen(), ATTR_MASK,
			HighestScreen()-position, ATTR_MASK, -sizeof Host[0]);
	if (ptr == 0) {
	    return LowestScreen();
	}
    }
    return ptr-Host;
}

/* Clear3270 - called to clear the screen */

void
Clear3270()
{
    ClearArray(Host);
    DeleteAllFields();		/* get rid of all fields */
    BufferAddress = SetBufferAddress(0,0);
    CursorAddress = SetBufferAddress(0,0);
    Lowest = LowestScreen();
    Highest = HighestScreen();
}

/* AddHost - called to add a character to the buffer.
 *	We use a macro in this module, since we call it so
 *	often from loops.
 *
 *	NOTE: It is a macro, so don't go around using AddHost(p, *c++), or
 *	anything similar.  (I don't define any temporary variables, again
 *	just for the speed.)
 */
void
AddHost(position, character)
int	position;
char	character;
{
#   define	AddHostA(p,c)					\
    {								\
	if (IsStartField(p)) {					\
	    DeleteField(p);					\
	    Highest = HighestScreen();				\
	    Lowest = LowestScreen();				\
	    SetHighestLowest(p);				\
	}							\
	SetHost(p, c);						\
    }
#   define	AddHost(p,c)					\
    {								\
	if (c != GetHost(p)) {					\
	    SetHighestLowest(p);				\
	}							\
	AddHostA(p,c);						\
    }	/* end of macro of AddHost */

    AddHost(position, character);
}

/* returns the number of characters consumed */
int
DataFromNetwork(Buffer, count, control)
char	*Buffer;				/* what the data is */
register int	count;				/* and how much there is */
int	control;				/* this buffer ended block? */
{
    int origCount;
    register unsigned char *buffer = (unsigned char *)Buffer;
    register int c;
    register int i;
    static int Command;
    static int Wcc;

    origCount = count;

    /*
     * If this is the start of a new data stream, then look
     * for an op-code and (possibly) a WCC.
     */
    if (LastWasTerminated) {

	if (count < 2) {
	    if (count == 0) {
		ExitString("Short count received from host!\n", 1);
		return(count);
	    }
	    Command = buffer[0];
	    switch (Command) {		/* This had better be a read command */
	    case CMD_READ_MODIFIED:
	    case CMD_SNA_READ_MODIFIED:
	    case CMD_SNA_READ_MODIFIED_ALL:
		SetOiaOnlineA(&OperatorInformationArea);
		SetOiaModified();
		DoReadModified(Command);
		break;
	    case CMD_READ_BUFFER:
	    case CMD_SNA_READ_BUFFER:
		SetOiaOnlineA(&OperatorInformationArea);
		SetOiaModified();
		DoReadBuffer();
		break;
	    default:
		{
		    char s_buffer[100];

		    sprintf(s_buffer,
			"Unexpected read command code 0x%x received.\n",
								    Command);
		    ExitString(s_buffer, 1);
		    break;
		}
	    }
	    return(1);			/* We consumed everything */
	}
	Command = buffer[0];
	Wcc = buffer[1];
	if (Wcc & WCC_RESET_MDT) {
	    i = c = WhereAttrByte(LowestScreen());
	    do {
		if (HasMdt(i)) {
		    TurnOffMdt(i);
		}
		i = FieldInc(i);
	    } while (i != c);
	}

	switch (Command) {
	case CMD_ERASE_WRITE:
	case CMD_ERASE_WRITE_ALTERNATE:
	case CMD_SNA_ERASE_WRITE:
	case CMD_SNA_ERASE_WRITE_ALTERNATE:
	    {
		int newlines, newcolumns;

		SetOiaOnlineA(&OperatorInformationArea);
		ResetOiaTWait(&OperatorInformationArea);
		SetOiaModified();
		if ((Command == CMD_ERASE_WRITE)
				|| (Command == CMD_SNA_ERASE_WRITE)) {
		    newlines = 24;
		    newcolumns = 80;
		} else {
		    newlines = MaxNumberLines;
		    newcolumns = MaxNumberColumns;
		}
		if ((newlines != NumberLines)
				|| (newcolumns != NumberColumns)) {
			/*
			 * The LocalClearScreen() is really for when we
			 * are going from a larger screen to a smaller
			 * screen, and we need to clear off the stuff
			 * at the end of the lines, or the lines at
			 * the end of the screen.
			 */
		    LocalClearScreen();
		    NumberLines = newlines;
		    NumberColumns = newcolumns;
		    ScreenSize = NumberLines * NumberColumns;
		}
		Clear3270();
#if	!defined(PURE3274)
		if (TransparentClock == OutputClock) {
		    TransStop();
		}
#endif	/* !defined(PURE3274) */
		break;
	    }

	case CMD_ERASE_ALL_UNPROTECTED:
	case CMD_SNA_ERASE_ALL_UNPROTECTED:
	    SetOiaOnlineA(&OperatorInformationArea);
	    ResetOiaTWait(&OperatorInformationArea);
	    SetOiaModified();
	    CursorAddress = HighestScreen()+1;
	    for (i = LowestScreen(); i <= HighestScreen(); i = ScreenInc(i)) {
		if (IsUnProtected(i)) {
		    if (CursorAddress > i) {
			CursorAddress = i;
		    }
		    AddHost(i, '\0');
		}
		if (HasMdt(i)) {
		    TurnOffMdt(i);
		}
	    }
	    if (CursorAddress == HighestScreen()+1) {
		CursorAddress = SetBufferAddress(0,0);
	    }
	    UnLocked = 1;
	    AidByte = 0;
	    ResetOiaSystemLocked(&OperatorInformationArea);
	    SetOiaModified();
	    TerminalIn();
	    break;
	case CMD_WRITE:
	case CMD_SNA_WRITE:
	    SetOiaOnlineA(&OperatorInformationArea);
	    ResetOiaTWait(&OperatorInformationArea);
	    SetOiaModified();
	    break;
	default:
	    {
		char s_buffer[100];

		sprintf(s_buffer,
			"Unexpected write command code 0x%x received.\n",
								Command);
		ExitString(s_buffer, 1);
		break;
	    }
	}

	count -= 2;			/* strip off command and wcc */
	buffer += 2;

    } else {
#if	!defined(PURE3274)
	if (TransparentClock == OutputClock) {
	    TransOut(buffer, count, -1, control);
	    count = 0;
	}
#endif	/* !defined(PURE3274) */
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
		if ( ! (IsStartField(BufferAddress) &&
					FieldAttributes(BufferAddress) == c)) {
		    SetHighestLowest(BufferAddress);
		    NewField(BufferAddress,c);
		}
		BufferAddress = ScreenInc(BufferAddress);
		break;
	    case ORDER_SBA:
		Ensure(2);
		i = buffer[0];
		c = buffer[1];
#if	!defined(PURE3274)
		/* Check for transparent write */
		if ((i == 0) && ((c == 0) || (c == 1) || (c == 5))) {
		    TransparentClock = OutputClock+1;
		    TransOut(buffer+2, count-2, c, control);
		    buffer += count;
		    count -= count;
		    break;
		}
#endif	/* !defined(PURE3274) */
		BufferAddress = Addr3270(i, c);
		buffer += 2;
		count -= 2;
		break;
	    case ORDER_IC:
		CursorAddress = BufferAddress;
		break;
	    /*
	     * XXX - PT is supposed to null fill the screen buffer
	     * under certain draconian conditions.
	     */
	    case ORDER_PT:
		i = BufferAddress;
		do {
		    if (IsStartField(i)) {
			if (!IsProtected(ScreenInc(i))) {
			    break;
			}
		    }
		    i = ScreenInc(i);
		} while (i != HighestScreen());
		BufferAddress = ScreenInc(i);
		break;
	    case ORDER_RA:
		Ensure(3);
		i = Addr3270(buffer[0], buffer[1]);
		if ((i < 0) || (i > HighestScreen())) {
		    char s_buffer[200];

		    sprintf(s_buffer, "tn3270:  %s%d.\n\t%s%d%s%d%s\n",
			"Invalid 3270 order 'Repeat to Address' to address ",
			i,
			"(Screen currently set to ",
			NumberLines,
			" by ",
			NumberColumns,
			".)");
		    ExitString(s_buffer, 1);
		    /*NOTREACHED*/
		}
		c = buffer[2];
		if (c == ORDER_GE) {
		    Ensure(4);
		    c = buffer[3];
		    buffer += 4;
		    count -= 4;
		} else {
		    buffer += 3;
		    count -= 3;
		}
		do {
		    AddHost(BufferAddress, ebc_disp[c]);
		    BufferAddress = ScreenInc(BufferAddress);
		} while (BufferAddress != i);
		break;
	    case ORDER_EUA:    /* (from [here,there), ie: half open interval] */
		Ensure(2);
		/*
		 * Compiler error - msc version 4.0:
		 *			"expression too complicated".
		 */
		i = WhereAttrByte(BufferAddress);
		c = FieldAttributes(i);
		i = Addr3270(buffer[0], buffer[1]);
		if ((i < 0) || (i > HighestScreen())) {
		    char s_buffer[200];

		    sprintf(s_buffer, "tn3270:  %s%d.\n\t%s%d%s%d%s\n",
			"Invalid 3270 order 'Erase Unprotected to Address' to address ",
			i,
			"(Screen currently set to ",
			NumberLines,
			" by ",
			NumberColumns,
			".)");
		    ExitString(s_buffer, 1);
		    /*NOTREACHED*/
		}
		do {
		    if (IsStartField(BufferAddress)) {
			c = FieldAttributes(BufferAddress);
		    } else if (!IsProtectedAttr(BufferAddress, c)) {
			AddHost(BufferAddress, 0);
		    }
		    BufferAddress = ScreenInc(BufferAddress);
		} while (i != BufferAddress);
		buffer += 2;
		count -= 2;
		break;
	    case ORDER_GE:
		Ensure(2);
		/* XXX Should do SOMETHING! */
		/* XXX buffer += 0; */
		/* XXX count -= 0; *//* For now, just use this character */
		break;
	    case ORDER_YALE:		/* special YALE defined order */
		Ensure(2);	/* need at least two characters */
		if (*buffer == 0x5b) {
		    i = OptOrder(buffer+1, count-1, control);
		    if (i == 0) {
			return(origCount-(count+1));	/* come here again */
		    } else {
			buffer += 1 + i;
			count  -= (1 + i);
		    }
		}
		break;
	    default:
		{
		    char s_buffer[100];
		    static struct orders_def unk_order
						= { 0, "??", "(unknown)" };
		    struct orders_def *porder = &unk_order;
		    int s_i;

		    for (s_i = 0; s_i <= highestof(orders_def); s_i++) {
			if (orders_def[s_i].code == c) {
			    porder = &orders_def[s_i];
			    break;
			}
		    }
		    sprintf(s_buffer,
			"Unsupported order '%s' (%s, 0x%x) received.\n",
			porder->long_name, porder->short_name, c);
		    ExitString(s_buffer, 1);
		    /*NOTREACHED*/
		}
	    }
	    if (count < 0) {
		count = 0;
	    }
	} else {
	    /* Data comes in large clumps - take it all */
	    i = BufferAddress;
	    AddHostA(i, ebc_disp[c]);
	    SetHighestLowest(i);
	    i = ScreenInc(i);
	    c = *buffer;
	    while (count && !IsOrder(c)) {
		AddHostA(i, ebc_disp[c]);
		i = ScreenInc(i);
		if (i == LowestScreen()) {
		    SetHighestLowest(HighestScreen());
		}
		count--;
		buffer++;
		c = *buffer;
	    }
	    SetHighestLowest(i);
	    BufferAddress = i;
	}
    }
    if (count == 0) {
	if (control) {
#if	!defined(PURE3274)
	    OutputClock++;		/* time rolls on */
#endif	/* !defined(PURE3274) */
	    if (Wcc & WCC_RESTORE) {
#if	!defined(PURE3274)
		if (TransparentClock != OutputClock) {
		    AidByte = 0;
		}
#else	/* !defined(PURE3274) */
		AidByte = 0;
#endif	/* !defined(PURE3274) */
		UnLocked = 1;
		ResetOiaSystemLocked(&OperatorInformationArea);
		SetOiaModified();
		SetPsModified();
		TerminalIn();
	    }
	    if (Wcc & WCC_ALARM) {
		RingBell((char *)0);
	    }
	}
	LastWasTerminated = control;	/* state for next time */
	return(origCount);
    } else {
	return(origCount-count);
    }
}

/*
 * Init3270()
 *
 * Initialize any 3270 (controller) variables to an initial state
 * in preparation for accepting a connection.
 */

void
Init3270()
{
    int i;

    OptInit();		/* initialize mappings */

    ClearArray(Host);

    ClearArray(Orders);
    for (i = 0; i <= highestof(orders_def); i++) {
	Orders[orders_def[i].code] = 1;
    }

    DeleteAllFields();		/* Clear screen */
    Lowest = HighestScreen()+1;
    Highest = LowestScreen()-1;
    CursorAddress = BufferAddress = SetBufferAddress(0,0);
    UnLocked = 1;
#if	!defined(PURE3274)
    OutputClock = 1;
    TransparentClock = -1;
#endif	/* !defined(PURE3274) */
    SetOiaReady3274(&OperatorInformationArea);
}


void
Stop3270()
{
    ResetOiaReady3274(&OperatorInformationArea);
}
