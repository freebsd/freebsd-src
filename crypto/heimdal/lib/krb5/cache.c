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

#include "krb5_locl.h"

RCSID("$Id: cache.c,v 1.49 2002/05/29 16:08:23 joda Exp $");

/*
 * Add a new ccache type with operations `ops', overwriting any
 * existing one if `override'.
 * Return an error code or 0.
 */

krb5_error_code
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
 * Allocate memory for a new ccache in `id' with operations `ops'
 * and name `residual'.
 * Return 0 or an error code.
 */

static krb5_error_code
allocate_ccache (krb5_context context,
		 const krb5_cc_ops *ops,
		 const char *residual,
		 krb5_ccache *id)
{
    krb5_error_code ret;
    krb5_ccache p;

    p = malloc(sizeof(*p));
    if(p == NULL) {
	krb5_set_error_string(context, "malloc: out of memory");
	return KRB5_CC_NOMEM;
    }
    p->ops = ops;
    *id = p;
    ret = p->ops->resolve(context, id, residual);
    if(ret)
	free(p);
    return ret;
}

/*
 * Find and allocate a ccache in `id' from the specification in `residual'.
 * If the ccache name doesn't contain any colon, interpret it as a file name.
 * Return 0 or an error code.
 */

krb5_error_code
krb5_cc_resolve(krb5_context context,
		const char *name,
		krb5_ccache *id)
{
    int i;

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

/*
 * Generate a new ccache of type `ops' in `id'.
 * Return 0 or an error code.
 */

krb5_error_code
krb5_cc_gen_new(krb5_context context,
		const krb5_cc_ops *ops,
		krb5_ccache *id)
{
    krb5_ccache p;

    p = malloc (sizeof(*p));
    if (p == NULL) {
	krb5_set_error_string(context, "malloc: out of memory");
	return KRB5_CC_NOMEM;
    }
    p->ops = ops;
    *id = p;
    return p->ops->gen_new(context, id);
}

/*
 * Return the name of the ccache `id'
 */

const char*
krb5_cc_get_name(krb5_context context,
		 krb5_ccache id)
{
    return id->ops->get_name(context, id);
}

/*
 * Return the type of the ccache `id'.
 */

const char*
krb5_cc_get_type(krb5_context context,
		 krb5_ccache id)
{
    return id->ops->prefix;
}

/*
 * Return a pointer to a static string containing the default ccache name.
 */

const char*
krb5_cc_default_name(krb5_context context)
{
    static char name[1024];
    char *p;

    p = getenv("KRB5CCNAME");
    if(p)
	strlcpy (name, p, sizeof(name));
    else
	snprintf(name,
		 sizeof(name),
		 "FILE:/tmp/krb5cc_%u",
		 (unsigned)getuid());
    return name;
}

/*
 * Open the default ccache in `id'.
 * Return 0 or an error code.
 */

krb5_error_code
krb5_cc_default(krb5_context context,
		krb5_ccache *id)
{
    return krb5_cc_resolve(context, 
			   krb5_cc_default_name(context), 
			   id);
}

/*
 * Create a new ccache in `id' for `primary_principal'.
 * Return 0 or an error code.
 */

krb5_error_code
krb5_cc_initialize(krb5_context context,
		   krb5_ccache id,
		   krb5_principal primary_principal)
{
    return id->ops->init(context, id, primary_principal);
}


/*
 * Remove the ccache `id'.
 * Return 0 or an error code.
 */

krb5_error_code
krb5_cc_destroy(krb5_context context,
		krb5_ccache id)
{
    krb5_error_code ret;

    ret = id->ops->destroy(context, id);
    krb5_cc_close (context, id);
    return ret;
}

/*
 * Stop using the ccache `id' and free the related resources.
 * Return 0 or an error code.
 */

krb5_error_code
krb5_cc_close(krb5_context context,
	      krb5_ccache id)
{
    krb5_error_code ret;
    ret = id->ops->close(context, id);
    free(id);
    return ret;
}

/*
 * Store `creds' in the ccache `id'.
 * Return 0 or an error code.
 */

krb5_error_code
krb5_cc_store_cred(krb5_context context,
		   krb5_ccache id,
		   krb5_creds *creds)
{
    return id->ops->store(context, id, creds);
}

/*
 * Retrieve the credential identified by `mcreds' (and `whichfields')
 * from `id' in `creds'.
 * Return 0 or an error code.
 */

krb5_error_code
krb5_cc_retrieve_cred(krb5_context context,
		      krb5_ccache id,
		      krb5_flags whichfields,
		      const krb5_creds *mcreds,
		      krb5_creds *creds)
{
    krb5_error_code ret;
    krb5_cc_cursor cursor;
    krb5_cc_start_seq_get(context, id, &cursor);
    while((ret = krb5_cc_next_cred(context, id, &cursor, creds)) == 0){
	if(krb5_compare_creds(context, whichfields, mcreds, creds)){
	    ret = 0;
	    break;
	}
	krb5_free_creds_contents (context, creds);
    }
    krb5_cc_end_seq_get(context, id, &cursor);
    return ret;
}

/*
 * Return the principal of `id' in `principal'.
 * Return 0 or an error code.
 */

krb5_error_code
krb5_cc_get_principal(krb5_context context,
		      krb5_ccache id,
		      krb5_principal *principal)
{
    return id->ops->get_princ(context, id, principal);
}

/*
 * Start iterating over `id', `cursor' is initialized to the
 * beginning.
 * Return 0 or an error code.
 */

krb5_error_code
krb5_cc_start_seq_get (krb5_context context,
		       const krb5_ccache id,
		       krb5_cc_cursor *cursor)
{
    return id->ops->get_first(context, id, cursor);
}

/*
 * Retrieve the next cred pointed to by (`id', `cursor') in `creds'
 * and advance `cursor'.
 * Return 0 or an error code.
 */

krb5_error_code
krb5_cc_next_cred (krb5_context context,
		   const krb5_ccache id,
		   krb5_cc_cursor *cursor,
		   krb5_creds *creds)
{
    return id->ops->get_next(context, id, cursor, creds);
}

/*
 * Destroy the cursor `cursor'.
 */

krb5_error_code
krb5_cc_end_seq_get (krb5_context context,
		     const krb5_ccache id,
		     krb5_cc_cursor *cursor)
{
    return id->ops->end_get(context, id, cursor);
}

/*
 * Remove the credential identified by `cred', `which' from `id'.
 */

krb5_error_code
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

/*
 * Set the flags of `id' to `flags'.
 */

krb5_error_code
krb5_cc_set_flags(krb5_context context,
		  krb5_ccache id,
		  krb5_flags flags)
{
    return id->ops->set_flags(context, id, flags);
}
		    
/*
 * Copy the contents of `from' to `to'.
 */

krb5_error_code
krb5_cc_copy_cache(krb5_context context,
		   const krb5_ccache from,
		   krb5_ccache to)
{
    krb5_error_code ret;
    krb5_cc_cursor cursor;
    krb5_creds cred;
    krb5_principal princ;

    ret = krb5_cc_get_principal(context, from, &princ);
    if(ret)
	return ret;
    ret = krb5_cc_initialize(context, to, princ);
    if(ret){
	krb5_free_principal(context, princ);
	return ret;
    }
    ret = krb5_cc_start_seq_get(context, from, &cursor);
    if(ret){
	krb5_free_principal(context, princ);
	return ret;
    }
    while(ret == 0 && krb5_cc_next_cred(context, from, &cursor, &cred) == 0){
	ret = krb5_cc_store_cred(context, to, &cred);
	krb5_free_creds_contents (context, &cred);
    }
    krb5_cc_end_seq_get(context, from, &cursor);
    krb5_free_principal(context, princ);
    return ret;
}

/*
 * Return the version of `id'.
 */

krb5_error_code
krb5_cc_get_version(krb5_context context,
		    const krb5_ccache id)
{
    if(id->ops->get_version)
	return id->ops->get_version(context, id);
    else
	return 0;
}
