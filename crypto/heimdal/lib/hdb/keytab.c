/*
 * Copyright (c) 1999 - 2002 Kungliga Tekniska Högskolan
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

RCSID("$Id: keytab.c,v 1.5 2002/08/26 13:28:11 assar Exp $");

struct hdb_data {
    char *dbname;
    char *mkey;
};

/*
 * the format for HDB keytabs is:
 * HDB:[database:mkey]
 */

static krb5_error_code
hdb_resolve(krb5_context context, const char *name, krb5_keytab id)
{
    struct hdb_data *d;
    const char *db, *mkey;

    d = malloc(sizeof(*d));
    if(d == NULL) {
	krb5_set_error_string(context, "malloc: out of memory");
	return ENOMEM;
    }
    db = name;
    mkey = strchr(name, ':');
    if(mkey == NULL || mkey[1] == '\0') {
	if(*name == '\0')
	    d->dbname = NULL;
	else {
	    d->dbname = strdup(name);
	    if(d->dbname == NULL) {
		free(d);
		krb5_set_error_string(context, "malloc: out of memory");
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
		krb5_set_error_string(context, "malloc: out of memory");
		return ENOMEM;
	    }
	    memmove(d->dbname, db, mkey - db);
	    d->dbname[mkey - db] = '\0';
	}
	d->mkey = strdup(mkey + 1);
	if(d->mkey == NULL) {
	    free(d->dbname);
	    free(d);
	    krb5_set_error_string(context, "malloc: out of memory");
	    return ENOMEM;
	}
    }
    id->data = d;
    return 0;
}

static krb5_error_code
hdb_close(krb5_context context, krb5_keytab id)
{
    struct hdb_data *d = id->data;

    free(d->dbname);
    free(d->mkey);
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

static void
set_config (krb5_context context,
	    krb5_config_binding *binding,
	    const char **dbname,
	    const char **mkey)
{
    *dbname = krb5_config_get_string(context, binding, "dbname", NULL);
    *mkey   = krb5_config_get_string(context, binding, "mkey_file", NULL);
}

/*
 * try to figure out the database (`dbname') and master-key (`mkey')
 * that should be used for `principal'.
 */

static void
find_db (krb5_context context,
	 const char **dbname,
	 const char **mkey,
	 krb5_const_principal principal)
{
    const krb5_config_binding *top_bind = NULL;
    krb5_config_binding *default_binding = NULL;
    krb5_config_binding *db;
    krb5_realm *prealm = krb5_princ_realm(context, (krb5_principal)principal);

    *dbname = *mkey = NULL;

    while ((db = (krb5_config_binding *)
	    krb5_config_get_next(context,
				 NULL,
				 &top_bind,
				 krb5_config_list,
				 "kdc",
				 "database",
				 NULL)) != NULL) {
	const char *p;
	
	p = krb5_config_get_string (context, db, "realm", NULL);
	if (p == NULL) {
	    if(default_binding) {
		krb5_warnx(context, "WARNING: more than one realm-less "
			   "database specification");
		krb5_warnx(context, "WARNING: using the first encountered");
	    } else
		default_binding = db;
	} else if (strcmp (*prealm, p) == 0) {
	    set_config (context, db, dbname, mkey);
	    break;
	}
    }
    if (*dbname == NULL && default_binding != NULL)
	set_config (context, default_binding, dbname, mkey);
    if (*dbname == NULL)
	*dbname = HDB_DEFAULT_DB;
}

/*
 * find the keytab entry in `id' for `principal, kvno, enctype' and return
 * it in `entry'.  return 0 or an error code
 */

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
    HDB *db;
    const char *dbname = d->dbname;
    const char *mkey   = d->mkey;

    if (dbname == NULL)
	find_db (context, &dbname, &mkey, principal);

    ret = hdb_create (context, &db, dbname);
    if (ret)
	return ret;
    ret = hdb_set_master_keyfile (context, db, mkey);
    if (ret) {
	(*db->destroy)(context, db);
	return ret;
    }
	
    ret = (*db->open)(context, db, O_RDONLY, 0);
    if (ret) {
	(*db->destroy)(context, db);
	return ret;
    }
    ent.principal = (krb5_principal)principal;
    ret = (*db->fetch)(context, db, HDB_F_DECRYPT, &ent);
    (*db->close)(context, db);
    (*db->destroy)(context, db);

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
