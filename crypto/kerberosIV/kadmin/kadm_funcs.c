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
 * Kerberos administration server-side database manipulation routines
 */

/*
 * kadm_funcs.c
 * the actual database manipulation code
 */

#include "kadm_locl.h"

RCSID("$Id: kadm_funcs.c,v 1.16 1997/05/02 14:28:49 assar Exp $");

static int
check_access(char *pname, char *pinst, char *prealm, enum acl_types acltype)
{
    char checkname[MAX_K_NAME_SZ];
    char filename[MaxPathLen];

    snprintf(checkname, sizeof(checkname), "%s.%s@%s", pname, pinst, prealm);
    
    switch (acltype) {
    case ADDACL:
	snprintf(filename, sizeof(filename), "%s%s", acldir, ADD_ACL_FILE);
	break;
    case GETACL:
	snprintf(filename, sizeof(filename), "%s%s", acldir, GET_ACL_FILE);
	break;
    case MODACL:
	snprintf(filename, sizeof(filename), "%s%s", acldir, MOD_ACL_FILE);
	break;
    case DELACL:
	snprintf(filename, sizeof(filename), "%s%s", acldir, DEL_ACL_FILE);
	break;
    default:
	krb_log("WARNING in check_access: default case in switch");
	return 0;
    }
    return(acl_check(filename, checkname));
}

static int
wildcard(char *str)
{
    if (!strcmp(str, WILDCARD_STR))
	return(1);
    return(0);
}

static int
fail(int code, char *oper, char *princ)
{
    krb_log("ERROR: %s: %s (%s)", oper, princ, error_message(code));
    return code;
}

#define failadd(code) { fail(code, "ADD", victim); return code; }
#define faildelete(code) { fail(code, "DELETE", victim); return code; }
#define failget(code) { fail(code, "GET", victim); return code; }
#define failmod(code) { fail(code, "MOD", victim); return code; }
#define failchange(code) { fail(code, "CHANGE", admin); return code; }

int
kadm_add_entry (char *rname, char *rinstance, char *rrealm, 
		Kadm_vals *valsin, Kadm_vals *valsout)
{
    long numfound;		/* check how many we get written */
    int more;			/* pointer to more grabbed records */
    Principal data_i, data_o;		/* temporary principal */
    u_char flags[4];
    des_cblock newpw;
    Principal default_princ;
  
    char admin[MAX_K_NAME_SZ], victim[MAX_K_NAME_SZ];

    strcpy(admin, krb_unparse_name_long(rname, rinstance, rrealm));
    strcpy(victim, krb_unparse_name_long(valsin->name, valsin->instance, NULL));

    krb_log("ADD: %s by %s", victim, admin);

    if (!check_access(rname, rinstance, rrealm, ADDACL)) {
	krb_log("WARNING: ADD: %s permission denied", admin);
	return KADM_UNAUTH;
    }
  
    /* Need to check here for "legal" name and instance */
    if (wildcard(valsin->name) || wildcard(valsin->instance)) {
	failadd(KADM_ILL_WILDCARD);
    }

    numfound = kerb_get_principal(KERB_DEFAULT_NAME, KERB_DEFAULT_INST,
				  &default_princ, 1, &more);
    if (numfound == -1) {
	failadd(KADM_DB_INUSE);
    } else if (numfound != 1) {
	failadd(KADM_UK_RERROR);
    }

    kadm_vals_to_prin(valsin->fields, &data_i, valsin);
    strncpy(data_i.name, valsin->name, ANAME_SZ);
    strncpy(data_i.instance, valsin->instance, INST_SZ);

    if (!IS_FIELD(KADM_EXPDATE,valsin->fields))
	data_i.exp_date = default_princ.exp_date;
    if (!IS_FIELD(KADM_ATTR,valsin->fields))
	data_i.attributes = default_princ.attributes;
    if (!IS_FIELD(KADM_MAXLIFE,valsin->fields))
	data_i.max_life = default_princ.max_life; 

    memset(&default_princ, 0, sizeof(default_princ));

    /* convert to host order */
    data_i.key_low = ntohl(data_i.key_low);
    data_i.key_high = ntohl(data_i.key_high);


    copy_to_key(&data_i.key_low, &data_i.key_high, newpw);

    /* encrypt new key in master key */
    kdb_encrypt_key (&newpw, &newpw, &server_parm.master_key,
		     server_parm.master_key_schedule, DES_ENCRYPT);
    copy_from_key(newpw, &data_i.key_low, &data_i.key_high);
    memset(newpw, 0, sizeof(newpw));

    data_o = data_i;
    numfound = kerb_get_principal(valsin->name, valsin->instance, 
				  &data_o, 1, &more);
    if (numfound == -1) {
	failadd(KADM_DB_INUSE);
    } else if (numfound) {
	failadd(KADM_INUSE);
    } else {
	data_i.key_version++;
	data_i.kdc_key_ver = server_parm.master_key_version;
	strncpy(data_i.mod_name, rname, sizeof(data_i.mod_name)-1);
	strncpy(data_i.mod_instance, rinstance,
		sizeof(data_i.mod_instance)-1);

	numfound = kerb_put_principal(&data_i, 1);
	if (numfound == -1) {
	    failadd(KADM_DB_INUSE);
	} else if (numfound) {
	    failadd(KADM_UK_SERROR);
	} else {
	    numfound = kerb_get_principal(valsin->name, valsin->instance, 
					  &data_o, 1, &more);
	    if ((numfound!=1) || (more!=0)) {
		failadd(KADM_UK_RERROR);
	    }
	    memset(flags, 0, sizeof(flags));
	    SET_FIELD(KADM_NAME,flags);
	    SET_FIELD(KADM_INST,flags);
	    SET_FIELD(KADM_EXPDATE,flags);
	    SET_FIELD(KADM_ATTR,flags);
	    SET_FIELD(KADM_MAXLIFE,flags);
	    kadm_prin_to_vals(flags, valsout, &data_o);
	    krb_log("ADD: %s added", victim);
	    return KADM_DATA;		/* Set all the appropriate fields */
	}
    }
}

