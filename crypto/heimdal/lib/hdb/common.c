/*
 * Copyright (c) 1997-2002 Kungliga Tekniska Högskolan
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

#include "hdb_locl.h"

RCSID("$Id: common.c,v 1.11 2002/09/04 16:32:30 joda Exp $");

int
hdb_principal2key(krb5_context context, krb5_principal p, krb5_data *key)
{
    Principal new;
    size_t len;
    int ret;

    ret = copy_Principal(p, &new);
    if(ret) 
	return ret;
    new.name.name_type = 0;

    ASN1_MALLOC_ENCODE(Principal, key->data, key->length, &new, &len, ret);
    free_Principal(&new);
    return ret;
}

int
hdb_key2principal(krb5_context context, krb5_data *key, krb5_principal p)
{
    return decode_Principal(key->data, key->length, p, NULL);
}

int
hdb_entry2value(krb5_context context, hdb_entry *ent, krb5_data *value)
{
    size_t len;
    int ret;
    
    ASN1_MALLOC_ENCODE(hdb_entry, value->data, value->length, ent, &len, ret);
    return ret;
}

int
hdb_value2entry(krb5_context context, krb5_data *value, hdb_entry *ent)
{
    return decode_hdb_entry(value->data, value->length, ent, NULL);
}

krb5_error_code
_hdb_fetch(krb5_context context, HDB *db, unsigned flags, hdb_entry *entry)
{
    krb5_data key, value;
    int code = 0;

    hdb_principal2key(context, entry->principal, &key);
    code = db->_get(context, db, key, &value);
    krb5_data_free(&key);
    if(code)
	return code;
    hdb_value2entry(context, &value, entry);
    if (db->master_key_set && (flags & HDB_F_DECRYPT)) {
	code = hdb_unseal_keys (context, db, entry);
	if (code)
	    hdb_free_entry(context, entry);
    }
    krb5_data_free(&value);
    return code;
}

krb5_error_code
_hdb_store(krb5_context context, HDB *db, unsigned flags, hdb_entry *entry)
{
    krb5_data key, value;
    int code;

    if(entry->generation == NULL) {
	struct timeval t;
	entry->generation = malloc(sizeof(*entry->generation));
	if(entry->generation == NULL) {
	    krb5_set_error_string(context, "malloc: out of memory");
	    return ENOMEM;
	}
	gettimeofday(&t, NULL);
	entry->generation->time = t.tv_sec;
	entry->generation->usec = t.tv_usec;
	entry->generation->gen = 0;
    } else
	entry->generation->gen++;
    hdb_principal2key(context, entry->principal, &key);
    code = hdb_seal_keys(context, db, entry);
    if (code) {
	krb5_data_free(&key);
	return code;
    }
    hdb_entry2value(context, entry, &value);
    code = db->_put(context, db, flags & HDB_F_REPLACE, key, value);
    krb5_data_free(&value);
    krb5_data_free(&key);
    return code;
}

krb5_error_code
_hdb_remove(krb5_context context, HDB *db, hdb_entry *entry)
{
    krb5_data key;
    int code;

    hdb_principal2key(context, entry->principal, &key);
    code = db->_del(context, db, key);
    krb5_data_free(&key);
    return code;
}

