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

RCSID("$Id: purge.c,v 1.5 2001/05/11 00:54:01 assar Exp $");

/*
 * keep track of the highest version for every principal.
 */

struct e {
    krb5_principal principal;
    int max_vno;
    struct e *next;
};

static struct e *
get_entry (krb5_principal princ, struct e *head)
{
    struct e *e;

    for (e = head; e != NULL; e = e->next)
	if (krb5_principal_compare (context, princ, e->principal))
	    return e;
    return NULL;
}

static void
add_entry (krb5_principal princ, int vno, struct e **head)
{
    krb5_error_code ret;
    struct e *e;

    e = get_entry (princ, *head);
    if (e != NULL) {
	e->max_vno = max (e->max_vno, vno);
	return;
    }
    e = malloc (sizeof (*e));
    if (e == NULL)
	krb5_errx (context, 1, "malloc: out of memory");
    ret = krb5_copy_principal (context, princ, &e->principal);
    if (ret)
	krb5_err (context, 1, ret, "krb5_copy_principal");
    e->max_vno = vno;
    e->next    = *head;
    *head      = e;
}

static void
delete_list (struct e *head)
{
    while (head != NULL) {
	struct e *next = head->next;
	krb5_free_principal (context, head->principal);
	free (head);
	head = next;
    }
}

/*
 * Remove all entries that have newer versions and that are older
 * than `age'
 */

int
kt_purge(int argc, char **argv)
{
    krb5_error_code ret = 0;
    krb5_kt_cursor cursor;
    krb5_keytab keytab;
    krb5_keytab_entry entry;
    int help_flag = 0;
    char *age_str = "1 week";
    int age;
    struct getargs args[] = {
	{ "age",   0,  arg_string, NULL, "age to retire" },
	{ "help", 'h', arg_flag, NULL }
    };
    int num_args = sizeof(args) / sizeof(args[0]);
    int optind = 0;
    int i = 0;
    struct e *head = NULL;
    time_t judgement_day;

    args[i++].value = &age_str;
    args[i++].value = &help_flag;

    if(getarg(args, num_args, argc, argv, &optind)) {
	arg_printusage(args, num_args, "ktutil purge", "");
	return 1;
    }
    if(help_flag) {
	arg_printusage(args, num_args, "ktutil purge", "");
	return 1;
    }

    age = parse_time(age_str, "s");
    if(age < 0) {
	krb5_warnx(context, "unparasable time `%s'", age_str);
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

    ret = krb5_kt_start_seq_get(context, keytab, &cursor);
    if(ret){
	krb5_warn(context, ret, "krb5_kt_start_seq_get %s", keytab_string);
	goto out;
    }

    if (verbose_flag)
	fprintf (stderr, "Using keytab %s\n", keytab_string);
	
    while((ret = krb5_kt_next_entry(context, keytab, &entry, &cursor)) == 0) {
	add_entry (entry.principal, entry.vno, &head);
	krb5_kt_free_entry(context, &entry);
    }
    ret = krb5_kt_end_seq_get(context, keytab, &cursor);

    judgement_day = time (NULL);

    ret = krb5_kt_start_seq_get(context, keytab, &cursor);
    if(ret){
	krb5_warn(context, ret, "krb5_kt_start_seq_get, %s", keytab_string);
	goto out;
    }

    while((ret = krb5_kt_next_entry(context, keytab, &entry, &cursor)) == 0) {
	struct e *e = get_entry (entry.principal, head);

	if (e == NULL) {
	    krb5_warnx (context, "ignoring extra entry");
	    continue;
	}

	if (entry.vno < e->max_vno
	    && judgement_day - entry.timestamp > age) {
	    if (verbose_flag) {
		char *name_str;

		krb5_unparse_name (context, entry.principal, &name_str);
		printf ("removing %s vno %d\n", name_str, entry.vno);
		free (name_str);
	    }
	    ret = krb5_kt_remove_entry (context, keytab, &entry);
	    if (ret)
		krb5_warn (context, ret, "remove");
	}
	krb5_kt_free_entry(context, &entry);
    }
    ret = krb5_kt_end_seq_get(context, keytab, &cursor);

    delete_list (head);

 out:
    krb5_kt_close (context, keytab);
    return ret != 0;
}
