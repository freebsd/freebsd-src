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
RCSID("$OpenBSD: auth1.c,v 1.44 2002/09/26 11:38:43 markus Exp $");
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
#include "channels.h"
#include "session.h"
#include "uidswap.h"
#include "monitor_wrap.h"

/* import */
extern ServerOptions options;

/*
 * convert ssh auth msg type into description
 */
static char *
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
static void
do_authloop(Authctxt *authctxt)
{
	int authenticated = 0;
	u_int bits;
	Key *client_host_key;
	BIGNUM *n;
	char *client_user, *password;
	char info[1024];
	u_int dlen;
	u_int ulen;
	int type = 0;
	struct passwd *pw = authctxt->pw;

	debug("Attempting authentication for %s%.100s.",
	    authctxt->valid ? "" : "illegal user ", authctxt->user);

	/* If the user has no password, accept authentication immediately. */
	if (options.password_authentication &&
#if defined(KRB4) || defined(KRB5)
	    (!options.kerberos_authentication || options.kerberos_or_local_passwd) &&
#endif
	    PRIVSEP(auth_password(authctxt, ""))) {
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
		type = packet_read();

		/* Process the packet. */
		switch (type) {

#if defined(KRB4) || defined(KRB5)
		case SSH_CMSG_AUTH_KERBEROS:
			if (!options.kerberos_authentication) {
				verbose("Kerberos authentication disabled.");
			} else {
				char *kdata = packet_get_string(&dlen);
				packet_check_eom();

				if (kdata[0] == 4) { /* KRB_PROT_VERSION */
#ifdef KRB4
					KTEXT_ST tkt, reply;
					tkt.length = dlen;
					if (tkt.length < MAX_KTXT_LEN)
						memcpy(tkt.dat, kdata, tkt.length);

					if (PRIVSEP(auth_krb4(authctxt, &tkt,
					    &client_user, &reply))) {
						authenticated = 1;
						snprintf(info, sizeof(info),
						    " tktuser %.100s",
						    client_user);

						packet_start(
						    SSH_SMSG_AUTH_KERBEROS_RESPONSE);
						packet_put_string((char *)
						    reply.dat, reply.length);
						packet_send();
						packet_write_wait();
					}
#endif /* KRB4 */
				} else {
#ifdef KRB5
					krb5_data tkt, reply;
					tkt.length = dlen;
					tkt.data = kdata;

					if (PRIVSEP(auth_krb5(authctxt, &tkt,
					    &client_user, &reply))) {
						authenticated = 1;
						snprintf(info, sizeof(info),
						    " tktuser %.100s",
						    client_user);
 
 						/* Send response to client */
 						packet_start(
						    SSH_SMSG_AUTH_KERBEROS_RESPONSE);
 						packet_put_string((char *)
						    reply.data, reply.length);
 						packet_send();
 						packet_write_wait();

 						if (reply.length)
 							xfree(reply.data);
					}
#endif /* KRB5 */
				}
				xfree(kdata);
			}
			break;
#endif /* KRB4 || KRB5 */

#if defined(AFS) || defined(KRB5)
			/* XXX - punt on backward compatibility here. */
		case SSH_CMSG_HAVE_KERBEROS_TGT:
			packet_send_debug("Kerberos TGT passing disabled before authentication.");
			break;
#ifdef AFS
		case SSH_CMSG_HAVE_AFS_TOKEN:
			packet_send_debug("AFS token passing disabled before authentication.");
			break;
#endif /* AFS */
#endif /* AFS || KRB5 */

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
			packet_check_eom();

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
			client_host_key = key_new(KEY_RSA1);
			bits = packet_get_int();
			packet_get_bignum(client_host_key->rsa->e);
			packet_get_bignum(client_host_key->rsa->n);

			if (bits != BN_num_bits(client_host_key->rsa->n))
				verbose("Warning: keysize mismatch for client_host_key: "
				    "actual %d, announced %d",
				    BN_num_bits(client_host_key->rsa->n), bits);
			packet_check_eom();

			authenticated = auth_rhosts_rsa(pw, client_user,
			    client_host_key);
			key_free(client_host_key);

			snprintf(info, sizeof info, " ruser %.100s", client_user);
			break;

		case SSH_CMSG_AUTH_RSA:
			if (!options.rsa_authentication) {
				verbose("RSA authentication disabled.");
				break;
			}
			/* RSA authentication requested. */
			if ((n = BN_new()) == NULL)
				fatal("do_authloop: BN_new failed");
			packet_get_bignum(n);
			packet_check_eom();
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
			packet_check_eom();

			/* Try authentication with the password. */
			authenticated = PRIVSEP(auth_password(authctxt, password));

			memset(password, 0, strlen(password));
			xfree(password);
			break;

		case SSH_CMSG_AUTH_TIS:
			debug("rcvd SSH_CMSG_AUTH_TIS");
			if (options.challenge_response_authentication == 1) {
				char *challenge = get_challenge(authctxt);
				if (challenge != NULL) {
					debug("sending challenge '%s'", challenge);
					packet_start(SSH_SMSG_AUTH_TIS_CHALLENGE);
					packet_put_cstring(challenge);
					xfree(challenge);
					packet_send();
					packet_write_wait();
					continue;
				}
			}
			break;
		case SSH_CMSG_AUTH_TIS_RESPONSE:
			debug("rcvd SSH_CMSG_AUTH_TIS_RESPONSE");
			if (options.challenge_response_authentication == 1) {
				char *response = packet_get_string(&dlen);
				debug("got response '%s'", response);
				packet_check_eom();
				authenticated = verify_response(authctxt, response);
				memset(response, 'r', dlen);
				xfree(response);
			}
			break;

		default:
			/*
			 * Any unknown messages will be ignored (and failure
			 * returned) during authentication.
			 */
			log("Unknown message during authentication: type %d", type);
			break;
		}
#ifdef BSD_AUTH
		if (authctxt->as) {
			auth_close(authctxt->as);
			authctxt->as = NULL;
		}
#endif
		if (!authctxt->valid && authenticated)
			fatal("INTERNAL ERROR: authenticated invalid user %s",
			    authctxt->user);

#ifdef _UNICOS
		if (type == SSH_CMSG_AUTH_PASSWORD && !authenticated)
			cray_login_failure(authctxt->user, IA_UDBERR);
		if (authenticated && cray_access_denied(authctxt->user)) {
			authenticated = 0;
			fatal("Access denied for user %s.",authctxt->user);
		}
#endif /* _UNICOS */

#ifdef HAVE_CYGWIN
		if (authenticated &&
		    !check_nt_auth(type == SSH_CMSG_AUTH_PASSWORD, pw)) {
			packet_disconnect("Authentication rejected for uid %d.",
			pw == NULL ? -1 : pw->pw_uid);
			authenticated = 0;
		}
#else
		/* Special handling for root */
		if (!use_privsep &&
		    authenticated && authctxt->pw->pw_uid == 0 &&
		    !auth_root_allowed(get_authname(type)))
			authenticated = 0;
#endif
#ifdef USE_PAM
		if (!use_privsep && authenticated && 
		    !do_pam_account(pw->pw_name, client_user))
			authenticated = 0;
#endif

		/* Log before sending the reply */
		auth_log(authctxt, authenticated, get_authname(type), info);

		if (client_user != NULL) {
			xfree(client_user);
			client_user = NULL;
		}

		if (authenticated)
			return;

		if (authctxt->failures++ > AUTH_FAIL_MAX) {
			packet_disconnect(AUTH_FAIL_MSG, authctxt->user);
		}

		packet_start(SSH_SMSG_FAILURE);
		packet_send();
		packet_write_wait();
	}
}

