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

RCSID("$Id: kadm_stream.c,v 1.13 1998/10/22 15:38:01 joda Exp $");

static int
build_field_header(u_char *cont, /* container for fields data */
		   u_char **st)	/* stream */
{
  *st = malloc (4);
  if (*st == NULL)
      return -1;
  memcpy(*st, cont, 4);
  return 4;			/* return pointer to current stream location */
}

static int
check_field_header(u_char *st,	/* stream */
		   u_char *cont, /* container for fields data */
		   int maxlen)
{
  if (4 > maxlen)
      return(-1);
  memcpy(cont, st, 4);
  return 4;			/* return pointer to current stream location */
}

int
vts_string(char *dat,		/* a string to put on the stream */
	   u_char **st,		/* base pointer to the stream */
	   int loc)		/* offset into the stream for current data */
{
  void *tmp;

  tmp = realloc(*st, loc + strlen(dat) + 1);
  if(tmp == NULL)
    return -1;
  memcpy((char *)tmp + loc, dat, strlen(dat)+1);
  *st = tmp;
  return strlen(dat)+1;
}


static int
vts_short(u_int16_t dat,	/* the attributes field */
	  u_char **st,		/* a base pointer to the stream */
	  int loc)		/* offset into the stream for current data */
{
    unsigned char *p;

    p = realloc(*st, loc + 2);
    if(p == NULL)
	return -1;
    p[loc] = (dat >> 8) & 0xff;
    p[loc+1] = dat & 0xff;
    *st = p;
    return 2;
}

static int
vts_char(u_char dat,		/* the attributes field */
	 u_char **st,		/* a base pointer to the stream */
	 int loc)		/* offset into the stream for current data */
{
    unsigned char *p;

    p = realloc(*st, loc + 1);

    if(p == NULL)
	return -1;
    p[loc] = dat;
    *st = p;
    return 1;
}

int
vts_long(u_int32_t dat,		/* the attributes field */
	 u_char **st,		/* a base pointer to the stream */
	 int loc)		/* offset into the stream for current data */
{
    unsigned char *p;

    p = realloc(*st, loc + 4);
    if(p == NULL)
	return -1;
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
  int len;

  maxcount = min(maxlen - loc, stlen);

  if(maxcount <= 0)
      return -1;

  len = strnlen ((char *)st + loc, maxlen - loc);

  if (len >= stlen)
      return -1;

  memcpy(dat, st + loc, len);
  dat[len] = '\0';
  return len + 1;
}

static int
stv_short(u_char *st,		/* a base pointer to the stream */
	  u_int16_t *dat,	/* the attributes field */
	  int loc,		/* offset into the stream for current data */
	  int maxlen)
{
  if (maxlen - loc < 2)
      return -1;
  
  *dat = (st[loc] << 8) | st[loc + 1];
  return 2;
}

int
stv_long(u_char *st,		/* a base pointer to the stream */
	 u_int32_t *dat,	/* the attributes field */
	 int loc,		/* offset into the stream for current data */
	 int maxlen)		/* maximum length of st */
{
  if (maxlen - loc < 4)
      return -1;
  
  *dat = (st[loc] << 24) | (st[loc+1] << 16) | (st[loc+2] << 8) | st[loc+3];
  return 4;
}
    
static int
stv_char(u_char *st,		/* a base pointer to the stream */
	 u_char *dat,		/* the attributes field */
	 int loc,		/* offset into the stream for current data */
	 int maxlen)
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
  if (stsize < 0)
      return stsize;
  for (vsloop=31; vsloop>=0; vsloop--)
    if (IS_FIELD(vsloop,dt_in->fields)) {
      int tmp = 0;

      switch (vsloop) {
      case KADM_NAME:
	  tmp = vts_string(dt_in->name, dt_out, stsize);
	  break;
      case KADM_INST:
	  tmp = vts_string(dt_in->instance, dt_out, stsize);
	  break;
      case KADM_EXPDATE:
	  tmp = vts_long(dt_in->exp_date, dt_out, stsize);
	  break;
      case KADM_ATTR:
	  tmp = vts_short(dt_in->attributes, dt_out, stsize);
	  break;
      case KADM_MAXLIFE:
	  tmp = vts_char(dt_in->max_life, dt_out, stsize);
	  break;
      case KADM_DESKEY: 
	  tmp = vts_long(dt_in->key_high, dt_out, stsize);
	  if(tmp > 0)
	      tmp += vts_long(dt_in->key_low, dt_out, stsize + tmp);
	  break;
#ifdef EXTENDED_KADM
      case KADM_MODDATE:
	  tmp = vts_long(dt_in->mod_date, dt_out, stsize);
	  break;
      case KADM_MODNAME:
	  tmp = vts_string(dt_in->mod_name, dt_out, stsize);
	  break;
      case KADM_MODINST:
	  tmp = vts_string(dt_in->mod_instance, dt_out, stsize);
	  break;
      case KADM_KVNO:
	  tmp = vts_char(dt_in->key_version, dt_out, stsize);
	  break;
#endif
      default:
	  break;
      }
      if (tmp < 0) {
	  free(*dt_out);
	  return tmp;
      }
      stsize += tmp;
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
stream_to_vals(u_char *dt_in,
	       Kadm_vals *dt_out,
	       int maxlen)	/* max length to use */
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
#ifdef EXTENDED_KADM
	    case KADM_MODDATE:
		if ((status = stv_long(dt_in, &dt_out->mod_date, stsize,
				       maxlen)) < 0)
		    return(-1);
		stsize += status;
		break;
	    case KADM_MODNAME:
		if ((status = stv_string(dt_in, dt_out->mod_name, stsize,
					 sizeof(dt_out->mod_name), maxlen)) < 0)
		    return(-1);
		stsize += status;
		break;
	    case KADM_MODINST:
		if ((status = stv_string(dt_in, dt_out->mod_instance, stsize,
					 sizeof(dt_out->mod_instance), maxlen)) < 0)
		    return(-1);
		stsize += status;
		break;
	    case KADM_KVNO:
		if ((status = stv_char(dt_in, &dt_out->key_version, stsize,
				       maxlen)) < 0)
		    return(-1);
		stsize += status;
		break;
#endif
	    default:
		break;
	    }
    return stsize;
}  
