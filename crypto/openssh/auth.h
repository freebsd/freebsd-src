/*	$OpenBSD: auth.h,v 1.41 2002/09/26 11:38:43 markus Exp $	*/

/*
 * Copyright (c) 2000 Markus Friedl.  All rights reserved.
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
 *
 */

#ifndef AUTH_H
#define AUTH_H

#include "key.h"
#include "hostfile.h"
#include <openssl/rsa.h>

#ifdef HAVE_LOGIN_CAP
#include <login_cap.h>
#endif
#ifdef BSD_AUTH
#include <bsd_auth.h>
#endif
#ifdef KRB5
#include <krb5.h>
#endif

typedef struct Authctxt Authctxt;
typedef struct Authmethod Authmethod;
typedef struct KbdintDevice KbdintDevice;

struct Authctxt {
	int		 success;
	int		 postponed;
	int		 valid;
	int		 attempt;
	int		 failures;
	char		*user;
	char		*service;
	struct passwd	*pw;
	char		*style;
	void		*kbdintctxt;
#ifdef BSD_AUTH
	auth_session_t	*as;
#endif
#ifdef KRB4
	char		*krb4_ticket_file;
#endif
#ifdef KRB5
	krb5_context	 krb5_ctx;
	krb5_auth_context krb5_auth_ctx;
	krb5_ccache	 krb5_fwd_ccache;
	krb5_principal	 krb5_user;
	char		*krb5_ticket_file;
#endif
};

struct Authmethod {
	char	*name;
	int	(*userauth)(Authctxt *authctxt);
	int	*enabled;
};

/*
 * Keyboard interactive device:
 * init_ctx	returns: non NULL upon success
 * query	returns: 0 - success, otherwise failure
 * respond	returns: 0 - success, 1 - need further interaction,
 *		otherwise - failure
 */
struct KbdintDevice
{
	const char *name;
	void*	(*init_ctx)(Authctxt*);
	int	(*query)(void *ctx, char **name, char **infotxt,
		    u_int *numprompts, char ***prompts, u_int **echo_on);
	int	(*respond)(void *ctx, u_int numresp, char **responses);
	void	(*free_ctx)(void *ctx);
};

int      auth_rhosts(struct passwd *, const char *);
int
auth_rhosts2(struct passwd *, const char *, const char *, const char *);

int	 auth_rhosts_rsa(struct passwd *, char *, Key *);
int      auth_password(Authctxt *, const char *);
int      auth_rsa(struct passwd *, BIGNUM *);
int      auth_rsa_challenge_dialog(Key *);
BIGNUM	*auth_rsa_generate_challenge(Key *);
int	 auth_rsa_verify_response(Key *, BIGNUM *, u_char[]);
int	 auth_rsa_key_allowed(struct passwd *, BIGNUM *, Key **);

int	 auth_rhosts_rsa_key_allowed(struct passwd *, char *, char *, Key *);
int	 hostbased_key_allowed(struct passwd *, const char *, char *, Key *);
int	 user_key_allowed(struct passwd *, Key *);

#ifdef KRB4
#include <krb.h>
int     auth_krb4(Authctxt *, KTEXT, char **, KTEXT);
int	auth_krb4_password(Authctxt *, const char *);
void    krb4_cleanup_proc(void *);

#ifdef AFS
#include <kafs.h>
int     auth_krb4_tgt(Authctxt *, const char *);
int     auth_afs_token(Authctxt *, const char *);
#endif /* AFS */

#endif /* KRB4 */

#ifdef KRB5
int	auth_krb5(Authctxt *authctxt, krb5_data *auth, char **client, krb5_data *);
int	auth_krb5_tgt(Authctxt *authctxt, krb5_data *tgt);
int	auth_krb5_password(Authctxt *authctxt, const char *password);
void	krb5_cleanup_proc(void *authctxt);
#endif /* KRB5 */

#include "auth-pam.h"
#include "auth2-pam.h"

Authctxt *do_authentication(void);
Authctxt *do_authentication2(void);

Authctxt *authctxt_new(void);
void	auth_log(Authctxt *, int, char *, char *);
void	userauth_finish(Authctxt *, int, char *);
int	auth_root_allowed(char *);

char	*auth2_read_banner(void);

void	privsep_challenge_enable(void);

int	auth2_challenge(Authctxt *, char *);
void	auth2_challenge_stop(Authctxt *);
int	bsdauth_query(void *, char **, char **, u_int *, char ***, u_int **);
int	bsdauth_respond(void *, u_int, char **);
int	skey_query(void *, char **, char **, u_int *, char ***, u_int **);
int	skey_respond(void *, u_int, char **);

int	allowed_user(struct passwd *);
struct passwd * getpwnamallow(const char *user);

char	*get_challenge(Authctxt *);
int	verify_response(Authctxt *, const char *);

struct passwd * auth_get_user(void);

char	*expand_filename(const char *, struct passwd *);
char	*authorized_keys_file(struct passwd *);
char	*authorized_keys_file2(struct passwd *);

int
secure_filename(FILE *, const char *, struct passwd *, char *, size_t);

HostStatus
check_key_in_hostfiles(struct passwd *, Key *, const char *,
    const char *, const char *);

/* hostkey handling */
Key	*get_hostkey_by_index(int);
Key	*get_hostkey_by_type(int);
int	 get_hostkey_index(Key *);
int	 ssh1_session_key(BIGNUM *);

/* debug messages during authentication */
void	 auth_debug_add(const char *fmt,...) __attribute__((format(printf, 1, 2)));
void	 auth_debug_send(void);
void	 auth_debug_reset(void);

#define AUTH_FAIL_MAX 6
#define AUTH_FAIL_LOG (AUTH_FAIL_MAX/2)
#define AUTH_FAIL_MSG "Too many authentication failures for %.100s"

#define SKEY_PROMPT "\nS/Key Password: "
#endif
