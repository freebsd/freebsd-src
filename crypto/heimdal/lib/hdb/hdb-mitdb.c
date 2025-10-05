/*
 * Copyright (c) 1997 - 2001 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
 * All rights reserved.
 *
 * Portions Copyright (c) 2009 Apple Inc. All rights reserved.
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

#define KRB5_KDB_DISALLOW_POSTDATED	0x00000001
#define KRB5_KDB_DISALLOW_FORWARDABLE	0x00000002
#define KRB5_KDB_DISALLOW_TGT_BASED	0x00000004
#define KRB5_KDB_DISALLOW_RENEWABLE	0x00000008
#define KRB5_KDB_DISALLOW_PROXIABLE	0x00000010
#define KRB5_KDB_DISALLOW_DUP_SKEY	0x00000020
#define KRB5_KDB_DISALLOW_ALL_TIX	0x00000040
#define KRB5_KDB_REQUIRES_PRE_AUTH	0x00000080
#define KRB5_KDB_REQUIRES_HW_AUTH	0x00000100
#define KRB5_KDB_REQUIRES_PWCHANGE	0x00000200
#define KRB5_KDB_DISALLOW_SVR		0x00001000
#define KRB5_KDB_PWCHANGE_SERVICE	0x00002000
#define KRB5_KDB_SUPPORT_DESMD5		0x00004000
#define KRB5_KDB_NEW_PRINC		0x00008000

/*

key: krb5_unparse_name  + NUL

 16: baselength
 32: attributes
 32: max time
 32: max renewable time
 32: client expire
 32: passwd expire
 32: last successful passwd
 32: last failed attempt
 32: num of failed attempts
 16: num tl data
 16: num data data
 16: principal length
 length: principal
 for num tl data times
    16: tl data type
    16: tl data length
    length: length
 for num key data times
    16: version (num keyblocks)
    16: kvno
    for version times:
        16: type
        16: length
        length: keydata


key_data_contents[0]

	int16: length
	read-of-data: key-encrypted, key-usage 0, master-key

salt:
    version2 = salt in key_data->key_data_contents[1]
    else default salt.

*/

#include "hdb_locl.h"

static void
attr_to_flags(unsigned attr, HDBFlags *flags)
{
    flags->postdate =		!(attr & KRB5_KDB_DISALLOW_POSTDATED);
    flags->forwardable =	!(attr & KRB5_KDB_DISALLOW_FORWARDABLE);
    flags->initial =	       !!(attr & KRB5_KDB_DISALLOW_TGT_BASED);
    flags->renewable =		!(attr & KRB5_KDB_DISALLOW_RENEWABLE);
    flags->proxiable =		!(attr & KRB5_KDB_DISALLOW_PROXIABLE);
    /* DUP_SKEY */
    flags->invalid =	       !!(attr & KRB5_KDB_DISALLOW_ALL_TIX);
    flags->require_preauth =   !!(attr & KRB5_KDB_REQUIRES_PRE_AUTH);
    flags->require_hwauth =    !!(attr & KRB5_KDB_REQUIRES_HW_AUTH);
    flags->server =		!(attr & KRB5_KDB_DISALLOW_SVR);
    flags->change_pw = 	       !!(attr & KRB5_KDB_PWCHANGE_SERVICE);
    flags->client =	        1; /* XXX */
}

#define KDB_V1_BASE_LENGTH 38

#define CHECK(x) do { if ((x)) goto out; } while(0)

#ifdef HAVE_DB1
static krb5_error_code
mdb_principal2key(krb5_context context,
		  krb5_const_principal principal,
		  krb5_data *key)
{
    krb5_error_code ret;
    char *str;

    ret = krb5_unparse_name(context, principal, &str);
    if (ret)
	return ret;
    key->data = str;
    key->length = strlen(str) + 1;
    return 0;
}
#endif /* HAVE_DB1 */

#define KRB5_KDB_SALTTYPE_NORMAL	0
#define KRB5_KDB_SALTTYPE_V4		1
#define KRB5_KDB_SALTTYPE_NOREALM	2
#define KRB5_KDB_SALTTYPE_ONLYREALM	3
#define KRB5_KDB_SALTTYPE_SPECIAL	4
#define KRB5_KDB_SALTTYPE_AFS3		5
#define KRB5_KDB_SALTTYPE_CERTHASH	6

