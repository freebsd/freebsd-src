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

#include "kadm5_locl.h"

RCSID("$Id: set_keys.c,v 1.25 2001/08/13 15:12:16 joda Exp $");

/*
 * the known and used DES enctypes
 */

static krb5_enctype des_types[] = { ETYPE_DES_CBC_CRC,
 				    ETYPE_DES_CBC_MD4,
 				    ETYPE_DES_CBC_MD5 };
static unsigned n_des_types = sizeof(des_types) / sizeof(des_types[0]);

static krb5_error_code
make_keys(krb5_context context, krb5_principal principal, const char *password,
	  Key **keys_ret, size_t *num_keys_ret)
{
    krb5_enctype all_etypes[] = { ETYPE_DES3_CBC_SHA1,
				  ETYPE_DES_CBC_MD5,
				  ETYPE_DES_CBC_MD4,
				  ETYPE_DES_CBC_CRC };


    krb5_enctype e;

    krb5_error_code ret = 0;
    char **ktypes, **kp;

    Key *keys = NULL, *tmp;
    int num_keys = 0;
    Key key;

    int i;
    char *v4_ktypes[] = {"des3:pw-salt", "v4", NULL};

    ktypes = krb5_config_get_strings(context, NULL, "kadmin", 
				     "default_keys", NULL);

    /* for each entry in `default_keys' try to parse it as a sequence
       of etype:salttype:salt, syntax of this if something like:
       [(des|des3|etype):](pw|afs3)[:string], if etype is omitted it
       means all etypes, and if string is omitted is means the default
       string (for that principal). Additional special values:
       v5 == pw-salt, and
       v4 == des:pw-salt:
       afs or afs3 == des:afs3-salt
    */

    if (ktypes == NULL
	&& krb5_config_get_bool (context, NULL, "kadmin",
				 "use_v4_salt", NULL))
	ktypes = v4_ktypes;

    for(kp = ktypes; kp && *kp; kp++) {
	krb5_enctype *etypes;
	int num_etypes;
	krb5_salt salt;
	krb5_boolean salt_set;

	const char *p;
	char buf[3][256];
	int num_buf = 0;

	p = *kp;
	if(strcmp(p, "v5") == 0)
	    p = "pw-salt";
	else if(strcmp(p, "v4") == 0)
	    p = "des:pw-salt:";
	else if(strcmp(p, "afs") == 0 || strcmp(p, "afs3") == 0)
	    p = "des:afs3-salt";
	
	/* split p in a list of :-separated strings */
	for(num_buf = 0; num_buf < 3; num_buf++)
	    if(strsep_copy(&p, ":", buf[num_buf], sizeof(buf[num_buf])) == -1)
		break;

	etypes = NULL;
	num_etypes = 0;
	memset(&salt, 0, sizeof(salt));
	salt_set = FALSE;

	for(i = 0; i < num_buf; i++) {
	    if(etypes == NULL) {
		/* this might be a etype specifier */
		/* XXX there should be a string_to_etypes handling
                   special cases like `des' and `all' */
		if(strcmp(buf[i], "des") == 0) {
		    etypes = all_etypes + 1;
		    num_etypes = 3;
		    continue;
		} else if(strcmp(buf[i], "des3") == 0) {
		    e = ETYPE_DES3_CBC_SHA1;
		    etypes = &e;
		    num_etypes = 1;
		    continue;
		} else {
		    ret = krb5_string_to_enctype(context, buf[i], &e);
		    if(ret == 0) {
			etypes = &e;
			num_etypes = 1;
			continue;
		    }
		}
	    }
	    if(salt.salttype == 0) {
		/* interpret string as a salt specifier, if no etype
                   is set, this sets default values */
		/* XXX should perhaps use string_to_salttype, but that
                   interface sucks */
		if(strcmp(buf[i], "pw-salt") == 0) {
		    if(etypes == NULL) {
			etypes = all_etypes;
			num_etypes = 4;
		    }
		    salt.salttype = KRB5_PW_SALT;
		} else if(strcmp(buf[i], "afs3-salt") == 0) {
		    if(etypes == NULL) {
			etypes = all_etypes + 1;
			num_etypes = 3;
		    }
		    salt.salttype = KRB5_AFS3_SALT;
		}
	    } else {
		/* if there is a final string, use it as the string to
                   salt with, this is mostly useful with null salt for
                   v4 compat, and a cell name for afs compat */
		salt.saltvalue.data = buf[i];
		salt.saltvalue.length = strlen(buf[i]);
		salt_set = TRUE;
	    }
	}

	if(etypes == NULL || salt.salttype == 0) {	    
	    krb5_warnx(context, "bad value for default_keys `%s'", *kp);
	    continue;
	}

	if(!salt_set) {
	    /* make up default salt */
	    if(salt.salttype == KRB5_PW_SALT)
		ret = krb5_get_pw_salt(context, principal, &salt);
	    else if(salt.salttype == KRB5_AFS3_SALT) {
		krb5_realm *realm = krb5_princ_realm(context, principal);
		salt.saltvalue.data = strdup(*realm);
		if(salt.saltvalue.data == NULL) {
		    krb5_set_error_string(context, "out of memory while "
					  "parsinig salt specifiers");
		    ret = ENOMEM;
		    goto out;
		}
		strlwr(salt.saltvalue.data);
		salt.saltvalue.length = strlen(*realm);
		salt_set = 1;
	    }
	}
	memset(&key, 0, sizeof(key));
	for(i = 0; i < num_etypes; i++) {
	    Key *k;
	    for(k = keys; k < keys + num_keys; k++) {
		if(k->key.keytype == etypes[i] &&
		   ((k->salt != NULL && 
		     k->salt->type == salt.salttype &&
		     k->salt->salt.length == salt.saltvalue.length &&
		     memcmp(k->salt->salt.data, salt.saltvalue.data, 
			    salt.saltvalue.length) == 0) ||
		    (k->salt == NULL && 
		     salt.salttype == KRB5_PW_SALT && 
		     !salt_set)))
		    goto next_etype;
	    }
		       
	    ret = krb5_string_to_key_salt (context,
					   etypes[i],
					   password,
					   salt,
					   &key.key);

	    if(ret)
		goto out;

	    if (salt.salttype != KRB5_PW_SALT || salt_set) {
		key.salt = malloc (sizeof(*key.salt));
		if (key.salt == NULL) {
		    free_Key(&key);
		    ret = ENOMEM;
		    goto out;
		}
		key.salt->type = salt.salttype;
		krb5_data_zero (&key.salt->salt);

		/* is the salt has not been set explicitly, it will be
		   the default salt, so there's no need to explicitly
		   copy it */
		if (salt_set) {
		    ret = krb5_data_copy(&key.salt->salt, 
					 salt.saltvalue.data, 
					 salt.saltvalue.length);
		    if (ret) {
			free_Key(&key);
			goto out;
		    }
		}
	    }
	    tmp = realloc(keys, (num_keys + 1) * sizeof(*keys));
	    if(tmp == NULL) {
		free_Key(&key);
		ret = ENOMEM;
		goto out;
	    }
	    keys = tmp;
	    keys[num_keys++] = key;
	  next_etype:;
	}
    }

    if(num_keys == 0) {
	/* if we didn't manage to find a single valid key, create a
           default set */
	/* XXX only do this is there is no `default_keys'? */
	krb5_salt v5_salt;
	tmp = realloc(keys, (num_keys + 4) * sizeof(*keys));
	if(tmp == NULL) {
	    ret = ENOMEM;
	    goto out;
	}
	keys = tmp;
	ret = krb5_get_pw_salt(context, principal, &v5_salt);
	if(ret)
	    goto out;
	for(i = 0; i < 4; i++) {
	    memset(&key, 0, sizeof(key));
	    ret = krb5_string_to_key_salt(context, all_etypes[i], password, 
					  v5_salt, &key.key);
	    if(ret) {
		krb5_free_salt(context, v5_salt);
		goto out;
	    }
	    keys[num_keys++] = key;
	}
	krb5_free_salt(context, v5_salt);
    }

  out:
    if(ret == 0) {
	*keys_ret = keys;
	*num_keys_ret = num_keys;
    } else {
	for(i = 0; i < num_keys; i++) {
	    free_Key(&keys[i]);
	}
	free(keys);
    }
    return ret;
}

