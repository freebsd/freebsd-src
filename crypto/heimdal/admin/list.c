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

RCSID("$Id: list.c,v 1.1 2000/01/02 04:41:02 assar Exp $");

int
kt_list(int argc, char **argv)
{
    krb5_error_code ret;
    krb5_kt_cursor cursor;
    krb5_keytab_entry entry;

    ret = krb5_kt_start_seq_get(context, keytab, &cursor);
    if(ret){
	krb5_warn(context, ret, "krb5_kt_start_seq_get");
	return 1;
    }
    printf("%s", "Version");
    printf("  ");
    printf("%-15s", "Type");
    printf("  ");
    printf("%s", "Principal");
    printf("\n");
    while((ret = krb5_kt_next_entry(context, keytab, &entry, &cursor)) == 0){
	char *p;
	printf("   %3d ", entry.vno);
	printf("  ");
	ret = krb5_enctype_to_string(context, entry.keyblock.keytype, &p);
	if (ret != 0) 
	    asprintf(&p, "unknown (%d)", entry.keyblock.keytype);
	printf("%-15s", p);
	free(p);
	printf("  ");
	krb5_unparse_name(context, entry.principal, &p);
	printf("%s ", p);
	free(p);
	printf("\n");
	if (verbose_flag) {
	    char tstamp[256];
	    struct tm *tm;
	    time_t ts = entry.timestamp;

	    tm = gmtime (&ts);
	    strftime (tstamp, sizeof(tstamp), "%Y-%m-%d %H:%M:%S UTC", tm);
	    printf("   Timestamp: %s\n", tstamp);
	}
	krb5_kt_free_entry(context, &entry);
    }
    ret = krb5_kt_end_seq_get(context, keytab, &cursor);
    return 0;
}
