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

#include "krb5_locl.h"

RCSID("$Id: keytab.c,v 1.52 2002/01/30 10:09:35 joda Exp $");

/*
 * Register a new keytab in `ops'
 * Return 0 or an error.
 */

krb5_error_code
krb5_kt_register(krb5_context context,
		 const krb5_kt_ops *ops)
{
    struct krb5_keytab_data *tmp;

    tmp = realloc(context->kt_types,
		  (context->num_kt_types + 1) * sizeof(*context->kt_types));
    if(tmp == NULL) {
	krb5_set_error_string(context, "malloc: out of memory");
	return ENOMEM;
    }
    memcpy(&tmp[context->num_kt_types], ops,
	   sizeof(tmp[context->num_kt_types]));
    context->kt_types = tmp;
    context->num_kt_types++;
    return 0;
}

/*
 * Resolve the keytab name (of the form `type:residual') in `name'
 * into a keytab in `id'.
 * Return 0 or an error
 */

krb5_error_code
krb5_kt_resolve(krb5_context context,
		const char *name,
		krb5_keytab *id)
{
    krb5_keytab k;
    int i;
    const char *type, *residual;
    size_t type_len;
    krb5_error_code ret;

    residual = strchr(name, ':');
    if(residual == NULL) {
	type = "FILE";
	type_len = strlen(type);
	residual = name;
    } else {
	type = name;
	type_len = residual - name;
	residual++;
    }
    
    for(i = 0; i < context->num_kt_types; i++) {
	if(strncasecmp(type, context->kt_types[i].prefix, type_len) == 0)
	    break;
    }
    if(i == context->num_kt_types) {
	krb5_set_error_string(context, "unknown keytab type %.*s", 
			      (int)type_len, type);
	return KRB5_KT_UNKNOWN_TYPE;
    }
    
    k = malloc (sizeof(*k));
    if (k == NULL) {
	krb5_set_error_string(context, "malloc: out of memory");
	return ENOMEM;
    }
    memcpy(k, &context->kt_types[i], sizeof(*k));
    k->data = NULL;
    ret = (*k->resolve)(context, residual, k);
    if(ret) {
	free(k);
	k = NULL;
    }
    *id = k;
    return ret;
}

/*
 * copy the name of the default keytab into `name'.
 * Return 0 or KRB5_CONFIG_NOTENUFSPACE if `namesize' is too short.
 */

krb5_error_code
krb5_kt_default_name(krb5_context context, char *name, size_t namesize)
{
    if (strlcpy (name, context->default_keytab, namesize) >= namesize) {
	krb5_clear_error_string (context);
	return KRB5_CONFIG_NOTENUFSPACE;
    }
    return 0;
}

/*
 * copy the name of the default modify keytab into `name'.
 * Return 0 or KRB5_CONFIG_NOTENUFSPACE if `namesize' is too short.
 */

krb5_error_code
krb5_kt_default_modify_name(krb5_context context, char *name, size_t namesize)
{
    const char *kt = NULL;
    if(context->default_keytab_modify == NULL) {
	if(strncasecmp(context->default_keytab, "ANY:", 4) != 0)
	    kt = context->default_keytab;
	else {
	    size_t len = strcspn(context->default_keytab + 4, ",");
	    if(len >= namesize) {
		krb5_clear_error_string(context);
		return KRB5_CONFIG_NOTENUFSPACE;
	    }
	    strlcpy(name, context->default_keytab + 4, namesize);
	    name[len] = '\0';
	    return 0;
	}    
    } else
	kt = context->default_keytab_modify;
    if (strlcpy (name, kt, namesize) >= namesize) {
	krb5_clear_error_string (context);
	return KRB5_CONFIG_NOTENUFSPACE;
    }
    return 0;
}

/*
 * Set `id' to the default keytab.
 * Return 0 or an error.
 */

krb5_error_code
krb5_kt_default(krb5_context context, krb5_keytab *id)
{
    return krb5_kt_resolve (context, context->default_keytab, id);
}

/*
 * Read the key identified by `(principal, vno, enctype)' from the
 * keytab in `keyprocarg' (the default if == NULL) into `*key'.
 * Return 0 or an error.
 */