/*
 * Performs authentication of an incoming connection.  Session key has already
 * been exchanged and encryption is enabled.
 */
Authctxt *
do_authentication(void)
{
	Authctxt *authctxt;
	u_int ulen;
	char *user, *style = NULL;

	/* Get the name of the user that we wish to log in as. */
	packet_read_expect(SSH_CMSG_USER);

	/* Get the user name. */
	user = packet_get_string(&ulen);
	packet_check_eom();

	if ((style = strchr(user, ':')) != NULL)
		*style++ = '\0';

#ifdef KRB5
	/* XXX - SSH.com Kerberos v5 braindeath. */
	if ((datafellows & SSH_BUG_K5USER) &&
	    options.kerberos_authentication) {
		char *p;
		if ((p = strchr(user, '@')) != NULL)
			*p = '\0';
	}
#endif

	authctxt = authctxt_new();
	authctxt->user = user;
	authctxt->style = style;

	/* Verify that the user is a valid user. */
	if ((authctxt->pw = PRIVSEP(getpwnamallow(user))) != NULL)
		authctxt->valid = 1;
	else
		debug("do_authentication: illegal user %s", user);

	setproctitle("%s%s", authctxt->pw ? user : "unknown",
	    use_privsep ? " [net]" : "");

#ifdef USE_PAM
	PRIVSEP(start_pam(authctxt->pw == NULL ? "NOUSER" : user));
#endif

	/*
	 * If we are not running as root, the user must have the same uid as
	 * the server. (Unless you are running Windows)
	 */
#ifndef HAVE_CYGWIN
	if (!use_privsep && getuid() != 0 && authctxt->pw &&
	    authctxt->pw->pw_uid != getuid())
		packet_disconnect("Cannot change user when server not running as root.");
#endif

	/*
	 * Loop until the user has been authenticated or the connection is
	 * closed, do_authloop() returns only if authentication is successful
	 */
	do_authloop(authctxt);

	/* The user has been authenticated and accepted. */
	packet_start(SSH_SMSG_SUCCESS);
	packet_send();
	packet_write_wait();

	return (authctxt);
}
