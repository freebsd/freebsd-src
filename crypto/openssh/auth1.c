/*
 * Copyright (c) 1995 Tatu Ylonen <ylo@cs.hut.fi>, Espoo, Finland
 *                    All rights reserved
 *
 * As far as I am concerned, the code I have written for this software
 * can be used freely for any purpose.  Any derived versions of this
 * software must be clearly marked as such, and if the derived work is
 * incompatible with the protocol description in the RFC file, it must be
 * called by a name other than "ssh" or "Secure Shell".
 */

#include "includes.h"
RCSID("$OpenBSD: auth1.c,v 1.22 2001/03/23 12:02:49 markus Exp $");
RCSID("$FreeBSD$");

#include "xmalloc.h"
#include "rsa.h"
#include "ssh1.h"
#include "packet.h"
#include "buffer.h"
#include "mpaux.h"
#include "log.h"
#include "servconf.h"
#include "compat.h"
#include "auth.h"
#include "auth-pam.h"
#include "session.h"
#include "canohost.h"
#include "misc.h"
#include <login_cap.h>
#include <security/pam_appl.h>

#ifdef KRB5
extern krb5_context ssh_context;
krb5_principal tkt_client = NULL;    /* Principal from the received ticket. 
Also is used as an indication of succesful krb5 authentization. */
#endif

/* import */
extern ServerOptions options;

/*
 * convert ssh auth msg type into description
 */
char *
get_authname(int type)
{
	static char buf[1024];
	switch (type) {
	case SSH_CMSG_AUTH_PASSWORD:
		return "password";
	case SSH_CMSG_AUTH_RSA:
		return "rsa";
	case SSH_CMSG_AUTH_RHOSTS_RSA:
		return "rhosts-rsa";
	case SSH_CMSG_AUTH_RHOSTS:
		return "rhosts";
	case SSH_CMSG_AUTH_TIS:
	case SSH_CMSG_AUTH_TIS_RESPONSE:
		return "challenge-response";
#if defined(KRB4) || defined(KRB5)
	case SSH_CMSG_AUTH_KERBEROS:
		return "kerberos";
#endif
	}
	snprintf(buf, sizeof buf, "bad-auth-msg-%d", type);
	return buf;
}

/*
 * read packets, try to authenticate the user and
 * return only if authentication is successful
 */
