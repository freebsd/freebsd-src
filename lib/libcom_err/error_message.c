/*
 * $Header: /pub/FreeBSD/FreeBSD-CVS/src/lib/libcom_err/error_message.c,v 1.1.1.1 1995/01/14 22:23:41 wollman Exp $
 * $Source: /pub/FreeBSD/FreeBSD-CVS/src/lib/libcom_err/error_message.c,v $
 * $Locker:  $
 *
 * Copyright 1987 by the Student Information Processing Board
 * of the Massachusetts Institute of Technology
 *
 * For copyright info, see "mit-sipb-copyright.h".
 */

#include <stdio.h>
#include "error_table.h"
#include "mit-sipb-copyright.h"
#include "internal.h"

static const char rcsid[] =
    "$Header: /pub/FreeBSD/FreeBSD-CVS/src/lib/libcom_err/error_message.c,v 1.1.1.1 1995/01/14 22:23:41 wollman Exp $";
static const char copyright[] =
    "Copyright 1986, 1987, 1988 by the Student Information Processing Board\nand the department of Information Systems\nof the Massachusetts Institute of Technology";

static char buffer[25];

struct et_list * _et_list = (struct et_list *) NULL;

const char * error_message (code)
long	code;
{
    int offset;
    struct et_list *et;
    int table_num;
    int started = 0;
    char *cp;

    offset = code & ((1<<ERRCODE_RANGE)-1);
    table_num = code - offset;
    if (!table_num) {
	if (offset < sys_nerr)
	    return(sys_errlist[offset]);
	else
	    goto oops;
    }
    for (et = _et_list; et; et = et->next) {
	if (et->table->base == table_num) {
	    /* This is the right table */
	    if (et->table->n_msgs <= offset)
		goto oops;
	    return(et->table->msgs[offset]);
	}
    }
oops:
    strcpy (buffer, "Unknown code ");
    if (table_num) {
	strcat (buffer, error_table_name (table_num));
	strcat (buffer, " ");
    }
    for (cp = buffer; *cp; cp++)
	;
    if (offset >= 100) {
	*cp++ = '0' + offset / 100;
	offset %= 100;
	started++;
    }
    if (started || offset >= 10) {
	*cp++ = '0' + offset / 10;
	offset %= 10;
    }
    *cp++ = '0' + offset;
    *cp = '\0';
    return(buffer);
}