/*
 * Set the keys of `ent' to the string-to-key of `password'
 */

kadm5_ret_t
_kadm5_set_keys(kadm5_server_context *context,
		hdb_entry *ent, 
		const char *password)
{
    kadm5_ret_t ret;
    Key *keys;
    size_t num_keys;

    ret = make_keys(context->context, ent->principal, password, 
		    &keys, &num_keys);

    if(ret)
	return ret;
    
    _kadm5_free_keys (context, ent->keys.len, ent->keys.val);
    ent->keys.val = keys;
    ent->keys.len = num_keys;
    ent->kvno++;
    return 0;
}

/*
 * Set the keys of `ent' to (`n_key_data', `key_data')
 */

kadm5_ret_t
_kadm5_set_keys2(kadm5_server_context *context,
		 hdb_entry *ent, 
		 int16_t n_key_data, 
		 krb5_key_data *key_data)
{
    krb5_error_code ret;
    int i;
    unsigned len;
    Key *keys;

    len  = n_key_data;
    keys = malloc (len * sizeof(*keys));
    if (keys == NULL)
	return ENOMEM;

    _kadm5_init_keys (keys, len);

    for(i = 0; i < n_key_data; i++) {
	keys[i].mkvno = NULL;
	keys[i].key.keytype = key_data[i].key_data_type[0];
	ret = krb5_data_copy(&keys[i].key.keyvalue,
			     key_data[i].key_data_contents[0],
			     key_data[i].key_data_length[0]);
	if(ret)
	    goto out;
	if(key_data[i].key_data_ver == 2) {
	    Salt *salt;

	    salt = malloc(sizeof(*salt));
	    if(salt == NULL) {
		ret = ENOMEM;
		goto out;
	    }
	    keys[i].salt = salt;
	    salt->type = key_data[i].key_data_type[1];
	    krb5_data_copy(&salt->salt, 
			   key_data[i].key_data_contents[1],
			   key_data[i].key_data_length[1]);
	} else
	    keys[i].salt = NULL;
    }
    _kadm5_free_keys (context, ent->keys.len, ent->keys.val);
    ent->keys.len = len;
    ent->keys.val = keys;
    ent->kvno++;
    return 0;
 out:
    _kadm5_free_keys (context, len, keys);
    return ret;
}

