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

RCSID("$Id: cpw.c,v 1.11 2000/04/12 10:45:54 assar Exp $");

struct cpw_entry_data {
    int random_key;
    int random_password;
    char *password;
    krb5_key_data *key_data;
};

static struct getargs args[] = {
    { "random-key",	'r',	arg_flag,	NULL, "set random key" },
    { "random-password", 0,	arg_flag,	NULL, "set random password" },
    { "password",	'p',	arg_string,	NULL, "princial's password" },
    { "key",		 0,	arg_string,	NULL, "DES key in hex" }
};

static int num_args = sizeof(args) / sizeof(args[0]);

static void
usage(void)
{
    arg_printusage(args, num_args, "cpw", "principal...");
}

static int
set_random_key (krb5_principal principal)
{
    krb5_error_code ret;
    int i;
    krb5_keyblock *keys;
    int num_keys;

    ret = kadm5_randkey_principal(kadm_handle, principal, &keys, &num_keys);
    if(ret)
	return ret;
    for(i = 0; i < num_keys; i++)
	krb5_free_keyblock_contents(context, &keys[i]);
    free(keys);
    return 0;
}

static int
set_random_password (krb5_principal principal)
{
    krb5_error_code ret;
    char pw[128];

    random_password (pw, sizeof(pw));
    ret = kadm5_chpass_principal(kadm_handle, principal, pw);
    if (ret == 0) {
	char *princ_name;

	krb5_unparse_name(context, principal, &princ_name);

	printf ("%s's password set to `%s'\n", princ_name, pw);
	free (princ_name);
    }
    memset (pw, 0, sizeof(pw));
    return ret;
}

static int
set_password (krb5_principal principal, char *password)
{
    krb5_error_code ret = 0;
    char pwbuf[128];

    if(password == NULL) {
	char *princ_name;
	char *prompt;

	krb5_unparse_name(context, principal, &princ_name);
	asprintf(&prompt, "%s's Password: ", princ_name);
	free (princ_name);
	ret = des_read_pw_string(pwbuf, sizeof(pwbuf), prompt, 1);
	free (prompt);
	if(ret){
	    return 0; /* XXX error code? */
	}
	password = pwbuf;
    }
    if(ret == 0)
	ret = kadm5_chpass_principal(kadm_handle, principal, password);
    memset(pwbuf, 0, sizeof(pwbuf));
    return ret;
}

static int
set_key_data (krb5_principal principal, krb5_key_data *key_data)
{
    krb5_error_code ret;

    ret = kadm5_chpass_principal_with_key (kadm_handle, principal,
					   3, key_data);
    return ret;
}

static int
do_cpw_entry(krb5_principal principal, void *data)
{
    struct cpw_entry_data *e = data;
    
    if (e->random_key)
	return set_random_key (principal);
    else if (e->random_password)
	return set_random_password (principal);
    else if (e->key_data)
	return set_key_data (principal, e->key_data);
    else
	return set_password (principal, e->password);
}

int
cpw_entry(int argc, char **argv)
{
    krb5_error_code ret;
    int i;
    int optind = 0;
    struct cpw_entry_data data;
    int num;
    char *key_string;
    krb5_key_data key_data[3];

    data.random_key      = 0;
    data.random_password = 0;
    data.password        = NULL;
    data.key_data	 = NULL;

    key_string = NULL;

    args[0].value = &data.random_key;
    args[1].value = &data.random_password;
    args[2].value = &data.password;
    args[3].value = &key_string;
    if(getarg(args, num_args, argc, argv, &optind)){
	usage();
	return 0;
    }

    num = 0;
    if (data.random_key)
	++num;
    if (data.random_password)
	++num;
    if (data.password)
	++num;
    if (key_string)
	++num;

    if (num > 1) {
	printf ("give only one of "
		"--random-key, --random-password, --password, --key\n");
	return 0;
    }
	
    if (key_string) {
	const char *error;

	if (parse_des_key (key_string, key_data, &error)) {
	    printf ("failed parsing key `%s': %s\n", key_string, error);
	    return 0;
	}
	data.key_data = key_data;
    }

    argc -= optind;
    argv += optind;

    for(i = 0; i < argc; i++)
	ret = foreach_principal(argv[i], do_cpw_entry, &data);

    if (data.key_data) {
	int16_t dummy;
	kadm5_free_key_data (kadm_handle, &dummy, key_data);
    }

    return 0;
}