int
kadm_delete_entry (char *rname, char *rinstance, char *rrealm, 
		   Kadm_vals *valsin)
{
    int ret;

    char admin[MAX_K_NAME_SZ], victim[MAX_K_NAME_SZ];
    
    strcpy(admin, krb_unparse_name_long(rname, rinstance, rrealm));
    strcpy(victim, krb_unparse_name_long(valsin->name, valsin->instance, NULL));

    krb_log("DELETE: %s by %s", victim, admin);

    if (!check_access(rname, rinstance, rrealm, DELACL)) {
	krb_log("WARNING: DELETE: %s permission denied", admin);
	return KADM_UNAUTH;
    }
    
    /* Need to check here for "legal" name and instance */
    if (wildcard(valsin->name) || wildcard(valsin->instance)) {
	faildelete(KADM_ILL_WILDCARD);
    }
  
#define EQ(V,N,I) (strcmp((V)->name, (N)) == 0 && strcmp((V)->instance, (I)) == 0)

    if(EQ(valsin, PWSERV_NAME, KRB_MASTER) ||
       EQ(valsin, "K", "M") ||
       EQ(valsin, "default", "") ||
       EQ(valsin, KRB_TICKET_GRANTING_TICKET, server_parm.krbrlm)){
	krb_log("WARNING: DELETE: %s is immutable", victim);
	return KADM_IMMUTABLE; /* XXX */
    }
    
    ret = kerb_delete_principal(valsin->name, valsin->instance);
    if(ret == -1)
	return KADM_DB_INUSE; /* XXX */
    krb_log("DELETE: %s removed.", victim);
    return KADM_SUCCESS;
}


int
kadm_get_entry (char *rname, char *rinstance, char *rrealm, 
		Kadm_vals *valsin, u_char *flags, Kadm_vals *valsout)
{
    long numfound;		/* check how many were returned */
    int more;			/* To point to more name.instances */
    Principal data_o;		/* Data object to hold Principal */
    
    char admin[MAX_K_NAME_SZ], victim[MAX_K_NAME_SZ];
    
    strcpy(admin, krb_unparse_name_long(rname, rinstance, rrealm));
    strcpy(victim, krb_unparse_name_long(valsin->name, valsin->instance, NULL));
    
    krb_log("GET: %s by %s", victim, admin);

    if (!check_access(rname, rinstance, rrealm, GETACL)) {
	krb_log("WARNING: GET: %s permission denied", admin);
	return KADM_UNAUTH;
    }
  
    if (wildcard(valsin->name) || wildcard(valsin->instance)) {
	failget(KADM_ILL_WILDCARD);
    }

    /* Look up the record in the database */
    numfound = kerb_get_principal(valsin->name, valsin->instance, 
				  &data_o, 1, &more);
    if (numfound == -1) {
	failget(KADM_DB_INUSE);
    }  else if (numfound) {	/* We got the record, let's return it */
	kadm_prin_to_vals(flags, valsout, &data_o);
	krb_log("GET: %s retrieved", victim);
	return KADM_DATA; /* Set all the appropriate fields */
    } else {
	failget(KADM_NOENTRY);	/* Else whimper and moan */
    }
}

