/*
 * Copyright (c) 2007 Kungliga Tekniska Högskolan
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

#include "kdc_locl.h"

RCSID("$Id: windc.c 20559 2007-04-24 16:00:07Z lha $");

static krb5plugin_windc_ftable *windcft;
static void *windcctx;

/*
 * Pick the first WINDC module that we find.
 */

krb5_error_code
krb5_kdc_windc_init(krb5_context context)
{
    struct krb5_plugin *list = NULL, *e;
    krb5_error_code ret;

    ret = _krb5_plugin_find(context, PLUGIN_TYPE_DATA, "windc", &list);
    if(ret != 0 || list == NULL)
	return 0;

    for (e = list; e != NULL; e = _krb5_plugin_get_next(e)) {

	windcft = _krb5_plugin_get_symbol(e);
	if (windcft->minor_version < KRB5_WINDC_PLUGING_MINOR)
	    continue;
	
	(*windcft->init)(context, &windcctx);
	break;
    }
    if (e == NULL) {
	_krb5_plugin_free(list);
	krb5_set_error_string(context, "Did not find any WINDC plugin");
	windcft = NULL;
	return ENOENT;
    }

    return 0;
}


krb5_error_code 
_kdc_pac_generate(krb5_context context,
		  hdb_entry_ex *client, 
		  krb5_pac *pac)
{
    *pac = NULL;
    if (windcft == NULL)
	return 0;
    return (windcft->pac_generate)(windcctx, context, client, pac);
}

krb5_error_code 
_kdc_pac_verify(krb5_context context, 
		const krb5_principal client_principal,
		hdb_entry_ex *client,
		hdb_entry_ex *server,
		krb5_pac *pac)
{
    if (windcft == NULL) {
	krb5_set_error_string(context, "Can't verify PAC, no function");
	return EINVAL;
    }
    return (windcft->pac_verify)(windcctx, context, 
				 client_principal, client, server, pac);
}

krb5_error_code
_kdc_windc_client_access(krb5_context context,
			 struct hdb_entry_ex *client,
			 KDC_REQ *req)
{
    if (windcft == NULL)
	return 0;
    return (windcft->client_access)(windcctx, context, client, req);
}
