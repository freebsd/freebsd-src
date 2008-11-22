/* $OpenBSD: monitor_wrap.h,v 1.20 2006/08/03 03:34:42 deraadt Exp $ */
/* $FreeBSD$ */

/*
 * Copyright 2002 Niels Provos <provos@citi.umich.edu>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _MM_WRAP_H_
#define _MM_WRAP_H_

extern int use_privsep;
#define PRIVSEP(x)	(use_privsep ? mm_##x : x)

enum mm_keytype {MM_NOKEY, MM_HOSTKEY, MM_USERKEY, MM_RSAHOSTKEY, MM_RSAUSERKEY};

struct monitor;
struct mm_master;
struct Authctxt;

int mm_is_monitor(void);
DH *mm_choose_dh(int, int, int);
int mm_key_sign(Key *, u_char **, u_int *, u_char *, u_int);
void mm_inform_authserv(char *, char *);
struct passwd *mm_getpwnamallow(const char *);
char *mm_auth2_read_banner(void);
int mm_auth_password(struct Authctxt *, char *);
int mm_key_allowed(enum mm_keytype, char *, char *, Key *);
int mm_user_key_allowed(struct passwd *, Key *);
int mm_hostbased_key_allowed(struct passwd *, char *, char *, Key *);
int mm_auth_rhosts_rsa_key_allowed(struct passwd *, char *, char *, Key *);
int mm_key_verify(Key *, u_char *, u_int, u_char *, u_int);
int mm_auth_rsa_key_allowed(struct passwd *, BIGNUM *, Key **);
int mm_auth_rsa_verify_response(Key *, BIGNUM *, u_char *);
BIGNUM *mm_auth_rsa_generate_challenge(Key *);

#ifdef GSSAPI
OM_uint32 mm_ssh_gssapi_server_ctx(Gssctxt **, gss_OID);
OM_uint32 mm_ssh_gssapi_accept_ctx(Gssctxt *,
   gss_buffer_desc *, gss_buffer_desc *, OM_uint32 *);
int mm_ssh_gssapi_userok(char *user);
OM_uint32 mm_ssh_gssapi_checkmic(Gssctxt *, gss_buffer_t, gss_buffer_t);
#endif

#ifdef USE_PAM
void mm_start_pam(struct Authctxt *);
u_int mm_do_pam_account(void);
void *mm_sshpam_init_ctx(struct Authctxt *);
int mm_sshpam_query(void *, char **, char **, u_int *, char ***, u_int **);
int mm_sshpam_respond(void *, u_int, char **);
void mm_sshpam_free_ctx(void *);
#endif

#ifdef SSH_AUDIT_EVENTS
#include "audit.h"
void mm_audit_event(ssh_audit_event_t);
void mm_audit_run_command(const char *);
#endif

struct Session;
void mm_terminate(void);
int mm_pty_allocate(int *, int *, char *, size_t);
void mm_session_pty_cleanup2(struct Session *);

/* SSHv1 interfaces */
void mm_ssh1_session_id(u_char *);
int mm_ssh1_session_key(BIGNUM *);

/* Key export functions */
struct Newkeys *mm_newkeys_from_blob(u_char *, int);
int mm_newkeys_to_blob(int, u_char **, u_int *);

void monitor_apply_keystate(struct monitor *);
void mm_get_keystate(struct monitor *);
void mm_send_keystate(struct monitor*);

/* bsdauth */
int mm_bsdauth_query(void *, char **, char **, u_int *, char ***, u_int **);
int mm_bsdauth_respond(void *, u_int, char **);

/* skey */
int mm_skey_query(void *, char **, char **, u_int *, char ***, u_int **);
int mm_skey_respond(void *, u_int, char **);

/* zlib allocation hooks */

void *mm_zalloc(struct mm_master *, u_int, u_int);
void mm_zfree(struct mm_master *, void *);
void mm_init_compression(struct mm_master *);

#endif /* _MM_WRAP_H_ */
