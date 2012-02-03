/*
 * Copyright (c) 1997-2005 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden). 
 *
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

#include "kdc_locl.h"

RCSID("$Id: process.c 20959 2007-06-07 04:46:06Z lha $");

/*
 *
 */

void
krb5_kdc_update_time(struct timeval *tv)
{
    if (tv == NULL)
	gettimeofday(&_kdc_now, NULL);
    else
	_kdc_now = *tv;
}

/*
 * handle the request in `buf, len', from `addr' (or `from' as a string),
 * sending a reply in `reply'.
 */

int
krb5_kdc_process_request(krb5_context context, 
			 krb5_kdc_configuration *config,
			 unsigned char *buf, 
			 size_t len, 
			 krb5_data *reply,
			 krb5_boolean *prependlength,
			 const char *from,
			 struct sockaddr *addr,
			 int datagram_reply)
{
    KDC_REQ req;
    Ticket ticket;
    DigestREQ digestreq;
    Kx509Request kx509req;
    krb5_error_code ret;
    size_t i;

    if(decode_AS_REQ(buf, len, &req, &i) == 0){
	krb5_data req_buffer;

	req_buffer.data = buf;
	req_buffer.length = len;

	ret = _kdc_as_rep(context, config, &req, &req_buffer, 
			  reply, from, addr, datagram_reply);
	free_AS_REQ(&req);
	return ret;
    }else if(decode_TGS_REQ(buf, len, &req, &i) == 0){
	ret = _kdc_tgs_rep(context, config, &req, reply, from, addr, datagram_reply);
	free_TGS_REQ(&req);
	return ret;
    }else if(decode_Ticket(buf, len, &ticket, &i) == 0){
	ret = _kdc_do_524(context, config, &ticket, reply, from, addr);
	free_Ticket(&ticket);
	return ret;
    }else if(decode_DigestREQ(buf, len, &digestreq, &i) == 0){
	ret = _kdc_do_digest(context, config, &digestreq, reply, from, addr);
	free_DigestREQ(&digestreq);
	return ret;
    } else if (_kdc_try_kx509_request(buf, len, &kx509req, &i) == 0) {
	ret = _kdc_do_kx509(context, config, &kx509req, reply, from, addr);
	free_Kx509Request(&kx509req);
	return ret;
    } else if(_kdc_maybe_version4(buf, len)){
	*prependlength = FALSE; /* elbitapmoc sdrawkcab XXX */
	_kdc_do_version4(context, config, buf, len, reply, from, 
			 (struct sockaddr_in*)addr);
	return 0;
    } else if (config->enable_kaserver) {
	ret = _kdc_do_kaserver(context, config, buf, len, reply, from,
			       (struct sockaddr_in*)addr);
	return ret;
    }
			  
    return -1;
}

/*
 * handle the request in `buf, len', from `addr' (or `from' as a string),
 * sending a reply in `reply'.
 *
 * This only processes krb5 requests
 */

int
krb5_kdc_process_krb5_request(krb5_context context, 
			      krb5_kdc_configuration *config,
			      unsigned char *buf, 
			      size_t len, 
			      krb5_data *reply,
			      const char *from,
			      struct sockaddr *addr,
			      int datagram_reply)
{
    KDC_REQ req;
    krb5_error_code ret;
    size_t i;

    if(decode_AS_REQ(buf, len, &req, &i) == 0){
	krb5_data req_buffer;

	req_buffer.data = buf;
	req_buffer.length = len;

	ret = _kdc_as_rep(context, config, &req, &req_buffer,
			  reply, from, addr, datagram_reply);
	free_AS_REQ(&req);
	return ret;
    }else if(decode_TGS_REQ(buf, len, &req, &i) == 0){
	ret = _kdc_tgs_rep(context, config, &req, reply, from, addr, datagram_reply);
	free_TGS_REQ(&req);
	return ret;
    }
    return -1;
}

/*
 *
 */

int
krb5_kdc_save_request(krb5_context context, 
		      const char *fn,
		      const unsigned char *buf,
		      size_t len,
		      const krb5_data *reply,
		      const struct sockaddr *sa)
{
    krb5_storage *sp;
    krb5_address a;
    int fd, ret;
    uint32_t t;
    krb5_data d;

    memset(&a, 0, sizeof(a));

    d.data = rk_UNCONST(buf);
    d.length = len;
    t = _kdc_now.tv_sec;

    fd = open(fn, O_WRONLY|O_CREAT|O_APPEND, 0600);
    if (fd < 0) {
	krb5_set_error_string(context, "Failed to open: %s", fn);
	return errno;
    }
    
    sp = krb5_storage_from_fd(fd);
    close(fd);
    if (sp == NULL) {
	krb5_set_error_string(context, "Storage failed to open fd");
	return ENOMEM;
    }

    ret = krb5_sockaddr2address(context, sa, &a);
    if (ret)
	goto out;

    krb5_store_uint32(sp, 1);
    krb5_store_uint32(sp, t);
    krb5_store_address(sp, a);
    krb5_store_data(sp, d);
    {
	Der_class cl;
	Der_type ty;
	unsigned int tag;
	ret = der_get_tag (reply->data, reply->length,
			   &cl, &ty, &tag, NULL);
	if (ret) {
	    krb5_store_uint32(sp, 0xffffffff);
	    krb5_store_uint32(sp, 0xffffffff);
	} else {
	    krb5_store_uint32(sp, MAKE_TAG(cl, ty, 0));
	    krb5_store_uint32(sp, tag);
	}
    }

    krb5_free_address(context, &a);
out:
    krb5_storage_free(sp);

    return 0;
}
