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
static char sccsid[] = "@(#)api_exch.c	4.2 (Berkeley) 4/26/91";
#endif /* not lint */

#include <stdio.h>

#include "../general/general.h"

#include "api_exch.h"

static int sock;		/* Socket number */

static char whoarewe[40] = "";
#define	WHO_ARE_WE()	fprintf(stderr, "(API %s) ", whoarewe);

static enum {CONTENTION, SEND, RECEIVE } conversation;

static struct exch_exch exch_state;

static unsigned int
    my_sequence,
    your_sequence;

static char ibuffer[4000], *ibuf_next, *ibuf_last;
#define	IBUFADDED(i)		ibuf_last += (i)
#define	IBUFAVAILABLE()		(ibuf_last-ibuf_next)
#define	IBUFFER()		ibuffer
#define	IBUFFREE()		(ibuffer+sizeof ibuffer-ibuf_last-1)
#define	IBUFGETBYTES(w,l)	{ memcpy(w, ibuf_next, l); ibuf_next += l; }
#define	IBUFRESET()		(ibuf_next = ibuf_last = ibuffer)

char obuffer[4000], *obuf_next;
#define	OBUFADDBYTES(w,l)	{ memcpy(obuf_next, w, l); obuf_next += l; }
#define	OBUFAVAILABLE()		(obuf_next - obuffer)
#define	OBUFFER()		obuffer
#define	OBUFRESET()		obuf_next = obuffer
#define	OBUFROOM()		(obuffer+sizeof obuffer-obuf_next)


static int
outflush()
{
    int length = OBUFAVAILABLE();

    if (length != 0) {
	if (write(sock, OBUFFER(), length) != length) {
	    WHO_ARE_WE();
	    perror("write");
	    return -1;
	}
	OBUFRESET();
    }
    return 0;				/* All OK */
}


static int
iget(location, length)
char	*location;
int	length;
{
    int count;

    if (OBUFAVAILABLE()) {
	if (outflush() == -1) {
	    return -1;
	}
    }
    if ((count = IBUFAVAILABLE()) != 0) {
	if (count > length) {
	    count = length;
	}
	IBUFGETBYTES(location, count);
	length -= count;
	location += count;
    }
    while (length) {
	if (ibuf_next == ibuf_last) {
	    IBUFRESET();
	}
	if ((count = read(sock, IBUFFER(), IBUFFREE())) < 0) {
	    WHO_ARE_WE();
	    perror("read");
	    return -1;
	}
	if (count == 0) {
	    /* Reading past end-of-file */
	    WHO_ARE_WE();
	    fprintf(stderr, "End of file read\r\n");
	    return -1;
	}
	IBUFADDED(count);
	if (count > length) {
	    count = length;
	}
	IBUFGETBYTES(location, count);
	length -= count;
	location += count;
    }
    return 0;
}

static char *
exch_to_ascii(exch)
int exch;			/* opcode to decode */
{
    switch (exch) {
    case EXCH_EXCH_COMMAND:
	return "Command";
    case EXCH_EXCH_TYPE:
	return "Type";
    case EXCH_EXCH_TURNAROUND:
	return "Turnaround";
    case EXCH_EXCH_RTS:
	return "Request to Send";
    default:
	{
	    static char unknown[40];

	    sprintf(unknown, "(Unknown exchange 0x%02x)", exch&0xff);
	    return unknown;
	}
    }
}

/*
 * Send the exch structure, updating the sequnce number field.
 */

static int
send_state()
{
    if (OBUFROOM() < sizeof exch_state) {
	if (outflush() == -1) {
	    return -1;
	}
    }
    my_sequence = (my_sequence+1)&0xff;
    exch_state.my_sequence = my_sequence;
    exch_state.your_sequence = your_sequence;
    OBUFADDBYTES((char *)&exch_state, sizeof exch_state);
    return 0;
}

/*
 * Receive the exch structure from the other side, checking
 * sequence numbering.
 */

static int
receive_state()
{
    if (iget((char *)&exch_state, sizeof exch_state) == -1) {
	return -1;
    }
    if (conversation != CONTENTION) {
	if (exch_state.your_sequence != my_sequence) {
	    WHO_ARE_WE();
	    fprintf(stderr, "Send sequence number mismatch.\n");
	    return -1;
	}
	if (exch_state.my_sequence != ((++your_sequence)&0xff)) {
	    WHO_ARE_WE();
	    fprintf(stderr, "Receive sequence number mismatch.\n");
	    return -1;
	}
    }
    your_sequence = exch_state.my_sequence;
    return 0;
}

