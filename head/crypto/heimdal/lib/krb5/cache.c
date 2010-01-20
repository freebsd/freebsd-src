/*
 * Copyright (c) 1997 - 2007 Kungliga Tekniska Högskolan
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

RCSID("$Id: cache.c 22127 2007-12-04 00:54:37Z lha $");

/**
 * Add a new ccache type with operations `ops', overwriting any
 * existing one if `override'.
 *
 * @param context a Keberos context
 * @param ops type of plugin symbol
 * @param override flag to select if the registration is to overide
 * an existing ops with the same name.
 *
 * @return Return an error code or 0.
 *
 * @ingroup krb5_ccache
 */

krb5_error_code KRB5_LIB_FUNCTION
krb5_cc_register(krb5_context context, 
		 const krb5_cc_ops *ops, 
		 krb5_boolean override)
{
    int i;

    for(i = 0; i < context->num_cc_ops && context->cc_ops[i].prefix; i++) {
	if(strcmp(context->cc_ops[i].prefix, ops->prefix) == 0) {
	    if(!override) {
		krb5_set_error_string(context,
				      "ccache type %s already exists",
				      ops->prefix);
		return KRB5_CC_TYPE_EXISTS;
	    }
	    break;
	}
    }
    if(i == context->num_cc_ops) {
	krb5_cc_ops *o = realloc(context->cc_ops,
				 (context->num_cc_ops + 1) *
				 sizeof(*context->cc_ops));
	if(o == NULL) {
	    krb5_set_error_string(context, "malloc: out of memory");
	    return KRB5_CC_NOMEM;
	}
	context->num_cc_ops++;
	context->cc_ops = o;
	memset(context->cc_ops + i, 0, 
	       (context->num_cc_ops - i) * sizeof(*context->cc_ops));
    }
    memcpy(&context->cc_ops[i], ops, sizeof(context->cc_ops[i]));
    return 0;
}

/*
 * Allocate the memory for a `id' and the that function table to
 * `ops'. Returns 0 or and error code.
 */

krb5_error_code
_krb5_cc_allocate(krb5_context context, 
		  const krb5_cc_ops *ops,
		  krb5_ccache *id)
{
    krb5_ccache p;

    p = malloc (sizeof(*p));
    if(p == NULL) {
	krb5_set_error_string(context, "malloc: out of memory");
	return KRB5_CC_NOMEM;
    }
    p->ops = ops;
    *id = p;

    return 0;
}

/*
 * Allocate memory for a new ccache in `id' with operations `ops'
 * and name `residual'. Return 0 or an error code.
 */

static krb5_error_code
allocate_ccache (krb5_context context,
		 const krb5_cc_ops *ops,
		 const char *residual,
		 krb5_ccache *id)
{
    krb5_error_code ret;

    ret = _krb5_cc_allocate(context, ops, id);
    if (ret)
	return ret;
    ret = (*id)->ops->resolve(context, id, residual);
    if(ret)
	free(*id);
    return ret;
}

/**
 * Find and allocate a ccache in `id' from the specification in `residual'.
 * If the ccache name doesn't contain any colon, interpret it as a file name.
 *
 * @param context a Keberos context.
 * @param name string name of a credential cache.
 * @param id return pointer to a found credential cache.
 *
 * @return Return 0 or an error code. In case of an error, id is set
 * to NULL.
 *
 * @ingroup krb5_ccache
 */


krb5_error_code KRB5_LIB_FUNCTION
krb5_cc_resolve(krb5_context context,
		const char *name,
		krb5_ccache *id)
{
    int i;

    *id = NULL;

    for(i = 0; i < context->num_cc_ops && context->cc_ops[i].prefix; i++) {
	size_t prefix_len = strlen(context->cc_ops[i].prefix);

	if(strncmp(context->cc_ops[i].prefix, name, prefix_len) == 0
	   && name[prefix_len] == ':') {
	    return allocate_ccache (context, &context->cc_ops[i],
				    name + prefix_len + 1,
				    id);
	}
    }
    if (strchr (name, ':') == NULL)
	return allocate_ccache (context, &krb5_fcc_ops, name, id);
    else {
	krb5_set_error_string(context, "unknown ccache type %s", name);
	return KRB5_CC_UNKNOWN_TYPE;
    }
}

