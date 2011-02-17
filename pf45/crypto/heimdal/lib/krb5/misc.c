/*
 * Copyright (c) 1997 Kungliga Tekniska Högskolan
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

RCSID("$Id: misc.c 21174 2007-06-19 10:10:58Z lha $");

krb5_error_code KRB5_LIB_FUNCTION
_krb5_s4u2self_to_checksumdata(krb5_context context, 
			       const PA_S4U2Self *self, 
			       krb5_data *data)
{
    krb5_error_code ret;
    krb5_ssize_t ssize;
    krb5_storage *sp;
    size_t size;
    int i;

    sp = krb5_storage_emem();
    if (sp == NULL) {
	krb5_clear_error_string(context);
	return ENOMEM;
    }
    krb5_storage_set_flags(sp, KRB5_STORAGE_BYTEORDER_LE);
    ret = krb5_store_int32(sp, self->name.name_type);
    if (ret)
	goto out;
    for (i = 0; i < self->name.name_string.len; i++) {
	size = strlen(self->name.name_string.val[i]);
	ssize = krb5_storage_write(sp, self->name.name_string.val[i], size);
	if (ssize != size) {
	    ret = ENOMEM;
	    goto out;
	}
    }
    size = strlen(self->realm);
    ssize = krb5_storage_write(sp, self->realm, size);
    if (ssize != size) {
	ret = ENOMEM;
	goto out;
    }
    size = strlen(self->auth);
    ssize = krb5_storage_write(sp, self->auth, size);
    if (ssize != size) {
	ret = ENOMEM;
	goto out;
    }

    ret = krb5_storage_to_data(sp, data);
    krb5_storage_free(sp);
    return ret;

out:
    krb5_clear_error_string(context);
    return ret;
}
