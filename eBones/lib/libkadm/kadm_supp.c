/*
 * Copyright 1988 by the Massachusetts Institute of Technology.
 *
 * For copying and distribution information, please see the file
 * Copyright.MIT.
 *
 * Support functions for Kerberos administration server & clients
 */

#if 0
#ifndef	lint
static char rcsid_kadm_supp_c[] =
"Header: /afs/athena.mit.edu/astaff/project/kerberos/src/lib/kadm/RCS/kadm_supp.c,v 4.1 89/09/26 09:21:07 jtkohl Exp ";
static const char rcsid[] =
	"$FreeBSD$";
#endif	lint
#endif

/*
  kadm_supp.c
  this holds the support routines for the kerberos administration server

    error: prints out a kadm error message, returns
    fatal: prints out a kadm fatal error message, exits
    prin_vals: prints out data associated with a Principal in the vals
           structure
*/

#include <string.h>
#include <time.h>
#include <kadm.h>
#include <krb_db.h>

/*
prin_vals:
  recieves    : a vals structure
*/
void
prin_vals(vals)
Kadm_vals *vals;
{
   printf("Info in Database for %s.%s:\n", vals->name, vals->instance);
   printf("   Max Life: %d   Exp Date: %s\n",vals->max_life,
	  asctime(localtime((long *)&vals->exp_date)));
   printf("   Attribs: %.2x  key: %lu %lu\n",vals->attributes,
	  vals->key_low, vals->key_high);
}

#ifdef notdef
nierror(s)
int s;
{
    printf("Kerberos admin server loses..... %s\n",error_message(s));
    return(s);
}
#endif

/* kadm_prin_to_vals takes a fields arguments, a Kadm_vals and a Principal,
   it copies the fields in Principal specified by fields into Kadm_vals,
   i.e from old to new */

void
kadm_prin_to_vals(fields, new, old)
u_char fields[FLDSZ];
Kadm_vals *new;
Principal *old;
{
  bzero((char *)new, sizeof(*new));
  if (IS_FIELD(KADM_NAME,fields)) {
      (void) strncpy(new->name, old->name, ANAME_SZ);
      SET_FIELD(KADM_NAME, new->fields);
  }
  if (IS_FIELD(KADM_INST,fields)) {
      (void) strncpy(new->instance, old->instance, INST_SZ);
      SET_FIELD(KADM_INST, new->fields);
  }
  if (IS_FIELD(KADM_EXPDATE,fields)) {
      new->exp_date   = old->exp_date;
      SET_FIELD(KADM_EXPDATE, new->fields);
  }
  if (IS_FIELD(KADM_ATTR,fields)) {
    new->attributes = old->attributes;
      SET_FIELD(KADM_MAXLIFE, new->fields);
  }
  if (IS_FIELD(KADM_MAXLIFE,fields)) {
    new->max_life   = old->max_life;
      SET_FIELD(KADM_MAXLIFE, new->fields);
  }
  if (IS_FIELD(KADM_DESKEY,fields)) {
    new->key_low    = old->key_low;
    new->key_high   = old->key_high;
    SET_FIELD(KADM_DESKEY, new->fields);
  }
}

void
kadm_vals_to_prin(fields, new, old)
u_char fields[FLDSZ];
Principal *new;
Kadm_vals *old;
{

  bzero((char *)new, sizeof(*new));
  if (IS_FIELD(KADM_NAME,fields))
    (void) strncpy(new->name, old->name, ANAME_SZ);
  if (IS_FIELD(KADM_INST,fields))
    (void) strncpy(new->instance, old->instance, INST_SZ);
  if (IS_FIELD(KADM_EXPDATE,fields))
    new->exp_date   = old->exp_date;
  if (IS_FIELD(KADM_ATTR,fields))
    new->attributes = old->attributes;
  if (IS_FIELD(KADM_MAXLIFE,fields))
    new->max_life   = old->max_life;
  if (IS_FIELD(KADM_DESKEY,fields)) {
    new->key_low    = old->key_low;
    new->key_high   = old->key_high;
  }
}