krb5_error_code
krb5_kt_read_service_key(krb5_context context,
			 krb5_pointer keyprocarg,
			 krb5_principal principal,
			 krb5_kvno vno,
			 krb5_enctype enctype,
			 krb5_keyblock **key)
{
    krb5_keytab keytab;
    krb5_keytab_entry entry;
    krb5_error_code ret;

    if (keyprocarg)
	ret = krb5_kt_resolve (context, keyprocarg, &keytab);
    else
	ret = krb5_kt_default (context, &keytab);

    if (ret)
	return ret;

    ret = krb5_kt_get_entry (context, keytab, principal, vno, enctype, &entry);
    krb5_kt_close (context, keytab);
    if (ret)
	return ret;
    ret = krb5_copy_keyblock (context, &entry.keyblock, key);
    krb5_kt_free_entry(context, &entry);
    return ret;
}

/*
 * Retrieve the name of the keytab `keytab' into `name', `namesize'
 * Return 0 or an error.
 */

krb5_error_code
krb5_kt_get_name(krb5_context context, 
		 krb5_keytab keytab,
		 char *name,
		 size_t namesize)
{
    return (*keytab->get_name)(context, keytab, name, namesize);
}

/*
 * Finish using the keytab in `id'.  All resources will be released.
 * Return 0 or an error.
 */

krb5_error_code
krb5_kt_close(krb5_context context, 
	      krb5_keytab id)
{
    krb5_error_code ret;

    ret = (*id->close)(context, id);
    if(ret == 0)
	free(id);
    return ret;
}

/*
 * Compare `entry' against `principal, vno, enctype'.
 * Any of `principal, vno, enctype' might be 0 which acts as a wildcard.
 * Return TRUE if they compare the same, FALSE otherwise.
 */

krb5_boolean
krb5_kt_compare(krb5_context context,
		krb5_keytab_entry *entry, 
		krb5_const_principal principal,
		krb5_kvno vno,
		krb5_enctype enctype)
{
    if(principal != NULL && 
       !krb5_principal_compare(context, entry->principal, principal))
	return FALSE;
    if(vno && vno != entry->vno)
	return FALSE;
    if(enctype && enctype != entry->keyblock.keytype)
	return FALSE;
    return TRUE;
}

/*
 * Retrieve the keytab entry for `principal, kvno, enctype' into `entry'
 * from the keytab `id'.
 * Return 0 or an error.
 */

krb5_error_code
krb5_kt_get_entry(krb5_context context,
		  krb5_keytab id,
		  krb5_const_principal principal,
		  krb5_kvno kvno,
		  krb5_enctype enctype,
		  krb5_keytab_entry *entry)
{
    krb5_keytab_entry tmp;
    krb5_error_code ret;
    krb5_kt_cursor cursor;

    if(id->get)
	return (*id->get)(context, id, principal, kvno, enctype, entry);

    ret = krb5_kt_start_seq_get (context, id, &cursor);
    if (ret)
	return KRB5_KT_NOTFOUND; /* XXX i.e. file not found */

    entry->vno = 0;
    while (krb5_kt_next_entry(context, id, &tmp, &cursor) == 0) {
	if (krb5_kt_compare(context, &tmp, principal, 0, enctype)) {
	    if (kvno == tmp.vno) {
		krb5_kt_copy_entry_contents (context, &tmp, entry);
		krb5_kt_free_entry (context, &tmp);
		krb5_kt_end_seq_get(context, id, &cursor);
		return 0;
	    } else if (kvno == 0 && tmp.vno > entry->vno) {
		if (entry->vno)
		    krb5_kt_free_entry (context, entry);
		krb5_kt_copy_entry_contents (context, &tmp, entry);
	    }
	}
	krb5_kt_free_entry(context, &tmp);
    }
    krb5_kt_end_seq_get (context, id, &cursor);
    if (entry->vno) {
	return 0;
    } else {
	char princ[256], kt_name[256];

	krb5_unparse_name_fixed (context, principal, princ, sizeof(princ));
	krb5_kt_get_name (context, id, kt_name, sizeof(kt_name));

	krb5_set_error_string (context,
 			       "failed to find %s in keytab %s",
			       princ, kt_name);
	return KRB5_KT_NOTFOUND;
    }
}

