/*
 * Copyright (c) 1997, 1998 Kungliga Tekniska Högskolan
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

#include "hprop.h"

RCSID("$Id: hprop-common.c,v 1.7 1999/12/02 17:04:59 joda Exp $");

krb5_error_code 
send_priv(krb5_context context, krb5_auth_context ac,
	  krb5_data *data, int fd)
{
    krb5_data packet;
    krb5_error_code ret;

    ret = krb5_mk_priv (context,
			ac,
			data,
			&packet,
			NULL);
    if (ret)
	return ret;
    
    ret = krb5_write_message (context, &fd, &packet);
    krb5_data_free(&packet);
    return ret;
}

krb5_error_code
recv_priv(krb5_context context, krb5_auth_context ac, int fd, krb5_data *out)
{
    krb5_error_code ret;
    krb5_data data;

    ret = krb5_read_message (context, &fd, &data);
    if (ret)
	return ret;

    ret = krb5_rd_priv(context, ac, &data, out, NULL);
    krb5_data_free (&data);
    return ret;
}

krb5_error_code
send_clear(krb5_context context, int fd, krb5_data data)
{
    return krb5_write_message (context, &fd, &data);
}

krb5_error_code
recv_clear(krb5_context context, int fd, krb5_data *out)
{
    return krb5_read_message (context, &fd, out);
}
