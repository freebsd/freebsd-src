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
 * Stream conversion functions for Kerberos administration server
 */

/*
  kadm_stream.c
  this holds the stream support routines for the kerberos administration server

    vals_to_stream: converts a vals struct to a stream for transmission
       internals build_field_header, vts_[string, char, long, short]
    stream_to_vals: converts a stream to a vals struct
       internals check_field_header, stv_[string, char, long, short]
    error: prints out a kadm error message, returns
    fatal: prints out a kadm fatal error message, exits
*/

#include "kadm_locl.h"

RCSID("$Id: kadm_stream.c,v 1.11 1997/05/02 10:28:05 joda Exp $");

static int
build_field_header(u_char *cont, u_char **st)
             			/* container for fields data */
            			/* stream */
{
  *st = (u_char *) malloc (4);
  memcpy(*st, cont, 4);
  return 4;			/* return pointer to current stream location */
}

static int
check_field_header(u_char *st, u_char *cont, int maxlen)
           			/* stream */
             			/* container for fields data */
           
{
  if (4 > maxlen)
      return(-1);
  memcpy(cont, st, 4);
  return 4;			/* return pointer to current stream location */
}

int
vts_string(char *dat, u_char **st, int loc)
          			/* a string to put on the stream */
            			/* base pointer to the stream */
        			/* offset into the stream for current data */
{
  *st = (u_char *) realloc (*st, (unsigned) (loc + strlen(dat) + 1));
  memcpy(*st + loc, dat, strlen(dat)+1);
  return strlen(dat)+1;
}


static int
vts_short(u_int16_t dat, u_char **st, int loc)
            			/* the attributes field */
            			/* a base pointer to the stream */
        			/* offset into the stream for current data */
{
    unsigned char *p;
    p = realloc(*st, loc + 2);
    if(p == NULL){
	abort();
    }
    p[loc] = (dat >> 8) & 0xff;
    p[loc+1] = dat & 0xff;
    *st = p;
    return 2;
}

static int
vts_char(u_char dat, u_char **st, int loc)
           			/* the attributes field */
            			/* a base pointer to the stream */
        			/* offset into the stream for current data */
{
    unsigned char *p = realloc(*st, loc + 1);
    if(p == NULL){
	abort();
    }
    p[loc] = dat;
    *st = p;
    return 1;
}

int
vts_long(u_int32_t dat, u_char **st, int loc)
           			/* the attributes field */
            			/* a base pointer to the stream */
        			/* offset into the stream for current data */
{
    unsigned char *p = realloc(*st, loc + 4);
    if(p == NULL){
	abort();
    }
    p[loc] = (dat >> 24) & 0xff;
    p[loc+1] = (dat >> 16) & 0xff;
    p[loc+2] = (dat >> 8) & 0xff;
    p[loc+3] = dat & 0xff;
    *st = p;
    return 4;
}
    
int
stv_string(u_char *st,		/* base pointer to the stream */
	   char *dat,		/* a string to read from the stream */
	   int loc,		/* offset into the stream for current data */
	   int stlen,		/* max length of string to copy in */
	   int maxlen)		/* max length of input stream */
{
  int maxcount;				/* max count of chars to copy */

  maxcount = min(maxlen - loc, stlen);

  if(maxcount <= 0)
      return -1;

  strncpy(dat, (char *)st + loc, maxcount);

  if (dat[maxcount-1]) /* not null-term --> not enuf room */
      return(-1);
  return strlen(dat)+1;
}

static int
stv_short(u_char *st, u_int16_t *dat, int loc, int maxlen)
           			/* a base pointer to the stream */
             			/* the attributes field */
        			/* offset into the stream for current data */
           
{
  if (maxlen - loc < 2)
      return -1;
  
  *dat = (st[loc] << 8) | st[loc + 1];
  return 2;
}

int
stv_long(u_char *st, u_int32_t *dat, int loc, int maxlen)
           			/* a base pointer to the stream */
            			/* the attributes field */
        			/* offset into the stream for current data */
           			/* maximum length of st */
{
  if (maxlen - loc < 4)
      return -1;
  
  *dat = (st[loc] << 24) | (st[loc+1] << 16) | (st[loc+2] << 8) | st[loc+3];
  return 4;
}
    