int
kadm_mod_entry (char *rname, char *rinstance, char *rrealm, 
		Kadm_vals *valsin, Kadm_vals *valsin2, Kadm_vals *valsout)
{
    long numfound;
    int more;
    Principal data_o, temp_key;
    u_char fields[4];
    des_cblock newpw;

    char admin[MAX_K_NAME_SZ], victim[MAX_K_NAME_SZ];
    
    strcpy(admin, krb_unparse_name_long(rname, rinstance, rrealm));
    strcpy(victim, krb_unparse_name_long(valsin->name, valsin->instance, NULL));
    
    krb_log("MOD: %s by %s", victim, admin);

    if (wildcard(valsin->name) || wildcard(valsin->instance)) {
	failmod(KADM_ILL_WILDCARD);
    }
  
    if (!check_access(rname, rinstance, rrealm, MODACL)) {
	krb_log("WARNING: MOD: %s permission denied", admin);
	return KADM_UNAUTH;
    }
    
    numfound = kerb_get_principal(valsin->name, valsin->instance, 
				  &data_o, 1, &more);
    if (numfound == -1) {
	failmod(KADM_DB_INUSE);
    } else if (numfound) {
	kadm_vals_to_prin(valsin2->fields, &temp_key, valsin2);
	strncpy(data_o.name, valsin->name, ANAME_SZ);
	strncpy(data_o.instance, valsin->instance, INST_SZ);
	if (IS_FIELD(KADM_EXPDATE,valsin2->fields))
	    data_o.exp_date = temp_key.exp_date;
	if (IS_FIELD(KADM_ATTR,valsin2->fields))
	    data_o.attributes = temp_key.attributes;
	if (IS_FIELD(KADM_MAXLIFE,valsin2->fields))
	    data_o.max_life = temp_key.max_life; 
	if (IS_FIELD(KADM_DESKEY,valsin2->fields)) {
	    data_o.key_version++;
	    data_o.kdc_key_ver = server_parm.master_key_version;


	    /* convert to host order */
	    temp_key.key_low = ntohl(temp_key.key_low);
	    temp_key.key_high = ntohl(temp_key.key_high);


	    copy_to_key(&temp_key.key_low, &temp_key.key_high, newpw);

	    /* encrypt new key in master key */
	    kdb_encrypt_key (&newpw, &newpw, &server_parm.master_key,
			     server_parm.master_key_schedule, DES_ENCRYPT);
	    copy_from_key(newpw, &data_o.key_low, &data_o.key_high);
	    memset(newpw, 0, sizeof(newpw));
	}
	memset(&temp_key, 0, sizeof(temp_key));

	strncpy(data_o.mod_name, rname, sizeof(data_o.mod_name)-1);
	strncpy(data_o.mod_instance, rinstance,
		sizeof(data_o.mod_instance)-1);
	more = kerb_put_principal(&data_o, 1);

	memset(&data_o, 0, sizeof(data_o));

	if (more == -1) {
	    failmod(KADM_DB_INUSE);
	} else if (more) {
	    failmod(KADM_UK_SERROR);
	} else {
	    numfound = kerb_get_principal(valsin->name, valsin->instance, 
					  &data_o, 1, &more);
	    if ((more!=0)||(numfound!=1)) {
		failmod(KADM_UK_RERROR);
	    }
	    memset(fields, 0, sizeof(fields));
	    SET_FIELD(KADM_NAME,fields);
	    SET_FIELD(KADM_INST,fields);
	    SET_FIELD(KADM_EXPDATE,fields);
	    SET_FIELD(KADM_ATTR,fields);
	    SET_FIELD(KADM_MAXLIFE,fields);
	    kadm_prin_to_vals(fields, valsout, &data_o);
	    krb_log("MOD: %s modified", victim);
	    return KADM_DATA;		/* Set all the appropriate fields */
	}
    }
    else {
	failmod(KADM_NOENTRY);
    }
}

int
kadm_change (char *rname, char *rinstance, char *rrealm, unsigned char *newpw)
{
    long numfound;
    int more;
    Principal data_o;
    des_cblock local_pw;

    char admin[MAX_K_NAME_SZ];
    
    strcpy(admin, krb_unparse_name_long(rname, rinstance, rrealm));
    
    krb_log("CHANGE: %s", admin);

    if (strcmp(server_parm.krbrlm, rrealm)) {
	krb_log("ERROR: CHANGE: request from wrong realm %s", rrealm);
	return(KADM_WRONG_REALM);
    }

    if (wildcard(rname) || wildcard(rinstance)) {
	failchange(KADM_ILL_WILDCARD);
    }

    memcpy(local_pw, newpw, sizeof(local_pw));
  
    /* encrypt new key in master key */
    kdb_encrypt_key (&local_pw, &local_pw, &server_parm.master_key,
		     server_parm.master_key_schedule, DES_ENCRYPT);

    numfound = kerb_get_principal(rname, rinstance, 
				  &data_o, 1, &more);
    if (numfound == -1) {
	failchange(KADM_DB_INUSE);
    } else if (numfound) {
	copy_from_key(local_pw, &data_o.key_low, &data_o.key_high);
	data_o.key_version++;
	data_o.kdc_key_ver = server_parm.master_key_version;
	strncpy(data_o.mod_name, rname, sizeof(data_o.mod_name)-1);
	strncpy(data_o.mod_instance, rinstance,
		sizeof(data_o.mod_instance)-1);
	more = kerb_put_principal(&data_o, 1);
	memset(local_pw, 0, sizeof(local_pw));
	memset(&data_o, 0, sizeof(data_o));
	if (more == -1) {
	    failchange(KADM_DB_INUSE);
	} else if (more) {
	    failchange(KADM_UK_SERROR);
	} else {
	    krb_log("CHANGE: %s's password changed", admin);
	    return KADM_SUCCESS;
	}
    }
    else {
	failchange(KADM_NOENTRY);
    }
}
