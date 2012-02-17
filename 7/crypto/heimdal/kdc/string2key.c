/*
 * Copyright (c) 1997-2003 Kungliga Tekniska Högskolan
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

#include "headers.h"
#include <getarg.h>

RCSID("$Id: string2key.c,v 1.20 2003/03/25 12:28:52 joda Exp $");

int version5;
int version4;
int afs;
char *principal;
char *cell;
char *password;
const char *keytype_str = "des3-cbc-sha1";
int version;
int help;

struct getargs args[] = {
    { "version5", '5', arg_flag,   &version5, "Output Kerberos v5 string-to-key" },
    { "version4", '4', arg_flag,   &version4, "Output Kerberos v4 string-to-key" },
    { "afs",      'a', arg_flag,   &afs, "Output AFS string-to-key" },
    { "cell",     'c', arg_string, &cell, "AFS cell to use", "cell" },
    { "password", 'w', arg_string, &password, "Password to use", "password" },
    { "principal",'p', arg_string, &principal, "Kerberos v5 principal to use", "principal" },
    { "keytype",  'k', arg_string, &keytype_str, "Keytype" },
    { "version",    0, arg_flag,   &version, "print version" },
    { "help",       0, arg_flag,   &help, NULL }
};

int num_args = sizeof(args) / sizeof(args[0]);

static void
usage(int status)
{
    arg_printusage (args, num_args, NULL, "password");
    exit(status);
}

static void
tokey(krb5_context context, 
      krb5_enctype enctype, 
      const char *password, 
      krb5_salt salt, 
      const char *label)
{
    int i;
    krb5_keyblock key;
    char *e;
    krb5_string_to_key_salt(context, enctype, password, salt, &key);
    krb5_enctype_to_string(context, enctype, &e);
    printf(label, e);
    printf(": ");
    for(i = 0; i < key.keyvalue.length; i++)
	printf("%02x", ((unsigned char*)key.keyvalue.data)[i]);
    printf("\n");
    krb5_free_keyblock_contents(context, &key);
}

int
main(int argc, char **argv)
{
    krb5_context context;
    krb5_principal princ;
    krb5_salt salt;
    int optind;
    char buf[1024];
    krb5_enctype etype;
    krb5_error_code ret;

    optind = krb5_program_setup(&context, argc, argv, args, num_args, NULL);

    if(help)
	usage(0);
    
    if(version){
	print_version (NULL);
	return 0;
    }

    argc -= optind;
    argv += optind;

    if (argc > 1)
	usage(1);

    if(!version5 && !version4 && !afs)
	version5 = 1;

    ret = krb5_string_to_enctype(context, keytype_str, &etype);
    if(ret) {
	krb5_keytype keytype;
	int *etypes;
	unsigned num;
	ret = krb5_string_to_keytype(context, keytype_str, &keytype);
	if(ret)
	    krb5_err(context, 1, ret, "%s", keytype_str);
	ret = krb5_keytype_to_enctypes(context, keytype, &num, &etypes);
	if(ret)
	    krb5_err(context, 1, ret, "%s", keytype_str);
	if(num == 0)
	    krb5_errx(context, 1, "there are no encryption types for that keytype");
	etype = etypes[0];
	krb5_enctype_to_string(context, etype, &keytype_str);
	if(num > 1 && version5)
	    krb5_warnx(context, "ambiguous keytype, using %s", keytype_str);
    }
    
    if((etype != ETYPE_DES_CBC_CRC &&
	etype != ETYPE_DES_CBC_MD4 &&
	etype != ETYPE_DES_CBC_MD5) &&
       (afs || version4)) {
	if(!version5) {
	    etype = ETYPE_DES_CBC_CRC;
	} else {
	    krb5_errx(context, 1, 
		      "DES is the only valid keytype for AFS and Kerberos 4");
	}
    }

    if(version5 && principal == NULL){
	printf("Kerberos v5 principal: ");
	if(fgets(buf, sizeof(buf), stdin) == NULL)
	    return 1;
	if(buf[strlen(buf) - 1] == '\n')
	    buf[strlen(buf) - 1] = '\0';
	principal = estrdup(buf);
    }
    if(afs && cell == NULL){
	printf("AFS cell: ");
	if(fgets(buf, sizeof(buf), stdin) == NULL)
	    return 1;
	if(buf[strlen(buf) - 1] == '\n')
	    buf[strlen(buf) - 1] = '\0';
	cell = estrdup(buf);
    }
    if(argv[0])
	password = argv[0];
    if(password == NULL){
	if(des_read_pw_string(buf, sizeof(buf), "Password: ", 0))
	    return 1;
	password = buf;
    }
	
    if(version5){
	krb5_parse_name(context, principal, &princ);
	krb5_get_pw_salt(context, princ, &salt);
	tokey(context, etype, password, salt, "Kerberos 5 (%s)");
	krb5_free_salt(context, salt);
    }
    if(version4){
	salt.salttype = KRB5_PW_SALT;
	salt.saltvalue.length = 0;
	salt.saltvalue.data = NULL;
	tokey(context, ETYPE_DES_CBC_MD5, password, salt, "Kerberos 4");
    }
    if(afs){
	salt.salttype = KRB5_AFS3_SALT;
	salt.saltvalue.length = strlen(cell);
	salt.saltvalue.data = cell;
	tokey(context, ETYPE_DES_CBC_MD5, password, salt, "AFS");
    }
    return 0;
}
