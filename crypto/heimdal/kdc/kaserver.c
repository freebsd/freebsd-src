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

#include "kdc_locl.h"

RCSID("$Id: kaserver.c,v 1.16 2001/02/05 10:49:43 assar Exp $");

#ifdef KASERVER

#include <rx.h>

#define KA_AUTHENTICATION_SERVICE 731
#define KA_TICKET_GRANTING_SERVICE 732
#define KA_MAINTENANCE_SERVICE 733

#define AUTHENTICATE_OLD	 1
#define CHANGEPASSWORD		 2
#define GETTICKET_OLD		 3
#define SETPASSWORD		 4
#define SETFIELDS		 5
#define CREATEUSER		 6
#define DELETEUSER		 7
#define GETENTRY		 8
#define LISTENTRY		 9
#define GETSTATS		10
#define DEBUG			11
#define GETPASSWORD		12
#define GETRANDOMKEY		13
#define AUTHENTICATE		21
#define AUTHENTICATE_V2		22
#define GETTICKET		23

/* XXX - Where do we get these? */

#define RXGEN_OPCODE (-455)

#define KADATABASEINCONSISTENT                   (180480L)
#define KAEXIST                                  (180481L)
#define KAIO                                     (180482L)
#define KACREATEFAIL                             (180483L)
#define KANOENT                                  (180484L)
#define KAEMPTY                                  (180485L)
#define KABADNAME                                (180486L)
#define KABADINDEX                               (180487L)
#define KANOAUTH                                 (180488L)
#define KAANSWERTOOLONG                          (180489L)
#define KABADREQUEST                             (180490L)
#define KAOLDINTERFACE                           (180491L)
#define KABADARGUMENT                            (180492L)
#define KABADCMD                                 (180493L)
#define KANOKEYS                                 (180494L)
#define KAREADPW                                 (180495L)
#define KABADKEY                                 (180496L)
#define KAUBIKINIT                               (180497L)
#define KAUBIKCALL                               (180498L)
#define KABADPROTOCOL                            (180499L)
#define KANOCELLS                                (180500L)
#define KANOCELL                                 (180501L)
#define KATOOMANYUBIKS                           (180502L)
#define KATOOMANYKEYS                            (180503L)
#define KABADTICKET                              (180504L)
#define KAUNKNOWNKEY                             (180505L)
#define KAKEYCACHEINVALID                        (180506L)
#define KABADSERVER                              (180507L)
#define KABADUSER                                (180508L)
#define KABADCPW                                 (180509L)
#define KABADCREATE                              (180510L)
#define KANOTICKET                               (180511L)
#define KAASSOCUSER                              (180512L)
#define KANOTSPECIAL                             (180513L)
#define KACLOCKSKEW                              (180514L)
#define KANORECURSE                              (180515L)
#define KARXFAIL                                 (180516L)
#define KANULLPASSWORD                           (180517L)
#define KAINTERNALERROR                          (180518L)
#define KAPWEXPIRED                              (180519L)
#define KAREUSED                                 (180520L)
#define KATOOSOON                                (180521L)
#define KALOCKED                                 (180522L)

static void
decode_rx_header (krb5_storage *sp,
		  struct rx_header *h)
{
    krb5_ret_int32(sp, &h->epoch);
    krb5_ret_int32(sp, &h->connid);
    krb5_ret_int32(sp, &h->callid);
    krb5_ret_int32(sp, &h->seqno);
    krb5_ret_int32(sp, &h->serialno);
    krb5_ret_int8(sp,  &h->type);
    krb5_ret_int8(sp,  &h->flags);
    krb5_ret_int8(sp,  &h->status);
    krb5_ret_int8(sp,  &h->secindex);
    krb5_ret_int16(sp, &h->reserved);
    krb5_ret_int16(sp, &h->serviceid);
}

static void
encode_rx_header (struct rx_header *h,
		  krb5_storage *sp)
{
    krb5_store_int32(sp, h->epoch);
    krb5_store_int32(sp, h->connid);
    krb5_store_int32(sp, h->callid);
    krb5_store_int32(sp, h->seqno);
    krb5_store_int32(sp, h->serialno);
    krb5_store_int8(sp,  h->type);
    krb5_store_int8(sp,  h->flags);
    krb5_store_int8(sp,  h->status);
    krb5_store_int8(sp,  h->secindex);
    krb5_store_int16(sp, h->reserved);
    krb5_store_int16(sp, h->serviceid);
}

