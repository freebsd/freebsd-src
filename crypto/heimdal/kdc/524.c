/*
 * Copyright (c) 1997-2001 Kungliga Tekniska Högskolan
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

RCSID("$Id: 524.c,v 1.20 2001/05/14 06:17:47 assar Exp $");

#ifdef KRB4

/*
 * fetch the server from `t', returning the name in malloced memory in
 * `spn' and the entry itself in `server'
 */

static krb5_error_code
fetch_server (const Ticket *t,
	      char **spn,
	      hdb_entry **server,
	      const char *from)
{
    krb5_error_code ret;
    krb5_principal sprinc;

    ret = principalname2krb5_principal(&sprinc, t->sname, t->realm);
    if (ret) {
	kdc_log(0, "principalname2krb5_principal: %s",
		krb5_get_err_text(context, ret));
	return ret;
    }
    ret = krb5_unparse_name(context, sprinc, spn);
    if (ret) {
	krb5_free_principal(context, sprinc);
	kdc_log(0, "krb5_unparse_name: %s", krb5_get_err_text(context, ret));
	return ret;
    }
    ret = db_fetch(sprinc, server);
    krb5_free_principal(context, sprinc);
    if (ret) {
	kdc_log(0,
	"Request to convert ticket from %s for unknown principal %s: %s",
		from, *spn, krb5_get_err_text(context, ret));
	if (ret == ENOENT)
	    ret = KRB5KDC_ERR_S_PRINCIPAL_UNKNOWN;
	return ret;
    }
    return 0;
}

static krb5_error_code
log_524 (const EncTicketPart *et,
	 const char *from,
	 const char *spn)
{
    krb5_principal client;
    char *cpn;
    krb5_error_code ret;

    ret = principalname2krb5_principal(&client, et->cname, et->crealm);
    if (ret) {
	kdc_log(0, "principalname2krb5_principal: %s",
		krb5_get_err_text (context, ret));
	return ret;
    }
    ret = krb5_unparse_name(context, client, &cpn);
    if (ret) {
	krb5_free_principal(context, client);
	kdc_log(0, "krb5_unparse_name: %s",
		krb5_get_err_text (context, ret));
	return ret;
    }
    kdc_log(1, "524-REQ %s from %s for %s", cpn, from, spn);
    free(cpn);
    krb5_free_principal(context, client);
    return 0;
}

static krb5_error_code
verify_flags (const EncTicketPart *et,
	      const char *spn)
{
    if(et->endtime < kdc_time){
	kdc_log(0, "Ticket expired (%s)", spn);
	return KRB5KRB_AP_ERR_TKT_EXPIRED;
    }
    if(et->flags.invalid){
	kdc_log(0, "Ticket not valid (%s)", spn);
	return KRB5KRB_AP_ERR_TKT_NYV;
    }
    return 0;
}

/*
 * set the `et->caddr' to the most appropriate address to use, where
 * `addr' is the address the request was received from.
 */

static krb5_error_code
set_address (EncTicketPart *et,
	     struct sockaddr *addr,
	     const char *from)
{
    krb5_error_code ret;
    krb5_address *v4_addr;

    v4_addr = malloc (sizeof(*v4_addr));
    if (v4_addr == NULL)
	return ENOMEM;

    ret = krb5_sockaddr2address(context, addr, v4_addr);
    if(ret) {
	free (v4_addr);
	kdc_log(0, "Failed to convert address (%s)", from);
	return ret;
    }
	    
    if (et->caddr && !krb5_address_search (context, v4_addr, et->caddr)) {
	kdc_log(0, "Incorrect network address (%s)", from);
	krb5_free_address(context, v4_addr);
	free (v4_addr);
	return KRB5KRB_AP_ERR_BADADDR;
    }
    if(v4_addr->addr_type == KRB5_ADDRESS_INET) {
	/* we need to collapse the addresses in the ticket to a
	   single address; best guess is to use the address the
	   connection came from */
	
	if (et->caddr != NULL) {
	    free_HostAddresses(et->caddr);
	} else {
	    et->caddr = malloc (sizeof (*et->caddr));
	    if (et->caddr == NULL) {
		krb5_free_address(context, v4_addr);
		free(v4_addr);
		return ENOMEM;
	    }
	}
	et->caddr->val = v4_addr;
	et->caddr->len = 1;
    } else {
	krb5_free_address(context, v4_addr);
	free(v4_addr);
    }
    return 0;
}

