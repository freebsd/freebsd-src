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

RCSID("$Id: srvcreate.c,v 1.3 1999/12/02 17:04:53 joda Exp $");

/* convert a version 5 keytab to a version 4 srvtab */

#ifndef KEYFILE
#define KEYFILE "/etc/srvtab"
#endif

static char *srvtab = KEYFILE;
static int help_flag;
static int verbose;

static struct getargs args[] = {
    { "srvtab", 's', arg_string, &srvtab, "srvtab to create", "file" },
    { "help", 'h', arg_flag, &help_flag },
    { "verbose", 'v', arg_flag, &verbose },
};

static int num_args = sizeof(args) / sizeof(args[0]);

int
srvcreate(int argc, char **argv)
{
    krb5_error_code ret;
    int optind = 0;
    int fd;
    krb5_kt_cursor cursor;
    krb5_keytab_entry entry;
    char service[100], instance[100], realm[100];
    int8_t kvno;

    if(getarg(args, num_args, argc, argv, &optind)){
	arg_printusage(args, num_args, "ktutil srvcreate", "");
	return 1;
    }
    if(help_flag){
	arg_printusage(args, num_args, "ktutil srvcreate", "");
	return 0;
    }

    argc -= optind;
    argv += optind;

    if (argc != 0) {
	arg_printusage(args, num_args, "ktutil srvcreate", "");
	return 1;
    }

    ret = krb5_kt_start_seq_get(context, keytab, &cursor);
    if(ret){
        krb5_warn(context, ret, "krb5_kt_start_seq_get");
        return 1;
    }

    fd = open(srvtab, O_WRONLY |O_APPEND |O_CREAT, 0600);
    if(fd < 0){
	krb5_warn(context, errno, "%s", srvtab);
	return 1;
    }

    while((ret = krb5_kt_next_entry(context, keytab, &entry, &cursor)) == 0){
      ret = krb5_524_conv_principal(context, entry.principal,
				    service, instance, realm);
      if(ret) {
        krb5_warn(context, ret, "krb5_524_conv_principal");
        close(fd);
        return 1;
      }
      if ( (entry.keyblock.keyvalue.length == 8) && 
           (entry.keyblock.keytype == ETYPE_DES_CBC_MD5) ) {
	  if (verbose) {
	      printf ("%s.%s@%s vno %d\n", service, instance, realm,
		      entry.vno);
	  }

	  write(fd, service, strlen(service)+1);
          write(fd, instance, strlen(instance)+1);
          write(fd, realm, strlen(realm)+1);
          kvno = entry.vno;
          write(fd, &kvno, sizeof(kvno));
          write(fd, entry.keyblock. keyvalue.data, 8);
      }
      krb5_kt_free_entry(context, &entry);
    }

    close(fd);
    ret = krb5_kt_end_seq_get(context, keytab, &cursor);
    return ret;
}
