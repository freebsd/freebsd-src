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

#include "ktutil_locl.h"

RCSID("$Id: copy.c,v 1.1 2000/01/02 04:41:01 assar Exp $");

int
kt_copy (int argc, char **argv)
{
    krb5_error_code ret;
    int help_flag = 0;
    int optind = 0;
    krb5_keytab src_keytab, dst_keytab;
    krb5_kt_cursor cursor;
    krb5_keytab_entry entry;

    struct getargs args[] = {
	{ "help", 'h', arg_flag, NULL}
    };

    int num_args = sizeof(args) / sizeof(args[0]);
    int i = 0;

    args[i++].value = &help_flag;

    if(getarg(args, num_args, argc, argv, &optind)) {
	arg_printusage(args, num_args, "ktutil copy",
		       "keytab-src keytab-dest");
	return 0;
    }
    if (help_flag) {
	arg_printusage(args, num_args, "ktutil copy",
		       "keytab-src keytab-dest");
	return 0;
    }

    argv += optind;
    argc -= optind;

    if (argc != 2) {
	arg_printusage(args, num_args, "ktutil copy",
		       "keytab-src keytab-dest");
	return 0;
    }

    ret = krb5_kt_resolve (context, argv[0], &src_keytab);
    if (ret) {
	krb5_warn (context, ret, "resolving src keytab `%s'", argv[0]);
	return 0;
    }

    ret = krb5_kt_resolve (context, argv[1], &dst_keytab);
    if (ret) {
	krb5_kt_close (context, src_keytab);
	krb5_warn (context, ret, "resolving dst keytab `%s'", argv[1]);
	return 0;
    }

    ret = krb5_kt_start_seq_get (context, src_keytab, &cursor);
    if (ret) {
	krb5_warn (context, ret, "krb5_kt_start_seq_get");
	goto fail;
    }

    while((ret = krb5_kt_next_entry(context, src_keytab,
				    &entry, &cursor)) == 0) {
	ret = krb5_kt_add_entry (context, dst_keytab, &entry);
	if (verbose_flag) {
	    char *name_str;

	    krb5_unparse_name (context, entry.principal, &name_str);
	    printf ("copying %s\n", name_str);
	    free (name_str);
	}

	krb5_kt_free_entry (context, &entry);
	if (ret) {
	    krb5_warn (context, ret, "krb5_kt_add_entry");
	    break;
	}
    }
    krb5_kt_end_seq_get (context, src_keytab, &cursor);

fail:
    krb5_kt_close (context, src_keytab);
    krb5_kt_close (context, dst_keytab);
    return 0;
}
