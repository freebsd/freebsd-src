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

#include <krb5_locl.h>

RCSID("$Id: rd_req.c,v 1.45 2001/05/14 06:14:50 assar Exp $");

static krb5_error_code
decrypt_tkt_enc_part (krb5_context context,
		      krb5_keyblock *key,
		      EncryptedData *enc_part,
		      EncTicketPart *decr_part)
{
    krb5_error_code ret;
    krb5_data plain;
    size_t len;
    krb5_crypto crypto;

    ret = krb5_crypto_init(context, key, 0, &crypto);
    if (ret)
	return ret;
    ret = krb5_decrypt_EncryptedData (context,
				      crypto,
				      KRB5_KU_TICKET,
				      enc_part,
				      &plain);
    krb5_crypto_destroy(context, crypto);
    if (ret)
	return ret;

    ret = krb5_decode_EncTicketPart(context, plain.data, plain.length, 
				    decr_part, &len);
    krb5_data_free (&plain);
    return ret;
}

static krb5_error_code
decrypt_authenticator (krb5_context context,
		       EncryptionKey *key,
		       EncryptedData *enc_part,
		       Authenticator *authenticator,
		       krb5_key_usage usage)
{
    krb5_error_code ret;
    krb5_data plain;
    size_t len;
    krb5_crypto crypto;

    ret = krb5_crypto_init(context, key, 0, &crypto);
    if (ret)
	return ret;
    ret = krb5_decrypt_EncryptedData (context,
				      crypto,
				      usage /* KRB5_KU_AP_REQ_AUTH */,
				      enc_part,
				      &plain);
    /* for backwards compatibility, also try the old usage */
    if (ret && usage == KRB5_KU_TGS_REQ_AUTH)
	ret = krb5_decrypt_EncryptedData (context,
					  crypto,
					  KRB5_KU_AP_REQ_AUTH,
					  enc_part,
					  &plain);
    krb5_crypto_destroy(context, crypto);
    if (ret)
	return ret;

    ret = krb5_decode_Authenticator(context, plain.data, plain.length, 
				    authenticator, &len);
    krb5_data_free (&plain);
    return ret;
}

krb5_error_code
krb5_decode_ap_req(krb5_context context,
		   const krb5_data *inbuf,
		   krb5_ap_req *ap_req)
{
    krb5_error_code ret;
    size_t len;
    ret = decode_AP_REQ(inbuf->data, inbuf->length, ap_req, &len);
    if (ret)
	return ret;
    if (ap_req->pvno != 5){
	free_AP_REQ(ap_req);
	krb5_clear_error_string (context);
	return KRB5KRB_AP_ERR_BADVERSION;
    }
    if (ap_req->msg_type != krb_ap_req){
	free_AP_REQ(ap_req);
	krb5_clear_error_string (context);
	return KRB5KRB_AP_ERR_MSG_TYPE;
    }
    if (ap_req->ticket.tkt_vno != 5){
	free_AP_REQ(ap_req);
	krb5_clear_error_string (context);
	return KRB5KRB_AP_ERR_BADVERSION;
    }
    return 0;
}

krb5_error_code
krb5_decrypt_ticket(krb5_context context,
		    Ticket *ticket,
		    krb5_keyblock *key,
		    EncTicketPart *out,
		    krb5_flags flags)
{
    EncTicketPart t;
    krb5_error_code ret;
    ret = decrypt_tkt_enc_part (context, key, &ticket->enc_part, &t);
    if (ret)
	return ret;
    
    {
	krb5_timestamp now;
	time_t start = t.authtime;

	krb5_timeofday (context, &now);
	if(t.starttime)
	    start = *t.starttime;
	if(start - now > context->max_skew
	   || (t.flags.invalid
	       && !(flags & KRB5_VERIFY_AP_REQ_IGNORE_INVALID))) {
	    free_EncTicketPart(&t);
	    krb5_clear_error_string (context);
	    return KRB5KRB_AP_ERR_TKT_NYV;
	}
	if(now - t.endtime > context->max_skew) {
	    free_EncTicketPart(&t);
	    krb5_clear_error_string (context);
	    return KRB5KRB_AP_ERR_TKT_EXPIRED;
	}
    }
    
    if(out)
	*out = t;
    else
	free_EncTicketPart(&t);
    return 0;
}

