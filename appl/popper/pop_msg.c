/*
 * Copyright (c) 1989 Regents of the University of California.
 * All rights reserved.  The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 */

#include <popper.h>
RCSID("$Id$");

/*
 *  msg:    Send a formatted line to the POP client
 */

int
pop_msg(POP *p, int stat, const char *format, ...)
{
    char	       *mp;
    char                message[MAXLINELEN];
    va_list             ap;

    va_start(ap, format);

    /*  Point to the message buffer */
    mp = message;

    /*  Format the POP status code at the beginning of the message */
    snprintf (mp, sizeof(message), "%s ",
	      (stat == POP_SUCCESS) ? POP_OK : POP_ERR);

    /*  Point past the POP status indicator in the message message */
    mp += strlen(mp);

    /*  Append the message (formatted, if necessary) */
    if (format)
	vsnprintf (mp, sizeof(message) - strlen(message),
		   format, ap);

    /*  Log the message if debugging is turned on */
#ifdef DEBUG
    if (p->debug && stat == POP_SUCCESS)
        pop_log(p,POP_DEBUG,"%s",message);
#endif /* DEBUG */

    /*  Log the message if a failure occurred */
    if (stat != POP_SUCCESS)
        pop_log(p,POP_PRIORITY,"%s",message);

    /*  Append the <CR><LF> */
    strlcat(message, "\r\n", sizeof(message));

    /*  Send the message to the client */
    fputs(message, p->output);
    fflush(p->output);

    va_end(ap);
    return(stat);
}