/*
 * Set the keys of `ent' to `n_keys, keys'
 */

kadm5_ret_t
_kadm5_set_keys3(kadm5_server_context *context,
		 hdb_entry *ent,
		 int n_keys,
		 krb5_keyblock *keyblocks)
{
    krb5_error_code ret;
    int i;
    unsigned len;
    Key *keys;

    len  = n_keys;
    keys = malloc (len * sizeof(*keys));
    if (keys == NULL)
	return ENOMEM;

    _kadm5_init_keys (keys, len);

    for(i = 0; i < n_keys; i++) {
	keys[i].mkvno = NULL;
	ret = krb5_copy_keyblock_contents (context->context,
					   &keyblocks[i],
					   &keys[i].key);
	if(ret)
	    goto out;
	keys[i].salt = NULL;
    }
    _kadm5_free_keys (context, ent->keys.len, ent->keys.val);
    ent->keys.len = len;
    ent->keys.val = keys;
    ent->kvno++;
    return 0;
 out:
    _kadm5_free_keys (context, len, keys);
    return ret;
}

/*
 * Set the keys of `ent' to random keys and return them in `n_keys'
 * and `new_keys'.
 */

kadm5_ret_t
_kadm5_set_keys_randomly (kadm5_server_context *context,
			  hdb_entry *ent,
			  krb5_keyblock **new_keys,
			  int *n_keys)
{
    kadm5_ret_t ret = 0;
    int i;
    unsigned len;
    krb5_keyblock *keys;
    Key *hkeys;

    len  = n_des_types + 1;
    keys = malloc (len * sizeof(*keys));
    if (keys == NULL)
	return ENOMEM;

    for (i = 0; i < len; ++i) {
	keys[i].keyvalue.length = 0;
	keys[i].keyvalue.data   = NULL;
    }

    hkeys = malloc (len * sizeof(*hkeys));
    if (hkeys == NULL) {
	free (keys);
	return ENOMEM;
    }

    _kadm5_init_keys (hkeys, len);

    ret = krb5_generate_random_keyblock (context->context,
					 des_types[0],
					 &keys[0]);
    if (ret)
	goto out;

    ret = krb5_copy_keyblock_contents (context->context,
				       &keys[0],
				       &hkeys[0].key);
    if (ret)
	goto out;

    for (i = 1; i < n_des_types; ++i) {
	ret = krb5_copy_keyblock_contents (context->context,
					   &keys[0],
					   &keys[i]);
	if (ret)
	    goto out;
	keys[i].keytype = des_types[i];
	ret = krb5_copy_keyblock_contents (context->context,
					   &keys[0],
					   &hkeys[i].key);
	if (ret)
	    goto out;
	hkeys[i].key.keytype = des_types[i];
    }

    ret = krb5_generate_random_keyblock (context->context,
					 ETYPE_DES3_CBC_SHA1,
					 &keys[n_des_types]);
    if (ret)
	goto out;

    ret = krb5_copy_keyblock_contents (context->context,
				       &keys[n_des_types],
				       &hkeys[n_des_types].key);
    if (ret)
	goto out;

    _kadm5_free_keys (context, ent->keys.len, ent->keys.val);
    ent->keys.len = len;
    ent->keys.val = hkeys;
    ent->kvno++;
    *new_keys     = keys;
    *n_keys       = len;
    return ret;
out:
    for (i = 0; i < len; ++i)
	krb5_free_keyblock_contents (context->context, &keys[i]);
    free (keys);
    _kadm5_free_keys (context, len, hkeys);
    return ret;
}
