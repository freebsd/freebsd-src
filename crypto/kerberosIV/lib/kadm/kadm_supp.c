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
 * Support functions for Kerberos administration server & clients
 */

/*
  kadm_supp.c
  this holds the support routines for the kerberos administration server

    error: prints out a kadm error message, returns
    fatal: prints out a kadm fatal error message, exits
    prin_vals: prints out data associated with a Principal in the vals
           structure
*/

#include "kadm_locl.h"
    
RCSID("$Id: kadm_supp.c,v 1.14 1999/09/16 20:41:46 assar Exp $");

static void
time2str(char *buf, size_t len, time_t t)
{
    strftime(buf, len, "%Y-%m-%d %H:%M:%S", localtime(&t));
}

/*
prin_vals:
  recieves    : a vals structure
*/
void
prin_vals(Kadm_vals *vals)
{
    char date[32];
    if(IS_FIELD(KADM_NAME, vals->fields) && IS_FIELD(KADM_INST, vals->fields))
	printf("%20s: %s\n", "Principal", 
	       krb_unparse_name_long(vals->name, vals->instance, NULL));
    else {
	printf("Dump of funny entry:\n");
	if(IS_FIELD(KADM_NAME, vals->fields))
	    printf("%20s: %s\n", "Name", vals->name);
	if(IS_FIELD(KADM_INST, vals->fields))
	    printf("%20s: %s\n", "Instance", vals->instance);
    }
    if(IS_FIELD(KADM_MAXLIFE, vals->fields))
	printf("%20s: %d (%s)\n", "Max ticket life", 
	       vals->max_life, 
	       krb_life_to_atime(vals->max_life));
    if(IS_FIELD(KADM_EXPDATE, vals->fields)) {
	time2str(date, sizeof(date), vals->exp_date);
	printf("%20s: %s\n", "Expiration date", date);
    }
    if(IS_FIELD(KADM_ATTR, vals->fields))
	printf("%20s: %d\n", "Attributes",
	       vals->attributes);
    if(IS_FIELD(KADM_DESKEY, vals->fields))
	printf("%20s: %#lx %#lx\n", "Key",
	       (unsigned long)vals->key_low,
	       (unsigned long)vals->key_high);

#ifdef EXTENDED_KADM
    if (IS_FIELD(KADM_MODDATE,vals->fields)) {
	time2str(date, sizeof(date), vals->mod_date);
	printf("%20s: %s\n", "Modification date", date);
    }
    if (IS_FIELD(KADM_MODNAME,vals->fields) && 
	IS_FIELD(KADM_MODINST,vals->fields))
	printf("%20s: %s\n", "Modifier", 
	       krb_unparse_name_long(vals->mod_name, vals->mod_instance, NULL));
    if (IS_FIELD(KADM_KVNO,vals->fields))
	printf("%20s: %d\n", "Key version", vals->key_version);
#endif

#if 0
    printf("Info in Database for %s.%s:\n", vals->name, vals->instance);
    printf("   Max Life: %d (%s)   Exp Date: %s\n",
	   vals->max_life,
	   krb_life_to_atime(vals->max_life), 
	   asctime(k_localtime(&vals->exp_date)));
    printf("   Attribs: %.2x  key: %#lx %#lx\n",
	   vals->attributes,
	   (unsigned long)vals->key_low,
	   (unsigned long)vals->key_high);
#endif
}

/* kadm_prin_to_vals takes a fields arguments, a Kadm_vals and a Principal,
   it copies the fields in Principal specified by fields into Kadm_vals, 
   i.e from old to new */

void
kadm_prin_to_vals(u_char *fields, Kadm_vals *new, Principal *old)
{
    memset(new, 0, sizeof(*new));
    if (IS_FIELD(KADM_NAME,fields)) {
	strlcpy(new->name, old->name, ANAME_SZ); 
	SET_FIELD(KADM_NAME, new->fields);
    }
    if (IS_FIELD(KADM_INST,fields)) {
	strlcpy(new->instance, old->instance, INST_SZ); 
	SET_FIELD(KADM_INST, new->fields);
    }      
    if (IS_FIELD(KADM_EXPDATE,fields)) {
	new->exp_date   = old->exp_date; 
	SET_FIELD(KADM_EXPDATE, new->fields);
    }      
    if (IS_FIELD(KADM_ATTR,fields)) {
	new->attributes = old->attributes; 
	SET_FIELD(KADM_ATTR, new->fields);
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
#ifdef EXTENDED_KADM
    if (IS_FIELD(KADM_MODDATE,fields)) {
	new->mod_date = old->mod_date;
	SET_FIELD(KADM_MODDATE, new->fields);
    }
    if (IS_FIELD(KADM_MODNAME,fields)) {
	strlcpy(new->mod_name, old->mod_name, ANAME_SZ);
	SET_FIELD(KADM_MODNAME, new->fields);
    }
    if (IS_FIELD(KADM_MODINST,fields)) {
	strlcpy(new->mod_instance, old->mod_instance, ANAME_SZ);
	SET_FIELD(KADM_MODINST, new->fields);
    }
    if (IS_FIELD(KADM_KVNO,fields)) {
	new->key_version = old->key_version;
	SET_FIELD(KADM_KVNO, new->fields);
    }
#endif
}

void
kadm_vals_to_prin(u_char *fields, Principal *new, Kadm_vals *old)
{

    memset(new, 0, sizeof(*new));
    if (IS_FIELD(KADM_NAME,fields))
	strlcpy(new->name, old->name, ANAME_SZ); 
    if (IS_FIELD(KADM_INST,fields))
	strlcpy(new->instance, old->instance, INST_SZ); 
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
#ifdef EXTENDED_KADM
    if (IS_FIELD(KADM_MODDATE,fields))
	new->mod_date = old->mod_date;
    if (IS_FIELD(KADM_MODNAME,fields))
	strlcpy(new->mod_name, old->mod_name, ANAME_SZ);
    if (IS_FIELD(KADM_MODINST,fields))
	strlcpy(new->mod_instance, old->mod_instance, ANAME_SZ);
    if (IS_FIELD(KADM_KVNO,fields))
	new->key_version = old->key_version;
#endif
}
