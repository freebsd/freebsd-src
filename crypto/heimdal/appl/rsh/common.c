/*
 * Copyright (c) 1997 - 1999 Kungliga Tekniska Högskolan
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

#include "rsh_locl.h"
RCSID("$Id: common.c,v 1.12 1999/12/02 17:04:56 joda Exp $");

ssize_t
do_read (int fd,
	 void *buf,
	 size_t sz)
{
    int ret;

    if (do_encrypt) {
#ifdef KRB4
	if (auth_method == AUTH_KRB4) {
	    return des_enc_read (fd, buf, sz, schedule, &iv);
	} else
#endif /* KRB4 */
        if(auth_method == AUTH_KRB5) {
	    u_int32_t len, outer_len;
	    int status;
	    krb5_data data;
	    void *edata;

	    ret = krb5_net_read (context, &fd, &len, 4);
	    if (ret <= 0)
		return ret;
	    len = ntohl(len);
	    if (len > sz)
		abort ();
	    outer_len = krb5_get_wrapped_length (context, crypto, len);
	    edata = malloc (outer_len);
	    if (edata == NULL)
		errx (1, "malloc: cannot allocate %u bytes", outer_len);
	    ret = krb5_net_read (context, &fd, edata, outer_len);
	    if (ret <= 0)
		return ret;

	    status = krb5_decrypt(context, crypto, KRB5_KU_OTHER_ENCRYPTED, 
				  edata, outer_len, &data);
	    free (edata);
	    
	    if (status)
		errx (1, "%s", krb5_get_err_text (context, status));
	    memcpy (buf, data.data, len);
	    krb5_data_free (&data);
	    return len;
	} else {
	    abort ();
	}
    } else
	return read (fd, buf, sz);
}

ssize_t
do_write (int fd, void *buf, size_t sz)
{
    if (do_encrypt) {
#ifdef KRB4
	if(auth_method == AUTH_KRB4) {
	    return des_enc_write (fd, buf, sz, schedule, &iv);
	} else
#endif /* KRB4 */
	if(auth_method == AUTH_KRB5) {
	    krb5_error_code status;
	    krb5_data data;
	    u_int32_t len;
	    int ret;

	    status = krb5_encrypt(context, crypto, KRB5_KU_OTHER_ENCRYPTED,
				  buf, sz, &data);
	    
	    if (status)
		errx (1, "%s", krb5_get_err_text(context, status));

	    assert (krb5_get_wrapped_length (context, crypto,
					     sz) == data.length);

	    len = htonl(sz);
	    ret = krb5_net_write (context, &fd, &len, 4);
	    if (ret != 4)
		return ret;
	    ret = krb5_net_write (context, &fd, data.data, data.length);
	    if (ret != data.length)
		return ret;
	    free (data.data);
	    return sz;
	} else {
	    abort();
	}
    } else
	return write (fd, buf, sz);
}