void
do_authloop(Authctxt *authctxt)
{
	int authenticated = 0;
	u_int bits;
	RSA *client_host_key;
	BIGNUM *n;
	char *client_user, *password;
	char info[1024];
	u_int dlen;
	int plen, nlen, elen;
	u_int ulen;
	int type = 0;
	struct passwd *pw = authctxt->pw;
	void (*authlog) (const char *fmt,...) = verbose;
#ifdef HAVE_LOGIN_CAP
	login_cap_t *lc;
#endif /* HAVE_LOGIN_CAP */
#ifdef USE_PAM
	struct inverted_pam_cookie *pam_cookie;
#endif /* USE_PAM */
#if defined(HAVE_LOGIN_CAP) || defined(LOGIN_ACCESS)
	const char *from_host, *from_ip;

	from_host = get_canonical_hostname(options.reverse_mapping_check);
	from_ip = get_remote_ipaddr();
#endif /* HAVE_LOGIN_CAP || LOGIN_ACCESS */
#if 0
#ifdef KRB5
	{
	  	krb5_error_code ret;
		
		ret = krb5_init_context(&ssh_context);
		if (ret)
		 	verbose("Error while initializing Kerberos V5."); 
		krb5_init_ets(ssh_context);
		
	}
#endif /* KRB5 */
#endif

	debug("Attempting authentication for %s%.100s.",
	     authctxt->valid ? "" : "illegal user ", authctxt->user);

	/* If the user has no password, accept authentication immediately. */
	if (options.password_authentication &&
#if defined(KRB4) || defined(KRB5)
	    (!options.kerberos_authentication
#if defined(KRB4)
	    || options.krb4_or_local_passwd
#endif
	    ) &&
#endif
#ifdef USE_PAM
	    auth_pam_password(authctxt, "")
#else
	    auth_password(authctxt, "")
#endif
		) {
		auth_log(authctxt, 1, "without authentication", "");
		return;
	}

	/* Indicate that authentication is needed. */
	packet_start(SSH_SMSG_FAILURE);
	packet_send();
	packet_write_wait();

	client_user = NULL;

	for (;;) {
		/* default to fail */
		authenticated = 0;

		info[0] = '\0';

		/* Get a packet from the client. */
		type = packet_read(&plen);

		/* Process the packet. */
		switch (type) {
#ifdef AFS
#ifndef KRB5
		case SSH_CMSG_HAVE_KERBEROS_TGT:
			if (!options.krb4_tgt_passing) {
				/* packet_get_all(); */
				verbose("Kerberos v4 tgt passing disabled.");
				break;
			} else {
				/* Accept Kerberos v4 tgt. */
				char *tgt = packet_get_string(&dlen);
				packet_integrity_check(plen, 4 + dlen, type);
				if (!auth_krb4_tgt(pw, tgt))
					verbose("Kerberos v4 tgt REFUSED for %.100ss", authctxt->user);
				xfree(tgt);
			}
			continue;
#endif /* !KRB5 */
		case SSH_CMSG_HAVE_AFS_TOKEN:
			if (!options.afs_token_passing || !k_hasafs()) {
				verbose("AFS token passing disabled.");
				break;
			} else {
				/* Accept AFS token. */
				char *token_string = packet_get_string(&dlen);
				packet_integrity_check(plen, 4 + dlen, type);
				if (!auth_afs_token(pw, token_string))
					verbose("AFS token REFUSED for %.100s", authctxt->user);
				xfree(token_string);
			}
			continue;
#endif /* AFS */
#if defined(KRB4) || defined(KRB5)
		case SSH_CMSG_AUTH_KERBEROS:
			if (!options.kerberos_authentication) {
				verbose("Kerberos authentication disabled.");
				break;
			} else {
				/* Try Kerberos authentication. */
				u_int len;
				char *tkt_user = NULL;
				char *kdata = packet_get_string(&len);
				packet_integrity_check(plen, 4 + len, type);

				if (!authctxt->valid) {
					/* Do nothing. */
				} else if (kdata[0] == 4) {	/* 4 == KRB_PROT_VERSION */
#ifdef KRB4
					KTEXT_ST auth;

					auth.length = len;
					if (auth.length < MAX_KTXT_LEN)
						memcpy(auth.dat, kdata, auth.length);
					authenticated = auth_krb4(pw->pw_name, &auth, &tkt_user);

					if (authenticated) {
						snprintf(info, sizeof info,
						    " tktuser %.100s", tkt_user);
						xfree(tkt_user);
					}
#else
					verbose("Kerberos v4 authentication disabled.");
#endif /* KRB4 */
				} else {
#ifndef KRB5
					verbose("Kerberos v5 authentication disabled.");
#else
				  	krb5_data k5data; 
					k5data.length = len;
					k5data.data = kdata;
  #if 0	
					if (krb5_init_context(&ssh_context)) {
						verbose("Error while initializing Kerberos V5.");
						break;
					}
					krb5_init_ets(ssh_context);
  #endif
					/* pw->name is passed just for logging purposes */
					if (auth_krb5(pw->pw_name, &k5data, &tkt_client)) {
					  	/* authorize client against .k5login */
					  	if (krb5_kuserok(ssh_context,
						      tkt_client,
						      pw->pw_name))
						  	authenticated = 1;
					}
#endif /* KRB5 */
  				}
				xfree(kdata);
  			}
  			break;
#endif /* KRB4 || KRB5 */

		case SSH_CMSG_AUTH_RHOSTS:
			if (!options.rhosts_authentication) {
				verbose("Rhosts authentication disabled.");
				break;
			}
			/*
			 * Get client user name.  Note that we just have to
			 * trust the client; this is one reason why rhosts
			 * authentication is insecure. (Another is
			 * IP-spoofing on a local network.)
			 */
			client_user = packet_get_string(&ulen);
			packet_integrity_check(plen, 4 + ulen, type);

			/* Try to authenticate using /etc/hosts.equiv and .rhosts. */
			authenticated = auth_rhosts(pw, client_user);

			snprintf(info, sizeof info, " ruser %.100s", client_user);
			break;

		case SSH_CMSG_AUTH_RHOSTS_RSA:
			if (!options.rhosts_rsa_authentication) {
				verbose("Rhosts with RSA authentication disabled.");
				break;
			}
			/*
			 * Get client user name.  Note that we just have to
			 * trust the client; root on the client machine can
			 * claim to be any user.
			 */
			client_user = packet_get_string(&ulen);

			/* Get the client host key. */
			client_host_key = RSA_new();
			if (client_host_key == NULL)
				fatal("RSA_new failed");
			client_host_key->e = BN_new();
			client_host_key->n = BN_new();
			if (client_host_key->e == NULL || client_host_key->n == NULL)
				fatal("BN_new failed");
			bits = packet_get_int();
			packet_get_bignum(client_host_key->e, &elen);
			packet_get_bignum(client_host_key->n, &nlen);

			if (bits != BN_num_bits(client_host_key->n))
				verbose("Warning: keysize mismatch for client_host_key: "
				    "actual %d, announced %d", BN_num_bits(client_host_key->n), bits);
			packet_integrity_check(plen, (4 + ulen) + 4 + elen + nlen, type);

			authenticated = auth_rhosts_rsa(pw, client_user, client_host_key);
			RSA_free(client_host_key);

			snprintf(info, sizeof info, " ruser %.100s", client_user);
			break;

		case SSH_CMSG_AUTH_RSA:
			if (!options.rsa_authentication) {
				verbose("RSA authentication disabled.");
				break;
			}
			/* RSA authentication requested. */
			n = BN_new();
			packet_get_bignum(n, &nlen);
			packet_integrity_check(plen, nlen, type);
			authenticated = auth_rsa(pw, n);
			BN_clear_free(n);
			break;

		case SSH_CMSG_AUTH_PASSWORD:
			if (!options.password_authentication) {
				verbose("Password authentication disabled.");
				break;
			}
			/*
			 * Read user password.  It is in plain text, but was
			 * transmitted over the encrypted channel so it is
			 * not visible to an outside observer.
			 */
			password = packet_get_string(&dlen);
			packet_integrity_check(plen, 4 + dlen, type);

#ifdef USE_PAM
			/* Do PAM auth with password */
			authenticated = auth_pam_password(authctxt, password);
#else /* !USE_PAM */
			/* Try authentication with the password. */
			authenticated = auth_password(authctxt, password);
#endif /* USE_PAM */

			memset(password, 0, strlen(password));
			xfree(password);
			break;

#ifdef USE_PAM
		case SSH_CMSG_AUTH_TIS:
			debug("rcvd SSH_CMSG_AUTH_TIS: Trying PAM");
			pam_cookie = ipam_start_auth("csshd", pw->pw_name);
			/* We now have data available to send as a challenge */
			if (pam_cookie->num_msg != 1 ||
			    (pam_cookie->msg[0]->msg_style != PAM_PROMPT_ECHO_OFF &&
			     pam_cookie->msg[0]->msg_style != PAM_PROMPT_ECHO_ON)) {
			    /* We got several challenges or an unknown challenge type */
			    ipam_free_cookie(pam_cookie);
			    pam_cookie = NULL;
			    break;
			}
			packet_start(SSH_SMSG_AUTH_TIS_CHALLENGE);
			packet_put_string(pam_cookie->msg[0]->msg, strlen(pam_cookie->msg[0]->msg));
			packet_send();
			packet_write_wait();
			continue;
		case SSH_CMSG_AUTH_TIS_RESPONSE:
			debug("rcvd SSH_CMSG_AUTH_TIS_RESPONSE");
			if (pam_cookie == NULL)
			    break;
			{
			    char *response = packet_get_string(&dlen);
			    
			    packet_integrity_check(plen, 4 + dlen, type);
			    pam_cookie->resp[0]->resp = strdup(response);
			    xfree(response);
			    authenticated = ipam_complete_auth(pam_cookie);
			    ipam_free_cookie(pam_cookie);
			    pam_cookie = NULL;
			}
			break;
#elif defined(SKEY)
		case SSH_CMSG_AUTH_TIS:
			debug("rcvd SSH_CMSG_AUTH_TIS");
			if (options.challenge_reponse_authentication == 1) {
				char *challenge = get_challenge(authctxt, authctxt->style);
				if (challenge != NULL) {
					debug("sending challenge '%s'", challenge);
					packet_start(SSH_SMSG_AUTH_TIS_CHALLENGE);
					packet_put_cstring(challenge);
					packet_send();
					packet_write_wait();
					continue;
				}
			}
			break;
		case SSH_CMSG_AUTH_TIS_RESPONSE:
			debug("rcvd SSH_CMSG_AUTH_TIS_RESPONSE");
			if (options.challenge_reponse_authentication == 1) {
				char *response = packet_get_string(&dlen);
				debug("got response '%s'", response);
				packet_integrity_check(plen, 4 + dlen, type);
				authenticated = verify_response(authctxt, response);
				memset(response, 'r', dlen);
				xfree(response);
			}
			break;
#else
		case SSH_CMSG_AUTH_TIS:
			/* TIS Authentication is unsupported */
			log("TIS authentication unsupported.");
			break;
#endif
#ifdef KRB5
		case SSH_CMSG_HAVE_KERBEROS_TGT:
			/* Passing krb5 ticket */
			if (!options.krb5_tgt_passing 
                            /*|| !options.krb5_authentication */) {
				verbose("Kerberos v5 tgt passing disabled.");
				break;
			}
			
			if (tkt_client == NULL) {
			  /* passing tgt without krb5 authentication */
			}
			
			{
			  krb5_data tgt;
			  u_int tgtlen;
			  tgt.data = packet_get_string(&tgtlen);
			  tgt.length = tgtlen;
			  
			  if (!auth_krb5_tgt(pw->pw_name, &tgt, tkt_client))
			    verbose ("Kerberos V5 TGT refused for %.100s", pw->pw_name);
			  xfree(tgt.data);
			      
			  break;
			}
#endif /* KRB5 */

		default:
			/*
			 * Any unknown messages will be ignored (and failure
			 * returned) during authentication.
			 */
			log("Unknown message during authentication: type %d", type);
			break;
		}

#ifdef HAVE_LOGIN_CAP
		if (pw != NULL) {
		  lc = login_getpwclass(pw);
		  if (lc == NULL)
			lc = login_getclassbyname(NULL, pw);
		  if (!auth_hostok(lc, from_host, from_ip)) {
			log("Denied connection for %.200s from %.200s [%.200s].",
		      pw->pw_name, from_host, from_ip);
			packet_disconnect("Sorry, you are not allowed to connect.");
		  }
		  if (!auth_timeok(lc, time(NULL))) {
			log("LOGIN %.200s REFUSED (TIME) FROM %.200s",
		      pw->pw_name, from_host);
			packet_disconnect("Logins not available right now.");
		  }
		  login_close(lc);
		  lc = NULL;
		}
#endif  /* HAVE_LOGIN_CAP */
#ifdef LOGIN_ACCESS
		if (pw != NULL && !login_access(pw->pw_name, from_host)) {
		  log("Denied connection for %.200s from %.200s [%.200s].",
		      pw->pw_name, from_host, from_ip);
		  packet_disconnect("Sorry, you are not allowed to connect.");
		}
#endif /* LOGIN_ACCESS */
#ifdef BSD_AUTH
		if (authctxt->as) {
			auth_close(authctxt->as);
			authctxt->as = NULL;
		}
#endif
		if (!authctxt->valid && authenticated)
			fatal("INTERNAL ERROR: authenticated invalid user %s",
			    authctxt->user);

		/* Special handling for root */
		if (authenticated && authctxt->pw->pw_uid == 0 &&
		    !auth_root_allowed(get_authname(type)))
			authenticated = 0;

		if (pw != NULL && pw->pw_uid == 0)
		  log("ROOT LOGIN as '%.100s' from %.100s",
		      pw->pw_name,
			  get_canonical_hostname(options.reverse_mapping_check));

		/* Log before sending the reply */
		auth_log(authctxt, authenticated, get_authname(type), info);

#ifdef USE_PAM
		if (authenticated && !do_pam_account(pw->pw_name, client_user))
			authenticated = 0;
#endif

		if (client_user != NULL) {
			xfree(client_user);
			client_user = NULL;
		}

		if (authenticated)
			return;

		if (authctxt->failures++ > AUTH_FAIL_MAX)
			packet_disconnect(AUTH_FAIL_MSG, authctxt->user);

		packet_start(SSH_SMSG_FAILURE);
		packet_send();
		packet_write_wait();
	}
}

