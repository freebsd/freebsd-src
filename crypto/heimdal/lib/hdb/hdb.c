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

#include "hdb_locl.h"

RCSID("$Id: hdb.c,v 1.35 1999/12/02 17:05:05 joda Exp $");

krb5_error_code
hdb_next_enctype2key(krb5_context context,
		     hdb_entry *e,
		     krb5_enctype enctype,
		     Key **key)
{
    Key *k;
    
    for (k = *key ? *key : e->keys.val; 
	 k < e->keys.val + e->keys.len; 
	 k++)
	if(k->key.keytype == enctype){
	    *key = k;
	    return 0;
	}
    return KRB5_PROG_ETYPE_NOSUPP; /* XXX */
}

krb5_error_code
hdb_enctype2key(krb5_context context, 
		hdb_entry *e, 
		krb5_enctype enctype, 
		Key **key)
{
    *key = NULL;
    return hdb_next_enctype2key(context, e, enctype, key);
}

/* this is a bit ugly, but will get better when the crypto framework
   gets fixed */

krb5_error_code
hdb_process_master_key(krb5_context context, EncryptionKey key, 
		       krb5_data *schedule)
{
    krb5_error_code ret;

    if(key.keytype != ETYPE_DES_CBC_MD5)
	return KRB5_PROG_KEYTYPE_NOSUPP;

    ret = krb5_data_alloc (schedule, sizeof(des_key_schedule));
    if (ret)
	return ret;

    des_set_key((des_cblock*)key.keyvalue.data, schedule->data);
    return 0;
}

krb5_error_code
hdb_read_master_key(krb5_context context, const char *filename, 
		    EncryptionKey *key)
{
    FILE *f;
    unsigned char buf[256];
    size_t len;
    krb5_error_code ret;
    if(filename == NULL)
	filename = HDB_DB_DIR "/m-key";
    f = fopen(filename, "r");
    if(f == NULL)
	return errno;
    len = fread(buf, 1, sizeof(buf), f);
    if(ferror(f))
	ret = errno;
    else
	ret = decode_EncryptionKey(buf, len, key, &len);
    fclose(f);
    memset(buf, 0, sizeof(buf));
    return ret;
}

void
_hdb_unseal_keys_int(hdb_entry *ent, int key_version, krb5_data schedule)
{
    int i;
    for(i = 0; i < ent->keys.len; i++){
	des_cblock iv;
	int num = 0;
	if(ent->keys.val[i].mkvno == NULL)
	    continue;
	if(*ent->keys.val[i].mkvno != key_version)
	    ;
	memset(&iv, 0, sizeof(iv));
	
	des_cfb64_encrypt(ent->keys.val[i].key.keyvalue.data, 
			  ent->keys.val[i].key.keyvalue.data, 
			  ent->keys.val[i].key.keyvalue.length, 
			  schedule.data, &iv, &num, 0);
	free(ent->keys.val[i].mkvno);
	ent->keys.val[i].mkvno = NULL;
    }
}

void
hdb_unseal_keys(HDB *db, hdb_entry *ent)
{
    if (db->master_key_set == 0)
	return;
    _hdb_unseal_keys_int(ent, db->master_key_version, db->master_key);
}

void
_hdb_seal_keys_int(hdb_entry *ent, int key_version, krb5_data schedule)
{
    int i;
    for(i = 0; i < ent->keys.len; i++){
	des_cblock iv;
	int num = 0;

	if(ent->keys.val[i].mkvno != NULL)
	    continue;
	memset(&iv, 0, sizeof(iv));
	des_cfb64_encrypt(ent->keys.val[i].key.keyvalue.data, 
			  ent->keys.val[i].key.keyvalue.data, 
			  ent->keys.val[i].key.keyvalue.length, 
			  schedule.data, &iv, &num, 1);
	ent->keys.val[i].mkvno = malloc(sizeof(*ent->keys.val[i].mkvno));
	*ent->keys.val[i].mkvno = key_version;
    }
}

void
hdb_seal_keys(HDB *db, hdb_entry *ent)
{
    if (db->master_key_set == 0)
	return;
    
    _hdb_seal_keys_int(ent, db->master_key_version, db->master_key);
}

void
hdb_free_key(Key *key)
{
    memset(key->key.keyvalue.data, 
	   0,
	   key->key.keyvalue.length);
    free_Key(key);
    free(key);
}


