/*
 * Copyright (c) 1997 - 2001 Kungliga Tekniska Högskolan
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

#include "ktutil_locl.h"

RCSID("$Id: add.c,v 1.3 2001/07/23 09:46:40 joda Exp $");

int
kt_add(int argc, char **argv)
{
    krb5_error_code ret;
    krb5_keytab keytab;
    krb5_keytab_entry entry;
    char buf[128];
    char *principal_string = NULL;
    int kvno = -1;
    char *enctype_string = NULL;
    krb5_enctype enctype;
    char *password_string = NULL;
    int salt_flag = 1;
    int random_flag = 0;
    int help_flag = 0;
    struct getargs args[] = {
	{ "principal", 'p', arg_string, NULL, "principal of key", "principal"},
	{ "kvno", 'V', arg_integer, NULL, "key version of key" },
	{ "enctype", 'e', arg_string, NULL, "encryption type of key" },
	{ "password", 'w', arg_string, NULL, "password for key"},
	{ "salt", 's',	arg_negative_flag, NULL, "no salt" },
	{ "random",  'r', arg_flag, NULL, "generate random key" },
	{ "help", 'h', arg_flag, NULL }
    };
    int num_args = sizeof(args) / sizeof(args[0]);
    int optind = 0;
    int i = 0;
    args[i++].value = &principal_string;
    args[i++].value = &kvno;
    args[i++].value = &enctype_string;
    args[i++].value = &password_string;
    args[i++].value = &salt_flag;
    args[i++].value = &random_flag;
    args[i++].value = &help_flag;

    if(getarg(args, num_args, argc, argv, &optind)) {
	arg_printusage(args, num_args, "ktutil add", "");
	return 1;
    }
    if(help_flag) {
	arg_printusage(args, num_args, "ktutil add", "");
	return 1;
    }
    if((keytab = ktutil_open_keytab()) == NULL)
	return 1;

    memset(&entry, 0, sizeof(entry));
    if(principal_string == NULL) {
	printf("Principal: ");
	if (fgets(buf, sizeof(buf), stdin) == NULL)
	    return 1;
	buf[strcspn(buf, "\r\n")] = '\0';
	principal_string = buf;
    }
    ret = krb5_parse_name(context, principal_string, &entry.principal);
    if(ret) {
	krb5_warn(context, ret, "%s", principal_string);
	goto out;
    }
    if(enctype_string == NULL) {
	printf("Encryption type: ");
	if (fgets(buf, sizeof(buf), stdin) == NULL)
	    goto out;
	buf[strcspn(buf, "\r\n")] = '\0';
	enctype_string = buf;
    }
    ret = krb5_string_to_enctype(context, enctype_string, &enctype);
    if(ret) {
	int t;
	if(sscanf(enctype_string, "%d", &t) == 1)
	    enctype = t;
	else {
	    krb5_warn(context, ret, "%s", enctype_string);
	    goto out;
	}
    }
    if(kvno == -1) {
	printf("Key version: ");
	if (fgets(buf, sizeof(buf), stdin) == NULL)
	    goto out;
	buf[strcspn(buf, "\r\n")] = '\0';
	kvno = atoi(buf);
    }
    if(password_string == NULL && random_flag == 0) {
	if(des_read_pw_string(buf, sizeof(buf), "Password: ", 1))
	    goto out;
	password_string = buf;
    }
    if(password_string) {
	if (!salt_flag) {
	    krb5_salt salt;
	    krb5_data pw;

	    salt.salttype         = KRB5_PW_SALT;
	    salt.saltvalue.data   = NULL;
	    salt.saltvalue.length = 0;
	    pw.data = (void*)password_string;
	    pw.length = strlen(password_string);
	    krb5_string_to_key_data_salt(context, enctype, pw, salt,
					 &entry.keyblock);
        } else {
	    krb5_string_to_key(context, enctype, password_string, 
			       entry.principal, &entry.keyblock);
	}
	memset (password_string, 0, strlen(password_string));
    } else {
	krb5_generate_random_keyblock(context, enctype, &entry.keyblock);
    }
    entry.vno = kvno;
    entry.timestamp = time (NULL);
    ret = krb5_kt_add_entry(context, keytab, &entry);
    if(ret)
	krb5_warn(context, ret, "add");
 out:
    krb5_kt_free_entry(context, &entry);
    krb5_kt_close(context, keytab);
    return 0;
}
