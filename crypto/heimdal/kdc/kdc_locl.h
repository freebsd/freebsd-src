/*
 * Copyright (c) 1997-2003 Kungliga Tekniska Högskolan
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

/* 
 * $Id: kdc_locl.h,v 1.58 2003/03/18 00:23:06 lha Exp $ 
 */

#ifndef __KDC_LOCL_H__
#define __KDC_LOCL_H__

#include "headers.h"

extern krb5_context context;

extern int require_preauth;
extern sig_atomic_t exit_flag;
extern size_t max_request;
extern time_t kdc_warn_pwexpire;
extern struct dbinfo {
    char *realm;
    char *dbname;
    char *mkey_file;
    struct dbinfo *next;
} *databases;
extern HDB **db;
extern int num_db;
extern const char *port_str;
extern krb5_addresses explicit_addresses;

extern int enable_http;
extern krb5_boolean encode_as_rep_as_tgs_rep;
extern krb5_boolean check_ticket_addresses;
extern krb5_boolean allow_null_ticket_addresses;
extern krb5_boolean allow_anonymous;
enum { TRPOLICY_ALWAYS_CHECK,
       TRPOLICY_ALLOW_PER_PRINCIPAL, 
       TRPOLICY_ALWAYS_HONOUR_REQUEST };
extern int trpolicy;
extern int enable_524;
extern int enable_v4_cross_realm;

#ifdef KRB4
extern char *v4_realm;
extern int enable_v4;
extern krb5_boolean enable_kaserver;
#endif

#define _PATH_KDC_CONF		HDB_DB_DIR "/kdc.conf"
#define DEFAULT_LOG_DEST	"0-1/FILE:" HDB_DB_DIR "/kdc.log"

extern struct timeval now;
#define kdc_time (now.tv_sec)

krb5_error_code as_rep (KDC_REQ*, krb5_data*, const char*, struct sockaddr*);
void configure (int, char**);
krb5_error_code db_fetch (krb5_principal, hdb_entry**);
void free_ent(hdb_entry *);
void kdc_log (int, const char*, ...)
    __attribute__ ((format (printf, 2,3)));

char* kdc_log_msg (int, const char*, ...)
    __attribute__ ((format (printf, 2,3)));
char* kdc_log_msg_va (int, const char*, va_list)
    __attribute__ ((format (printf, 2,0)));
void kdc_openlog (void);
void loop (void);
void set_master_key (EncryptionKey);
krb5_error_code tgs_rep (KDC_REQ*, krb5_data*, const char*, struct sockaddr *);
Key* unseal_key (Key*);
krb5_error_code check_flags(hdb_entry *client, const char *client_name,
			    hdb_entry *server, const char *server_name,
			    krb5_boolean is_as_req);

krb5_error_code get_des_key(hdb_entry*, krb5_boolean, krb5_boolean, Key**);
krb5_error_code encode_v4_ticket (void*, size_t, const EncTicketPart*, 
				  const PrincipalName*, size_t*);
krb5_error_code do_524 (const Ticket*, krb5_data*, const char*, struct sockaddr*);

#ifdef KRB4
krb5_error_code db_fetch4 (const char*, const char*, const char*, hdb_entry**);
krb5_error_code do_version4 (unsigned char*, size_t, krb5_data*, const char*, 
			     struct sockaddr_in*);
int maybe_version4 (unsigned char*, int);
#endif

#ifdef KRB4
krb5_error_code do_kaserver (unsigned char*, size_t, krb5_data*, const char*, 
			     struct sockaddr_in*);
#endif

#ifdef HAVE_OPENSSL
#define des_new_random_key des_random_key
#endif

#endif /* __KDC_LOCL_H__ */