static void
init_reply_header (struct rx_header *hdr,
		   struct rx_header *reply_hdr,
		   u_char type,
		   u_char flags)
{
    reply_hdr->epoch     = hdr->epoch;
    reply_hdr->connid    = hdr->connid;
    reply_hdr->callid    = hdr->callid;
    reply_hdr->seqno     = 1;
    reply_hdr->serialno  = 1;
    reply_hdr->type      = type;
    reply_hdr->flags     = flags;
    reply_hdr->status    = 0;
    reply_hdr->secindex  = 0;
    reply_hdr->reserved  = 0;
    reply_hdr->serviceid = hdr->serviceid;
}

static void
make_error_reply (struct rx_header *hdr,
		  u_int32_t ret,
		  krb5_data *reply)

{
    krb5_storage *sp;
    struct rx_header reply_hdr;

    init_reply_header (hdr, &reply_hdr, HT_ABORT, HF_LAST);
    sp = krb5_storage_emem();
    encode_rx_header (&reply_hdr, sp);
    krb5_store_int32(sp, ret);
    krb5_storage_to_data (sp, reply);
    krb5_storage_free (sp);
}

static krb5_error_code
krb5_ret_xdr_data(krb5_storage *sp,
		  krb5_data *data)
{
    int ret;
    int size;
    ret = krb5_ret_int32(sp, &size);
    if(ret)
	return ret;
    data->length = size;
    if (size) {
	u_char foo[4];
	size_t pad = (4 - size % 4) % 4;

	data->data = malloc(size);
	if (data->data == NULL)
	    return ENOMEM;
	ret = sp->fetch(sp, data->data, size);
	if(ret != size)
	    return (ret < 0)? errno : KRB5_CC_END;
	if (pad) {
	    ret = sp->fetch(sp, foo, pad);
	    if (ret != pad)
		return (ret < 0)? errno : KRB5_CC_END;
	}
    } else
	data->data = NULL;
    return 0;
}

static krb5_error_code
krb5_store_xdr_data(krb5_storage *sp,
		    krb5_data data)
{
    u_char zero[4] = {0, 0, 0, 0};
    int ret;
    size_t pad;

    ret = krb5_store_int32(sp, data.length);
    if(ret < 0)
	return ret;
    ret = sp->store(sp, data.data, data.length);
    if(ret != data.length){
	if(ret < 0)
	    return errno;
	return KRB5_CC_END;
    }
    pad = (4 - data.length % 4) % 4;
    if (pad) {
	ret = sp->store(sp, zero, pad);
	if (ret != pad) {
	    if (ret < 0)
		return errno;
	    return KRB5_CC_END;
	}
    }
    return 0;
}


