/* 
  Copyright (C) 1989 by the Massachusetts Institute of Technology

   Export of this software from the United States of America is assumed
   to require a specific license from the United States Government.
   It is the responsibility of any person or organization contemplating
   export to obtain such a license before exporting.

WITHIN THAT CONSTRAINT, permission to use, copy, modify, and
distribute this software and its documentation for any purpose and
without fee is hereby granted, provided that the above copyright
notice appear in all copies and that both that copyright notice and
this permission notice appear in supporting documentation, and that
the name of M.I.T. not be used in advertising or publicity pertaining
to distribution of the software without specific, written prior
permission.  M.I.T. makes no representations about the suitability of
this software for any purpose.  It is provided "as is" without express
or implied warranty.

  */

/*
 * Kerberos administration server-side subroutines
 */

#include "kadm_locl.h"

RCSID("$Id: kadm_server.c,v 1.9 1997/05/02 10:29:08 joda Exp $");

/* 
kadm_ser_cpw - the server side of the change_password routine
  recieves    : KTEXT, {key}
  returns     : CKSUM, RETCODE
  acl         : caller can change only own password

Replaces the password (i.e. des key) of the caller with that specified in key.
Returns no actual data from the master server, since this is called by a user
*/
int
kadm_ser_cpw(u_char *dat, int len, AUTH_DAT *ad, u_char **datout, int *outlen)
{
    u_int32_t keylow, keyhigh;
    des_cblock newkey;
    int status;
    int stvlen=0;
    char *pw_msg;
    char pword[MAX_KPW_LEN];
    char *strings[4];

    /* take key off the stream, and change the database */

    if ((status = stv_long(dat, &keyhigh, 0, len)) < 0)
	return(KADM_LENGTH_ERROR);
    stvlen=status;
    if ((status = stv_long(dat, &keylow, stvlen, len)) < 0)
	return(KADM_LENGTH_ERROR);
    stvlen+=status;

    if((status = stv_string(dat, pword, stvlen, sizeof(pword), len))<0)
      pword[0]=0;

    keylow = ntohl(keylow);
    keyhigh = ntohl(keyhigh);
    memcpy(((char *)newkey) + 4, &keyhigh, 4);
    memcpy(newkey, &keylow, 4);

    strings[0] = ad->pname;
    strings[1] = ad->pinst;
    strings[2] = ad->prealm;
    strings[3] = NULL;
    status = kadm_pw_check(pword, &newkey, &pw_msg, strings);
    
    memset(pword, 0, sizeof(pword));
    memset(dat, 0, len);

    if(status != KADM_SUCCESS){
      *datout=malloc(0);
      *outlen=vts_string(pw_msg, datout, 0);
      return status;
    }
    *datout=0;
    *outlen=0;

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
kadm_ser_add(u_char *dat, int len, AUTH_DAT *ad, u_char **datout, int *outlen)
{
  Kadm_vals values, retvals;
  long status;

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
kadm_ser_mod(u_char *dat, int len, AUTH_DAT *ad, u_char **datout, int *outlen)
{
  Kadm_vals vals1, vals2, retvals;
  int wh;
  long status;

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

int
kadm_ser_delete(u_char *dat, int len, AUTH_DAT *ad, 
		u_char **datout, int *outlen)
{
    Kadm_vals values;
    int wh;
    int status;
    
    if((wh = stream_to_vals(dat, &values, len)) < 0)
	return KADM_LENGTH_ERROR;
    if(wh != len)
	return KADM_LENGTH_ERROR;
    status = kadm_delete_entry(ad->pname, ad->pinst, ad->prealm, 
			       &values);
    *outlen = 0;
    return status;
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
kadm_ser_get(u_char *dat, int len, AUTH_DAT *ad, u_char **datout, int *outlen)
{
  Kadm_vals values, retvals;
  u_char fl[FLDSZ];
  int loop,wh;
  long status;

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

