/*
 * $Header: /pub/FreeBSD/FreeBSD-CVS/src/lib/libcom_err/init_et.c,v 1.1.1.1 1995/01/14 22:23:41 wollman Exp $
 * $Source: /pub/FreeBSD/FreeBSD-CVS/src/lib/libcom_err/init_et.c,v $
 * $Locker:  $
 *
 * Copyright 1986, 1987, 1988 by MIT Information Systems and
 *	the MIT Student Information Processing Board.
 *
 * For copyright info, see mit-sipb-copyright.h.
 */

#include <stdio.h>
#include "error_table.h"
#include "mit-sipb-copyright.h"

#ifndef __STDC__
#define const
#endif

#ifndef	lint
static const char rcsid_init_et_c[] =
    "$Header: /pub/FreeBSD/FreeBSD-CVS/src/lib/libcom_err/init_et.c,v 1.1.1.1 1995/01/14 22:23:41 wollman Exp $";
#endif

extern char *malloc(), *realloc();

struct foobar {
    struct et_list etl;
    struct error_table et;
};

extern struct et_list * _et_list;

int init_error_table(msgs, base, count)
    const char * const * msgs;
    int base;
    int count;
{
    struct foobar * new_et;

    if (!base || !count || !msgs)
	return 0;

    new_et = (struct foobar *) malloc(sizeof(struct foobar));
    if (!new_et)
	return errno;	/* oops */
    new_et->etl.table = &new_et->et;
    new_et->et.msgs = msgs;
    new_et->et.base = base;
    new_et->et.n_msgs= count;

    new_et->etl.next = _et_list;
    _et_list = &new_et->etl;
    return 0;
}
