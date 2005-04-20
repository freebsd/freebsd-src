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

RCSID("$Id: keyblock.c,v 1.12 2001/05/14 06:14:48 assar Exp $");

void
krb5_free_keyblock_contents(krb5_context context,
			    krb5_keyblock *keyblock)
{
    if(keyblock) {
	if (keyblock->keyvalue.data != NULL)
	    memset(keyblock->keyvalue.data, 0, keyblock->keyvalue.length);
	krb5_data_free (&keyblock->keyvalue);
    }
}

void
krb5_free_keyblock(krb5_context context,
		   krb5_keyblock *keyblock)
{
    if(keyblock){
	krb5_free_keyblock_contents(context, keyblock);
	free(keyblock);
    }
}

krb5_error_code
krb5_copy_keyblock_contents (krb5_context context,
			     const krb5_keyblock *inblock,
			     krb5_keyblock *to)
{
    return copy_EncryptionKey(inblock, to);
}

krb5_error_code
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
