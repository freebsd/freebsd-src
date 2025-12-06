/*
 * Copyright (c) 2000 - 2004 Kungliga Tekniska Högskolan
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
#include <assert.h>
#ifndef O_BINARY
#define O_BINARY 0
#endif

struct hdb_master_key_data {
    krb5_keytab_entry keytab;
    krb5_crypto crypto;
    struct hdb_master_key_data *next;
    unsigned int key_usage;
};

void
hdb_free_master_key(krb5_context context, hdb_master_key mkey)
{
    struct hdb_master_key_data *ptr;
    while(mkey) {
	krb5_kt_free_entry(context, &mkey->keytab);
	if (mkey->crypto)
	    krb5_crypto_destroy(context, mkey->crypto);
	ptr = mkey;
	mkey = mkey->next;
	free(ptr);
    }
}

krb5_error_code
hdb_process_master_key(krb5_context context,
		       int kvno, krb5_keyblock *key, krb5_enctype etype,
		       hdb_master_key *mkey)
{
    krb5_error_code ret;

    *mkey = calloc(1, sizeof(**mkey));
    if(*mkey == NULL) {
	krb5_set_error_message(context, ENOMEM, "malloc: out of memory");
	return ENOMEM;
    }
    (*mkey)->key_usage = HDB_KU_MKEY;
    (*mkey)->keytab.vno = kvno;
    ret = krb5_parse_name(context, "K/M", &(*mkey)->keytab.principal);
    if(ret)
	goto fail;
    ret = krb5_copy_keyblock_contents(context, key, &(*mkey)->keytab.keyblock);
    if(ret)
	goto fail;
    if(etype != 0)
	(*mkey)->keytab.keyblock.keytype = etype;
    (*mkey)->keytab.timestamp = time(NULL);
    ret = krb5_crypto_init(context, key, etype, &(*mkey)->crypto);
    if(ret)
	goto fail;
    return 0;
 fail:
    hdb_free_master_key(context, *mkey);
    *mkey = NULL;
    return ret;
}

krb5_error_code
hdb_add_master_key(krb5_context context, krb5_keyblock *key,
		   hdb_master_key *inout)
{
    int vno = 0;
    hdb_master_key p;
    krb5_error_code ret;

    for(p = *inout; p; p = p->next)
	vno = max(vno, p->keytab.vno);
    vno++;
    ret = hdb_process_master_key(context, vno, key, 0, &p);
    if(ret)
	return ret;
    p->next = *inout;
    *inout = p;
    return 0;
}

static krb5_error_code
read_master_keytab(krb5_context context, const char *filename,
		   hdb_master_key *mkey)
{
    krb5_error_code ret;
    krb5_keytab id;
    krb5_kt_cursor cursor;
    krb5_keytab_entry entry;
    hdb_master_key p;

    ret = krb5_kt_resolve(context, filename, &id);
    if(ret)
	return ret;

    ret = krb5_kt_start_seq_get(context, id, &cursor);
    if(ret)
	goto out;
    *mkey = NULL;
    while(krb5_kt_next_entry(context, id, &entry, &cursor) == 0) {
	p = calloc(1, sizeof(*p));
	if(p == NULL) {
	    krb5_kt_end_seq_get(context, id, &cursor);
	    ret = ENOMEM;
	    goto out;
	}
	p->keytab = entry;
	ret = krb5_crypto_init(context, &p->keytab.keyblock, 0, &p->crypto);
	p->next = *mkey;
	*mkey = p;
    }
    krb5_kt_end_seq_get(context, id, &cursor);
  out:
    krb5_kt_close(context, id);
    return ret;
}

/* read a MIT master keyfile */
static krb5_error_code
read_master_mit(krb5_context context, const char *filename,
		int byteorder, hdb_master_key *mkey)
{
    int fd;
    krb5_error_code ret;
    krb5_storage *sp;
    int16_t enctype;
    krb5_keyblock key;

    fd = open(filename, O_RDONLY | O_BINARY);
    if(fd < 0) {
	int save_errno = errno;
	krb5_set_error_message(context, save_errno, "failed to open %s: %s",
			       filename, strerror(save_errno));
	return save_errno;
    }
    sp = krb5_storage_from_fd(fd);
    if(sp == NULL) {
	close(fd);
	return errno;
    }
    krb5_storage_set_flags(sp, byteorder);
    /* could possibly use ret_keyblock here, but do it with more
       checks for now */
    {
	ret = krb5_ret_int16(sp, &enctype);
	if (ret)
	    goto out;
	ret = krb5_enctype_valid(context, enctype);
	if (ret)
	   goto out;
	key.keytype = enctype;
	ret = krb5_ret_data(sp, &key.keyvalue);
	if(ret)
	    goto out;
    }
    ret = hdb_process_master_key(context, 1, &key, 0, mkey);
    krb5_free_keyblock_contents(context, &key);
  out:
    krb5_storage_free(sp);
    close(fd);
    return ret;
}

