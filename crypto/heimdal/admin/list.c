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

RCSID("$Id: list.c,v 1.3 2000/06/29 08:21:40 joda Exp $");

static int help_flag;
static int list_keys;
static int list_timestamp;

static struct getargs args[] = {
    { "help",      'h', arg_flag, &help_flag },
    { "keys",	   0,   arg_flag, &list_keys, "show key value" },
    { "timestamp", 0,   arg_flag, &list_timestamp, "show timestamp" },
};

static int num_args = sizeof(args) / sizeof(args[0]);

struct key_info {
    char *version;
    char *etype;
    char *principal;
    char *timestamp;
    char *key;
    struct key_info *next;
};

int
kt_list(int argc, char **argv)
{
    krb5_error_code ret;
    krb5_kt_cursor cursor;
    krb5_keytab_entry entry;
    int optind = 0;
    struct key_info *ki, **kie = &ki, *kp;

    int max_version = sizeof("Vno") - 1;
    int max_etype = sizeof("Type") - 1;
    int max_principal = sizeof("Principal") - 1;
    int max_timestamp = sizeof("Date") - 1;
    int max_key = sizeof("Key") - 1;

    if(verbose_flag)
	list_timestamp = 1;

    if(getarg(args, num_args, argc, argv, &optind)){
	arg_printusage(args, num_args, "ktutil list", "");
	return 1;
    }
    if(help_flag){
	arg_printusage(args, num_args, "ktutil list", "");
	return 0;
    }

    ret = krb5_kt_start_seq_get(context, keytab, &cursor);
    if(ret){
	krb5_warn(context, ret, "krb5_kt_start_seq_get %s", keytab_string);
	return 1;
    }
    while((ret = krb5_kt_next_entry(context, keytab, &entry, &cursor)) == 0){
#define CHECK_MAX(F) if(max_##F < strlen(kp->F)) max_##F = strlen(kp->F)

	kp = malloc(sizeof(*kp));

	asprintf(&kp->version, "%d", entry.vno);
	CHECK_MAX(version);
	ret = krb5_enctype_to_string(context, 
				     entry.keyblock.keytype, &kp->etype);
	if (ret != 0) 
	    asprintf(&kp->etype, "unknown (%d)", entry.keyblock.keytype);
	CHECK_MAX(etype);
	krb5_unparse_name_short(context, entry.principal, &kp->principal);
	CHECK_MAX(principal);
	if (list_timestamp) {
	    char tstamp[256];

	    krb5_format_time(context, entry.timestamp, 
			     tstamp, sizeof(tstamp), FALSE);

	    kp->timestamp = strdup(tstamp);
	    CHECK_MAX(timestamp);
	}
	if(list_keys) {
	    int i;
	    kp->key = malloc(2 * entry.keyblock.keyvalue.length + 1);
	    for(i = 0; i < entry.keyblock.keyvalue.length; i++)
		snprintf(kp->key + 2 * i, 3, "%02x", 
			 ((unsigned char*)entry.keyblock.keyvalue.data)[i]);
	    CHECK_MAX(key);
	}
	kp->next = NULL;
	*kie = kp;
	kie = &kp->next;
	krb5_kt_free_entry(context, &entry);
    }
    ret = krb5_kt_end_seq_get(context, keytab, &cursor);

    printf("%-*s  %-*s  %-*s", max_version, "Vno", 
	   max_etype, "Type", 
	   max_principal, "Principal");
    if(list_timestamp)
	printf("  %-*s", max_timestamp, "Date");
    if(list_keys)
	printf("  %s", "Key");
    printf("\n");

    for(kp = ki; kp; ) {
	printf("%*s  %-*s  %-*s", max_version, kp->version, 
	       max_etype, kp->etype, 
	       max_principal, kp->principal);
	if(list_timestamp)
	    printf("  %-*s", max_timestamp, kp->timestamp);
	if(list_keys)
	    printf("  %s", kp->key);
	printf("\n");

	/* free entries */
	free(kp->version);
	free(kp->etype);
	free(kp->principal);
	if(list_timestamp)
	    free(kp->timestamp);
	if(list_keys) {
	    memset(kp->key, 0, strlen(kp->key));
	    free(kp->key);
	}
	ki = kp;
	kp = kp->next;
	free(ki);
    }
    return 0;
}