krb5_error_code
hdb_lock(int fd, int operation)
{
    int i, code;
    for(i = 0; i < 3; i++){
	code = flock(fd, (operation == HDB_RLOCK ? LOCK_SH : LOCK_EX) | LOCK_NB);
	if(code == 0 || errno != EWOULDBLOCK)
	    break;
	sleep(1);
    }
    if(code == 0)
	return 0;
    if(errno == EWOULDBLOCK)
	return HDB_ERR_DB_INUSE;
    return HDB_ERR_CANT_LOCK_DB;
}

krb5_error_code
hdb_unlock(int fd)
{
    int code;
    code = flock(fd, LOCK_UN);
    if(code)
	return 4711 /* XXX */;
    return 0;
}

void
hdb_free_entry(krb5_context context, hdb_entry *ent)
{
    int i;

    for(i = 0; i < ent->keys.len; ++i) {
	Key *k = &ent->keys.val[i];

	memset (k->key.keyvalue.data, 0, k->key.keyvalue.length);
    }
    free_hdb_entry(ent);
}

krb5_error_code
hdb_foreach(krb5_context context,
	    HDB *db,
	    unsigned flags,
	    hdb_foreach_func_t func,
	    void *data)
{
    krb5_error_code ret;
    hdb_entry entry;
    ret = db->firstkey(context, db, flags, &entry);
    while(ret == 0){
	ret = (*func)(context, db, &entry, data);
	hdb_free_entry(context, &entry);
	if(ret == 0)
	    ret = db->nextkey(context, db, flags, &entry);
    }
    if(ret == HDB_ERR_NOENTRY)
	ret = 0;
    return ret;
}

krb5_error_code
hdb_check_db_format(krb5_context context, HDB *db)
{
    krb5_data tag;
    krb5_data version;
    krb5_error_code ret;
    unsigned ver;
    int foo;

    tag.data = HDB_DB_FORMAT_ENTRY;
    tag.length = strlen(tag.data);
    ret = (*db->_get)(context, db, tag, &version);
    if(ret)
	return ret;
    foo = sscanf(version.data, "%u", &ver);
    krb5_data_free (&version);
    if (foo != 1)
	return HDB_ERR_BADVERSION;
    if(ver != HDB_DB_FORMAT)
	return HDB_ERR_BADVERSION;
    return 0;
}

krb5_error_code
hdb_init_db(krb5_context context, HDB *db)
{
    krb5_error_code ret;
    krb5_data tag;
    krb5_data version;
    char ver[32];
    
    ret = hdb_check_db_format(context, db);
    if(ret != HDB_ERR_NOENTRY)
	return ret;
    
    tag.data = HDB_DB_FORMAT_ENTRY;
    tag.length = strlen(tag.data);
    snprintf(ver, sizeof(ver), "%u", HDB_DB_FORMAT);
    version.data = ver;
    version.length = strlen(version.data) + 1; /* zero terminated */
    ret = (*db->_put)(context, db, 0, tag, version);
    return ret;
}

krb5_error_code
hdb_create(krb5_context context, HDB **db, const char *filename)
{
    krb5_error_code ret = 0;
    if(filename == NULL)
	filename = HDB_DEFAULT_DB;
    initialize_hdb_error_table_r(&context->et_list);
#ifdef HAVE_DB_H
    ret = hdb_db_create(context, db, filename);
#elif HAVE_NDBM_H
    ret = hdb_ndbm_create(context, db, filename);
#else
    krb5_errx(context, 1, "No database support! (hdb_create)");
#endif
    return ret;
}

krb5_error_code
hdb_set_master_key (krb5_context context,
		    HDB *db,
		    EncryptionKey key)
{
    krb5_error_code ret;

    ret = hdb_process_master_key(context, key, &db->master_key);
    if (ret)
	return ret;
#if 0 /* XXX - why? */
    des_set_random_generator_seed(key.keyvalue.data);
#endif
    db->master_key_set = 1;
    db->master_key_version = 0; /* XXX */
    return 0;
}

krb5_error_code
hdb_set_master_keyfile (krb5_context context,
			HDB *db,
			const char *keyfile)
{
    EncryptionKey key;
    krb5_error_code ret;

    ret = hdb_read_master_key(context, keyfile, &key);
    if (ret) {
	if (ret != ENOENT)
	    return ret;
	return 0;
    }
    ret = hdb_set_master_key(context, db, key);
    memset(key.keyvalue.data, 0, key.keyvalue.length);
    free_EncryptionKey(&key);
    return ret;
}

krb5_error_code
hdb_clear_master_key (krb5_context context,
		      HDB *db)
{
    if (db->master_key_set) {
	memset(db->master_key.data, 0, db->master_key.length);
	krb5_data_free(&db->master_key);
	db->master_key_set = 0;
    }
    return 0;
}