static krb5_error_code
fix_salt(krb5_context context, hdb_entry *ent, int key_num)
{
    krb5_error_code ret;
    Salt *salt = ent->keys.val[key_num].salt;
    /* fix salt type */
    switch((int)salt->type) {
    case KRB5_KDB_SALTTYPE_NORMAL:
	salt->type = KRB5_PADATA_PW_SALT;
	break;
    case KRB5_KDB_SALTTYPE_V4:
	krb5_data_free(&salt->salt);
	salt->type = KRB5_PADATA_PW_SALT;
	break;
    case KRB5_KDB_SALTTYPE_NOREALM:
    {
	size_t len;
	size_t i;
	char *p;

	len = 0;
	for (i = 0; i < ent->principal->name.name_string.len; ++i)
	    len += strlen(ent->principal->name.name_string.val[i]);
	ret = krb5_data_alloc (&salt->salt, len);
	if (ret)
	    return ret;
	p = salt->salt.data;
	for (i = 0; i < ent->principal->name.name_string.len; ++i) {
	    memcpy (p,
		    ent->principal->name.name_string.val[i],
		    strlen(ent->principal->name.name_string.val[i]));
	    p += strlen(ent->principal->name.name_string.val[i]);
	}

	salt->type = KRB5_PADATA_PW_SALT;
	break;
    }
    case KRB5_KDB_SALTTYPE_ONLYREALM:
	krb5_data_free(&salt->salt);
	ret = krb5_data_copy(&salt->salt,
			     ent->principal->realm,
			     strlen(ent->principal->realm));
	if(ret)
	    return ret;
	salt->type = KRB5_PADATA_PW_SALT;
	break;
    case KRB5_KDB_SALTTYPE_SPECIAL:
	salt->type = KRB5_PADATA_PW_SALT;
	break;
    case KRB5_KDB_SALTTYPE_AFS3:
	krb5_data_free(&salt->salt);
	ret = krb5_data_copy(&salt->salt,
		       ent->principal->realm,
		       strlen(ent->principal->realm));
	if(ret)
	    return ret;
	salt->type = KRB5_PADATA_AFS3_SALT;
	break;
    case KRB5_KDB_SALTTYPE_CERTHASH:
	krb5_data_free(&salt->salt);
	free(ent->keys.val[key_num].salt);
	ent->keys.val[key_num].salt = NULL;
	break;
    default:
	abort();
    }
    return 0;
}


