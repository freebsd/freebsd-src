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

RCSID("$Id: remove.c,v 1.2 2001/05/10 15:44:58 assar Exp $");

int
kt_remove(int argc, char **argv)
{
    krb5_error_code ret = 0;
    krb5_keytab_entry entry;
    krb5_keytab keytab;
    char *principal_string = NULL;
    krb5_principal principal = NULL;
    int kvno = 0;
    char *keytype_string = NULL;
    krb5_enctype enctype = 0;
    int help_flag = 0;
    struct getargs args[] = {
	{ "principal", 'p', arg_string, NULL, "principal to remove" },
	{ "kvno", 'V', arg_integer, NULL, "key version to remove" },
	{ "enctype", 'e', arg_string, NULL, "enctype to remove" },
	{ "help", 'h', arg_flag, NULL }
    };
    int num_args = sizeof(args) / sizeof(args[0]);
    int optind = 0;
    int i = 0;
    args[i++].value = &principal_string;
    args[i++].value = &kvno;
    args[i++].value = &keytype_string;
    args[i++].value = &help_flag;
    if(getarg(args, num_args, argc, argv, &optind)) {
	arg_printusage(args, num_args, "ktutil remove", "");
	return 1;
    }
    if(help_flag) {
	arg_printusage(args, num_args, "ktutil remove", "");
	return 0;
    }
    if(principal_string) {
	ret = krb5_parse_name(context, principal_string, &principal);
	if(ret) {
	    krb5_warn(context, ret, "%s", principal_string);
	    return 1;
	}
    }
    if(keytype_string) {
	ret = krb5_string_to_enctype(context, keytype_string, &enctype);
	if(ret) {
	    int t;
	    if(sscanf(keytype_string, "%d", &t) == 1)
		enctype = t;
	    else {
		krb5_warn(context, ret, "%s", keytype_string);
		if(principal)
		    krb5_free_principal(context, principal);
		return 1;
	    }
	}
    }
    if (!principal && !enctype && !kvno) {
	krb5_warnx(context, 
		   "You must give at least one of "
		   "principal, enctype or kvno.");
	return 1;
    }

    if (keytab_string == NULL) {
	ret = krb5_kt_default_modify_name (context, keytab_buf,
					   sizeof(keytab_buf));
	if (ret) {
	    krb5_warn(context, ret, "krb5_kt_default_modify_name");
	    return 1;
	}
	keytab_string = keytab_buf;
    }
    ret = krb5_kt_resolve(context, keytab_string, &keytab);
    if (ret) {
	krb5_warn(context, ret, "resolving keytab %s", keytab_string);
	return 1;
    }

    if (verbose_flag)
	fprintf (stderr, "Using keytab %s\n", keytab_string);
	
    entry.principal = principal;
    entry.keyblock.keytype = enctype;
    entry.vno = kvno;
    ret = krb5_kt_remove_entry(context, keytab, &entry);
    krb5_kt_close(context, keytab);
    if(ret)
	krb5_warn(context, ret, "remove");
    if(principal)
	krb5_free_principal(context, principal);
    return 0;
}