/**
 * Generate a new ccache of type `ops' in `id'.
 *
 * @return Return 0 or an error code.
 *
 * @ingroup krb5_ccache
 */


krb5_error_code KRB5_LIB_FUNCTION
krb5_cc_gen_new(krb5_context context,
		const krb5_cc_ops *ops,
		krb5_ccache *id)
{
    return krb5_cc_new_unique(context, ops->prefix, NULL, id);
}

/**
 * Generates a new unique ccache of `type` in `id'. If `type' is NULL,
 * the library chooses the default credential cache type. The supplied
 * `hint' (that can be NULL) is a string that the credential cache
 * type can use to base the name of the credential on, this is to make
 * it easier for the user to differentiate the credentials.
 *
 * @return Returns 0 or an error code.
 *
 * @ingroup krb5_ccache
 */

krb5_error_code KRB5_LIB_FUNCTION
krb5_cc_new_unique(krb5_context context, const char *type, 
		   const char *hint, krb5_ccache *id)
{
    const krb5_cc_ops *ops = KRB5_DEFAULT_CCTYPE;
    krb5_error_code ret;

    if (type) {
	ops = krb5_cc_get_prefix_ops(context, type);
	if (ops == NULL) {
	    krb5_set_error_string(context,
				  "Credential cache type %s is unknown", type);
	    return KRB5_CC_UNKNOWN_TYPE;
	}
    }

    ret = _krb5_cc_allocate(context, ops, id);
    if (ret)
	return ret;
    return (*id)->ops->gen_new(context, id);
}

/**
 * Return the name of the ccache `id'
 *
 * @ingroup krb5_ccache
 */


const char* KRB5_LIB_FUNCTION
krb5_cc_get_name(krb5_context context,
		 krb5_ccache id)
{
    return id->ops->get_name(context, id);
}

/**
 * Return the type of the ccache `id'.
 *
 * @ingroup krb5_ccache
 */


const char* KRB5_LIB_FUNCTION
krb5_cc_get_type(krb5_context context,
		 krb5_ccache id)
{
    return id->ops->prefix;
}

/**
 * Return the complete resolvable name the ccache `id' in `str´.
 * `str` should be freed with free(3).
 * Returns 0 or an error (and then *str is set to NULL).
 *
 * @ingroup krb5_ccache
 */


krb5_error_code KRB5_LIB_FUNCTION
krb5_cc_get_full_name(krb5_context context,
		      krb5_ccache id,
		      char **str)
{
    const char *type, *name;

    *str = NULL;

    type = krb5_cc_get_type(context, id);
    if (type == NULL) {
	krb5_set_error_string(context, "cache have no name of type");
	return KRB5_CC_UNKNOWN_TYPE;
    }

    name = krb5_cc_get_name(context, id);
    if (name == NULL) {
	krb5_set_error_string(context, "cache of type %s have no name", type);
	return KRB5_CC_BADNAME;
    }
    
    if (asprintf(str, "%s:%s", type, name) == -1) {
	krb5_set_error_string(context, "malloc - out of memory");
	*str = NULL;
	return ENOMEM;
    }
    return 0;
}

/**
 * Return krb5_cc_ops of a the ccache `id'.
 *
 * @ingroup krb5_ccache
 */


const krb5_cc_ops *
krb5_cc_get_ops(krb5_context context, krb5_ccache id)
{
    return id->ops;
}

/*
 * Expand variables in `str' into `res'
 */

