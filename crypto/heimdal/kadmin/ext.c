/*
 * Copyright (c) 1997 - 2002 Kungliga Tekniska Högskolan
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

RCSID("$Id: ext.c,v 1.8 2002/02/11 14:29:52 joda Exp $");

struct ext_keytab_data {
    krb5_keytab keytab;
};

static struct getargs args[] = {
    { "keytab",		'k',	arg_string,	NULL, "keytab to use" },
};

static int num_args = sizeof(args) / sizeof(args[0]);

static void
usage(void)
{
    arg_printusage(args, num_args, "ext", "principal...");
}

static int
do_ext_keytab(krb5_principal principal, void *data)
{
    krb5_error_code ret;
    int i;
    kadm5_principal_ent_rec princ;
    struct ext_keytab_data *e = data;

    ret = kadm5_get_principal(kadm_handle, principal, &princ, 
			      KADM5_PRINCIPAL|KADM5_KVNO|KADM5_KEY_DATA);
    if(ret)
	return ret;
    for(i = 0; i < princ.n_key_data; i++){
	krb5_keytab_entry key;
	krb5_key_data *k = &princ.key_data[i];
	key.principal = princ.principal;
	key.vno = k->key_data_kvno;
	key.keyblock.keytype = k->key_data_type[0];
	key.keyblock.keyvalue.length = k->key_data_length[0];
	key.keyblock.keyvalue.data = k->key_data_contents[0];
	key.timestamp = time(NULL);
	ret = krb5_kt_add_entry(context, e->keytab, &key);
	if(ret)
	    krb5_warn(context, ret, "krb5_kt_add_entry");
    }
    kadm5_free_principal_ent(kadm_handle, &princ);
    return 0;
}

int
ext_keytab(int argc, char **argv)
{
    krb5_error_code ret;
    int i;
    int optind = 0;
    char *keytab = NULL;
    struct ext_keytab_data data;
    
    args[0].value = &keytab;
    if(getarg(args, num_args, argc, argv, &optind)){
	usage();
	return 0;
    }
    if (keytab == NULL)
	ret = krb5_kt_default(context, &data.keytab);
    else
	ret = krb5_kt_resolve(context, keytab, &data.keytab);

    if(ret){
	krb5_warn(context, ret, "krb5_kt_resolve");
	return 0;
    }

    argc -= optind;
    argv += optind;

    for(i = 0; i < argc; i++) 
	foreach_principal(argv[i], do_ext_keytab, "ext", &data);

    krb5_kt_close(context, data.keytab);

    return 0;
}
