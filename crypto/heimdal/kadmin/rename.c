/*
 * Copyright (c) 1997-2000 Kungliga Tekniska Högskolan
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

RCSID("$Id: rename.c,v 1.3 2000/09/10 19:19:20 joda Exp $");

static struct getargs args[] = {
    { "help", 'h', arg_flag, NULL }
};

static int num_args = sizeof(args) / sizeof(args[0]);

static void
usage(void)
{
    arg_printusage (args, num_args, "rename", "from to");
}

int
rename_entry(int argc, char **argv)
{
    int optind = 0;
    int help_flag = 0;

    krb5_error_code ret;
    krb5_principal princ1, princ2;

    args[0].value = &help_flag;

    if(getarg(args, num_args, argc, argv, &optind)) {
	usage ();
	return 0;
    }
    if(argc - optind < 3 || help_flag) {
	usage ();
	return 0;
    }

    ret = krb5_parse_name(context, argv[1], &princ1);
    if(ret){
	krb5_warn(context, ret, "krb5_parse_name(%s)", argv[1]);
	return 0;
    }
    ret = krb5_parse_name(context, argv[2], &princ2);
    if(ret){
	krb5_free_principal(context, princ2);
	krb5_warn(context, ret, "krb5_parse_name(%s)", argv[2]);
	return 0;
    }
    ret = kadm5_rename_principal(kadm_handle, princ1, princ2);
    if(ret)
	krb5_warn(context, ret, "rename");
    krb5_free_principal(context, princ1);
    krb5_free_principal(context, princ2);
    return 0;
}

