/*
 * Copyright 1988 by the Massachusetts Institute of Technology.
 *
 * For copying and distribution information, please see the file
 * Copyright.MIT.
 *
 * Kerberos administration server-side subroutines
 */

#if 0
#ifndef	lint
static char rcsid_kadm_server_c[] =
"Header: /afs/athena.mit.edu/astaff/project/kerberos/src/kadmin/RCS/kadm_server.c,v 4.2 89/09/26 09:30:23 jtkohl Exp ";
#endif	lint
#endif

#include <string.h>
#include <kadm.h>
#include <kadm_err.h>
#include "kadm_server.h"

/*
kadm_ser_cpw - the server side of the change_password routine
  recieves    : KTEXT, {key}
  returns     : CKSUM, RETCODE
  acl         : caller can change only own password

Replaces the password (i.e. des key) of the caller with that specified in key.
Returns no actual data from the master server, since this is called by a user
*/
int
kadm_ser_cpw(dat, len, ad, datout, outlen)
u_char *dat;
int len;
AUTH_DAT *ad;
u_char **datout;
int *outlen;
{
    unsigned long keylow, keyhigh;
    des_cblock newkey;
    int stvlen;

    /* take key off the stream, and change the database */

    if ((stvlen = stv_long(dat, &keyhigh, 0, len)) < 0)
	return(KADM_LENGTH_ERROR);
    if (stv_long(dat, &keylow, stvlen, len) < 0)
	return(KADM_LENGTH_ERROR);

    keylow = ntohl(keylow);
    keyhigh = ntohl(keyhigh);
    bcopy((char *)&keyhigh, (char *)(((long *)newkey) + 1), 4);
    bcopy((char *)&keylow, (char *)newkey, 4);
    *datout = 0;
    *outlen = 0;

    return(kadm_change(ad->pname, ad->pinst, ad->prealm, newkey));
}

/*
kadm_ser_add - the server side of the add_entry routine
  recieves    : KTEXT, {values}
  returns     : CKSUM, RETCODE, {values}
  acl         : su, sms (as alloc)

Adds and entry containing values to the database
returns the values of the entry, so if you leave certain fields blank you will
   be able to determine the default values they are set to
*/
int
kadm_ser_add(dat,len,ad, datout, outlen)
u_char *dat;
int len;
AUTH_DAT *ad;
u_char **datout;
int *outlen;
{
  Kadm_vals values, retvals;
  int status;

  if ((status = stream_to_vals(dat, &values, len)) < 0)
      return(KADM_LENGTH_ERROR);
  if ((status = kadm_add_entry(ad->pname, ad->pinst, ad->prealm,
			      &values, &retvals)) == KADM_DATA) {
      *outlen = vals_to_stream(&retvals,datout);
      return KADM_SUCCESS;
  } else {
      *outlen = 0;
      return status;
  }
}

/*
kadm_ser_mod - the server side of the mod_entry routine
  recieves    : KTEXT, {values, values}
  returns     : CKSUM, RETCODE, {values}
  acl         : su, sms (as register or dealloc)

Modifies all entries corresponding to the first values so they match the
   second values.
returns the values for the changed entries
*/
int
kadm_ser_mod(dat,len,ad, datout, outlen)
u_char *dat;
int len;
AUTH_DAT *ad;
u_char **datout;
int *outlen;
{
  Kadm_vals vals1, vals2, retvals;
  int wh;
  int status;

  if ((wh = stream_to_vals(dat, &vals1, len)) < 0)
      return KADM_LENGTH_ERROR;
  if ((status = stream_to_vals(dat+wh,&vals2, len-wh)) < 0)
      return KADM_LENGTH_ERROR;
  if ((status = kadm_mod_entry(ad->pname, ad->pinst, ad->prealm, &vals1,
			       &vals2, &retvals)) == KADM_DATA) {
      *outlen = vals_to_stream(&retvals,datout);
      return KADM_SUCCESS;
  } else {
      *outlen = 0;
      return status;
  }
}

/*
kadm_ser_get
  recieves   : KTEXT, {values, flags}
  returns    : CKSUM, RETCODE, {count, values, values, values}
  acl        : su

gets the fields requested by flags from all entries matching values
returns this data for each matching recipient, after a count of how many such
  matches there were
*/
int
kadm_ser_get(dat,len,ad, datout, outlen)
u_char *dat;
int len;
AUTH_DAT *ad;
u_char **datout;
int *outlen;
{
  Kadm_vals values, retvals;
  u_char fl[FLDSZ];
  int loop,wh;
  int status;

  if ((wh = stream_to_vals(dat, &values, len)) < 0)
      return KADM_LENGTH_ERROR;
  if (wh + FLDSZ > len)
      return KADM_LENGTH_ERROR;
  for (loop=FLDSZ-1; loop>=0; loop--)
    fl[loop] = dat[wh++];
  if ((status = kadm_get_entry(ad->pname, ad->pinst, ad->prealm,
			      &values, fl, &retvals)) == KADM_DATA) {
      *outlen = vals_to_stream(&retvals,datout);
      return KADM_SUCCESS;
  } else {
      *outlen = 0;
      return status;
  }
}