/*
 * Performs authentication of an incoming connection.  Session key has already
 * been exchanged and encryption is enabled.
 */
void
do_authentication()
{
	Authctxt *authctxt;
	struct passwd *pw;
	int plen;
	u_int ulen;
	char *user, *style = NULL;

	/* Get the name of the user that we wish to log in as. */
	packet_read_expect(&plen, SSH_CMSG_USER);

	/* Get the user name. */
	user = packet_get_string(&ulen);
	packet_integrity_check(plen, (4 + ulen), SSH_CMSG_USER);

	if ((style = strchr(user, ':')) != NULL)
		*style++ = 0;

	authctxt = authctxt_new();
	authctxt->user = user;
	authctxt->style = style;

	/* Verify that the user is a valid user. */
	pw = getpwnam(user);
	if (pw && allowed_user(pw)) {
		authctxt->valid = 1;
		pw = pwcopy(pw);
	} else {
		debug("do_authentication: illegal user %s", user);
		pw = NULL;
	}
	authctxt->pw = pw;

#ifdef USE_PAM
	if (pw != NULL)
		start_pam(pw);
#endif
	setproctitle("%s", pw ? user : "unknown");

	/*
	 * If we are not running as root, the user must have the same uid as
	 * the server.
	 */
	if (getuid() != 0 && pw && pw->pw_uid != getuid())
		packet_disconnect("Cannot change user when server not running as root.");

	/*
	 * Loop until the user has been authenticated or the connection is
	 * closed, do_authloop() returns only if authentication is successful
	 */
	do_authloop(authctxt);

	/* The user has been authenticated and accepted. */
	packet_start(SSH_SMSG_SUCCESS);
	packet_send();
	packet_write_wait();

	/* Perform session preparation. */
	do_authenticated(authctxt);
}