krb5_error_code
_krb5_expand_default_cc_name(krb5_context context, const char *str, char **res)
{
    size_t tlen, len = 0;
    char *tmp, *tmp2, *append;

    *res = NULL;

    while (str && *str) {
	tmp = strstr(str, "%{");
	if (tmp && tmp != str) {
	    append = malloc((tmp - str) + 1);
	    if (append) {
		memcpy(append, str, tmp - str);
		append[tmp - str] = '\0';
	    }
	    str = tmp;
	} else if (tmp) {
	    tmp2 = strchr(tmp, '}');
	    if (tmp2 == NULL) {
		free(*res);
		*res = NULL;
		krb5_set_error_string(context, "variable missing }");
		return KRB5_CONFIG_BADFORMAT;
	    }
	    if (strncasecmp(tmp, "%{uid}", 6) == 0)
		asprintf(&append, "%u", (unsigned)getuid());
	    else if (strncasecmp(tmp, "%{null}", 7) == 0)
		append = strdup("");
	    else {
		free(*res);
		*res = NULL;
		krb5_set_error_string(context, 
				      "expand default cache unknown "
				      "variable \"%.*s\"",
				      (int)(tmp2 - tmp) - 2, tmp + 2);
		return KRB5_CONFIG_BADFORMAT;
	    }
	    str = tmp2 + 1;
	} else {
	    append = strdup(str);
	    str = NULL;
	}
	if (append == NULL) {
	    free(*res);
	    *res = NULL;
	    krb5_set_error_string(context, "malloc - out of memory");
	    return ENOMEM;
	}
	
	tlen = strlen(append);
	tmp = realloc(*res, len + tlen + 1);
	if (tmp == NULL) {
	    free(append);
	    free(*res);
	    *res = NULL;
	    krb5_set_error_string(context, "malloc - out of memory");
	    return ENOMEM;
	}
	*res = tmp;
	memcpy(*res + len, append, tlen + 1);
	len = len + tlen;
	free(append);
    }    
    return 0;
}

/*
 * Return non-zero if envirnoment that will determine default krb5cc
 * name has changed.
 */

static int
environment_changed(krb5_context context)
{
    const char *e;

    /* if the cc name was set, don't change it */
    if (context->default_cc_name_set)
	return 0;

    if(issuid())
	return 0;

    e = getenv("KRB5CCNAME");
    if (e == NULL) {
	if (context->default_cc_name_env) {
	    free(context->default_cc_name_env);
	    context->default_cc_name_env = NULL;
	    return 1;
	}
    } else {
	if (context->default_cc_name_env == NULL)
	    return 1;
	if (strcmp(e, context->default_cc_name_env) != 0)
	    return 1;
    }
    return 0;
}

/**
 * Set the default cc name for `context' to `name'.
 *
 * @ingroup krb5_ccache
 */


krb5_error_code KRB5_LIB_FUNCTION
krb5_cc_set_default_name(krb5_context context, const char *name)
{
    krb5_error_code ret = 0;
    char *p;

    if (name == NULL) {
	const char *e = NULL;

	if(!issuid()) {
	    e = getenv("KRB5CCNAME");
	    if (e) {
		p = strdup(e);
		if (context->default_cc_name_env)
		    free(context->default_cc_name_env);
		context->default_cc_name_env = strdup(e);
	    }
	}
	if (e == NULL) {
	    e = krb5_config_get_string(context, NULL, "libdefaults",
				       "default_cc_name", NULL);
	    if (e) {
		ret = _krb5_expand_default_cc_name(context, e, &p);
		if (ret)
		    return ret;
	    }
	    if (e == NULL) {
		const krb5_cc_ops *ops = KRB5_DEFAULT_CCTYPE;
		ret = (*ops->default_name)(context, &p);
		if (ret)
		    return ret;
	    }
	}
	context->default_cc_name_set = 0;
    } else {
	p = strdup(name);
	context->default_cc_name_set = 1;
    }

    if (p == NULL) {
	krb5_set_error_string(context, "malloc - out of memory");
	return ENOMEM;
    }

    if (context->default_cc_name)
	free(context->default_cc_name);

    context->default_cc_name = p;

    return ret;
}