krb5_error_code
_hdb_mdb_value2entry(krb5_context context, krb5_data *data,
                     krb5_kvno kvno, hdb_entry *entry)
{
    krb5_error_code ret;
    krb5_storage *sp;
    uint32_t u32;
    uint16_t u16, num_keys, num_tl;
    ssize_t sz;
    size_t i, j;
    char *p;

    sp = krb5_storage_from_data(data);
    if (sp == NULL) {
	krb5_set_error_message(context, ENOMEM, "out of memory");
	return ENOMEM;
    }

    krb5_storage_set_byteorder(sp, KRB5_STORAGE_BYTEORDER_LE);

    /*
     * 16: baselength
     *
     * The story here is that these 16 bits have to be a constant:
     * KDB_V1_BASE_LENGTH.  Once upon a time a different value here
     * would have been used to indicate the presence of "extra data"
     * between the "base" contents and the {principal name, TL data,
     * keys} that follow it.  Nothing supports such "extra data"
     * nowadays, so neither do we here.
     *
     * XXX But... surely we ought to log about this extra data, or skip
     * it, or something, in case anyone has MIT KDBs with ancient
     * entries in them...  Logging would allow the admin to know which
     * entries to dump with MIT krb5's kdb5_util.
     */
    CHECK(ret = krb5_ret_uint16(sp, &u16));
    if (u16 != KDB_V1_BASE_LENGTH) { ret = EINVAL; goto out; }
    /* 32: attributes */
    CHECK(ret = krb5_ret_uint32(sp, &u32));
    attr_to_flags(u32, &entry->flags);

    /* 32: max time */
    CHECK(ret = krb5_ret_uint32(sp, &u32));
    if (u32) {
	entry->max_life = malloc(sizeof(*entry->max_life));
	*entry->max_life = u32;
    }
    /* 32: max renewable time */
    CHECK(ret = krb5_ret_uint32(sp, &u32));
    if (u32) {
	entry->max_renew = malloc(sizeof(*entry->max_renew));
	*entry->max_renew = u32;
    }
    /* 32: client expire */
    CHECK(ret = krb5_ret_uint32(sp, &u32));
    if (u32) {
	entry->valid_end = malloc(sizeof(*entry->valid_end));
	*entry->valid_end = u32;
    }
    /* 32: passwd expire */
    CHECK(ret = krb5_ret_uint32(sp, &u32));
    if (u32) {
	entry->pw_end = malloc(sizeof(*entry->pw_end));
	*entry->pw_end = u32;
    }
    /* 32: last successful passwd */
    CHECK(ret = krb5_ret_uint32(sp, &u32));
    /* 32: last failed attempt */
    CHECK(ret = krb5_ret_uint32(sp, &u32));
    /* 32: num of failed attempts */
    CHECK(ret = krb5_ret_uint32(sp, &u32));
    /* 16: num tl data */
    CHECK(ret = krb5_ret_uint16(sp, &u16));
    num_tl = u16;
    /* 16: num key data */
    CHECK(ret = krb5_ret_uint16(sp, &u16));
    num_keys = u16;
    /* 16: principal length */
    CHECK(ret = krb5_ret_uint16(sp, &u16));
    /* length: principal */
    {
	/*
	 * Note that the principal name includes the NUL in the entry,
	 * but we don't want to take chances, so we add an extra NUL.
	 */
	p = malloc(u16 + 1);
	if (p == NULL) {
	    ret = ENOMEM;
	    goto out;
	}
	sz = krb5_storage_read(sp, p, u16);
        if (sz != u16) {
            ret = EINVAL; /* XXX */
            goto out;
        }
	p[u16] = '\0';
	CHECK(ret = krb5_parse_name(context, p, &entry->principal));
	free(p);
    }
    /* for num tl data times
           16: tl data type
           16: tl data length
           length: length */
#define mit_KRB5_TL_LAST_PWD_CHANGE     1
#define mit_KRB5_TL_MOD_PRINC           2
    for (i = 0; i < num_tl; i++) {
        int tl_type;
        krb5_principal modby;
	/* 16: TL data type */
	CHECK(ret = krb5_ret_uint16(sp, &u16));
        tl_type = u16;
	/* 16: TL data length */
	CHECK(ret = krb5_ret_uint16(sp, &u16));
        /*
         * For rollback to MIT purposes we really must understand some
         * TL data!
         *
         * XXX Move all this to separate functions, one per-TL type.
         */
        switch (tl_type) {
        case mit_KRB5_TL_LAST_PWD_CHANGE:
            CHECK(ret = krb5_ret_uint32(sp, &u32));
            CHECK(ret = hdb_entry_set_pw_change_time(context, entry, u32));
            break;
        case mit_KRB5_TL_MOD_PRINC:
            if (u16 < 5) {
                ret = EINVAL; /* XXX */
                goto out;
            }
            CHECK(ret = krb5_ret_uint32(sp, &u32)); /* mod time */
            p = malloc(u16 - 4 + 1);
            if (!p) {
                ret = ENOMEM;
                goto out;
            }
            p[u16 - 4] = '\0';
            sz = krb5_storage_read(sp, p, u16 - 4);
            if (sz != u16 - 4) { 
                ret = EINVAL; /* XXX */
                goto out;
            }
            CHECK(ret = krb5_parse_name(context, p, &modby));
            ret = hdb_set_last_modified_by(context, entry, modby, u32);
            krb5_free_principal(context, modby);
            free(p);
            break;
        default:
            krb5_storage_seek(sp, u16, SEEK_CUR);
            break;
        }
    }
    /*
     * for num key data times
     * 16: "version"
     * 16: kvno
     * for version times:
     *     16: type
     *     16: length
     *     length: keydata
     *
     * "version" here is really 1 or 2, the first meaning there's only
     * keys for this kvno, the second meaning there's keys and salt[s?].
     * That's right... hold that gag reflex, you can do it.
     */
    for (i = 0; i < num_keys; i++) {
	int keep = 0;
	uint16_t version;
	void *ptr;

	CHECK(ret = krb5_ret_uint16(sp, &u16));
	version = u16;
	CHECK(ret = krb5_ret_uint16(sp, &u16));

	/*
	 * First time through, and until we find one matching key,
	 * entry->kvno == 0.
	 */
	if ((entry->kvno < u16) && (kvno == 0 || kvno == u16)) {
	    keep = 1;
	    entry->kvno = u16;
	    /*
	     * Found a higher kvno than earlier, so free the old highest
	     * kvno keys.
	     *
	     * XXX Of course, we actually want to extract the old kvnos
	     * as well, for some of the kadm5 APIs.  We shouldn't free
	     * these keys, but keep them elsewhere.
	     */
	    for (j = 0; j < entry->keys.len; j++)
		free_Key(&entry->keys.val[j]);
	    free(entry->keys.val);
	    entry->keys.len = 0;
	    entry->keys.val = NULL;
	} else if (entry->kvno == u16)
	    /* Accumulate keys */
	    keep = 1;

	if (keep) {
	    Key *k;

	    ptr = realloc(entry->keys.val, sizeof(entry->keys.val[0]) * (entry->keys.len + 1));
	    if (ptr == NULL) {
		ret = ENOMEM;
		goto out;
	    }
	    entry->keys.val = ptr;

	    /* k points to current Key */
	    k = &entry->keys.val[entry->keys.len];

	    memset(k, 0, sizeof(*k));
	    entry->keys.len += 1;

	    k->mkvno = malloc(sizeof(*k->mkvno));
	    if (k->mkvno == NULL) {
		ret = ENOMEM;
		goto out;
	    }
	    *k->mkvno = 1;

	    for (j = 0; j < version; j++) {
		uint16_t type;
		CHECK(ret = krb5_ret_uint16(sp, &type));
		CHECK(ret = krb5_ret_uint16(sp, &u16));
		if (j == 0) {
		    /* This "version" means we have a key */
		    k->key.keytype = type;
		    if (u16 < 2) {
			ret = EINVAL;
			goto out;
		    }
		    /*
		     * MIT stores keys encrypted keys as {16-bit length
		     * of plaintext key, {encrypted key}}.  The reason
		     * for this is that the Kerberos cryptosystem is not
		     * length-preserving.  Heimdal's approach is to
		     * truncate the plaintext to the expected length of
		     * the key given its enctype, so we ignore this
		     * 16-bit length-of-plaintext-key field.
		     */
		    krb5_storage_seek(sp, 2, SEEK_CUR); /* skip real length */
		    k->key.keyvalue.length = u16 - 2;   /* adjust cipher len */
		    k->key.keyvalue.data = malloc(k->key.keyvalue.length);
		    krb5_storage_read(sp, k->key.keyvalue.data,
				      k->key.keyvalue.length);
		} else if (j == 1) {
		    /* This "version" means we have a salt */
		    k->salt = calloc(1, sizeof(*k->salt));
		    if (k->salt == NULL) {
			ret = ENOMEM;
			goto out;
		    }
		    k->salt->type = type;
		    if (u16 != 0) {
			k->salt->salt.data = malloc(u16);
			if (k->salt->salt.data == NULL) {
			    ret = ENOMEM;
			    goto out;
			}
			k->salt->salt.length = u16;
			krb5_storage_read(sp, k->salt->salt.data, k->salt->salt.length);
		    }
		    fix_salt(context, entry, entry->keys.len - 1);
		} else {
		    /*
		     * Whatever this "version" might be, we skip it
		     *
		     * XXX A krb5.conf parameter requesting that we log
		     * about strangeness like this, or return an error
		     * from here, might be nice.
		     */
		    krb5_storage_seek(sp, u16, SEEK_CUR);
		}
	    }
	} else {
	    /*
	     * XXX For now we skip older kvnos, but we should extract
	     * them...
	     */
	    for (j = 0; j < version; j++) {
		/* enctype */
		CHECK(ret = krb5_ret_uint16(sp, &u16));
		/* encrypted key (or plaintext salt) */
		CHECK(ret = krb5_ret_uint16(sp, &u16));
		krb5_storage_seek(sp, u16, SEEK_CUR);
	    }
	}
    }

    if (entry->kvno == 0 && kvno != 0) {
	ret = HDB_ERR_NOT_FOUND_HERE;
	goto out;
    }

    return 0;
 out:
    if (ret == HEIM_ERR_EOF)
	/* Better error code than "end of file" */
	ret = HEIM_ERR_BAD_HDBENT_ENCODING;
    return ret;
}

