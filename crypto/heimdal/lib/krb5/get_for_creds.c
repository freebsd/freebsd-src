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

#include <krb5_locl.h>

RCSID("$Id: get_for_creds.c,v 1.32 2002/03/10 23:12:23 assar Exp $");

static krb5_error_code
add_addrs(krb5_context context,
	  krb5_addresses *addr,
	  struct addrinfo *ai)
{
    krb5_error_code ret;
    unsigned n, i, j;
    void *tmp;
    struct addrinfo *a;

    n = 0;
    for (a = ai; a != NULL; a = a->ai_next)
	++n;

    i = addr->len;
    addr->len += n;
    tmp = realloc(addr->val, addr->len * sizeof(*addr->val));
    if (tmp == NULL) {
	krb5_set_error_string(context, "malloc: out of memory");
	ret = ENOMEM;
	goto fail;
    }
    addr->val = tmp;
    for (j = i; j < addr->len; ++j) {
	addr->val[i].addr_type = 0;
	krb5_data_zero(&addr->val[i].address);
    }
    for (a = ai; a != NULL; a = a->ai_next) {
	ret = krb5_sockaddr2address (context, a->ai_addr, &addr->val[i]);
	if (ret == 0)
	    ++i;
	else if (ret == KRB5_PROG_ATYPE_NOSUPP)
	    krb5_clear_error_string (context);
	else
	    goto fail;
    }
    addr->len = i;
    return 0;
fail:
    krb5_free_addresses (context, addr);
    return ret;
}

/*
 * Forward credentials for `client' to host `hostname`,
 * making them forwardable if `forwardable', and returning the
 * blob of data to sent in `out_data'.
 * If hostname == NULL, pick it from `server'
 */

krb5_error_code
krb5_fwd_tgt_creds (krb5_context	context,
		    krb5_auth_context	auth_context,
		    const char		*hostname,
		    krb5_principal	client,
		    krb5_principal	server,
		    krb5_ccache		ccache,
		    int			forwardable,
		    krb5_data		*out_data)
{
    krb5_flags flags = 0;
    krb5_creds creds;
    krb5_error_code ret;
    krb5_const_realm client_realm;

    flags |= KDC_OPT_FORWARDED;

    if (forwardable)
	flags |= KDC_OPT_FORWARDABLE;

    if (hostname == NULL &&
	krb5_principal_get_type(context, server) == KRB5_NT_SRV_HST) {
	const char *inst = krb5_principal_get_comp_string(context, server, 0);
	const char *host = krb5_principal_get_comp_string(context, server, 1);

	if (inst != NULL &&
	    strcmp(inst, "host") == 0 &&
	    host != NULL && 
	    krb5_principal_get_comp_string(context, server, 2) == NULL)
	    hostname = host;
    }

    client_realm = krb5_principal_get_realm(context, client);
    
    memset (&creds, 0, sizeof(creds));
    creds.client = client;

    ret = krb5_build_principal(context,
			       &creds.server,
			       strlen(client_realm),
			       client_realm,
			       KRB5_TGS_NAME,
			       client_realm,
			       NULL);
    if (ret)
	return ret;

    ret = krb5_get_forwarded_creds (context,
				    auth_context,
				    ccache,
				    flags,
				    hostname,
				    &creds,
				    out_data);
    return ret;
}

/*
 *
 */