static krb5_error_code
create_reply_ticket (struct rx_header *hdr,
		     Key *skey,
		     char *name, char *instance, char *realm,
		     struct sockaddr_in *addr,
		     int life,
		     int kvno,
		     int32_t max_seq_len,
		     char *sname, char *sinstance,
		     u_int32_t challenge,
		     char *label,
		     des_cblock *key,
		     krb5_data *reply)
{
    KTEXT_ST ticket;
    des_cblock session;
    krb5_storage *sp;
    krb5_data enc_data;
    des_key_schedule schedule;
    struct rx_header reply_hdr;
    des_cblock zero;
    size_t pad;
    unsigned fyrtiosjuelva;

    /* create the ticket */

    des_new_random_key(&session);

    krb_create_ticket (&ticket, 0, name, instance, realm,
		       addr->sin_addr.s_addr,
		       &session, life, kdc_time,
		       sname, sinstance, skey->key.keyvalue.data);

    /* create the encrypted part of the reply */
    sp = krb5_storage_emem ();
    krb5_generate_random_block(&fyrtiosjuelva, sizeof(fyrtiosjuelva));
    fyrtiosjuelva &= 0xffffffff;
    krb5_store_int32 (sp, fyrtiosjuelva);
    krb5_store_int32 (sp, challenge);
    sp->store  (sp, session, 8);
    memset (&session, 0, sizeof(session));
    krb5_store_int32 (sp, kdc_time);
    krb5_store_int32 (sp, kdc_time + krb_life_to_time (0, life));
    krb5_store_int32 (sp, kvno);
    krb5_store_int32 (sp, ticket.length);
    krb5_store_stringz (sp, name);
    krb5_store_stringz (sp, instance);
#if 1 /* XXX - Why shouldn't the realm go here? */
    krb5_store_stringz (sp, "");
#else
    krb5_store_stringz (sp, realm);
#endif
    krb5_store_stringz (sp, sname);
    krb5_store_stringz (sp, sinstance);
    sp->store (sp, ticket.dat, ticket.length);
    sp->store (sp, label, strlen(label));

    /* pad to DES block */
    memset (zero, 0, sizeof(zero));
    pad = (8 - sp->seek (sp, 0, SEEK_CUR) % 8) % 8;
    sp->store (sp, zero, pad);

    krb5_storage_to_data (sp, &enc_data);
    krb5_storage_free (sp);

    if (enc_data.length > max_seq_len) {
	krb5_data_free (&enc_data);
	make_error_reply (hdr, KAANSWERTOOLONG, reply);
	return 0;
    }

    /* encrypt it */
    des_set_key (key, schedule);
    des_pcbc_encrypt ((des_cblock *)enc_data.data,
		      (des_cblock *)enc_data.data,
		      enc_data.length,
		      schedule,
		      key,
		      DES_ENCRYPT);
    memset (&schedule, 0, sizeof(schedule));

    /* create the reply packet */
    init_reply_header (hdr, &reply_hdr, HT_DATA, HF_LAST);
    sp = krb5_storage_emem ();
    encode_rx_header (&reply_hdr, sp);
    krb5_store_int32 (sp, max_seq_len);
    krb5_store_xdr_data (sp, enc_data);
    krb5_data_free (&enc_data);
    krb5_storage_to_data (sp, reply);
    krb5_storage_free (sp);
    return 0;
}

static krb5_error_code
unparse_auth_args (krb5_storage *sp,
		   char **name,
		   char **instance,
		   time_t *start_time,
		   time_t *end_time,
		   krb5_data *request,
		   int32_t *max_seq_len)
{
    krb5_data data;
    int32_t tmp;

    krb5_ret_xdr_data (sp, &data);
    *name = malloc(data.length + 1);
    if (*name == NULL)
	return ENOMEM;
    memcpy (*name, data.data, data.length);
    (*name)[data.length] = '\0';
    krb5_data_free (&data);

    krb5_ret_xdr_data (sp, &data);
    *instance = malloc(data.length + 1);
    if (*instance == NULL) {
	free (*name);
	return ENOMEM;
    }
    memcpy (*instance, data.data, data.length);
    (*instance)[data.length] = '\0';
    krb5_data_free (&data);

    krb5_ret_int32 (sp, &tmp);
    *start_time = tmp;
    krb5_ret_int32 (sp, &tmp);
    *end_time = tmp;
    krb5_ret_xdr_data (sp, request);
    krb5_ret_int32 (sp, max_seq_len);
    /* ignore the rest */
    return 0;
}

