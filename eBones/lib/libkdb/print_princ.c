/*
 * Copyright 1988 by the Massachusetts Institute of Technology.
 * For copying and distribution information, please see the file
 * <Copyright.MIT>.
 *
 *	from: $Header: /home/ncvs/src/eBones/lib/libkdb/print_princ.c,v 1.4 1995/09/07 21:37:13 markm Exp $
 *	$FreeBSD$
 */

#if 0
#ifndef	lint
static char rcsid[] =
"$FreeBSD$";
#endif	lint
#endif

#include <stdio.h>
#include <time.h>
#include <sys/types.h>
#include <sys/time.h>
#include <strings.h>
#include <krb.h>
#include <krb_db.h>

extern int debug;

long    kerb_debug;
static struct tm *time_p;

void
krb_print_principal(a_n)
    Principal *a_n;
{
    /* run-time database does not contain string versions */
    time_p = localtime(&(a_n->exp_date));

    fprintf(stderr,
    "\n%s %s expires %4d-%2d-%2d %2d:%2d, max_life %d*5 = %d min  attr 0x%02x",
    a_n->name, a_n->instance,
    time_p->tm_year > 1900 ? time_p->tm_year : time_p->tm_year + 1900,
    time_p->tm_mon + 1, time_p->tm_mday,
    time_p->tm_hour, time_p->tm_min,
    a_n->max_life, 5 * a_n->max_life, a_n->attributes);

    fprintf(stderr,
    "\n\tkey_ver %d  k_low 0x%08lx  k_high 0x%08lx  akv %d  exists %d\n",
    a_n->key_version, a_n->key_low, a_n->key_high,
    a_n->kdc_key_ver, (int)a_n->old);

    fflush(stderr);
}