krb5_error_code
krb5_get_forwarded_creds (krb5_context	    context,
			  krb5_auth_context auth_context,
			  krb5_ccache       ccache,
			  krb5_flags        flags,
			  const char        *hostname,
			  krb5_creds        *in_creds,
			  krb5_data         *out_data)
{
    krb5_error_code ret;
    krb5_creds *out_creds;
    krb5_addresses addrs;
    KRB_CRED cred;
    KrbCredInfo *krb_cred_info;
    EncKrbCredPart enc_krb_cred_part;
    size_t len;
    u_char buf[1024];
    int32_t sec, usec;
    krb5_kdc_flags kdc_flags;
    krb5_crypto crypto;
    struct addrinfo *ai;
    int save_errno;

    addrs.len = 0;
    addrs.val = NULL;

    ret = getaddrinfo (hostname, NULL, NULL, &ai);
    if (ret) {
	save_errno = errno;
	krb5_set_error_string(context, "resolving %s: %s",
			      hostname, gai_strerror(ret));
	return krb5_eai_to_heim_errno(ret, save_errno);
    }

    ret = add_addrs (context, &addrs, ai);
    freeaddrinfo (ai);
    if (ret)
	return ret;

    kdc_flags.i = flags;

    ret = krb5_get_kdc_cred (context,
			     ccache,
			     kdc_flags,
			     &addrs,
			     NULL,
			     in_creds,
			     &out_creds);
    krb5_free_addresses (context, &addrs);
    if (ret) {
	return ret;
    }

    memset (&cred, 0, sizeof(cred));
    cred.pvno = 5;
    cred.msg_type = krb_cred;
    ALLOC_SEQ(&cred.tickets, 1);
    if (cred.tickets.val == NULL) {
	ret = ENOMEM;
	krb5_set_error_string(context, "malloc: out of memory");
	goto out2;
    }
    ret = decode_Ticket(out_creds->ticket.data,
			out_creds->ticket.length,
			cred.tickets.val, &len);
    if (ret)
	goto out3;

    memset (&enc_krb_cred_part, 0, sizeof(enc_krb_cred_part));
    ALLOC_SEQ(&enc_krb_cred_part.ticket_info, 1);
    if (enc_krb_cred_part.ticket_info.val == NULL) {
	ret = ENOMEM;
	krb5_set_error_string(context, "malloc: out of memory");
	goto out4;
    }
    
    krb5_us_timeofday (context, &sec, &usec);

    ALLOC(enc_krb_cred_part.timestamp, 1);
    if (enc_krb_cred_part.timestamp == NULL) {
	ret = ENOMEM;
	krb5_set_error_string(context, "malloc: out of memory");
	goto out4;
    }
    *enc_krb_cred_part.timestamp = sec;
    ALLOC(enc_krb_cred_part.usec, 1);
    if (enc_krb_cred_part.usec == NULL) {
 	ret = ENOMEM;
	krb5_set_error_string(context, "malloc: out of memory");
	goto out4;
    }
    *enc_krb_cred_part.usec      = usec;

    if (auth_context->local_address && auth_context->local_port) {
	krb5_boolean noaddr;
	const krb5_realm *realm;

	realm = krb5_princ_realm(context, out_creds->server);
	krb5_appdefault_boolean(context, NULL, *realm, "no-addresses", FALSE,
				&noaddr);
	if (!noaddr) {
	    ret = krb5_make_addrport (context,
				      &enc_krb_cred_part.s_address,
				      auth_context->local_address,
				      auth_context->local_port);
	    if (ret)
		goto out4;
	}
    }

    if (auth_context->remote_address) {
	if (auth_context->remote_port) {
	    krb5_boolean noaddr;
	    const krb5_realm *realm;

	    realm = krb5_princ_realm(context, out_creds->server);
	    krb5_appdefault_boolean(context, NULL, *realm, "no-addresses",
				    FALSE, &noaddr);
	    if (!noaddr) {
		ret = krb5_make_addrport (context,
					  &enc_krb_cred_part.r_address,
					  auth_context->remote_address,
					  auth_context->remote_port);
		if (ret)
		    goto out4;
	    }
	} else {
	    ALLOC(enc_krb_cred_part.r_address, 1);
	    if (enc_krb_cred_part.r_address == NULL) {
		ret = ENOMEM;
		krb5_set_error_string(context, "malloc: out of memory");
		goto out4;
	    }

	    ret = krb5_copy_address (context, auth_context->remote_address,
				     enc_krb_cred_part.r_address);
	    if (ret)
		goto out4;
	}
    }

    /* fill ticket_info.val[0] */

    enc_krb_cred_part.ticket_info.len = 1;

    krb_cred_info = enc_krb_cred_part.ticket_info.val;

    copy_EncryptionKey (&out_creds->session, &krb_cred_info->key);
    ALLOC(krb_cred_info->prealm, 1);
    copy_Realm (&out_creds->client->realm, krb_cred_info->prealm);
    ALLOC(krb_cred_info->pname, 1);
    copy_PrincipalName(&out_creds->client->name, krb_cred_info->pname);
    ALLOC(krb_cred_info->flags, 1);
    *krb_cred_info->flags          = out_creds->flags.b;
    ALLOC(krb_cred_info->authtime, 1);
    *krb_cred_info->authtime       = out_creds->times.authtime;
    ALLOC(krb_cred_info->starttime, 1);
    *krb_cred_info->starttime      = out_creds->times.starttime;
    ALLOC(krb_cred_info->endtime, 1);
    *krb_cred_info->endtime        = out_creds->times.endtime;
    ALLOC(krb_cred_info->renew_till, 1);
    *krb_cred_info->renew_till = out_creds->times.renew_till;
    ALLOC(krb_cred_info->srealm, 1);
    copy_Realm (&out_creds->server->realm, krb_cred_info->srealm);
    ALLOC(krb_cred_info->sname, 1);
    copy_PrincipalName (&out_creds->server->name, krb_cred_info->sname);
    ALLOC(krb_cred_info->caddr, 1);
    copy_HostAddresses (&out_creds->addresses, krb_cred_info->caddr);

    krb5_free_creds (context, out_creds);

    /* encode EncKrbCredPart */

    ret = krb5_encode_EncKrbCredPart (context,
				      buf + sizeof(buf) - 1, sizeof(buf),
				      &enc_krb_cred_part, &len);
    free_EncKrbCredPart (&enc_krb_cred_part);
    if (ret) {
	free_KRB_CRED(&cred);
	return ret;
    }    

    ret = krb5_crypto_init(context, auth_context->local_subkey, 0, &crypto);
    if (ret) {
	free_KRB_CRED(&cred);
	return ret;
    }
    ret = krb5_encrypt_EncryptedData (context,
				      crypto,
				      KRB5_KU_KRB_CRED,
				      buf + sizeof(buf) - len,
				      len,
				      0,
				      &cred.enc_part);
    krb5_crypto_destroy(context, crypto);
    if (ret) {
	free_KRB_CRED(&cred);
	return ret;
    }

    ret = encode_KRB_CRED (buf + sizeof(buf) - 1, sizeof(buf),
			   &cred, &len);
    free_KRB_CRED (&cred);
    if (ret)
	return ret;
    out_data->length = len;
    out_data->data   = malloc(len);
    if (out_data->data == NULL) {
	krb5_set_error_string(context, "malloc: out of memory");
	return ENOMEM;
    }
    memcpy (out_data->data, buf + sizeof(buf) - len, len);
    return 0;
out4:
    free_EncKrbCredPart(&enc_krb_cred_part);
out3:
    free_KRB_CRED(&cred);
out2:
    krb5_free_creds (context, out_creds);
    return ret;
}