#if 0
static krb5_error_code
mdb_entry2value(krb5_context context, hdb_entry *entry, krb5_data *data)
{
    return EINVAL;
}
#endif

#if HAVE_DB1

#if defined(HAVE_DB_185_H)
#include <db_185.h>
#elif defined(HAVE_DB_H)
#include <db.h>
#endif


static krb5_error_code
mdb_close(krb5_context context, HDB *db)
{
    DB *d = (DB*)db->hdb_db;
    (*d->close)(d);
    return 0;
}

static krb5_error_code
mdb_destroy(krb5_context context, HDB *db)
{
    krb5_error_code ret;

    ret = hdb_clear_master_key (context, db);
    free(db->hdb_name);
    free(db);
    return ret;
}

static krb5_error_code
mdb_lock(krb5_context context, HDB *db, int operation)
{
    DB *d = (DB*)db->hdb_db;
    int fd = (*d->fd)(d);
    if(fd < 0) {
	krb5_set_error_message(context, HDB_ERR_CANT_LOCK_DB,
			       "Can't lock database: %s", db->hdb_name);
	return HDB_ERR_CANT_LOCK_DB;
    }
    return hdb_lock(fd, operation);
}

static krb5_error_code
mdb_unlock(krb5_context context, HDB *db)
{
    DB *d = (DB*)db->hdb_db;
    int fd = (*d->fd)(d);
    if(fd < 0) {
	krb5_set_error_message(context, HDB_ERR_CANT_LOCK_DB,
			       "Can't unlock database: %s", db->hdb_name);
	return HDB_ERR_CANT_LOCK_DB;
    }
    return hdb_unlock(fd);
}


