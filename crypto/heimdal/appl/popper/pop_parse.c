/*
 * Copyright (c) 1989 Regents of the University of California.
 * All rights reserved.  The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 */

#include <popper.h>
RCSID("$Id: pop_parse.c,v 1.9 1999/03/13 21:17:27 assar Exp $");

/* 
 *  parse:  Parse a raw input line from a POP client 
 *  into null-delimited tokens
 */

int
pop_parse(POP *p, char *buf)
{
    char            *   mp;
    int        i;
    
    /*  Loop through the POP command array */
    for (mp = buf, i = 0; ; i++) {
    
        /*  Skip leading spaces and tabs in the message */
        while (isspace((unsigned char)*mp))mp++;

        /*  Are we at the end of the message? */
        if (*mp == 0) break;

        /*  Have we already obtained the maximum allowable parameters? */
        if (i >= MAXPARMCOUNT) {
            pop_msg(p,POP_FAILURE,"Too many arguments supplied.");
            return(-1);
        }

        /*  Point to the start of the token */
        p->pop_parm[i] = mp;

        /*  Search for the first space character (end of the token) */
        while (!isspace((unsigned char)*mp) && *mp) mp++;

        /*  Delimit the token with a null */
        if (*mp) *mp++ = 0;
    }

    /*  Were any parameters passed at all? */
    if (i == 0) return (-1);

    /*  Convert the first token (POP command) to lower case */
    strlwr(p->pop_command);

    /*  Return the number of tokens extracted minus the command itself */
    return (i-1);
    
}