/*
 * process a 5->4 request, based on `t', and received `from, addr',
 * returning the reply in `reply'
 */

krb5_error_code
do_524(const Ticket *t, krb5_data *reply,
       const char *from, struct sockaddr *addr)
{
    krb5_error_code ret = 0;
    krb5_crypto crypto;
    hdb_entry *server = NULL;
    Key *skey;
    krb5_data et_data;
    EncTicketPart et;
    EncryptedData ticket;
    krb5_storage *sp;
    char *spn = NULL;
    unsigned char buf[MAX_KTXT_LEN + 4 * 4];
    size_t len;
    
    if(!enable_524) {
	ret = KRB5KDC_ERR_POLICY;
	kdc_log(0, "Rejected ticket conversion request from %s", from);
	goto out;
    }

    ret = fetch_server (t, &spn, &server, from);
    if (ret) {
	goto out;
    }

    ret = hdb_enctype2key(context, server, t->enc_part.etype, &skey);
    if(ret){
	kdc_log(0, "No suitable key found for server (%s) from %s", spn, from);
	goto out;
    }
    ret = krb5_crypto_init(context, &skey->key, 0, &crypto);
    if (ret) {
	kdc_log(0, "krb5_crypto_init failed: %s",
		krb5_get_err_text(context, ret));
	goto out;
    }
    ret = krb5_decrypt_EncryptedData (context,
				      crypto,
				      KRB5_KU_TICKET,
				      &t->enc_part,
				      &et_data);
    krb5_crypto_destroy(context, crypto);
    if(ret){
	kdc_log(0, "Failed to decrypt ticket from %s for %s", from, spn);
	goto out;
    }
    ret = krb5_decode_EncTicketPart(context, et_data.data, et_data.length, 
				    &et, &len);
    krb5_data_free(&et_data);
    if(ret){
	kdc_log(0, "Failed to decode ticket from %s for %s", from, spn);
	goto out;
    }

    ret = log_524 (&et, from, spn);
    if (ret) {
	free_EncTicketPart(&et);
	goto out;
    }

    ret = verify_flags (&et, spn);
    if (ret) {
	free_EncTicketPart(&et);
	goto out;
    }

    ret = set_address (&et, addr, from);
    if (ret) {
	free_EncTicketPart(&et);
	goto out;
    }
    ret = encode_v4_ticket(buf + sizeof(buf) - 1, sizeof(buf),
			   &et, &t->sname, &len);
    free_EncTicketPart(&et);
    if(ret){
	kdc_log(0, "Failed to encode v4 ticket (%s)", spn);
	goto out;
    }
    ret = get_des_key(server, FALSE, &skey);
    if(ret){
	kdc_log(0, "No DES key for server (%s)", spn);
	goto out;
    }
    ret = encrypt_v4_ticket(buf + sizeof(buf) - len, len, 
			    skey->key.keyvalue.data, &ticket);
    if(ret){
	kdc_log(0, "Failed to encrypt v4 ticket (%s)", spn);
	goto out;
    }
out:
    /* make reply */
    memset(buf, 0, sizeof(buf));
    sp = krb5_storage_from_mem(buf, sizeof(buf));
    krb5_store_int32(sp, ret);
    if(ret == 0){
	krb5_store_int32(sp, server->kvno); /* is this right? */
	krb5_store_data(sp, ticket.cipher);
	/* Aargh! This is coded as a KTEXT_ST. */
	sp->seek(sp, MAX_KTXT_LEN - ticket.cipher.length, SEEK_CUR);
	krb5_store_int32(sp, 0); /* mbz */
	free_EncryptedData(&ticket);
    }
    ret = krb5_storage_to_data(sp, reply);
    krb5_storage_free(sp);
    
    if(spn)
	free(spn);
    if(server)
	free_ent (server);
    return ret;
}

#endif /* KRB4 */