static krb5_error_code
mdb_seq(krb5_context context, HDB *db,
       unsigned flags, hdb_entry_ex *entry, int flag)
{
    DB *d = (DB*)db->hdb_db;
    DBT key, value;
    krb5_data key_data, data;
    int code;

    code = db->hdb_lock(context, db, HDB_RLOCK);
    if(code == -1) {
	krb5_set_error_message(context, HDB_ERR_DB_INUSE, "Database %s in use", db->hdb_name);
	return HDB_ERR_DB_INUSE;
    }
    code = (*d->seq)(d, &key, &value, flag);
    db->hdb_unlock(context, db); /* XXX check value */
    if(code == -1) {
	code = errno;
	krb5_set_error_message(context, code, "Database %s seq error: %s",
			       db->hdb_name, strerror(code));
	return code;
    }
    if(code == 1) {
	krb5_clear_error_message(context);
	return HDB_ERR_NOENTRY;
    }

    key_data.data = key.data;
    key_data.length = key.size;
    data.data = value.data;
    data.length = value.size;
    memset(entry, 0, sizeof(*entry));

    if (_hdb_mdb_value2entry(context, &data, 0, &entry->entry))
	return mdb_seq(context, db, flags, entry, R_NEXT);

    if (db->hdb_master_key_set && (flags & HDB_F_DECRYPT)) {
	code = hdb_unseal_keys (context, db, &entry->entry);
	if (code)
	    hdb_free_entry (context, entry);
    }

    return code;
}


static krb5_error_code
mdb_firstkey(krb5_context context, HDB *db, unsigned flags, hdb_entry_ex *entry)
{
    return mdb_seq(context, db, flags, entry, R_FIRST);
}


static krb5_error_code
mdb_nextkey(krb5_context context, HDB *db, unsigned flags, hdb_entry_ex *entry)
{
    return mdb_seq(context, db, flags, entry, R_NEXT);
}

static krb5_error_code
mdb_rename(krb5_context context, HDB *db, const char *new_name)
{
    int ret;
    char *old, *new;

    asprintf(&old, "%s.db", db->hdb_name);
    asprintf(&new, "%s.db", new_name);
    ret = rename(old, new);
    free(old);
    free(new);
    if(ret)
	return errno;

    free(db->hdb_name);
    db->hdb_name = strdup(new_name);
    return 0;
}

static krb5_error_code
mdb__get(krb5_context context, HDB *db, krb5_data key, krb5_data *reply)
{
    DB *d = (DB*)db->hdb_db;
    DBT k, v;
    int code;

    k.data = key.data;
    k.size = key.length;
    code = db->hdb_lock(context, db, HDB_RLOCK);
    if(code)
	return code;
    code = (*d->get)(d, &k, &v, 0);
    db->hdb_unlock(context, db);
    if(code < 0) {
	code = errno;
	krb5_set_error_message(context, code, "Database %s get error: %s",
			       db->hdb_name, strerror(code));
	return code;
    }
    if(code == 1) {
	krb5_clear_error_message(context);
	return HDB_ERR_NOENTRY;
    }

    krb5_data_copy(reply, v.data, v.size);
    return 0;
}

static krb5_error_code
mdb__put(krb5_context context, HDB *db, int replace,
	krb5_data key, krb5_data value)
{
    DB *d = (DB*)db->hdb_db;
    DBT k, v;
    int code;

    k.data = key.data;
    k.size = key.length;
    v.data = value.data;
    v.size = value.length;
    code = db->hdb_lock(context, db, HDB_WLOCK);
    if(code)
	return code;
    code = (*d->put)(d, &k, &v, replace ? 0 : R_NOOVERWRITE);
    db->hdb_unlock(context, db);
    if(code < 0) {
	code = errno;
	krb5_set_error_message(context, code, "Database %s put error: %s",
			       db->hdb_name, strerror(code));
	return code;
    }
    if(code == 1) {
	krb5_clear_error_message(context);
	return HDB_ERR_EXISTS;
    }
    return 0;
}

static krb5_error_code
mdb__del(krb5_context context, HDB *db, krb5_data key)
{
    DB *d = (DB*)db->hdb_db;
    DBT k;
    krb5_error_code code;
    k.data = key.data;
    k.size = key.length;
    code = db->hdb_lock(context, db, HDB_WLOCK);
    if(code)
	return code;
    code = (*d->del)(d, &k, 0);
    db->hdb_unlock(context, db);
    if(code == 1) {
	code = errno;
	krb5_set_error_message(context, code, "Database %s put error: %s",
			       db->hdb_name, strerror(code));
	return code;
    }
    if(code < 0)
	return errno;
    return 0;
}

static krb5_error_code
mdb_fetch_kvno(krb5_context context, HDB *db, krb5_const_principal principal,
	       unsigned flags, krb5_kvno kvno, hdb_entry_ex *entry)
{
    krb5_data key, value;
    krb5_error_code ret;

    ret = mdb_principal2key(context, principal, &key);
    if (ret)
	return ret;
    ret = db->hdb__get(context, db, key, &value);
    krb5_data_free(&key);
    if(ret)
	return ret;
    ret = _hdb_mdb_value2entry(context, &value, kvno, &entry->entry);
    krb5_data_free(&value);
    if (ret)
	return ret;

    if (db->hdb_master_key_set && (flags & HDB_F_DECRYPT)) {
	ret = hdb_unseal_keys (context, db, &entry->entry);
	if (ret) {
	    hdb_free_entry(context, entry);
            return ret;
        }
    }

    return 0;
}

