/*
 * Copyright (c) 1999 Kungliga Tekniska Högskolan
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

/* keytab backend for HDB databases */

RCSID("$Id: keytab.c,v 1.2 1999/08/26 13:24:05 joda Exp $");

struct hdb_data {
    char *dbname;
    char *mkey;
    HDB *db;
};

static krb5_error_code
hdb_resolve(krb5_context context, const char *name, krb5_keytab id)
{
    krb5_error_code ret;
    struct hdb_data *d;
    const char *db, *mkey;
    d = malloc(sizeof(*d));
    if(d == NULL)
	return ENOMEM;
    db = name;
    mkey = strchr(name, ':');
    if(mkey == NULL || mkey[1] == '\0') {
	if(*name == '\0')
	    d->dbname = NULL;
	else {
	    d->dbname = strdup(name);
	    if(d->dbname == NULL) {
		free(d);
		return ENOMEM;
	    }
	}
	d->mkey = NULL;
    } else {
	if((mkey - db) == 0) {
	    d->dbname = NULL;
	} else {
	    d->dbname = malloc(mkey - db);
	    if(d->dbname == NULL) {
		free(d);
		return ENOMEM;
	    }
	    strncpy(d->dbname, db, mkey - db);
	    d->dbname[mkey - db] = '\0';
	}
	d->mkey = strdup(mkey + 1);
	if(d->mkey == NULL) {
	    free(d->dbname);
	    free(d);
	    return ENOMEM;
	}
    }
    ret = hdb_create(context, &d->db, d->dbname);
    if(ret) {
	free(d->dbname);
	free(d->mkey);
	free(d);
	return ret;
    }
    ret = hdb_set_master_keyfile (context, d->db, d->mkey);
    if(ret) {
	(*d->db->destroy)(context, d->db);
	free(d->dbname);
	free(d->mkey);
	free(d);
	return ret;
    }
    id->data = d;
    return 0;
}

static krb5_error_code
hdb_close(krb5_context context, krb5_keytab id)
{
    struct hdb_data *d = id->data;
    (*d->db->destroy)(context, d->db);
    free(d);
    return 0;
}

static krb5_error_code 
hdb_get_name(krb5_context context, 
	     krb5_keytab id, 
	     char *name, 
	     size_t namesize)
{
    struct hdb_data *d = id->data;
    snprintf(name, namesize, "%s%s%s", 
	     d->dbname ? d->dbname : "",
	     (d->dbname || d->mkey) ? ":" : "",
	     d->mkey ? d->mkey : "");
    return 0;
}

static krb5_error_code
hdb_get_entry(krb5_context context,
	      krb5_keytab id,
	      krb5_const_principal principal,
	      krb5_kvno kvno,
	      krb5_enctype enctype,
	      krb5_keytab_entry *entry)
{
    hdb_entry ent;
    krb5_error_code ret;
    struct hdb_data *d = id->data;
    int i;

    ret = (*d->db->open)(context, d->db, O_RDONLY, 0);
    if (ret)
	return ret;
    ent.principal = (krb5_principal)principal;
    ret = (*d->db->fetch)(context, d->db, HDB_F_DECRYPT, &ent);
    (*d->db->close)(context, d->db);
    if(ret == HDB_ERR_NOENTRY)
	return KRB5_KT_NOTFOUND;
    else if(ret)
	return ret;
    if(kvno && ent.kvno != kvno) {
	hdb_free_entry(context, &ent);
 	return KRB5_KT_NOTFOUND;
    }
    if(enctype == 0)
	if(ent.keys.len > 0)
	    enctype = ent.keys.val[0].key.keytype;
    ret = KRB5_KT_NOTFOUND;
    for(i = 0; i < ent.keys.len; i++) {
	if(ent.keys.val[i].key.keytype == enctype) {
	    krb5_copy_principal(context, principal, &entry->principal);
	    entry->vno = ent.kvno;
	    krb5_copy_keyblock_contents(context, 
					&ent.keys.val[i].key, 
					&entry->keyblock);
	    ret = 0;
	    break;
	}
    }
    hdb_free_entry(context, &ent);
    return ret;
}

krb5_kt_ops hdb_kt_ops = {
    "HDB",
    hdb_resolve,
    hdb_get_name,
    hdb_close,
    hdb_get_entry,
    NULL,		/* start_seq_get */
    NULL,		/* next_entry */
    NULL,		/* end_seq_get */
    NULL,		/* add */
    NULL		/* remove */
};