static void
do_authenticate (struct rx_header *hdr,
		 krb5_storage *sp,
		 struct sockaddr_in *addr,
		 krb5_data *reply)
{
    krb5_error_code ret;
    char *name = NULL;
    char *instance = NULL;
    time_t start_time;
    time_t end_time;
    krb5_data request;
    int32_t max_seq_len;
    hdb_entry *client_entry = NULL;
    hdb_entry *server_entry = NULL;
    Key *ckey = NULL;
    Key *skey = NULL;
    des_cblock key;
    des_key_schedule schedule;
    krb5_storage *reply_sp;
    time_t max_life;
    u_int8_t life;
    int32_t chal;
    char client_name[256];
    char server_name[256];
	
    krb5_data_zero (&request);

    unparse_auth_args (sp, &name, &instance, &start_time, &end_time,
		       &request, &max_seq_len);

    snprintf (client_name, sizeof(client_name), "%s.%s@%s",
	      name, instance, v4_realm);

    ret = db_fetch4 (name, instance, v4_realm, &client_entry);
    if (ret) {
	kdc_log(0, "Client not found in database: %s: %s",
		client_name, krb5_get_err_text(context, ret));
	make_error_reply (hdr, KANOENT, reply);
	goto out;
    }

    snprintf (server_name, sizeof(server_name), "%s.%s@%s",
	      "krbtgt", v4_realm, v4_realm);

    ret = db_fetch4 ("krbtgt", v4_realm, v4_realm, &server_entry);
    if (ret) {
	kdc_log(0, "Server not found in database: %s: %s",
		server_name, krb5_get_err_text(context, ret));
	make_error_reply (hdr, KANOENT, reply);
	goto out;
    }

    ret = check_flags (client_entry, client_name,
		       server_entry, server_name,
		       TRUE);
    if (ret) {
	make_error_reply (hdr, KAPWEXPIRED, reply);
	goto out;
    }

    /* find a DES key */
    ret = get_des_key(client_entry, TRUE, &ckey);
    if(ret){
	kdc_log(0, "%s", krb5_get_err_text(context, ret));
	make_error_reply (hdr, KANOKEYS, reply);
	goto out;
    }

    /* find a DES key */
    ret = get_des_key(server_entry, TRUE, &skey);
    if(ret){
	kdc_log(0, "%s", krb5_get_err_text(context, ret));
	make_error_reply (hdr, KANOKEYS, reply);
	goto out;
    }

    /* try to decode the `request' */
    memcpy (&key, ckey->key.keyvalue.data, sizeof(key));
    des_set_key (&key, schedule);
    des_pcbc_encrypt ((des_cblock *)request.data,
		      (des_cblock *)request.data,
		      request.length,
		      schedule,
		      &key,
		      DES_DECRYPT);
    memset (&schedule, 0, sizeof(schedule));

    /* check for the magic label */
    if (memcmp ((char *)request.data + 4, "gTGS", 4) != 0) {
	make_error_reply (hdr, KABADREQUEST, reply);
	goto out;
    }

    reply_sp = krb5_storage_from_mem (request.data, 4);
    krb5_ret_int32 (reply_sp, &chal);
    krb5_storage_free (reply_sp);

    if (abs(chal - kdc_time) > context->max_skew) {
	make_error_reply (hdr, KACLOCKSKEW, reply);
	goto out;
    }

    /* life */
    max_life = end_time - kdc_time;
    if (client_entry->max_life)
	max_life = min(max_life, *client_entry->max_life);
    if (server_entry->max_life)
	max_life = min(max_life, *server_entry->max_life);

    life = krb_time_to_life(kdc_time, kdc_time + max_life);

    create_reply_ticket (hdr, skey,
			 name, instance, v4_realm,
			 addr, life, server_entry->kvno,
			 max_seq_len,
			 "krbtgt", v4_realm,
			 chal + 1, "tgsT",
			 &key, reply);
    memset (&key, 0, sizeof(key));

out:
    if (request.length) {
	memset (request.data, 0, request.length);
	krb5_data_free (&request);
    }
    if (name)
	free (name);
    if (instance)
	free (instance);
    if (client_entry)
	free_ent (client_entry);
    if (server_entry)
	free_ent (server_entry);
}

static krb5_error_code
unparse_getticket_args (krb5_storage *sp,
			int *kvno,
			char **auth_domain,
			krb5_data *ticket,
			char **name,
			char **instance,
			krb5_data *times,
			int32_t *max_seq_len)
{
    krb5_data data;
    int32_t tmp;

    krb5_ret_int32 (sp, &tmp);
    *kvno = tmp;

    krb5_ret_xdr_data (sp, &data);
    *auth_domain = malloc(data.length + 1);
    if (*auth_domain == NULL)
	return ENOMEM;
    memcpy (*auth_domain, data.data, data.length);
    (*auth_domain)[data.length] = '\0';
    krb5_data_free (&data);

    krb5_ret_xdr_data (sp, ticket);

    krb5_ret_xdr_data (sp, &data);
    *name = malloc(data.length + 1);
    if (*name == NULL) {
	free (*auth_domain);
	return ENOMEM;
    }
    memcpy (*name, data.data, data.length);
    (*name)[data.length] = '\0';
    krb5_data_free (&data);

    krb5_ret_xdr_data (sp, &data);
    *instance = malloc(data.length + 1);
    if (*instance == NULL) {
	free (*auth_domain);
	free (*name);
	return ENOMEM;
    }
    memcpy (*instance, data.data, data.length);
    (*instance)[data.length] = '\0';
    krb5_data_free (&data);

    krb5_ret_xdr_data (sp, times);

    krb5_ret_int32 (sp, max_seq_len);
    /* ignore the rest */
    return 0;
}