/* read an old master key file */
static krb5_error_code
read_master_encryptionkey(krb5_context context, const char *filename,
			  hdb_master_key *mkey)
{
    int fd;
    krb5_keyblock key;
    krb5_error_code ret;
    unsigned char buf[256];
    ssize_t len;
    size_t ret_len;

    fd = open(filename, O_RDONLY | O_BINARY);
    if(fd < 0) {
	int save_errno = errno;
	krb5_set_error_message(context, save_errno, "failed to open %s: %s",
			      filename, strerror(save_errno));
	return save_errno;
    }

    len = read(fd, buf, sizeof(buf));
    close(fd);
    if(len < 0) {
	int save_errno = errno;
	krb5_set_error_message(context, save_errno, "error reading %s: %s",
			      filename, strerror(save_errno));
	return save_errno;
    }

    ret = decode_EncryptionKey(buf, len, &key, &ret_len);
    memset(buf, 0, sizeof(buf));
    if(ret)
	return ret;

    /* Originally, the keytype was just that, and later it got changed
       to des-cbc-md5, but we always used des in cfb64 mode. This
       should cover all cases, but will break if someone has hacked
       this code to really use des-cbc-md5 -- but then that's not my
       problem. */
    if(key.keytype == ETYPE_DES_CBC_CRC || key.keytype == ETYPE_DES_CBC_MD5)
	key.keytype = ETYPE_DES_CFB64_NONE;

    ret = hdb_process_master_key(context, 0, &key, 0, mkey);
    krb5_free_keyblock_contents(context, &key);
    return ret;
}

/* read a krb4 /.k style file */
static krb5_error_code
read_master_krb4(krb5_context context, const char *filename,
		 hdb_master_key *mkey)
{
    int fd;
    krb5_keyblock key;
    krb5_error_code ret;
    unsigned char buf[256];
    ssize_t len;

    fd = open(filename, O_RDONLY | O_BINARY);
    if(fd < 0) {
	int save_errno = errno;
	krb5_set_error_message(context, save_errno, "failed to open %s: %s",
			       filename, strerror(save_errno));
	return save_errno;
    }

    len = read(fd, buf, sizeof(buf));
    close(fd);
    if(len < 0) {
	int save_errno = errno;
	krb5_set_error_message(context, save_errno, "error reading %s: %s",
			       filename, strerror(save_errno));
	return save_errno;
    }
    if(len != 8) {
	krb5_set_error_message(context, HEIM_ERR_EOF,
			       "bad contents of %s", filename);
	return HEIM_ERR_EOF; /* XXX file might be too large */
    }

    memset(&key, 0, sizeof(key));
    key.keytype = ETYPE_DES_PCBC_NONE;
    ret = krb5_data_copy(&key.keyvalue, buf, len);
    memset(buf, 0, sizeof(buf));
    if(ret)
	return ret;

    ret = hdb_process_master_key(context, 0, &key, 0, mkey);
    krb5_free_keyblock_contents(context, &key);
    return ret;
}

krb5_error_code
hdb_read_master_key(krb5_context context, const char *filename,
		    hdb_master_key *mkey)
{
    FILE *f;
    unsigned char buf[16];
    krb5_error_code ret;

    off_t len;

    *mkey = NULL;

    if(filename == NULL)
	filename = HDB_DB_DIR "/m-key";

    f = fopen(filename, "r");
    if(f == NULL) {
	int save_errno = errno;
	krb5_set_error_message(context, save_errno, "failed to open %s: %s",
			       filename, strerror(save_errno));
	return save_errno;
    }

    if(fread(buf, 1, 2, f) != 2) {
	fclose(f);
	krb5_set_error_message(context, HEIM_ERR_EOF, "end of file reading %s", filename);
	return HEIM_ERR_EOF;
    }

    fseek(f, 0, SEEK_END);
    len = ftell(f);

    if(fclose(f) != 0)
	return errno;

    if(len < 0)
	return errno;

    if(len == 8) {
	ret = read_master_krb4(context, filename, mkey);
    } else if(buf[0] == 0x30 && len <= 127 && buf[1] == len - 2) {
	ret = read_master_encryptionkey(context, filename, mkey);
    } else if(buf[0] == 5 && buf[1] >= 1 && buf[1] <= 2) {
	ret = read_master_keytab(context, filename, mkey);
    } else {
      /*
       * Check both LittleEndian and BigEndian since they key file
       * might be moved from a machine with diffrent byte order, or
       * its running on MacOS X that always uses BE master keys.
       */
      ret = read_master_mit(context, filename, KRB5_STORAGE_BYTEORDER_LE, mkey);
      if (ret)
          ret = read_master_mit(context, filename, KRB5_STORAGE_BYTEORDER_BE, mkey);
    }
    return ret;
}