static krb5_error_code
mdb_store(krb5_context context, HDB *db, unsigned flags, hdb_entry_ex *entry)
{
    krb5_error_code ret;
    krb5_storage *sp = NULL;
    krb5_storage *spent = NULL;
    krb5_data line = { 0, 0 };
    krb5_data kdb_ent = { 0, 0 };
    krb5_data key = { 0, 0 };
    ssize_t sz;

    sp = krb5_storage_emem();
    if (!sp) return ENOMEM;
    ret = _hdb_set_master_key_usage(context, db, 0); /* MIT KDB uses KU 0 */
    ret = hdb_seal_keys(context, db, &entry->entry);
    if (ret) return ret;
    ret = entry2mit_string_int(context, sp, &entry->entry);
    if (ret) goto out;
    sz = krb5_storage_write(sp, "\n", 2); /* NUL-terminate */
    ret = ENOMEM;
    if (sz == -1) goto out;
    ret = krb5_storage_to_data(sp, &line);
    if (ret) goto out;

    ret = ENOMEM;
    spent = krb5_storage_emem();
    if (!spent) goto out;
    ret = _hdb_mit_dump2mitdb_entry(context, line.data, spent);
    if (ret) goto out;
    ret = krb5_storage_to_data(spent, &kdb_ent);
    if (ret) goto out;
    ret = mdb_principal2key(context, entry->entry.principal, &key);
    if (ret) goto out;
    ret = mdb__put(context, db, 1, key, kdb_ent);

out:
    if (sp)
        krb5_storage_free(sp);
    if (spent)
        krb5_storage_free(spent);
    krb5_data_free(&line);
    krb5_data_free(&kdb_ent);
    krb5_data_free(&key);

    return ret;
}

static krb5_error_code
mdb_remove(krb5_context context, HDB *db, krb5_const_principal principal)
{
    krb5_error_code code;
    krb5_data key;

    code = db->hdb__del(context, db, key);
    krb5_data_free(&key);
    return code;
}

static krb5_error_code
mdb_open(krb5_context context, HDB *db, int flags, mode_t mode)
{
    char *fn;
    char *actual_fn;
    krb5_error_code ret;
    struct stat st;

    asprintf(&fn, "%s.db", db->hdb_name);
    if (fn == NULL) {
	krb5_set_error_message(context, ENOMEM, "malloc: out of memory");
	return ENOMEM;
    }

     if (stat(fn, &st) == 0)
         actual_fn = fn;
     else
         actual_fn = db->hdb_name;
     db->hdb_db = dbopen(actual_fn, flags, mode, DB_BTREE, NULL);
    if (db->hdb_db == NULL) {
	switch (errno) {
#ifdef EFTYPE
	case EFTYPE:
#endif
	case EINVAL:
	    db->hdb_db = dbopen(actual_fn, flags, mode, DB_BTREE, NULL);
	}
    }
    free(fn);

    /* try to open without .db extension */
    if(db->hdb_db == NULL && errno == ENOENT)
	db->hdb_db = dbopen(db->hdb_name, flags, mode, DB_BTREE, NULL);
    if(db->hdb_db == NULL) {
	ret = errno;
	krb5_set_error_message(context, ret, "dbopen (%s): %s",
			      db->hdb_name, strerror(ret));
	return ret;
    }
#if 0
    /*
     * Don't do this -- MIT won't be able to handle the
     * HDB_DB_FORMAT_ENTRY key.
     */
    if ((flags & O_ACCMODE) != O_RDONLY)
	ret = hdb_init_db(context, db);
#endif
    ret = hdb_check_db_format(context, db);
    if (ret == HDB_ERR_NOENTRY) {
	krb5_clear_error_message(context);
	return 0;
    }
    if (ret) {
	mdb_close(context, db);
	krb5_set_error_message(context, ret, "hdb_open: failed %s database %s",
			      (flags & O_ACCMODE) == O_RDONLY ?
			      "checking format of" : "initialize",
			      db->hdb_name);
    }
    return ret;
}

krb5_error_code
hdb_mdb_create(krb5_context context, HDB **db,
	       const char *filename)
{
    *db = calloc(1, sizeof(**db));
    if (*db == NULL) {
	krb5_set_error_message(context, ENOMEM, "malloc: out of memory");
	return ENOMEM;
    }

    (*db)->hdb_db = NULL;
    (*db)->hdb_name = strdup(filename);
    if ((*db)->hdb_name == NULL) {
	free(*db);
	*db = NULL;
	krb5_set_error_message(context, ENOMEM, "malloc: out of memory");
	return ENOMEM;
    }
    (*db)->hdb_master_key_set = 0;
    (*db)->hdb_openp = 0;
    (*db)->hdb_capability_flags = 0;
    (*db)->hdb_open = mdb_open;
    (*db)->hdb_close = mdb_close;
    (*db)->hdb_fetch_kvno = mdb_fetch_kvno;
    (*db)->hdb_store = mdb_store;
    (*db)->hdb_remove = mdb_remove;
    (*db)->hdb_firstkey = mdb_firstkey;
    (*db)->hdb_nextkey= mdb_nextkey;
    (*db)->hdb_lock = mdb_lock;
    (*db)->hdb_unlock = mdb_unlock;
    (*db)->hdb_rename = mdb_rename;
    (*db)->hdb__get = mdb__get;
    (*db)->hdb__put = mdb__put;
    (*db)->hdb__del = mdb__del;
    (*db)->hdb_destroy = mdb_destroy;
    return 0;
}