/*
 * Copy the contents of `in' into `out'.
 * Return 0 or an error.
 */

krb5_error_code
krb5_kt_copy_entry_contents(krb5_context context,
			    const krb5_keytab_entry *in,
			    krb5_keytab_entry *out)
{
    krb5_error_code ret;

    memset(out, 0, sizeof(*out));
    out->vno = in->vno;

    ret = krb5_copy_principal (context, in->principal, &out->principal);
    if (ret)
	goto fail;
    ret = krb5_copy_keyblock_contents (context,
				       &in->keyblock,
				       &out->keyblock);
    if (ret)
	goto fail;
    out->timestamp = in->timestamp;
    return 0;
fail:
    krb5_kt_free_entry (context, out);
    return ret;
}

/*
 * Free the contents of `entry'.
 */

krb5_error_code
krb5_kt_free_entry(krb5_context context,
		   krb5_keytab_entry *entry)
{
  krb5_free_principal (context, entry->principal);
  krb5_free_keyblock_contents (context, &entry->keyblock);
  return 0;
}

#if 0
static int
xxxlock(int fd, int write)
{
    if(flock(fd, (write ? LOCK_EX : LOCK_SH) | LOCK_NB) < 0) {
	sleep(1);
	if(flock(fd, (write ? LOCK_EX : LOCK_SH) | LOCK_NB) < 0) 
	    return -1;
    }
    return 0;
}

static void
xxxunlock(int fd)
{
    flock(fd, LOCK_UN);
}
#endif

/*
 * Set `cursor' to point at the beginning of `id'.
 * Return 0 or an error.
 */

krb5_error_code
krb5_kt_start_seq_get(krb5_context context,
		      krb5_keytab id,
		      krb5_kt_cursor *cursor)
{
    if(id->start_seq_get == NULL) {
	krb5_set_error_string(context,
			      "start_seq_get is not supported in the %s "
			      " keytab", id->prefix);
	return HEIM_ERR_OPNOTSUPP;
    }
    return (*id->start_seq_get)(context, id, cursor);
}

/*
 * Get the next entry from `id' pointed to by `cursor' and advance the
 * `cursor'.
 * Return 0 or an error.
 */

krb5_error_code
krb5_kt_next_entry(krb5_context context,
		   krb5_keytab id,
		   krb5_keytab_entry *entry,
		   krb5_kt_cursor *cursor)
{
    if(id->next_entry == NULL) {
	krb5_set_error_string(context,
			      "next_entry is not supported in the %s "
			      " keytab", id->prefix);
	return HEIM_ERR_OPNOTSUPP;
    }
    return (*id->next_entry)(context, id, entry, cursor);
}

/*
 * Release all resources associated with `cursor'.
 */

krb5_error_code
krb5_kt_end_seq_get(krb5_context context,
		    krb5_keytab id,
		    krb5_kt_cursor *cursor)
{
    if(id->end_seq_get == NULL) {
	krb5_set_error_string(context,
			      "end_seq_get is not supported in the %s "
			      " keytab", id->prefix);
	return HEIM_ERR_OPNOTSUPP;
    }
    return (*id->end_seq_get)(context, id, cursor);
}

/*
 * Add the entry in `entry' to the keytab `id'.
 * Return 0 or an error.
 */

krb5_error_code
krb5_kt_add_entry(krb5_context context,
		  krb5_keytab id,
		  krb5_keytab_entry *entry)
{
    if(id->add == NULL) {
	krb5_set_error_string(context, "Add is not supported in the %s keytab",
			      id->prefix);
	return KRB5_KT_NOWRITE;
    }
    entry->timestamp = time(NULL);
    return (*id->add)(context, id,entry);
}

/*
 * Remove the entry `entry' from the keytab `id'.
 * Return 0 or an error.
 */

krb5_error_code
krb5_kt_remove_entry(krb5_context context,
		     krb5_keytab id,
		     krb5_keytab_entry *entry)
{
    if(id->remove == NULL) {
	krb5_set_error_string(context,
			      "Remove is not supported in the %s keytab",
			      id->prefix);
	return KRB5_KT_NOWRITE;
    }
    return (*id->remove)(context, id, entry);
}