krb5_error_code
hdb_write_master_key(krb5_context context, const char *filename,
		     hdb_master_key mkey)
{
    krb5_error_code ret;
    hdb_master_key p;
    krb5_keytab kt;

    if(filename == NULL)
	filename = HDB_DB_DIR "/m-key";

    ret = krb5_kt_resolve(context, filename, &kt);
    if(ret)
	return ret;

    for(p = mkey; p; p = p->next) {
	ret = krb5_kt_add_entry(context, kt, &p->keytab);
    }

    krb5_kt_close(context, kt);

    return ret;
}

krb5_error_code
_hdb_set_master_key_usage(krb5_context context, HDB *db, unsigned int key_usage)
{
    if (db->hdb_master_key_set == 0)
	return HDB_ERR_NO_MKEY;
    db->hdb_master_key->key_usage = key_usage;
    return 0;
}

hdb_master_key
_hdb_find_master_key(uint32_t *mkvno, hdb_master_key mkey)
{
    hdb_master_key ret = NULL;
    while(mkey) {
	if(ret == NULL && mkey->keytab.vno == 0)
	    ret = mkey;
	if(mkvno == NULL) {
	    if(ret == NULL || mkey->keytab.vno > ret->keytab.vno)
		ret = mkey;
	} else if((uint32_t)mkey->keytab.vno == *mkvno)
	    return mkey;
	mkey = mkey->next;
    }
    return ret;
}

int
_hdb_mkey_version(hdb_master_key mkey)
{
    return mkey->keytab.vno;
}

int
_hdb_mkey_decrypt(krb5_context context, hdb_master_key key,
		  krb5_key_usage usage,
		  void *ptr, size_t size, krb5_data *res)
{
    return krb5_decrypt(context, key->crypto, usage,
			ptr, size, res);
}

int
_hdb_mkey_encrypt(krb5_context context, hdb_master_key key,
		  krb5_key_usage usage,
		  const void *ptr, size_t size, krb5_data *res)
{
    return krb5_encrypt(context, key->crypto, usage,
			ptr, size, res);
}

/*
 * Unseal and optionally reseal the key in the MIT KDC master key.
 * If mit_key != NULL, the key is sealed using this key.
 */
static krb5_error_code
_hdb_reseal_key_mkey(krb5_context context, Key *k, hdb_master_key mkey,
    hdb_master_key mit_key)
{

    krb5_error_code ret;
    krb5_data mitres, res;
    size_t keysize;

    hdb_master_key key, mitkey;

    if(k->mkvno == NULL)
	return 0;

    key = _hdb_find_master_key(k->mkvno, mkey);

    if (key == NULL)
	return HDB_ERR_NO_MKEY;

    ret = _hdb_mkey_decrypt(context, key, HDB_KU_MKEY,
			    k->key.keyvalue.data,
			    k->key.keyvalue.length,
			    &res);
    if(ret == KRB5KRB_AP_ERR_BAD_INTEGRITY) {
	/* try to decrypt with MIT key usage */
	ret = _hdb_mkey_decrypt(context, key, 0,
			    k->key.keyvalue.data,
			    k->key.keyvalue.length,
			    &res);
    }
    if (ret)
	return ret;

    /* fixup keylength if the key got padded when encrypting it */
    ret = krb5_enctype_keysize(context, k->key.keytype, &keysize);
    if (ret) {
	krb5_data_free(&res);
	return ret;
    }
    if (keysize > res.length) {
	krb5_data_free(&res);
	return KRB5_BAD_KEYSIZE;
    }

    /* For mit_key != NULL, re-encrypt the key using the mitkey. */
    if (mit_key != NULL) {
	mitkey = _hdb_find_master_key(NULL, mit_key);
	if (mitkey == NULL) {
	    krb5_data_free(&res);
	    return HDB_ERR_NO_MKEY;
	}

	ret = _hdb_mkey_encrypt(context, mitkey, 0,
			    res.data,
			    keysize,
			    &mitres);
	krb5_data_free(&res);
	if (ret)
	    return ret;
    }

    krb5_data_free(&k->key.keyvalue);
    if (mit_key == NULL) {
	k->key.keyvalue = res;
	k->key.keyvalue.length = keysize;
	free(k->mkvno);
	k->mkvno = NULL;
    } else {
	k->key.keyvalue = mitres;
	*k->mkvno = mitkey->keytab.vno;
    }

    return 0;
}