/**
 * Return a pointer to a context static string containing the default
 * ccache name.
 *
 * @return String to the default credential cache name.
 *
 * @ingroup krb5_ccache
 */


const char* KRB5_LIB_FUNCTION
krb5_cc_default_name(krb5_context context)
{
    if (context->default_cc_name == NULL || environment_changed(context))
	krb5_cc_set_default_name(context, NULL);

    return context->default_cc_name;
}

/**
 * Open the default ccache in `id'.
 *
 * @return Return 0 or an error code.
 *
 * @ingroup krb5_ccache
 */


krb5_error_code KRB5_LIB_FUNCTION
krb5_cc_default(krb5_context context,
		krb5_ccache *id)
{
    const char *p = krb5_cc_default_name(context);

    if (p == NULL) {
	krb5_set_error_string(context, "malloc - out of memory");
	return ENOMEM;
    }
    return krb5_cc_resolve(context, p, id);
}

/**
 * Create a new ccache in `id' for `primary_principal'.
 *
 * @return Return 0 or an error code.
 *
 * @ingroup krb5_ccache
 */


krb5_error_code KRB5_LIB_FUNCTION
krb5_cc_initialize(krb5_context context,
		   krb5_ccache id,
		   krb5_principal primary_principal)
{
    return (*id->ops->init)(context, id, primary_principal);
}


/**
 * Remove the ccache `id'.
 *
 * @return Return 0 or an error code.
 *
 * @ingroup krb5_ccache
 */


krb5_error_code KRB5_LIB_FUNCTION
krb5_cc_destroy(krb5_context context,
		krb5_ccache id)
{
    krb5_error_code ret;

    ret = (*id->ops->destroy)(context, id);
    krb5_cc_close (context, id);
    return ret;
}

/**
 * Stop using the ccache `id' and free the related resources.
 *
 * @return Return 0 or an error code.
 *
 * @ingroup krb5_ccache
 */


krb5_error_code KRB5_LIB_FUNCTION
krb5_cc_close(krb5_context context,
	      krb5_ccache id)
{
    krb5_error_code ret;
    ret = (*id->ops->close)(context, id);
    free(id);
    return ret;
}

/**
 * Store `creds' in the ccache `id'.
 *
 * @return Return 0 or an error code.
 *
 * @ingroup krb5_ccache
 */


krb5_error_code KRB5_LIB_FUNCTION
krb5_cc_store_cred(krb5_context context,
		   krb5_ccache id,
		   krb5_creds *creds)
{
    return (*id->ops->store)(context, id, creds);
}

/**
 * Retrieve the credential identified by `mcreds' (and `whichfields')
 * from `id' in `creds'. 'creds' must be free by the caller using
 * krb5_free_cred_contents.
 *
 * @return Return 0 or an error code.
 *
 * @ingroup krb5_ccache
 */


krb5_error_code KRB5_LIB_FUNCTION
krb5_cc_retrieve_cred(krb5_context context,
		      krb5_ccache id,
		      krb5_flags whichfields,
		      const krb5_creds *mcreds,
		      krb5_creds *creds)
{
    krb5_error_code ret;
    krb5_cc_cursor cursor;

    if (id->ops->retrieve != NULL) {
	return (*id->ops->retrieve)(context, id, whichfields,
				    mcreds, creds);
    }

    ret = krb5_cc_start_seq_get(context, id, &cursor);
    if (ret)
	return ret;
    while((ret = krb5_cc_next_cred(context, id, &cursor, creds)) == 0){
	if(krb5_compare_creds(context, whichfields, mcreds, creds)){
	    ret = 0;
	    break;
	}
	krb5_free_cred_contents (context, creds);
    }
    krb5_cc_end_seq_get(context, id, &cursor);
    return ret;
}

/**
 * Return the principal of `id' in `principal'.
 *
 * @return Return 0 or an error code.
 *
 * @ingroup krb5_ccache
 */


