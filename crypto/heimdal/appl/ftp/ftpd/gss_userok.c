/*
 * Copyright (c) 1998 Kungliga Tekniska Högskolan
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

#include "ftpd_locl.h"
#include <gssapi.h>
#include <krb5.h>

RCSID("$Id: gss_userok.c,v 1.2 1999/12/02 16:58:31 joda Exp $");

/* XXX a bit too much of krb5 dependency here... 
   What is the correct way to do this? 
   */

extern krb5_context gssapi_krb5_context;

/* XXX sync with gssapi.c */
struct gss_data {
    gss_ctx_id_t context_hdl;
    char *client_name;
};

int gss_userok(void*, char*); /* to keep gcc happy */

int
gss_userok(void *app_data, char *username)
{
    struct gss_data *data = app_data;
    if(gssapi_krb5_context) {
	krb5_principal client;
	krb5_error_code ret;
	ret = krb5_parse_name(gssapi_krb5_context, data->client_name, &client);
	if(ret)
	    return 1;
	ret = krb5_kuserok(gssapi_krb5_context, client, username);
	krb5_free_principal(gssapi_krb5_context, client);
	return !ret;
    }
    return 1;
}
