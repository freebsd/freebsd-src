/*
 * Copyright (c) 1997 - 2002 Kungliga Tekniska Högskolan
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

RCSID("$Id: mk_error.c,v 1.17 2002/03/27 09:29:43 joda Exp $");

krb5_error_code
krb5_mk_error(krb5_context context,
	      krb5_error_code error_code,
	      const char *e_text,
	      const krb5_data *e_data,
	      const krb5_principal client,
	      const krb5_principal server,
	      time_t *client_time,
	      int *client_usec,
	      krb5_data *reply)
{
    KRB_ERROR msg;
    u_char *buf;
    size_t buf_size;
    int32_t sec, usec;
    size_t len;
    krb5_error_code ret = 0;

    krb5_us_timeofday (context, &sec, &usec);

    memset(&msg, 0, sizeof(msg));
    msg.pvno     = 5;
    msg.msg_type = krb_error;
    msg.stime    = sec;
    msg.susec    = usec;
    msg.ctime    = client_time;
    msg.cusec    = client_usec;
    /* Make sure we only send `protocol' error codes */
    if(error_code < KRB5KDC_ERR_NONE || error_code >= KRB5_ERR_RCSID) {
	if(e_text == NULL)
	    e_text = krb5_get_err_text(context, error_code);
	error_code = KRB5KRB_ERR_GENERIC;
    }
    msg.error_code = error_code - KRB5KDC_ERR_NONE;
    if (e_text)
	msg.e_text = (general_string*)&e_text;
    if (e_data)
	msg.e_data = (octet_string*)e_data;
    if(server){
	msg.realm = server->realm;
	msg.sname = server->name;
    }else{
	msg.realm = "<unspecified realm>";
    }
    if(client){
	msg.crealm = &client->realm;
	msg.cname = &client->name;
    }

    buf_size = 1024;
    buf = malloc (buf_size);
    if (buf == NULL) {
	krb5_set_error_string (context, "malloc: out of memory");
	return ENOMEM;
    }

    do {
	ret = encode_KRB_ERROR(buf + buf_size - 1,
			       buf_size,
			       &msg,
			       &len);
	if (ret) {
	    if (ret == ASN1_OVERFLOW) {
		u_char *tmp;

		buf_size *= 2;
		tmp = realloc (buf, buf_size);
		if (tmp == NULL) {
		    krb5_set_error_string (context, "malloc: out of memory");
		    ret = ENOMEM;
		    goto out;
		}
		buf = tmp;
	    } else {
		goto out;
	    }
	}
    } while (ret == ASN1_OVERFLOW);

    reply->length = len;
    reply->data = malloc(len);
    if (reply->data == NULL) {
	krb5_set_error_string (context, "malloc: out of memory");
	ret = ENOMEM;
	goto out;
    }
    memcpy (reply->data, buf + buf_size - len, len);
out:
    free (buf);
    return ret;
}
