/*
 * Copyright (c) 1997 - 2000 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden). 
 * All rights reserved. 
 *
 * Redistribution and use in source and binary forms, with or without 
 * modification, are permitted provided that the following conditions 
 * are met: 
 *
 * 1. Redistributions of source code must retain the above copyright 
 *    notice, this list of conditions and the following disclaimer. 
 *
 * 2. Redistributions in binary form must reproduce the above copyright 
 *    notice, this list of conditions and the following disclaimer in the 
 *    documentation and/or other materials provided with the distribution. 
 *
 * 3. Neither the name of the Institute nor the names of its contributors 
 *    may be used to endorse or promote products derived from this software 
 *    without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND 
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE 
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE 
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE 
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL 
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS 
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) 
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT 
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY 
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF 
 * SUCH DAMAGE. 
 */

#include "kadm5_locl.h"

RCSID("$Id: ent_setup.c,v 1.12 2000/03/23 23:02:35 assar Exp $");

#define set_value(X, V) do { if((X) == NULL) (X) = malloc(sizeof(*(X))); *(X) = V; } while(0)
#define set_null(X)     do { if((X) != NULL) free((X)); (X) = NULL; } while (0)

static void
attr_to_flags(unsigned attr, HDBFlags *flags)
{
    flags->postdate =		!(attr & KRB5_KDB_DISALLOW_POSTDATED);
    flags->forwardable =	!(attr & KRB5_KDB_DISALLOW_FORWARDABLE);
    flags->initial =	       !!(attr & KRB5_KDB_DISALLOW_TGT_BASED);
    flags->renewable =		!(attr & KRB5_KDB_DISALLOW_RENEWABLE);
    flags->proxiable =		!(attr & KRB5_KDB_DISALLOW_PROXIABLE);
    /* DUP_SKEY */
    flags->invalid =	       !!(attr & KRB5_KDB_DISALLOW_ALL_TIX);
    flags->require_preauth =   !!(attr & KRB5_KDB_REQUIRES_PRE_AUTH);
    /* HW_AUTH */
    flags->server =		!(attr & KRB5_KDB_DISALLOW_SVR);
    flags->change_pw = 	       !!(attr & KRB5_KDB_PWCHANGE_SERVICE);
    flags->client =	        1; /* XXX */
}

/*
 * Create the hdb entry `ent' based on data from `princ' with
 * `princ_mask' specifying what fields to be gotten from there and
 * `mask' specifying what fields we want filled in.
 */

kadm5_ret_t
_kadm5_setup_entry(kadm5_server_context *context,
		   hdb_entry *ent,
		   u_int32_t mask,
		   kadm5_principal_ent_t princ, 
		   u_int32_t princ_mask,
		   kadm5_principal_ent_t def,
		   u_int32_t def_mask)
{
    if(mask & KADM5_PRINC_EXPIRE_TIME
       && princ_mask & KADM5_PRINC_EXPIRE_TIME) {
	if (princ->princ_expire_time)
	    set_value(ent->valid_end, princ->princ_expire_time);
	else
	    set_null(ent->valid_end);
    }
    if(mask & KADM5_PW_EXPIRATION
       && princ_mask & KADM5_PW_EXPIRATION) {
	if (princ->pw_expiration)
	    set_value(ent->pw_end, princ->pw_expiration);
	else
	    set_null(ent->pw_end);
    }
    if(mask & KADM5_ATTRIBUTES) {
	if (princ_mask & KADM5_ATTRIBUTES) {
	    attr_to_flags(princ->attributes, &ent->flags);
	} else if(def_mask & KADM5_ATTRIBUTES) {
	    attr_to_flags(def->attributes, &ent->flags);
	    ent->flags.invalid = 0;
	} else {
	    ent->flags.client      = 1;
	    ent->flags.server      = 1;
	    ent->flags.forwardable = 1;
	    ent->flags.proxiable   = 1;
	    ent->flags.renewable   = 1;
	    ent->flags.postdate    = 1;
	}
    }
    if(mask & KADM5_MAX_LIFE) {
	if(princ_mask & KADM5_MAX_LIFE) {
	    if(princ->max_life)
	      set_value(ent->max_life, princ->max_life);
	    else
	      set_null(ent->max_life);
	} else if(def_mask & KADM5_MAX_LIFE) {
	    if(def->max_life)
	      set_value(ent->max_life, def->max_life);
	    else
	      set_null(ent->max_life);
	}
    }
    if(mask & KADM5_KVNO
       && princ_mask & KADM5_KVNO)
	ent->kvno = princ->kvno;
    if(mask & KADM5_MAX_RLIFE) {
	if(princ_mask & KADM5_MAX_RLIFE) {
	  if(princ->max_renewable_life)
	    set_value(ent->max_renew, princ->max_renewable_life);
	  else
	    set_null(ent->max_renew);
	} else if(def_mask & KADM5_MAX_RLIFE) {
	  if(def->max_renewable_life)
	    set_value(ent->max_renew, def->max_renewable_life);
	  else
	    set_null(ent->max_renew);
	}
    }
    if(mask & KADM5_KEY_DATA
       && princ_mask & KADM5_KEY_DATA) {
	_kadm5_set_keys2(context, ent, princ->n_key_data, princ->key_data);
    }
    if(mask & KADM5_TL_DATA) {
	/* XXX */
    }
    if(mask & KADM5_FAIL_AUTH_COUNT) {
	/* XXX */
    }
    return 0;
}
