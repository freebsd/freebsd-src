/*
 * Copyright 1986, 1987, 1988 by the Massachusetts Institute
 * of Technology.
 * For copying and distribution information, please see the file
 * <Copyright.MIT>.
 *
 * This routine dissects a a Kerberos 'safe msg',
 * checking its integrity, and returning a pointer to the application
 * data contained and its length.
 *
 * Returns 0 (RD_AP_OK) for success or an error code (RD_AP_...)
 *
 * Steve Miller    Project Athena  MIT/DEC
 *
 *	from: rd_err.c,v 4.5 89/01/13 17:26:38 steiner Exp $
 *	$FreeBSD$
 */

#if 0
#ifndef lint
static char rcsid[] =
"$FreeBSD$";
#endif /* lint */
#endif

/* system include files */
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/time.h>

/* application include files */
#include <krb.h>
#include <prot.h>

/*
 * Given an AUTH_MSG_APPL_ERR message, "in" and its length "in_length",
 * return the error code from the message in "code" and the text in
 * "m_data" as follows:
 *
 *	m_data->app_data	points to the error text
 *	m_data->app_length	points to the length of the error text
 *
 * If all goes well, return RD_AP_OK.  If the version number
 * is wrong, return RD_AP_VERSION, and if it's not an AUTH_MSG_APPL_ERR
 * type message, return RD_AP_MSG_TYPE.
 *
 * The AUTH_MSG_APPL_ERR message format can be found in mk_err.c
 */

int
krb_rd_err(in,in_length,code,m_data)
    u_char *in;                 /* pointer to the msg received */
    u_long in_length;           /* of in msg */
    long *code;                 /* received error code */
    MSG_DAT *m_data;
{
    register u_char *p;
    int swap_bytes = 0;
    p = in;                     /* beginning of message */

    if (*p++ != KRB_PROT_VERSION)
        return(RD_AP_VERSION);
    if (((*p) & ~1) != AUTH_MSG_APPL_ERR)
        return(RD_AP_MSG_TYPE);
    if ((*p++ & 1) != HOST_BYTE_ORDER)
        swap_bytes++;

    /* safely get code */
    bcopy((char *)p,(char *)code,sizeof(*code));
    if (swap_bytes)
        swap_u_long(*code);
    p += sizeof(*code);         /* skip over */

    m_data->app_data = p;       /* we're now at the error text
                                 * message */
    m_data->app_length = in_length;

    return(RD_AP_OK);           /* OK == 0 */
}