static int
enter_receive()
{
    switch (conversation) {
    case CONTENTION:
	exch_state.opcode = EXCH_EXCH_TURNAROUND;
	if (send_state() == -1) {
	    return -1;
	}
	if (receive_state() == -1) {
	    return -1;
	}
	if (exch_state.opcode != EXCH_EXCH_RTS) {
	    WHO_ARE_WE();
	    fprintf(stderr, "In CONTENTION state:  ");
	    if (exch_state.opcode == EXCH_EXCH_TURNAROUND) {
		fprintf(stderr,
		    "Both sides tried to enter RECEIVE state.\n");
	    } else {
		fprintf(stderr,
		    "Protocol error trying to enter RECEIVE state.\n");
	    }
	    return -1;
	}
	break;
    case SEND:
	exch_state.opcode = EXCH_EXCH_TURNAROUND;
	if (send_state() == -1) {
	    return -1;
	}
	break;
    }
    conversation = RECEIVE;
    return 0;
}

static int
enter_send()
{
    switch (conversation) {
    case CONTENTION:
	exch_state.opcode = EXCH_EXCH_RTS;
	if (send_state() == -1) {
	    return -1;
	}
	 /* fall through */
    case RECEIVE:
	if (receive_state() == -1) {
	    return -1;
	}
	if (exch_state.opcode != EXCH_EXCH_TURNAROUND) {
	    WHO_ARE_WE();
	    fprintf(stderr, "Conversation error - both sides in SEND state.\n");
	    return -1;
	}
    }
    conversation = SEND;
    return 0;
}

int
api_exch_nextcommand()
{
    if (conversation != RECEIVE) {
	if (enter_receive() == -1) {
	    return -1;
	}
    }
    if (receive_state() == -1) {
	return -1;
    }
    if (exch_state.opcode != EXCH_EXCH_COMMAND) {
	WHO_ARE_WE();
	fprintf(stderr, "Expected a %s exchange, received a %s exchange.\n",
	    exch_to_ascii(EXCH_EXCH_COMMAND), exch_to_ascii(exch_state.opcode));
	return -1;
    }
    return exch_state.command_or_type;
}


int
api_exch_incommand(command)
int command;
{
    int i;

    if ((i = api_exch_nextcommand()) == -1) {
	return -1;
    }
    if (i != command) {
	WHO_ARE_WE();
	fprintf(stderr, "Expected API command 0x%x, got API command 0x%x.\n",
				command, i);
	return -1;
    }
    return 0;
}


int
api_exch_outcommand(command)
int command;
{
    if (conversation != SEND) {
	if (enter_send() == -1) {
	    return -1;
	}
    }
    exch_state.command_or_type = command;
    exch_state.opcode = EXCH_EXCH_COMMAND;
    if (send_state() == -1) {
	return -1;
    } else {
	return 0;
    }
}


int
api_exch_outtype(type, length, location)
int
    type,
    length;
char
    *location;
{
    int netleng = length;

    if (conversation != SEND) {
	if (enter_send() == -1) {
	    return -1;
	}
    }
    exch_state.opcode = EXCH_EXCH_TYPE;
    exch_state.command_or_type = type;
    exch_state.length = netleng;
    if (send_state() == -1) {
	return -1;
    }
    if (length) {
	if (OBUFROOM() > length) {
	    OBUFADDBYTES(location, length);
	} else {
	    if (outflush() == -1) {
		return -1;
	    }
	    if (write(sock, location, length) != length) {
		WHO_ARE_WE();
		perror("write");
		return -1;
	    }
	}
    }
    return 0;
}


int
api_exch_intype(type, length, location)
int
    type,
    length;
char
    *location;
{
    int netleng = length;

    if (conversation != RECEIVE) {
	if (enter_receive() == -1) {
	    return -1;
	}
    }
    if (receive_state() == -1) {
	return -1;
    }
    if (exch_state.opcode != EXCH_EXCH_TYPE) {
	WHO_ARE_WE();
	fprintf(stderr,
	    "Expected to receive a %s exchange, received a %s exchange.\n",
	    exch_to_ascii(EXCH_EXCH_TYPE), exch_to_ascii(exch_state.opcode));
	return -1;
    }
    if (exch_state.command_or_type != type) {
	WHO_ARE_WE();
	fprintf(stderr, "Expected type 0x%x, got type 0x%x.\n",
	    type, exch_state.command_or_type);
	return -1;
    }
    if (exch_state.length != netleng) {
	fprintf(stderr, "Type 0x%x - expected length %d, received length %u.\n",
		type, length, exch_state.length);
	return -1;
    }
    if (iget(location, length) == -1) {
	return -1;
    }
    return 0;
}

int
api_exch_flush()
{
    return outflush();
}

int
api_exch_init(sock_number, ourname)
int sock_number;
char *ourname;
{
    extern char *strcpy();

    sock = sock_number;
    (void) strcpy(whoarewe, ourname);		/* For error messages */

    my_sequence = your_sequence = 0;

    conversation = CONTENTION;		/* We don't know which direction */

    IBUFRESET();
    OBUFRESET();

    return 0;
}