static void
do_getticket (struct rx_header *hdr,
	      krb5_storage *sp,
	      struct sockaddr_in *addr,
	      krb5_data *reply)
{
    krb5_error_code ret;
    int kvno;
    char *auth_domain = NULL;
    krb5_data aticket;
    char *name = NULL;
    char *instance = NULL;
    krb5_data times;
    int32_t max_seq_len;
    hdb_entry *server_entry = NULL;
    hdb_entry *krbtgt_entry = NULL;
    Key *kkey = NULL;
    Key *skey = NULL;
    des_cblock key;
    des_key_schedule schedule;
    des_cblock session;
    time_t max_life;
    int8_t life;
    time_t start_time, end_time;
    char pname[ANAME_SZ];
    char pinst[INST_SZ];
    char prealm[REALM_SZ];
    char server_name[256];

    krb5_data_zero (&aticket);
    krb5_data_zero (&times);

    unparse_getticket_args (sp, &kvno, &auth_domain, &aticket,
			    &name, &instance, &times, &max_seq_len);

    snprintf (server_name, sizeof(server_name),
	      "%s.%s@%s", name, instance, v4_realm);

    ret = db_fetch4 (name, instance, v4_realm, &server_entry);
    if (ret) {
	kdc_log(0, "Server not found in database: %s: %s",
		server_name, krb5_get_err_text(context, ret));
	make_error_reply (hdr, KANOENT, reply);
	goto out;
    }

    ret = check_flags (NULL, NULL,
		       server_entry, server_name,
		       FALSE);
    if (ret) {
	make_error_reply (hdr, KAPWEXPIRED, reply);
	goto out;
    }

    ret = db_fetch4 ("krbtgt", v4_realm, v4_realm, &krbtgt_entry);
    if (ret) {
	kdc_log(0, "Server not found in database: %s.%s@%s: %s",
		"krbtgt", v4_realm, v4_realm, krb5_get_err_text(context, ret));
	make_error_reply (hdr, KANOENT, reply);
	goto out;
    }

    /* find a DES key */
    ret = get_des_key(krbtgt_entry, TRUE, &kkey);
    if(ret){
	kdc_log(0, "%s", krb5_get_err_text(context, ret));
	make_error_reply (hdr, KANOKEYS, reply);
	goto out;
    }

    /* find a DES key */
    ret = get_des_key(server_entry, TRUE, &skey);
    if(ret){
	kdc_log(0, "%s", krb5_get_err_text(context, ret));
	make_error_reply (hdr, KANOKEYS, reply);
	goto out;
    }

    /* decrypt the incoming ticket */
    memcpy (&key, kkey->key.keyvalue.data, sizeof(key));

    /* unpack the ticket */
    {
	KTEXT_ST ticket;
	u_char flags;
	int life;
	u_int32_t time_sec;
	char sname[ANAME_SZ];
	char sinstance[SNAME_SZ];
	u_int32_t paddress;

	if (aticket.length > sizeof(ticket.dat)) {
	    kdc_log(0, "ticket too long (%u > %u)",
		    (unsigned)aticket.length,
		    (unsigned)sizeof(ticket.dat));
	    make_error_reply (hdr, KABADTICKET, reply);
	    goto out;
	}

	ticket.length = aticket.length;
	memcpy (ticket.dat, aticket.data, ticket.length);

	des_set_key (&key, schedule);
	decomp_ticket (&ticket, &flags, pname, pinst, prealm,
		       &paddress, session, &life, &time_sec,
		       sname, sinstance, 
		       &key, schedule);

	if (strcmp (sname, "krbtgt") != 0
	    || strcmp (sinstance, v4_realm) != 0) {
	    kdc_log(0, "no TGT: %s.%s for %s.%s@%s",
		    sname, sinstance,
		    pname, pinst, prealm);
	    make_error_reply (hdr, KABADTICKET, reply);
	    goto out;
	}

	if (kdc_time > krb_life_to_time(time_sec, life)) {
	    kdc_log(0, "TGT expired: %s.%s@%s",
		    pname, pinst, prealm);
	    make_error_reply (hdr, KABADTICKET, reply);
	    goto out;
	}
    }

    /* decrypt the times */
    des_set_key (&session, schedule);
    des_ecb_encrypt (times.data,
		     times.data,
		     schedule,
		     DES_DECRYPT);
    memset (&schedule, 0, sizeof(schedule));

    /* and extract them */
    {
	krb5_storage *sp;
	int32_t tmp;

	sp = krb5_storage_from_mem (times.data, times.length);
	krb5_ret_int32 (sp, &tmp);
	start_time = tmp;
	krb5_ret_int32 (sp, &tmp);
	end_time = tmp;
	krb5_storage_free (sp);
    }

    /* life */
    max_life = end_time - kdc_time;
    if (krbtgt_entry->max_life)
	max_life = min(max_life, *krbtgt_entry->max_life);
    if (server_entry->max_life)
	max_life = min(max_life, *server_entry->max_life);

    life = krb_time_to_life(kdc_time, kdc_time + max_life);

    create_reply_ticket (hdr, skey,
			 pname, pinst, prealm,
			 addr, life, server_entry->kvno,
			 max_seq_len,
			 name, instance,
			 0, "gtkt",
			 &session, reply);
    memset (&session, 0, sizeof(session));
    
out:
    if (aticket.length) {
	memset (aticket.data, 0, aticket.length);
	krb5_data_free (&aticket);
    }
    if (times.length) {
	memset (times.data, 0, times.length);
	krb5_data_free (&times);
    }
    if (auth_domain)
	free (auth_domain);
    if (name)
	free (name);
    if (instance)
	free (instance);
    if (krbtgt_entry)
	free_ent (krbtgt_entry);
    if (server_entry)
	free_ent (server_entry);
}