krb5_error_code
hdb_unseal_key_mkey(krb5_context context, Key *k, hdb_master_key mkey)
{

    krb5_error_code ret;

    ret = _hdb_reseal_key_mkey(context, k, mkey, NULL);
    return ret;
}

static krb5_error_code
_hdb_unseal_keys_mkey(krb5_context context, hdb_entry *ent, hdb_master_key mkey,
    hdb_master_key mitkey)
{
    krb5_error_code ret;
    size_t i;
    int got_one = 0;

    for(i = 0; i < ent->keys.len; i++){
	if (mitkey == NULL || mit_strong_etype(ent->keys.val[i].key.keytype)) {
	    ret = _hdb_reseal_key_mkey(context, &ent->keys.val[i], mkey,
		mitkey);
	    if (ret)
		return ret;
	    got_one = 1;
	}
    }

    /*
     * If none of the keys were string enough, create a strong key,
     * but one that is not encrypted in the MIT master key.  As such,
     * it will require a "change_password" once in the MIT KDC to
     * make it work.
     */
    if (got_one == 0 && mitkey != NULL && ent->keys.len > 0) {
	krb5_keyblock key;
	krb5_salt salt;

	krb5_free_keyblock_contents(context, &ent->keys.val[0].key);
	salt.salttype = KRB5_PW_SALT;
	salt.saltvalue.data = NULL;
	salt.saltvalue.length = 0;
	ret = krb5_string_to_key_salt(context, ETYPE_AES256_CTS_HMAC_SHA1_96,
	    "XXXX", salt, &ent->keys.val[0].key);
	if (ret)
	    return ret;
    }
    return 0;
}

krb5_error_code
hdb_unseal_keys_mkey(krb5_context context, hdb_entry *ent, hdb_master_key mkey)
{
    krb5_error_code ret;

    ret = _hdb_unseal_keys_mkey(context, ent, mkey, NULL);
    return ret;
}

krb5_error_code
hdb_unseal_keys(krb5_context context, HDB *db, hdb_entry *ent)
{
    if (db->hdb_master_key_set == 0)
	return 0;
    if (db->hdb_mit_key_set != 0)
	return _hdb_unseal_keys_mkey(context, ent, db->hdb_master_key,
	    db->hdb_mit_key);
    else
	return _hdb_unseal_keys_mkey(context, ent, db->hdb_master_key,
	    NULL);
}

#ifdef notnow
krb5_error_code
hdb_unseal_keys_kvno(krb5_context context, HDB *db, krb5_kvno kvno,
		     hdb_entry *ent)
{
    krb5_error_code ret = KRB5KRB_AP_ERR_NOKEY;	/* XXX need a better code? */
    HDB_extension *tmp;
    HDB_Ext_KeySet *hist_keys;
    hdb_keyset *tmp_keys;
    Key *tmp_val;
    unsigned int tmp_len;
    krb5_kvno tmp_kvno;
    int i, k;

    assert(kvno == 0 || kvno < ent->kvno);

    tmp = hdb_find_extension(ent, choice_HDB_extension_data_hist_keys);
    if (tmp == NULL)
	return ret;

    tmp_len = ent->keys.len;
    tmp_val = ent->keys.val;
    tmp_kvno = ent->kvno;

    hist_keys = &tmp->data.u.hist_keys;

    for (i = hist_keys->len - 1; i >= 0; i++) {
	if (kvno != 0 && hist_keys->val[i].kvno != kvno)
	    continue;
	for (k = 0; k < hist_keys->val[i].keys.len; k++) {
	    ret = _hdb_reseal_key_mkey(context,
				      &hist_keys->val[i].keys.val[k],
				      db->hdb_master_key, NULL);
	    if (ret)
		return (ret);
	}

	if (kvno == 0)
	    continue;

	/*
	 * NOTE: What follows is a bit of an ugly hack.
	 *
	 * This is the keyset we're being asked for, so we add the
	 * current keyset to the history, leave the one we were asked
	 * for in the history, and pretend the one we were asked for is
	 * also the current keyset.
	 *
	 * This is a bit of a defensive hack in case an entry fetched
	 * this way ever gets modified then stored: if the keyset is not
	 * changed we can detect this and put things back, else we won't
	 * drop any keysets from history by accident.
	 *
	 * Note too that we only ever get called with a non-zero kvno
	 * either in the KDC or in cases where we aren't changing the
	 * HDB entry anyways, which is why this is just a defensive
	 * hack.  We also don't fetch specific kvnos in the dump case,
	 * so there's no danger that we'll dump this entry and load it
	 * again, repeatedly causing the history to grow boundelessly.
	 */
	tmp_keys = realloc(hist_keys->val,
		      sizeof (*hist_keys->val) * (hist_keys->len + 1));
	if (tmp_keys == NULL)
	    return ENOMEM;

	memmove(&tmp_keys[1], tmp_keys,
		sizeof (*hist_keys->val) * hist_keys->len++);
	tmp_keys[0].keys.len = ent->keys.len;
	tmp_keys[0].keys.val = ent->keys.val;
	tmp_keys[0].kvno = ent->kvno;
	tmp_keys[0].replace_time = time(NULL);
	i++;
	ent->keys.len = hist_keys->val[i].keys.len;
	ent->keys.val = hist_keys->val[i].keys.val;
	ent->kvno = kvno;
    }

    return (ret);
}
#endif

