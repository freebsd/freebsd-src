/*
 * Copyright 1988 by the Massachusetts Institute of Technology.
 *
 * For copying and distribution information, please see the file
 * Copyright.MIT.
 *
 * Stream conversion functions for Kerberos administration server
 */

#if 0
#ifndef	lint
static char rcsid_kadm_stream_c[] =
"Header: /afs/athena.mit.edu/astaff/project/kerberos/src/lib/kadm/RCS/kadm_stream.c,v 4.2 89/09/26 09:20:48 jtkohl Exp ";
static const char rcsid[] =
	"$FreeBSD$";
#endif	lint
#endif

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

#include <string.h>
#include <kadm.h>

#define min(a,b) (((a) < (b)) ? (a) : (b))

/*
vals_to_stream
  recieves    : kadm_vals *, u_char *
  returns     : a realloced and filled in u_char *

this function creates a byte-stream representation of the kadm_vals structure
*/

int
vals_to_stream(dt_in, dt_out)
Kadm_vals *dt_in;
u_char **dt_out;
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

int
build_field_header(cont, st)
u_char *cont;			/* container for fields data */
u_char **st;			/* stream */
{
  *st = (u_char *) malloc (4);
  bcopy((char *) cont, (char *) *st, 4);
  return 4;			/* return pointer to current stream location */
}

int
vts_string(dat, st, loc)
char *dat;			/* a string to put on the stream */
u_char **st;			/* base pointer to the stream */
int loc;			/* offset into the stream for current data */
{
  *st = (u_char *) realloc ((char *)*st, (unsigned) (loc + strlen(dat) + 1));
  bcopy(dat, (char *)(*st + loc), strlen(dat)+1);
  return strlen(dat)+1;
}

int
vts_short(dat, st, loc)
u_short dat;			/* the attributes field */
u_char **st;			/* a base pointer to the stream */
int loc;			/* offset into the stream for current data */
{
  u_short temp;			/* to hold the net order short */

  temp = htons(dat);		/* convert to network order */
  *st = (u_char *) realloc ((char *)*st, (unsigned)(loc + sizeof(u_short)));
  bcopy((char *) &temp, (char *)(*st + loc), sizeof(u_short));
  return sizeof(u_short);
}

int
vts_long(dat, st, loc)
u_long dat;			/* the attributes field */
u_char **st;			/* a base pointer to the stream */
int loc;			/* offset into the stream for current data */
{
  u_long temp;			/* to hold the net order short */

  temp = htonl(dat);		/* convert to network order */
  *st = (u_char *) realloc ((char *)*st, (unsigned)(loc + sizeof(u_long)));
  bcopy((char *) &temp, (char *)(*st + loc), sizeof(u_long));
  return sizeof(u_long);
}

int
vts_char(dat, st, loc)
u_char dat;			/* the attributes field */
u_char **st;			/* a base pointer to the stream */
int loc;			/* offset into the stream for current data */
{
  *st = (u_char *) realloc ((char *)*st, (unsigned)(loc + sizeof(u_char)));
  (*st)[loc] = (u_char) dat;
  return 1;
}

/*
stream_to_vals
  recieves    : u_char *, kadm_vals *
  returns     : a kadm_vals filled in according to u_char *

this decodes a byte stream represntation of a vals struct into kadm_vals
*/
int
stream_to_vals(dt_in, dt_out, maxlen)
u_char *dt_in;
Kadm_vals *dt_out;
int maxlen;				/* max length to use */
{
  register int vsloop, stsize;		/* loop counter, stream size */
  register int status;

  bzero((char *) dt_out, sizeof(*dt_out));

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

int
check_field_header(st, cont, maxlen)
u_char *st;			/* stream */
u_char *cont;			/* container for fields data */
int maxlen;
{
  if (4 > maxlen)
      return(-1);
  bcopy((char *) st, (char *) cont, 4);
  return 4;			/* return pointer to current stream location */
}

int
stv_string(st, dat, loc, stlen, maxlen)
register u_char *st;		/* base pointer to the stream */
char *dat;			/* a string to read from the stream */
register int loc;		/* offset into the stream for current data */
int stlen;			/* max length of string to copy in */
int maxlen;			/* max length of input stream */
{
  int maxcount;				/* max count of chars to copy */

  maxcount = min(maxlen - loc, stlen);

  (void) strncpy(dat, (char *)st + loc, maxcount);

  if (dat[maxcount-1]) /* not null-term --> not enuf room */
      return(-1);
  return strlen(dat)+1;
}

int
stv_short(st, dat, loc, maxlen)
u_char *st;			/* a base pointer to the stream */
u_short *dat;			/* the attributes field */
int loc;			/* offset into the stream for current data */
int maxlen;
{
  u_short temp;			/* to hold the net order short */

  if (loc + sizeof(u_short) > maxlen)
      return(-1);
  bcopy((char *)((u_long)st+(u_long)loc), (char *) &temp, sizeof(u_short));
  *dat = ntohs(temp);		/* convert to network order */
  return sizeof(u_short);
}

int
stv_long(st, dat, loc, maxlen)
u_char *st;			/* a base pointer to the stream */
u_long *dat;			/* the attributes field */
int loc;			/* offset into the stream for current data */
int maxlen;			/* maximum length of st */
{
  u_long temp;			/* to hold the net order short */

  if (loc + sizeof(u_long) > maxlen)
      return(-1);
  bcopy((char *)((u_long)st+(u_long)loc), (char *) &temp, sizeof(u_long));
  *dat = ntohl(temp);		/* convert to network order */
  return sizeof(u_long);
}

int
stv_char(st, dat, loc, maxlen)
u_char *st;			/* a base pointer to the stream */
u_char *dat;			/* the attributes field */
int loc;			/* offset into the stream for current data */
int maxlen;
{
  if (loc + 1 > maxlen)
      return(-1);
  *dat = *(st + loc);
  return 1;
}