krb5_error_code
krb5_verify_authenticator_checksum(krb5_context context,
				   krb5_auth_context ac,
				   void *data,
				   size_t len)
{
    krb5_error_code ret;
    krb5_keyblock *key;
    krb5_authenticator authenticator;
    krb5_crypto crypto;
    
    ret = krb5_auth_getauthenticator (context,
				      ac,
				      &authenticator);
    if(ret)
	return ret;
    if(authenticator->cksum == NULL)
	return -17;
    ret = krb5_auth_con_getkey(context, ac, &key);
    if(ret) {
	krb5_free_authenticator(context, &authenticator);
	return ret;
    }
    ret = krb5_crypto_init(context, key, 0, &crypto);
    if(ret)
	goto out;
    ret = krb5_verify_checksum (context,
				crypto,
				KRB5_KU_AP_REQ_AUTH_CKSUM,
				data,
				len,
				authenticator->cksum);
    krb5_crypto_destroy(context, crypto);
out:
    krb5_free_authenticator(context, &authenticator);
    krb5_free_keyblock(context, key);
    return ret;
}

#if 0
static krb5_error_code
check_transited(krb5_context context,
		krb5_ticket *ticket)
{
    char **realms;
    int num_realms;
    krb5_error_code ret;

    if(ticket->ticket.transited.tr_type != DOMAIN_X500_COMPRESS)
	return KRB5KDC_ERR_TRTYPE_NOSUPP;

    ret = krb5_domain_x500_decode(ticket->ticket.transited.contents, 
				  &realms, &num_realms, 
				  ticket->client->realm,
				  ticket->server->realm);
    if(ret)
	return ret;
    ret = krb5_check_transited_realms(context, realms, num_realms, NULL);
    free(realms);
    return ret;
}
#endif

krb5_error_code
krb5_verify_ap_req(krb5_context context,
		   krb5_auth_context *auth_context,
		   krb5_ap_req *ap_req,
		   krb5_const_principal server,
		   krb5_keyblock *keyblock,
		   krb5_flags flags,
		   krb5_flags *ap_req_options,
		   krb5_ticket **ticket)
{
    return krb5_verify_ap_req2 (context,
				auth_context,
				ap_req,
				server,
				keyblock,
				flags,
				ap_req_options,
				ticket,
				KRB5_KU_AP_REQ_AUTH);
}

krb5_error_code
krb5_verify_ap_req2(krb5_context context,
		    krb5_auth_context *auth_context,
		    krb5_ap_req *ap_req,
		    krb5_const_principal server,
		    krb5_keyblock *keyblock,
		    krb5_flags flags,
		    krb5_flags *ap_req_options,
		    krb5_ticket **ticket,
		    krb5_key_usage usage)
{
    krb5_ticket t;
    krb5_auth_context ac;
    krb5_error_code ret;
    
    if (auth_context && *auth_context) {
	ac = *auth_context;
    } else {
	ret = krb5_auth_con_init (context, &ac);
	if (ret)
	    return ret;
    }

    if (ap_req->ap_options.use_session_key && ac->keyblock){
	ret = krb5_decrypt_ticket(context, &ap_req->ticket, 
				  ac->keyblock, 
				  &t.ticket,
				  flags);
	krb5_free_keyblock(context, ac->keyblock);
	ac->keyblock = NULL;
    }else
	ret = krb5_decrypt_ticket(context, &ap_req->ticket, 
				  keyblock, 
				  &t.ticket,
				  flags);
    
    if(ret)
	goto out;

    principalname2krb5_principal(&t.server, ap_req->ticket.sname, 
				 ap_req->ticket.realm);
    principalname2krb5_principal(&t.client, t.ticket.cname, 
				 t.ticket.crealm);

    /* save key */

    krb5_copy_keyblock(context, &t.ticket.key, &ac->keyblock);

    ret = decrypt_authenticator (context,
				 &t.ticket.key,
				 &ap_req->authenticator,
				 ac->authenticator,
				 usage);
    if (ret)
	goto out2;

    {
	krb5_principal p1, p2;
	krb5_boolean res;
	
	principalname2krb5_principal(&p1,
				     ac->authenticator->cname,
				     ac->authenticator->crealm);
	principalname2krb5_principal(&p2, 
				     t.ticket.cname,
				     t.ticket.crealm);
	res = krb5_principal_compare (context, p1, p2);
	krb5_free_principal (context, p1);
	krb5_free_principal (context, p2);
	if (!res) {
	    ret = KRB5KRB_AP_ERR_BADMATCH;
	    krb5_clear_error_string (context);
	    goto out2;
	}
    }

    /* check addresses */

    if (t.ticket.caddr
	&& ac->remote_address
	&& !krb5_address_search (context,
				 ac->remote_address,
				 t.ticket.caddr)) {
	ret = KRB5KRB_AP_ERR_BADADDR;
	krb5_clear_error_string (context);
	goto out2;
    }

    if (ac->authenticator->seq_number)
	ac->remote_seqnumber = *ac->authenticator->seq_number;

    /* XXX - Xor sequence numbers */

    /* XXX - subkeys? */
    /* And where should it be stored? */

    if (ac->authenticator->subkey) {
	krb5_copy_keyblock(context, 
			   ac->authenticator->subkey,
			   &ac->remote_subkey);
    }

    if (ap_req_options) {
	*ap_req_options = 0;
	if (ap_req->ap_options.use_session_key)
	    *ap_req_options |= AP_OPTS_USE_SESSION_KEY;
	if (ap_req->ap_options.mutual_required)
	    *ap_req_options |= AP_OPTS_MUTUAL_REQUIRED;
    }

    if(ticket){
	*ticket = malloc(sizeof(**ticket));
	**ticket = t;
    } else
	krb5_free_ticket (context, &t);
    if (auth_context) {
	if (*auth_context == NULL)
	    *auth_context = ac;
    } else
	krb5_auth_con_free (context, ac);
    return 0;
 out2:
    krb5_free_ticket (context, &t);
 out:
    if (auth_context == NULL || *auth_context == NULL)
	krb5_auth_con_free (context, ac);
    return ret;
}
		   