krb5_error_code
hdb_unseal_key(krb5_context context, HDB *db, Key *k)
{
    if (db->hdb_master_key_set == 0)
	return 0;
    return _hdb_reseal_key_mkey(context, k, db->hdb_master_key, NULL);
}

krb5_error_code
hdb_seal_key_mkey(krb5_context context, Key *k, hdb_master_key mkey)
{
    krb5_error_code ret;
    krb5_data res;
    hdb_master_key key;

    if(k->mkvno != NULL)
	return 0;

    key = _hdb_find_master_key(k->mkvno, mkey);

    if (key == NULL)
	return HDB_ERR_NO_MKEY;

    ret = _hdb_mkey_encrypt(context, key, HDB_KU_MKEY,
			    k->key.keyvalue.data,
			    k->key.keyvalue.length,
			    &res);
    if (ret)
	return ret;

    memset(k->key.keyvalue.data, 0, k->key.keyvalue.length);
    free(k->key.keyvalue.data);
    k->key.keyvalue = res;

    if (k->mkvno == NULL) {
	k->mkvno = malloc(sizeof(*k->mkvno));
	if (k->mkvno == NULL)
	    return ENOMEM;
    }
    *k->mkvno = key->keytab.vno;

    return 0;
}

krb5_error_code
hdb_seal_keys_mkey(krb5_context context, hdb_entry *ent, hdb_master_key mkey)
{
    size_t i;
    for(i = 0; i < ent->keys.len; i++){
	krb5_error_code ret;

	ret = hdb_seal_key_mkey(context, &ent->keys.val[i], mkey);
	if (ret)
	    return ret;
    }
    return 0;
}

krb5_error_code
hdb_seal_keys(krb5_context context, HDB *db, hdb_entry *ent)
{
    if (db->hdb_master_key_set == 0)
	return 0;

    return hdb_seal_keys_mkey(context, ent, db->hdb_master_key);
}

krb5_error_code
hdb_seal_key(krb5_context context, HDB *db, Key *k)
{
    if (db->hdb_master_key_set == 0)
	return 0;

    return hdb_seal_key_mkey(context, k, db->hdb_master_key);
}

krb5_error_code
hdb_set_master_key(krb5_context context,
		   HDB *db,
		   krb5_keyblock *key)
{
    krb5_error_code ret;
    hdb_master_key mkey;

    ret = hdb_process_master_key(context, 0, key, 0, &mkey);
    if (ret)
	return ret;
    db->hdb_master_key = mkey;
#if 0 /* XXX - why? */
    des_set_random_generator_seed(key.keyvalue.data);
#endif
    db->hdb_master_key_set = 1;
    db->hdb_master_key->key_usage = HDB_KU_MKEY;
    return 0;
}

krb5_error_code
hdb_set_master_keyfile (krb5_context context,
			HDB *db,
			const char *keyfile)
{
    hdb_master_key key;
    krb5_error_code ret;

    ret = hdb_read_master_key(context, keyfile, &key);
    if (ret) {
	if (ret != ENOENT)
	    return ret;
	krb5_clear_error_message(context);
	return 0;
    }
    db->hdb_master_key = key;
    db->hdb_master_key_set = 1;
    return ret;
}

krb5_error_code
hdb_clear_master_key (krb5_context context,
		      HDB *db)
{
    if (db->hdb_master_key_set) {
	hdb_free_master_key(context, db->hdb_master_key);
	db->hdb_master_key_set = 0;
    }
    return 0;
}
