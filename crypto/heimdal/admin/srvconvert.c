/*
 * Copyright (c) 1997, 1998, 1999 Kungliga Tekniska Högskolan
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

RCSID("$Id: srvconvert.c,v 1.11 2000/01/02 03:56:21 assar Exp $");

/* convert a version 4 srvtab to a version 5 keytab */

#ifndef KEYFILE
#define KEYFILE "/etc/srvtab"
#endif

static char *srvtab = KEYFILE;
static int help_flag;
static int verbose;

static struct getargs args[] = {
    { "srvtab", 's', arg_string, &srvtab, "srvtab to convert", "file" },
    { "help", 'h', arg_flag, &help_flag },
    { "verbose", 'v', arg_flag, &verbose },
};

static int num_args = sizeof(args) / sizeof(args[0]);

int
srvconv(int argc, char **argv)
{
    krb5_error_code ret;
    int optind = 0;
    int fd;
    krb5_storage *sp;

    if(getarg(args, num_args, argc, argv, &optind)){
	arg_printusage(args, num_args, "ktutil srvconvert", "");
	return 1;
    }
    if(help_flag){
	arg_printusage(args, num_args, "ktutil srvconvert", "");
	return 0;
    }

    argc -= optind;
    argv += optind;

    if (argc != 0) {
	arg_printusage(args, num_args, "ktutil srvconvert", "");
	return 1;
    }

    fd = open(srvtab, O_RDONLY);
    if(fd < 0){
	krb5_warn(context, errno, "%s", srvtab);
	return 1;
    }
    sp = krb5_storage_from_fd(fd);
    if(sp == NULL){
	close(fd);
	return 1;
    }
    while(1){
	char *service, *instance, *realm;
	int8_t kvno;
	des_cblock key;
	krb5_keytab_entry entry;
	
	ret = krb5_ret_stringz(sp, &service);
	if(ret == KRB5_CC_END) {
	    ret = 0;
	    break;
	}
	if(ret) {
	    krb5_warn(context, ret, "reading service");
	    break;
	}
	ret = krb5_ret_stringz(sp, &instance);
	if(ret) {
	    krb5_warn(context, ret, "reading instance");
	    free(service);
	    break;
	}
	ret = krb5_ret_stringz(sp, &realm);
	if(ret) {
	    krb5_warn(context, ret, "reading realm");
	    free(service);
	    free(instance);
	    break;
	}
	ret = krb5_425_conv_principal(context, service, instance, realm,
				      &entry.principal);
	free(service);
	free(instance);
	free(realm);
	if (ret) {
	    krb5_warn(context, ret, "krb5_425_conv_principal (%s.%s@%s)",
		      service, instance, realm);
	    break;
	}
	
	ret = krb5_ret_int8(sp, &kvno);
	if(ret) {
	    krb5_warn(context, ret, "reading kvno");
	    krb5_free_principal(context, entry.principal);
	    break;
	}
	ret = sp->fetch(sp, key, 8);
	if(ret < 0){
	    krb5_warn(context, errno, "reading key");
	    krb5_free_principal(context, entry.principal);
	    break;
	}
	if(ret < 8) {
	    krb5_warn(context, errno, "end of file while reading key");
	    krb5_free_principal(context, entry.principal);
	    break;
	}
	
	entry.vno = kvno;
	entry.timestamp = time (NULL);
	entry.keyblock.keyvalue.data = key;
	entry.keyblock.keyvalue.length = 8;
	
	if(verbose){
	    char *p;
	    ret = krb5_unparse_name(context, entry.principal, &p);
	    if(ret){
		krb5_warn(context, ret, "krb5_unparse_name");
		krb5_free_principal(context, entry.principal);
		break;
	    } else{
		fprintf(stderr, "Storing keytab for %s\n", p);
		free(p);
	    }
				    
	}
	entry.keyblock.keytype = ETYPE_DES_CBC_MD5;
	ret = krb5_kt_add_entry(context, keytab, &entry);
	entry.keyblock.keytype = ETYPE_DES_CBC_MD4;
	ret = krb5_kt_add_entry(context, keytab, &entry);
	entry.keyblock.keytype = ETYPE_DES_CBC_CRC;
	ret = krb5_kt_add_entry(context, keytab, &entry);
	krb5_free_principal(context, entry.principal);
	if(ret) {
	    krb5_warn(context, ret, "krb5_kt_add_entry");
	    break;
	}
    }
    krb5_storage_free(sp);
    close(fd);
    return ret;
}