#endif /* HAVE_DB1 */

/*
can have any number of princ stanzas.
format is as follows (only \n indicates newlines)
princ\t%d\t (%d is KRB5_KDB_V1_BASE_LENGTH, always 38)
%d\t (strlen of principal e.g. shadow/foo@ANDREW.CMU.EDU)
%d\t (number of tl_data)
%d\t (number of key data, e.g. how many keys for this user)
%d\t (extra data length)
%s\t (principal name)
%d\t (attributes)
%d\t (max lifetime, seconds)
%d\t (max renewable life, seconds)
%d\t (expiration, seconds since epoch or 2145830400 for never)
%d\t (password expiration, seconds, 0 for never)
%d\t (last successful auth, seconds since epoch)
%d\t (last failed auth, per above)
%d\t (failed auth count)
foreach tl_data 0 to number of tl_data - 1 as above
  %d\t%d\t (data type, data length)
  foreach tl_data 0 to length-1
    %02x (tl data contents[element n])
  except if tl_data length is 0
    %d (always -1)
  \t
foreach key 0 to number of keys - 1 as above
  %d\t%d\t (key data version, kvno)
  foreach version 0 to key data version - 1 (a key or a salt)
    %d\t%d\t(data type for this key, data length for this key)
    foreach key data length 0 to length-1
      %02x (key data contents[element n])
    except if key_data length is 0
      %d (always -1)
    \t
foreach extra data length 0 to length - 1
  %02x (extra data part)
unless no extra data
  %d (always -1)
;\n

*/

static char *
nexttoken(char **p)
{
    char *q;
    do {
	q = strsep(p, " \t");
    } while(q && *q == '\0');
    return q;
}

static size_t
getdata(char **p, unsigned char *buf, size_t len)
{
    size_t i;
    int v;
    char *q = nexttoken(p);
    i = 0;
    while(*q && i < len) {
	if(sscanf(q, "%02x", &v) != 1)
	    break;
	buf[i++] = v;
	q += 2;
    }
    return i;
}

static int
getint(char **p)
{
    int val;
    char *q = nexttoken(p);
    sscanf(q, "%d", &val);
    return val;
}

static unsigned int
getuint(char **p)
{
    int val;
    char *q = nexttoken(p);
    sscanf(q, "%u", &val);
    return val;
}

#define KRB5_KDB_SALTTYPE_NORMAL	0
#define KRB5_KDB_SALTTYPE_V4		1
#define KRB5_KDB_SALTTYPE_NOREALM	2
#define KRB5_KDB_SALTTYPE_ONLYREALM	3
#define KRB5_KDB_SALTTYPE_SPECIAL	4
#define KRB5_KDB_SALTTYPE_AFS3		5

#define CHECK_UINT(num)                            \
        if ((num) < 0 || (num) > INT_MAX) return EINVAL
#define CHECK_UINT16(num)                          \
        if ((num) < 0 || (num) > 1<<15) return EINVAL
#define CHECK_NUM(num, maxv)                     \
        if ((num) > (maxv)) return EINVAL

/*
 * This utility function converts an MIT dump entry to an MIT on-disk
 * encoded entry, which can then be decoded with _hdb_mdb_value2entry().
 * This allows us to have a single decoding function (_hdb_mdb_value2entry),
 * which makes the code cleaner (less code duplication), if a bit less
 * efficient.  It also will allow us to have a function to dump an HDB
 * entry in MIT format so we can dump HDB into MIT format for rollback
 * purposes.  And that will allow us to write to MIT KDBs, again
 * somewhat inefficiently, also for migration/rollback purposes.
 */