krb5_error_code KRB5_LIB_FUNCTION
krb5_cc_get_principal(krb5_context context,
		      krb5_ccache id,
		      krb5_principal *principal)
{
    return (*id->ops->get_princ)(context, id, principal);
}

/**
 * Start iterating over `id', `cursor' is initialized to the
 * beginning.
 *
 * @return Return 0 or an error code.
 *
 * @ingroup krb5_ccache
 */


krb5_error_code KRB5_LIB_FUNCTION
krb5_cc_start_seq_get (krb5_context context,
		       const krb5_ccache id,
		       krb5_cc_cursor *cursor)
{
    return (*id->ops->get_first)(context, id, cursor);
}

/**
 * Retrieve the next cred pointed to by (`id', `cursor') in `creds'
 * and advance `cursor'.
 *
 * @return Return 0 or an error code.
 *
 * @ingroup krb5_ccache
 */


krb5_error_code KRB5_LIB_FUNCTION
krb5_cc_next_cred (krb5_context context,
		   const krb5_ccache id,
		   krb5_cc_cursor *cursor,
		   krb5_creds *creds)
{
    return (*id->ops->get_next)(context, id, cursor, creds);
}

/**
 * Like krb5_cc_next_cred, but allow for selective retrieval
 *
 * @ingroup krb5_ccache
 */


krb5_error_code KRB5_LIB_FUNCTION
krb5_cc_next_cred_match(krb5_context context,
			const krb5_ccache id,
			krb5_cc_cursor * cursor,
			krb5_creds * creds,
			krb5_flags whichfields,
			const krb5_creds * mcreds)
{
    krb5_error_code ret;
    while (1) {
	ret = krb5_cc_next_cred(context, id, cursor, creds);
	if (ret)
	    return ret;
	if (mcreds == NULL || krb5_compare_creds(context, whichfields, mcreds, creds))
	    return 0;
	krb5_free_cred_contents(context, creds);
    }
}

/**
 * Destroy the cursor `cursor'.
 *
 * @ingroup krb5_ccache
 */


krb5_error_code KRB5_LIB_FUNCTION
krb5_cc_end_seq_get (krb5_context context,
		     const krb5_ccache id,
		     krb5_cc_cursor *cursor)
{
    return (*id->ops->end_get)(context, id, cursor);
}

/**
 * Remove the credential identified by `cred', `which' from `id'.
 *
 * @ingroup krb5_ccache
 */


krb5_error_code KRB5_LIB_FUNCTION
krb5_cc_remove_cred(krb5_context context,
		    krb5_ccache id,
		    krb5_flags which,
		    krb5_creds *cred)
{
    if(id->ops->remove_cred == NULL) {
	krb5_set_error_string(context,
			      "ccache %s does not support remove_cred",
			      id->ops->prefix);
	return EACCES; /* XXX */
    }
    return (*id->ops->remove_cred)(context, id, which, cred);
}

/**
 * Set the flags of `id' to `flags'.
 *
 * @ingroup krb5_ccache
 */


krb5_error_code KRB5_LIB_FUNCTION
krb5_cc_set_flags(krb5_context context,
		  krb5_ccache id,
		  krb5_flags flags)
{
    return (*id->ops->set_flags)(context, id, flags);
}
		    
/**
 * Copy the contents of `from' to `to'.
 *
 * @ingroup krb5_ccache
 */