krb5_error_code
do_kaserver(unsigned char *buf,
	    size_t len,
	    krb5_data *reply,
	    const char *from,
	    struct sockaddr_in *addr)
{
    krb5_error_code ret = 0;
    struct rx_header hdr;
    u_int32_t op;
    krb5_storage *sp;

    if (len < RX_HEADER_SIZE)
	return -1;
    sp = krb5_storage_from_mem (buf, len);

    decode_rx_header (sp, &hdr);
    buf += RX_HEADER_SIZE;
    len -= RX_HEADER_SIZE;

    switch (hdr.type) {
    case HT_DATA :
	break;
    case HT_ACK :
    case HT_BUSY :
    case HT_ABORT :
    case HT_ACKALL :
    case HT_CHAL :
    case HT_RESP :
    case HT_DEBUG :
    default:
	/* drop */
	goto out;
    }


    if (hdr.serviceid != KA_AUTHENTICATION_SERVICE
	&& hdr.serviceid != KA_TICKET_GRANTING_SERVICE) {
	ret = -1;
	goto out;
    }

    krb5_ret_int32(sp, &op);
    switch (op) {
    case AUTHENTICATE :
	do_authenticate (&hdr, sp, addr, reply);
	break;
    case GETTICKET :
	do_getticket (&hdr, sp, addr, reply);
	break;
    case AUTHENTICATE_OLD :
    case CHANGEPASSWORD :
    case GETTICKET_OLD :
    case SETPASSWORD :
    case SETFIELDS :
    case CREATEUSER :
    case DELETEUSER :
    case GETENTRY :
    case LISTENTRY :
    case GETSTATS :
    case DEBUG :
    case GETPASSWORD :
    case GETRANDOMKEY :
    case AUTHENTICATE_V2 :
    default :
	make_error_reply (&hdr, RXGEN_OPCODE, reply);
	break;
    }

out:
    krb5_storage_free (sp);
    return ret;
}

#endif /* KASERVER */