krb5_error_code
krb5_rd_req_with_keyblock(krb5_context context,
			  krb5_auth_context *auth_context,
			  const krb5_data *inbuf,
			  krb5_const_principal server,
			  krb5_keyblock *keyblock,
			  krb5_flags *ap_req_options,
			  krb5_ticket **ticket)
{
    krb5_error_code ret;
    krb5_ap_req ap_req;

    if (*auth_context == NULL) {
	ret = krb5_auth_con_init(context, auth_context);
	if (ret)
	    return ret;
    }

    ret = krb5_decode_ap_req(context, inbuf, &ap_req);
    if(ret)
	return ret;

    ret = krb5_verify_ap_req(context,
			     auth_context,
			     &ap_req,
			     server,
			     keyblock,
			     0,
			     ap_req_options,
			     ticket);

    free_AP_REQ(&ap_req);
    return ret;
}

static krb5_error_code
get_key_from_keytab(krb5_context context,
		    krb5_auth_context *auth_context,
		    krb5_ap_req *ap_req,
		    krb5_const_principal server,
		    krb5_keytab keytab,
		    krb5_keyblock **out_key)
{
    krb5_keytab_entry entry;
    krb5_error_code ret;
    int kvno;
    krb5_keytab real_keytab;

    if(keytab == NULL)
	krb5_kt_default(context, &real_keytab);
    else
	real_keytab = keytab;
    
    if (ap_req->ticket.enc_part.kvno)
	kvno = *ap_req->ticket.enc_part.kvno;
    else
	kvno = 0;

    ret = krb5_kt_get_entry (context,
			     real_keytab,
			     server,
			     kvno,
			     ap_req->ticket.enc_part.etype,
			     &entry);
    if(ret)
	goto out;
    ret = krb5_copy_keyblock(context, &entry.keyblock, out_key);
    krb5_kt_free_entry (context, &entry);
out:    
    if(keytab == NULL)
	krb5_kt_close(context, real_keytab);
    
    return ret;
}

krb5_error_code
krb5_rd_req(krb5_context context,
	    krb5_auth_context *auth_context,
	    const krb5_data *inbuf,
	    krb5_const_principal server,
	    krb5_keytab keytab,
	    krb5_flags *ap_req_options,
	    krb5_ticket **ticket)
{
    krb5_error_code ret;
    krb5_ap_req ap_req;
    krb5_keyblock *keyblock = NULL;
    krb5_principal service = NULL;

    if (*auth_context == NULL) {
	ret = krb5_auth_con_init(context, auth_context);
	if (ret)
	    return ret;
    }

    ret = krb5_decode_ap_req(context, inbuf, &ap_req);
    if(ret)
	return ret;

    if(server == NULL){
	principalname2krb5_principal(&service,
				     ap_req.ticket.sname,
				     ap_req.ticket.realm);
	server = service;
    }

    if(ap_req.ap_options.use_session_key == 0 || 
       (*auth_context)->keyblock == NULL){
	ret = get_key_from_keytab(context, 
				  auth_context, 
				  &ap_req,
				  server,
				  keytab,
				  &keyblock);
	if(ret)
	    goto out;
    }
	

    ret = krb5_verify_ap_req(context,
			     auth_context,
			     &ap_req,
			     server,
			     keyblock,
			     0,
			     ap_req_options,
			     ticket);

    if(keyblock != NULL)
	krb5_free_keyblock(context, keyblock);

out:
    free_AP_REQ(&ap_req);
    if(service)
	krb5_free_principal(context, service);
    return ret;
}