krb5_error_code KRB5_LIB_FUNCTION
krb5_cc_copy_cache_match(krb5_context context,
			 const krb5_ccache from,
			 krb5_ccache to,
			 krb5_flags whichfields,
			 const krb5_creds * mcreds,
			 unsigned int *matched)
{
    krb5_error_code ret;
    krb5_cc_cursor cursor;
    krb5_creds cred;
    krb5_principal princ;

    ret = krb5_cc_get_principal(context, from, &princ);
    if (ret)
	return ret;
    ret = krb5_cc_initialize(context, to, princ);
    if (ret) {
	krb5_free_principal(context, princ);
	return ret;
    }
    ret = krb5_cc_start_seq_get(context, from, &cursor);
    if (ret) {
	krb5_free_principal(context, princ);
	return ret;
    }
    if (matched)
	*matched = 0;
    while (ret == 0 &&
	   krb5_cc_next_cred_match(context, from, &cursor, &cred,
				   whichfields, mcreds) == 0) {
	if (matched)
	    (*matched)++;
	ret = krb5_cc_store_cred(context, to, &cred);
	krb5_free_cred_contents(context, &cred);
    }
    krb5_cc_end_seq_get(context, from, &cursor);
    krb5_free_principal(context, princ);
    return ret;
}

/**
 * Just like krb5_cc_copy_cache_match, but copy everything.
 *
 * @ingroup krb5_ccache
 */


krb5_error_code KRB5_LIB_FUNCTION
krb5_cc_copy_cache(krb5_context context,
		   const krb5_ccache from,
		   krb5_ccache to)
{
    return krb5_cc_copy_cache_match(context, from, to, 0, NULL, NULL);
}

/**
 * Return the version of `id'.
 *
 * @ingroup krb5_ccache
 */


krb5_error_code KRB5_LIB_FUNCTION
krb5_cc_get_version(krb5_context context,
		    const krb5_ccache id)
{
    if(id->ops->get_version)
	return (*id->ops->get_version)(context, id);
    else
	return 0;
}

/**
 * Clear `mcreds' so it can be used with krb5_cc_retrieve_cred
 *
 * @ingroup krb5_ccache
 */


void KRB5_LIB_FUNCTION
krb5_cc_clear_mcred(krb5_creds *mcred)
{
    memset(mcred, 0, sizeof(*mcred));
}

/**
 * Get the cc ops that is registered in `context' to handle the
 * `prefix'. `prefix' can be a complete credential cache name or a
 * prefix, the function will only use part up to the first colon (:)
 * if there is one.
 * Returns NULL if ops not found.
 *
 * @ingroup krb5_ccache
 */


const krb5_cc_ops *
krb5_cc_get_prefix_ops(krb5_context context, const char *prefix)
{
    char *p, *p1;
    int i;
    
    if (prefix[0] == '/')
	return &krb5_fcc_ops;

    p = strdup(prefix);
    if (p == NULL) {
	krb5_set_error_string(context, "malloc - out of memory");
	return NULL;
    }
    p1 = strchr(p, ':');
    if (p1)
	*p1 = '\0';

    for(i = 0; i < context->num_cc_ops && context->cc_ops[i].prefix; i++) {
	if(strcmp(context->cc_ops[i].prefix, p) == 0) {
	    free(p);
	    return &context->cc_ops[i];
	}
    }
    free(p);
    return NULL;
}

struct krb5_cc_cache_cursor_data {
    const krb5_cc_ops *ops;
    krb5_cc_cursor cursor;
};

/**
 * Start iterating over all caches of `type'. If `type' is NULL, the
 * default type is * used. `cursor' is initialized to the beginning.
 *
 * @return Return 0 or an error code.
 *
 * @ingroup krb5_ccache
 */


krb5_error_code KRB5_LIB_FUNCTION
krb5_cc_cache_get_first (krb5_context context,
			 const char *type,
			 krb5_cc_cache_cursor *cursor)
{
    const krb5_cc_ops *ops;
    krb5_error_code ret;

    if (type == NULL)
	type = krb5_cc_default_name(context);

    ops = krb5_cc_get_prefix_ops(context, type);
    if (ops == NULL) {
	krb5_set_error_string(context, "Unknown type \"%s\" when iterating "
			      "trying to iterate the credential caches", type);
	return KRB5_CC_UNKNOWN_TYPE;
    }

    if (ops->get_cache_first == NULL) {
	krb5_set_error_string(context, "Credential cache type %s doesn't support "
			      "iterations over caches", ops->prefix);
	return KRB5_CC_NOSUPP;
    }

    *cursor = calloc(1, sizeof(**cursor));
    if (*cursor == NULL) {
	krb5_set_error_string(context, "malloc - out of memory");
	return ENOMEM;
    }

    (*cursor)->ops = ops;

    ret = ops->get_cache_first(context, &(*cursor)->cursor);
    if (ret) {
	free(*cursor);
	*cursor = NULL;
    }
    return ret;
}

