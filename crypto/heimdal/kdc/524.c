/*
 * Copyright (c) 1997-1999 Kungliga Tekniska Högskolan
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

RCSID("$Id: 524.c,v 1.10 1999/12/02 17:04:58 joda Exp $");

#ifdef KRB4

krb5_error_code
do_524(Ticket *t, krb5_data *reply, const char *from, struct sockaddr *addr)
{
    krb5_error_code ret = 0;
    krb5_principal sprinc = NULL;
    krb5_crypto crypto;
    hdb_entry *server;
    Key *skey;
    krb5_data et_data;
    EncTicketPart et;
    EncryptedData ticket;
    krb5_storage *sp;
    char *spn = NULL;
    unsigned char buf[MAX_KTXT_LEN + 4 * 4];
    size_t len;
    
    principalname2krb5_principal(&sprinc, t->sname, t->realm);
    krb5_unparse_name(context, sprinc, &spn);
    server = db_fetch(sprinc);
    if(server == NULL){
	kdc_log(0, "Request to convert ticket from %s for unknown principal %s",
		from, spn);
	goto out;
    }
    ret = hdb_enctype2key(context, server, t->enc_part.etype, &skey);
    if(ret){
	kdc_log(0, "No suitable key found for server (%s) "
		"when converting ticket from ", spn, from);
	goto out;
    }
    krb5_crypto_init(context, &skey->key, 0, &crypto);
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
    {
	krb5_principal client;
	char *cpn;
	principalname2krb5_principal(&client, et.cname, et.crealm);
	krb5_unparse_name(context, client, &cpn);
	kdc_log(1, "524-REQ %s from %s for %s", cpn, from, spn);
	free(cpn);
	krb5_free_principal(context, client);
    }

    if(et.endtime < kdc_time){
	kdc_log(0, "Ticket expired (%s)", spn);
	free_EncTicketPart(&et);
	ret = KRB5KRB_AP_ERR_TKT_EXPIRED;
	goto out;
    }
    if(et.flags.invalid){
	kdc_log(0, "Ticket not valid (%s)", spn);
	free_EncTicketPart(&et);
	ret = KRB5KRB_AP_ERR_TKT_NYV;
	goto out;
    }
    {
	krb5_addresses *save_caddr, new_addr;
	krb5_address v4_addr;

	ret = krb5_sockaddr2address(addr, &v4_addr);
	if(ret) {
	    kdc_log(0, "Failed to convert address (%s)", spn);
	    free_EncTicketPart(&et);
	    goto out;
	}
	    
	if (et.caddr && !krb5_address_search (context, &v4_addr, et.caddr)) {
	    kdc_log(0, "Incorrect network address (%s)", spn);
	    free_EncTicketPart(&et);
	    krb5_free_address(context, &v4_addr);
	    ret = KRB5KRB_AP_ERR_BADADDR;
	    goto out;
	}
	if(v4_addr.addr_type == KRB5_ADDRESS_INET) {
	    /* we need to collapse the addresses in the ticket to a
	       single address; best guess is to use the address the
	       connection came from */
	    save_caddr = et.caddr;
	    new_addr.len = 1;
	    new_addr.val = &v4_addr;
	    et.caddr = &new_addr;
	}
	ret  = encode_v4_ticket(buf + sizeof(buf) - 1, sizeof(buf),
				&et, &t->sname, &len);
	if(v4_addr.addr_type == KRB5_ADDRESS_INET)
	    et.caddr = save_caddr;
    }
    free_EncTicketPart(&et);
    if(ret){
	kdc_log(0, "Failed to encode v4 ticket (%s)", spn);
	goto out;
    }
    ret = get_des_key(server, &skey);
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
    if(sprinc)
	krb5_free_principal(context, sprinc);
    hdb_free_entry(context, server);
    free(server);
    return ret;
}

#endif
