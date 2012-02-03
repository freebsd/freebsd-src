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

RCSID("$Id: keyblock.c 15167 2005-05-18 04:21:57Z lha $");

void KRB5_LIB_FUNCTION
krb5_keyblock_zero(krb5_keyblock *keyblock)
{
    keyblock->keytype = 0;
    krb5_data_zero(&keyblock->keyvalue);
}

void KRB5_LIB_FUNCTION
krb5_free_keyblock_contents(krb5_context context,
			    krb5_keyblock *keyblock)
{
    if(keyblock) {
	if (keyblock->keyvalue.data != NULL)
	    memset(keyblock->keyvalue.data, 0, keyblock->keyvalue.length);
	krb5_data_free (&keyblock->keyvalue);
	keyblock->keytype = ENCTYPE_NULL;
    }
}

void KRB5_LIB_FUNCTION
krb5_free_keyblock(krb5_context context,
		   krb5_keyblock *keyblock)
{
    if(keyblock){
	krb5_free_keyblock_contents(context, keyblock);
	free(keyblock);
    }
}

krb5_error_code KRB5_LIB_FUNCTION
krb5_copy_keyblock_contents (krb5_context context,
			     const krb5_keyblock *inblock,
			     krb5_keyblock *to)
{
    return copy_EncryptionKey(inblock, to);
}

krb5_error_code KRB5_LIB_FUNCTION
krb5_copy_keyblock (krb5_context context,
		    const krb5_keyblock *inblock,
		    krb5_keyblock **to)
{
    krb5_keyblock *k;

    k = malloc (sizeof(*k));
    if (k == NULL) {
	krb5_set_error_string(context, "malloc: out of memory");
	return ENOMEM;
    }
    *to = k;
    return krb5_copy_keyblock_contents (context, inblock, k);
}

krb5_enctype
krb5_keyblock_get_enctype(const krb5_keyblock *block)
{
    return block->keytype;
}

/*
 * Fill in `key' with key data of type `enctype' from `data' of length
 * `size'. Key should be freed using krb5_free_keyblock_contents.
 */

krb5_error_code KRB5_LIB_FUNCTION
krb5_keyblock_init(krb5_context context,
		   krb5_enctype type,
		   const void *data,
		   size_t size,
		   krb5_keyblock *key)
{
    krb5_error_code ret;
    size_t len;

    memset(key, 0, sizeof(*key));

    ret = krb5_enctype_keysize(context, type, &len);
    if (ret)
	return ret;

    if (len != size) {
	krb5_set_error_string(context, "Encryption key %d is %lu bytes "
			      "long, %lu was passed in",
			      type, (unsigned long)len, (unsigned long)size);
	return KRB5_PROG_ETYPE_NOSUPP;
    }
    ret = krb5_data_copy(&key->keyvalue, data, len);
    if(ret) {
	krb5_set_error_string(context, "malloc failed: %lu",
			      (unsigned long)len);
	return ret;
    }
    key->keytype = type;

    return 0;
}