/**
 * Retrieve the next cache pointed to by (`cursor') in `id'
 * and advance `cursor'.
 *
 * @return Return 0 or an error code.
 *
 * @ingroup krb5_ccache
 */


krb5_error_code KRB5_LIB_FUNCTION
krb5_cc_cache_next (krb5_context context,
		   krb5_cc_cache_cursor cursor,
		   krb5_ccache *id)
{
    return cursor->ops->get_cache_next(context, cursor->cursor, id);
}

/**
 * Destroy the cursor `cursor'.
 *
 * @return Return 0 or an error code.
 *
 * @ingroup krb5_ccache
 */


krb5_error_code KRB5_LIB_FUNCTION
krb5_cc_cache_end_seq_get (krb5_context context,
			   krb5_cc_cache_cursor cursor)
{
    krb5_error_code ret;
    ret = cursor->ops->end_cache_get(context, cursor->cursor);
    cursor->ops = NULL;
    free(cursor);
    return ret;
}

/**
 * Search for a matching credential cache of type `type' that have the
 * `principal' as the default principal. If NULL is used for `type',
 * the default type is used. On success, `id' needs to be freed with
 * krb5_cc_close or krb5_cc_destroy.
 *
 * @return On failure, error code is returned and `id' is set to NULL.
 *
 * @ingroup krb5_ccache
 */


krb5_error_code KRB5_LIB_FUNCTION
krb5_cc_cache_match (krb5_context context,
		     krb5_principal client,
		     const char *type,
		     krb5_ccache *id)
{
    krb5_cc_cache_cursor cursor;
    krb5_error_code ret;
    krb5_ccache cache = NULL;

    *id = NULL;

    ret = krb5_cc_cache_get_first (context, type, &cursor);
    if (ret)
	return ret;

    while ((ret = krb5_cc_cache_next (context, cursor, &cache)) == 0) {
	krb5_principal principal;

	ret = krb5_cc_get_principal(context, cache, &principal);
	if (ret == 0) {
	    krb5_boolean match;
	    
	    match = krb5_principal_compare(context, principal, client);
	    krb5_free_principal(context, principal);
	    if (match)
		break;
	}

	krb5_cc_close(context, cache);
	cache = NULL;
    }

    krb5_cc_cache_end_seq_get(context, cursor);

    if (cache == NULL) {
	char *str;

	krb5_unparse_name(context, client, &str);

	krb5_set_error_string(context, "Principal %s not found in a "
			  "credential cache", str ? str : "<out of memory>");
	if (str)
	    free(str);
	return KRB5_CC_NOTFOUND;
    }
    *id = cache;

    return 0;
}

/**
 * Move the content from one credential cache to another. The
 * operation is an atomic switch. 
 *
 * @param context a Keberos context
 * @param from the credential cache to move the content from
 * @param to the credential cache to move the content to

 * @return On sucess, from is freed. On failure, error code is
 * returned and from and to are both still allocated.
 *
 * @ingroup krb5_ccache
 */

krb5_error_code
krb5_cc_move(krb5_context context, krb5_ccache from, krb5_ccache to)
{
    krb5_error_code ret;

    if (strcmp(from->ops->prefix, to->ops->prefix) != 0) {
	krb5_set_error_string(context, "Moving credentials between diffrent "
			      "types not yet supported");
	return KRB5_CC_NOSUPP;
    }

    ret = (*to->ops->move)(context, from, to);
    if (ret == 0) {
	memset(from, 0, sizeof(*from));
	free(from);
    }
    return ret;
}