static int
stv_char(u_char *st, u_char *dat, int loc, int maxlen)
           			/* a base pointer to the stream */
            			/* the attributes field */
        			/* offset into the stream for current data */
           
{
  if (maxlen - loc < 1)
      return -1;
  
  *dat = st[loc];
  return 1;
}

/* 
vals_to_stream
  recieves    : kadm_vals *, u_char *
  returns     : a realloced and filled in u_char *
     
this function creates a byte-stream representation of the kadm_vals structure
*/
int
vals_to_stream(Kadm_vals *dt_in, u_char **dt_out)
{
  int vsloop, stsize;		/* loop counter, stream size */

  stsize = build_field_header(dt_in->fields, dt_out);
  for (vsloop=31; vsloop>=0; vsloop--)
    if (IS_FIELD(vsloop,dt_in->fields)) {
      switch (vsloop) {
      case KADM_NAME:
	  stsize+=vts_string(dt_in->name, dt_out, stsize);
	  break;
      case KADM_INST:
	  stsize+=vts_string(dt_in->instance, dt_out, stsize);
	  break;
      case KADM_EXPDATE:
	  stsize+=vts_long(dt_in->exp_date, dt_out, stsize);
	  break;
      case KADM_ATTR:
	  stsize+=vts_short(dt_in->attributes, dt_out, stsize);
	  break;
      case KADM_MAXLIFE:
	  stsize+=vts_char(dt_in->max_life, dt_out, stsize);
	  break;
      case KADM_DESKEY: 
	  stsize+=vts_long(dt_in->key_high, dt_out, stsize); 
	  stsize+=vts_long(dt_in->key_low, dt_out, stsize); 
	  break;
      default:
	  break;
      }
}
  return(stsize);
}  

/* 
stream_to_vals
  recieves    : u_char *, kadm_vals *
  returns     : a kadm_vals filled in according to u_char *
     
this decodes a byte stream represntation of a vals struct into kadm_vals
*/
int
stream_to_vals(u_char *dt_in, Kadm_vals *dt_out, int maxlen)
              
                  
           				/* max length to use */
{
  int vsloop, stsize;		/* loop counter, stream size */
  int status;

  memset(dt_out, 0, sizeof(*dt_out));

  stsize = check_field_header(dt_in, dt_out->fields, maxlen);
  if (stsize < 0)
      return(-1);
  for (vsloop=31; vsloop>=0; vsloop--)
    if (IS_FIELD(vsloop,dt_out->fields))
      switch (vsloop) {
      case KADM_NAME:
	  if ((status = stv_string(dt_in, dt_out->name, stsize,
				   sizeof(dt_out->name), maxlen)) < 0)
	      return(-1);
	  stsize += status;
	  break;
      case KADM_INST:
	  if ((status = stv_string(dt_in, dt_out->instance, stsize,
				   sizeof(dt_out->instance), maxlen)) < 0)
	      return(-1);
	  stsize += status;
	  break;
      case KADM_EXPDATE:
	  if ((status = stv_long(dt_in, &dt_out->exp_date, stsize,
				 maxlen)) < 0)
	      return(-1);
	  stsize += status;
	  break;
      case KADM_ATTR:
	  if ((status = stv_short(dt_in, &dt_out->attributes, stsize,
				  maxlen)) < 0)
	      return(-1);
	  stsize += status;
	  break;
      case KADM_MAXLIFE:
	  if ((status = stv_char(dt_in, &dt_out->max_life, stsize,
				 maxlen)) < 0)
	      return(-1);
	  stsize += status;
	  break;
      case KADM_DESKEY:
	  if ((status = stv_long(dt_in, &dt_out->key_high, stsize,
				    maxlen)) < 0)
	      return(-1);
	  stsize += status;
	  if ((status = stv_long(dt_in, &dt_out->key_low, stsize,
				    maxlen)) < 0)
	      return(-1);
	  stsize += status;
	  break;
      default:
	  break;
      }
  return stsize;
}  