int
_hdb_mit_dump2mitdb_entry(krb5_context context, char *line, krb5_storage *sp)
{
    krb5_error_code ret = EINVAL;
    char *p = line, *q;
    char *princ;
    ssize_t sz;
    size_t i;
    size_t princ_len;
    unsigned int num_tl_data;
    size_t num_key_data;
    unsigned int attributes;
    int tmp;

    krb5_storage_set_byteorder(sp, KRB5_STORAGE_BYTEORDER_LE);

    q = nexttoken(&p);
    if (strcmp(q, "kdb5_util") == 0 || strcmp(q, "policy") == 0 ||
        strcmp(q, "princ") != 0) {
        return -1;
    }
    if (getint(&p) != 38)
        return EINVAL;
#define KDB_V1_BASE_LENGTH 38
    ret = krb5_store_int16(sp, KDB_V1_BASE_LENGTH);
    if (ret) return ret;

    nexttoken(&p); /* length of principal */
    num_tl_data = getuint(&p); /* number of tl-data */
    num_key_data = getuint(&p); /* number of key-data */
    getint(&p);  /* length of extra data */
    princ = nexttoken(&p); /* principal name */

    attributes = getuint(&p); /* attributes */
    ret = krb5_store_uint32(sp, attributes);
    if (ret) return ret;

    tmp = getint(&p); /* max life */
    CHECK_UINT(tmp);
    ret = krb5_store_uint32(sp, tmp);
    if (ret) return ret;

    tmp = getint(&p); /* max renewable life */
    CHECK_UINT(tmp);
    ret = krb5_store_uint32(sp, tmp);
    if (ret) return ret;

    tmp = getint(&p); /* expiration */
    CHECK_UINT(tmp);
    ret = krb5_store_uint32(sp, tmp);
    if (ret) return ret;

    tmp = getint(&p); /* pw expiration */
    CHECK_UINT(tmp);
    ret = krb5_store_uint32(sp, tmp);
    if (ret) return ret;

    tmp = getint(&p); /* last auth */
    CHECK_UINT(tmp);
    ret = krb5_store_uint32(sp, tmp);
    if (ret) return ret;

    tmp = getint(&p); /* last failed auth */
    CHECK_UINT(tmp);
    ret = krb5_store_uint32(sp, tmp);
    if (ret) return ret;

    tmp = getint(&p); /* fail auth count */
    CHECK_UINT(tmp);
    ret = krb5_store_uint32(sp, tmp);
    if (ret) return ret;

    /* add TL data count */
    CHECK_NUM(num_tl_data, 1023);
    ret = krb5_store_uint16(sp, num_tl_data);
    if (ret) return ret;

    /* add key count */
    CHECK_NUM(num_key_data, 1023);
    ret = krb5_store_uint16(sp, num_key_data);
    if (ret) return ret;

    /* add principal unparsed name length and unparsed name */
    princ_len = strlen(princ);
    if (princ_len > (1<<15) - 1) return EINVAL;
    princ_len++; /* must count and write the NUL in the on-disk encoding */
    ret = krb5_store_uint16(sp, princ_len);
    if (ret) return ret;
    sz = krb5_storage_write(sp, princ, princ_len);
    if (sz == -1) return ENOMEM;

    /* scan and write TL data */
    for (i = 0; i < num_tl_data; i++) {
        int tl_type, tl_length;
        unsigned char *buf;

        tl_type = getint(&p); /* data type */
        tl_length = getint(&p); /* data length */

        CHECK_UINT16(tl_type);
        ret = krb5_store_uint16(sp, tl_type);
        if (ret) return ret;
        CHECK_UINT16(tl_length);
        ret = krb5_store_uint16(sp, tl_length);
        if (ret) return ret;

        if (tl_length) {
            buf = malloc(tl_length);
            if (!buf) return ENOMEM;
            if (getdata(&p, buf, tl_length) != tl_length) return EINVAL;
            sz = krb5_storage_write(sp, buf, tl_length);
            free(buf);
            if (sz == -1) return ENOMEM;
        } else {
            if (strcmp(nexttoken(&p), "-1") != 0) return EINVAL;
        }
    }

    for (i = 0; i < num_key_data; i++) {
        unsigned char *buf;
        int key_versions;
        int kvno;
        int keytype;
        int keylen;
        size_t k;

        key_versions = getint(&p); /* key data version */
        CHECK_UINT16(key_versions);
        ret = krb5_store_int16(sp, key_versions);
        if (ret) return ret;

        kvno = getint(&p);
        CHECK_UINT16(kvno);
        ret = krb5_store_int16(sp, kvno);
        if (ret) return ret;

        for (k = 0; k < key_versions; k++) {
            keytype = getint(&p);
            CHECK_UINT16(keytype);
            ret = krb5_store_int16(sp, keytype);
            if (ret) return ret;

            keylen = getint(&p);
            CHECK_UINT16(keylen);
            ret = krb5_store_int16(sp, keylen);
            if (ret) return ret;

            if (keylen) {
                buf = malloc(keylen);
                if (!buf) return ENOMEM;
                if (getdata(&p, buf, keylen) != keylen) return EINVAL;
                sz = krb5_storage_write(sp, buf, keylen);
                free(buf);
                if (sz == -1) return ENOMEM;
            } else {
                if (strcmp(nexttoken(&p), "-1") != 0) return EINVAL;
            }
        }
    }
    /*
     * The rest is "extra data", but there's never any and we wouldn't
     * know what to do with it.
     */
    /* nexttoken(&p); */
    return 0;
}

