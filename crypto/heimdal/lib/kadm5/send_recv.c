/*
 * Copyright (c) 1997, 1999 Kungliga Tekniska Högskolan
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

RCSID("$Id: send_recv.c,v 1.8 2000/07/11 16:00:58 joda Exp $");

kadm5_ret_t 
_kadm5_client_send(kadm5_client_context *context, krb5_storage *sp)
{
    krb5_data msg, out;
    krb5_error_code ret;
    size_t len;
    krb5_storage *sock;

    assert(context->sock != -1);

    len = sp->seek(sp, 0, SEEK_CUR);
    ret = krb5_data_alloc(&msg, len);
    sp->seek(sp, 0, SEEK_SET);
    sp->fetch(sp, msg.data, msg.length);
    
    ret = krb5_mk_priv(context->context, context->ac, &msg, &out, NULL);
    krb5_data_free(&msg);
    if(ret)
	return ret;
    
    sock = krb5_storage_from_fd(context->sock);
    if(sock == NULL) {
	krb5_data_free(&out);
	return ENOMEM;
    }
    
    ret = krb5_store_data(sock, out);
    krb5_storage_free(sock);
    krb5_data_free(&out);
    return ret;
}

kadm5_ret_t
_kadm5_client_recv(kadm5_client_context *context, krb5_data *reply)
{
    krb5_error_code ret;
    krb5_data data;
    krb5_storage *sock;

    sock = krb5_storage_from_fd(context->sock);
    if(sock == NULL)
	return ENOMEM;
    ret = krb5_ret_data(sock, &data);
    krb5_storage_free(sock);
    if(ret == KRB5_CC_END)
	return KADM5_RPC_ERROR;
    else if(ret)
	return ret;

    ret = krb5_rd_priv(context->context, context->ac, &data, reply, NULL);
    krb5_data_free(&data);
    return ret;
}

