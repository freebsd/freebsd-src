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

#include "krb5_locl.h"

RCSID("$Id: keytab_memory.c,v 1.4 2000/02/07 03:18:39 assar Exp $");

/* memory operations -------------------------------------------- */

struct mkt_data {
    krb5_keytab_entry *entries;
    int num_entries;
};

static krb5_error_code
mkt_resolve(krb5_context context, const char *name, krb5_keytab id)
{
    struct mkt_data *d;
    d = malloc(sizeof(*d));
    if(d == NULL)
	return ENOMEM;
    d->entries = NULL;
    d->num_entries = 0;
    id->data = d;
    return 0;
}

static krb5_error_code
mkt_close(krb5_context context, krb5_keytab id)
{
    struct mkt_data *d = id->data;
    int i;
    for(i = 0; i < d->num_entries; i++)
	krb5_kt_free_entry(context, &d->entries[i]);
    free(d->entries);
    free(d);
    return 0;
}

static krb5_error_code 
mkt_get_name(krb5_context context, 
	     krb5_keytab id, 
	     char *name, 
	     size_t namesize)
{
    strlcpy(name, "", namesize);
    return 0;
}

static krb5_error_code
mkt_start_seq_get(krb5_context context, 
		  krb5_keytab id, 
		  krb5_kt_cursor *c)
{
    /* XXX */
    c->fd = 0;
    return 0;
}

static krb5_error_code
mkt_next_entry(krb5_context context, 
	       krb5_keytab id, 
	       krb5_keytab_entry *entry, 
	       krb5_kt_cursor *c)
{
    struct mkt_data *d = id->data;
    if(c->fd >= d->num_entries)
	return KRB5_KT_END;
    return krb5_kt_copy_entry_contents(context, &d->entries[c->fd++], entry);
}

static krb5_error_code
mkt_end_seq_get(krb5_context context, 
		krb5_keytab id,
		krb5_kt_cursor *cursor)
{
    return 0;
}

static krb5_error_code
mkt_add_entry(krb5_context context,
	      krb5_keytab id,
	      krb5_keytab_entry *entry)
{
    struct mkt_data *d = id->data;
    krb5_keytab_entry *tmp;
    tmp = realloc(d->entries, (d->num_entries + 1) * sizeof(*d->entries));
    if(tmp == NULL)
	return ENOMEM;
    d->entries = tmp;
    return krb5_kt_copy_entry_contents(context, entry, 
				       &d->entries[d->num_entries++]);
}

static krb5_error_code
mkt_remove_entry(krb5_context context,
		 krb5_keytab id,
		 krb5_keytab_entry *entry)
{
    struct mkt_data *d = id->data;
    krb5_keytab_entry *e, *end;
    
    /* do this backwards to minimize copying */
    for(end = d->entries + d->num_entries, e = end - 1; e >= d->entries; e--) {
	if(krb5_kt_compare(context, e, entry->principal, 
			   entry->vno, entry->keyblock.keytype)) {
	    krb5_kt_free_entry(context, e);
	    memmove(e, e + 1, (end - e - 1) * sizeof(*e));
	    memset(end - 1, 0, sizeof(*end));
	    d->num_entries--;
	    end--;
	}
    }
    e = realloc(d->entries, d->num_entries * sizeof(*d->entries));
    if(e != NULL)
	d->entries = e;
    return 0;
}

const krb5_kt_ops krb5_mkt_ops = {
    "MEMORY",
    mkt_resolve,
    mkt_get_name,
    mkt_close,
    NULL, /* get */
    mkt_start_seq_get,
    mkt_next_entry,
    mkt_end_seq_get,
    mkt_add_entry,
    mkt_remove_entry
};
