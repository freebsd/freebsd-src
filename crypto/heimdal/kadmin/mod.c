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

#include "kadmin_locl.h"

RCSID("$Id: mod.c,v 1.10 2000/07/11 14:34:56 joda Exp $");

static int parse_args (krb5_context context, kadm5_principal_ent_t ent,
		       int argc, char **argv, int *optind, char *name,
		       int *mask);

static int
parse_args(krb5_context context, kadm5_principal_ent_t ent,
	    int argc, char **argv, int *optind, char *name,
	    int *mask)
{
    char *attr_str = NULL;
    char *max_life_str = NULL;
    char *max_rlife_str = NULL;
    char *expiration_str = NULL;
    char *pw_expiration_str = NULL;
    int new_kvno = -1;
    int ret, i;

    struct getargs args[] = {
	{"attributes",	'a',	arg_string, NULL, "Attributies",
	 "attributes"},
	{"max-ticket-life", 0,	arg_string, NULL, "max ticket lifetime",
	 "lifetime"},
	{"max-renewable-life",  0, arg_string,	NULL,
	 "max renewable lifetime", "lifetime" },
	{"expiration-time",	0,	arg_string, 
	 NULL, "Expiration time", "time"},
	{"pw-expiration-time",  0,	arg_string, 
	 NULL, "Password expiration time", "time"},
	{"kvno",  0,	arg_integer, 
	 NULL, "Key version number", "number"},
    };

    i = 0;
    args[i++].value = &attr_str;
    args[i++].value = &max_life_str;
    args[i++].value = &max_rlife_str;
    args[i++].value = &expiration_str;
    args[i++].value = &pw_expiration_str;
    args[i++].value = &new_kvno;

    *optind = 0; /* XXX */

    if(getarg(args, sizeof(args) / sizeof(args[0]), 
	      argc, argv, optind)){
	arg_printusage(args, 
		       sizeof(args) / sizeof(args[0]), 
		       name ? name : "",
		       "principal");
	return -1;
    }
    
    ret = set_entry(context, ent, mask, max_life_str, max_rlife_str, 
		    expiration_str, pw_expiration_str, attr_str);
    if (ret)
	return ret;

    if(new_kvno != -1) {
	ent->kvno = new_kvno;
	*mask |= KADM5_KVNO;
    }
    return 0;
}

int
mod_entry(int argc, char **argv)
{
    kadm5_principal_ent_rec princ;
    int mask = 0;
    krb5_error_code ret;
    krb5_principal princ_ent = NULL;
    int optind;

    memset (&princ, 0, sizeof(princ));

    ret = parse_args (context, &princ, argc, argv,
		      &optind, "mod", &mask);
    if (ret)
	return 0;

    argc -= optind;
    argv += optind;
    
    if (argc != 1) {
	printf ("Usage: mod [options] principal\n");
	return 0;
    }

    krb5_parse_name(context, argv[0], &princ_ent);

    if (mask == 0) {
	memset(&princ, 0, sizeof(princ));
	ret = kadm5_get_principal(kadm_handle, princ_ent, &princ, 
				  KADM5_PRINCIPAL | KADM5_ATTRIBUTES | 
				  KADM5_MAX_LIFE | KADM5_MAX_RLIFE |
				  KADM5_PRINC_EXPIRE_TIME |
				  KADM5_PW_EXPIRATION);
	krb5_free_principal (context, princ_ent);
	if (ret) {
	    printf ("no such principal: %s\n", argv[0]);
	    return 0;
	}
	edit_entry(&princ, &mask, NULL, 0);
    } else {
	princ.principal = princ_ent;
    }

    ret = kadm5_modify_principal(kadm_handle, &princ, mask);
    if(ret)
	krb5_warn(context, ret, "kadm5_modify_principal");
    kadm5_free_principal_ent(kadm_handle, &princ);
    return 0;
}
