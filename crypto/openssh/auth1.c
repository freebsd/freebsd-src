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
RCSID("$OpenBSD: auth1.c,v 1.4 2000/09/07 20:27:49 deraadt Exp $");

#include "xmalloc.h"
#include "rsa.h"
#include "ssh.h"
#include "packet.h"
#include "buffer.h"
#include "cipher.h"
#include "mpaux.h"
#include "servconf.h"
#include "compat.h"
#include "auth.h"
#include "session.h"

/* import */
extern ServerOptions options;
extern char *forced_command;

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
#ifdef KRB4
	case SSH_CMSG_AUTH_KERBEROS:
		return "kerberos";
#endif
#ifdef SKEY
	case SSH_CMSG_AUTH_TIS_RESPONSE:
		return "s/key";
#endif
	}
	snprintf(buf, sizeof buf, "bad-auth-msg-%d", type);
	return buf;
}

/*
 * The user does not exist or access is denied,
 * but fake indication that authentication is needed.
 */
void
do_fake_authloop1(char *user)
{
	int attempt = 0;

	log("Faking authloop for illegal user %.200s from %.200s port %d",
	    user,
	    get_remote_ipaddr(),
	    get_remote_port());

	/* Indicate that authentication is needed. */
	packet_start(SSH_SMSG_FAILURE);
	packet_send();
	packet_write_wait();

	/*
	 * Keep reading packets, and always respond with a failure.  This is
	 * to avoid disclosing whether such a user really exists.
	 */
	for (attempt = 1;; attempt++) {
		/* Read a packet.  This will not return if the client disconnects. */
		int plen;
		int type = packet_read(&plen);
#ifdef SKEY
		unsigned int dlen;
		char *password, *skeyinfo;
		password = NULL;
		/* Try to send a fake s/key challenge. */
		if (options.skey_authentication == 1 &&
		    (skeyinfo = skey_fake_keyinfo(user)) != NULL) {
			if (type == SSH_CMSG_AUTH_TIS) {
				packet_start(SSH_SMSG_AUTH_TIS_CHALLENGE);
				packet_put_string(skeyinfo, strlen(skeyinfo));
				packet_send();
				packet_write_wait();
				continue;
			} else if (type == SSH_CMSG_AUTH_PASSWORD &&
				   options.password_authentication &&
				   (password = packet_get_string(&dlen)) != NULL &&
				   dlen == 5 &&
				   strncasecmp(password, "s/key", 5) == 0 ) {
				packet_send_debug(skeyinfo);
			}
		}
		if (password != NULL)
			xfree(password);
#endif
		if (attempt > AUTH_FAIL_MAX)
			packet_disconnect(AUTH_FAIL_MSG, user);

		/*
		 * Send failure.  This should be indistinguishable from a
		 * failed authentication.
		 */
		packet_start(SSH_SMSG_FAILURE);
		packet_send();
		packet_write_wait();
	}
	/* NOTREACHED */
	abort();
}

/*
 * read packets and try to authenticate local user *pw.
 * return if authentication is successfull
 */
void
do_authloop(struct passwd * pw)
{
	int attempt = 0;
	unsigned int bits;
	RSA *client_host_key;
	BIGNUM *n;
	char *client_user, *password;
	char user[1024];
	unsigned int dlen;
	int plen, nlen, elen;
	unsigned int ulen;
	int type = 0;
	void (*authlog) (const char *fmt,...) = verbose;

	/* Indicate that authentication is needed. */
	packet_start(SSH_SMSG_FAILURE);
	packet_send();
	packet_write_wait();

	for (attempt = 1;; attempt++) {
		int authenticated = 0;
		strlcpy(user, "", sizeof user);

		/* Get a packet from the client. */
		type = packet_read(&plen);

		/* Process the packet. */
		switch (type) {
#ifdef AFS
		case SSH_CMSG_HAVE_KERBEROS_TGT:
			if (!options.kerberos_tgt_passing) {
				/* packet_get_all(); */
				verbose("Kerberos tgt passing disabled.");
				break;
			} else {
				/* Accept Kerberos tgt. */
				char *tgt = packet_get_string(&dlen);
				packet_integrity_check(plen, 4 + dlen, type);
				if (!auth_kerberos_tgt(pw, tgt))
					verbose("Kerberos tgt REFUSED for %s", pw->pw_name);
				xfree(tgt);
			}
			continue;

		case SSH_CMSG_HAVE_AFS_TOKEN:
			if (!options.afs_token_passing || !k_hasafs()) {
				/* packet_get_all(); */
				verbose("AFS token passing disabled.");
				break;
			} else {
				/* Accept AFS token. */
				char *token_string = packet_get_string(&dlen);
				packet_integrity_check(plen, 4 + dlen, type);
				if (!auth_afs_token(pw, token_string))
					verbose("AFS token REFUSED for %s", pw->pw_name);
				xfree(token_string);
			}
			continue;
#endif /* AFS */
#ifdef KRB4
		case SSH_CMSG_AUTH_KERBEROS:
			if (!options.kerberos_authentication) {
				/* packet_get_all(); */
				verbose("Kerberos authentication disabled.");
				break;
			} else {
				/* Try Kerberos v4 authentication. */
				KTEXT_ST auth;
				char *tkt_user = NULL;
				char *kdata = packet_get_string((unsigned int *) &auth.length);
				packet_integrity_check(plen, 4 + auth.length, type);

				if (auth.length < MAX_KTXT_LEN)
					memcpy(auth.dat, kdata, auth.length);
				xfree(kdata);

				authenticated = auth_krb4(pw->pw_name, &auth, &tkt_user);

				if (authenticated) {
					snprintf(user, sizeof user, " tktuser %s", tkt_user);
					xfree(tkt_user);
				}
			}
			break;
#endif /* KRB4 */

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

			/* Try to authenticate using /etc/hosts.equiv and
			   .rhosts. */
			authenticated = auth_rhosts(pw, client_user);

			snprintf(user, sizeof user, " ruser %s", client_user);
			xfree(client_user);
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
				log("Warning: keysize mismatch for client_host_key: "
				    "actual %d, announced %d", BN_num_bits(client_host_key->n), bits);
			packet_integrity_check(plen, (4 + ulen) + 4 + elen + nlen, type);

			authenticated = auth_rhosts_rsa(pw, client_user, client_host_key);
			RSA_free(client_host_key);

			snprintf(user, sizeof user, " ruser %s", client_user);
			xfree(client_user);
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

			/* Try authentication with the password. */
			authenticated = auth_password(pw, password);

			memset(password, 0, strlen(password));
			xfree(password);
			break;

#ifdef SKEY
		case SSH_CMSG_AUTH_TIS:
			debug("rcvd SSH_CMSG_AUTH_TIS");
			if (options.skey_authentication == 1) {
				char *skeyinfo = skey_keyinfo(pw->pw_name);
				if (skeyinfo == NULL) {
					debug("generating fake skeyinfo for %.100s.", pw->pw_name);
					skeyinfo = skey_fake_keyinfo(pw->pw_name);
				}
				if (skeyinfo != NULL) {
					/* we send our s/key- in tis-challenge messages */
					debug("sending challenge '%s'", skeyinfo);
					packet_start(SSH_SMSG_AUTH_TIS_CHALLENGE);
					packet_put_string(skeyinfo, strlen(skeyinfo));
					packet_send();
					packet_write_wait();
					continue;
				}
			}
			break;
		case SSH_CMSG_AUTH_TIS_RESPONSE:
			debug("rcvd SSH_CMSG_AUTH_TIS_RESPONSE");
			if (options.skey_authentication == 1) {
				char *response = packet_get_string(&dlen);
				debug("skey response == '%s'", response);
				packet_integrity_check(plen, 4 + dlen, type);
				authenticated = (skey_haskey(pw->pw_name) == 0 &&
						 skey_passcheck(pw->pw_name, response) != -1);
				xfree(response);
			}
			break;
#else
		case SSH_CMSG_AUTH_TIS:
			/* TIS Authentication is unsupported */
			log("TIS authentication unsupported.");
			break;
#endif

		default:
			/*
			 * Any unknown messages will be ignored (and failure
			 * returned) during authentication.
			 */
			log("Unknown message during authentication: type %d", type);
			break;
		}

		/*
		 * Check if the user is logging in as root and root logins
		 * are disallowed.
		 * Note that root login is allowed for forced commands.
		 */
		if (authenticated && pw->pw_uid == 0 && !options.permit_root_login) {
			if (forced_command) {
				log("Root login accepted for forced command.");
			} else {
				authenticated = 0;
				log("ROOT LOGIN REFUSED FROM %.200s",
				    get_canonical_hostname());
			}
		}

		/* Raise logging level */
		if (authenticated ||
		    attempt == AUTH_FAIL_LOG ||
		    type == SSH_CMSG_AUTH_PASSWORD)
			authlog = log;

		authlog("%s %s for %.200s from %.200s port %d%s",
			authenticated ? "Accepted" : "Failed",
			get_authname(type),
			pw->pw_uid == 0 ? "ROOT" : pw->pw_name,
			get_remote_ipaddr(),
			get_remote_port(),
			user);

		if (authenticated)
			return;

		if (attempt > AUTH_FAIL_MAX)
			packet_disconnect(AUTH_FAIL_MSG, pw->pw_name);

		/* Send a message indicating that the authentication attempt failed. */
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
	struct passwd *pw, pwcopy;
	int plen;
	unsigned int ulen;
	char *user;

	/* Get the name of the user that we wish to log in as. */
	packet_read_expect(&plen, SSH_CMSG_USER);

	/* Get the user name. */
	user = packet_get_string(&ulen);
	packet_integrity_check(plen, (4 + ulen), SSH_CMSG_USER);

	setproctitle("%s", user);

#ifdef AFS
	/* If machine has AFS, set process authentication group. */
	if (k_hasafs()) {
		k_setpag();
		k_unlog();
	}
#endif /* AFS */

	/* Verify that the user is a valid user. */
	pw = getpwnam(user);
	if (!pw || !allowed_user(pw))
		do_fake_authloop1(user);
	xfree(user);

	/* Take a copy of the returned structure. */
	memset(&pwcopy, 0, sizeof(pwcopy));
	pwcopy.pw_name = xstrdup(pw->pw_name);
	pwcopy.pw_passwd = xstrdup(pw->pw_passwd);
	pwcopy.pw_uid = pw->pw_uid;
	pwcopy.pw_gid = pw->pw_gid;
	pwcopy.pw_class = xstrdup(pw->pw_class);
	pwcopy.pw_dir = xstrdup(pw->pw_dir);
	pwcopy.pw_shell = xstrdup(pw->pw_shell);
	pw = &pwcopy;

	/*
	 * If we are not running as root, the user must have the same uid as
	 * the server.
	 */
	if (getuid() != 0 && pw->pw_uid != getuid())
		packet_disconnect("Cannot change user when server not running as root.");

	debug("Attempting authentication for %.100s.", pw->pw_name);

	/* If the user has no password, accept authentication immediately. */
	if (options.password_authentication &&
#ifdef KRB4
	    (!options.kerberos_authentication || options.kerberos_or_local_passwd) &&
#endif /* KRB4 */
	    auth_password(pw, "")) {
		/* Authentication with empty password succeeded. */
		log("Login for user %s from %.100s, accepted without authentication.",
		    pw->pw_name, get_remote_ipaddr());
	} else {
		/* Loop until the user has been authenticated or the
		   connection is closed, do_authloop() returns only if
		   authentication is successfull */
		do_authloop(pw);
	}

	/* The user has been authenticated and accepted. */
	packet_start(SSH_SMSG_SUCCESS);
	packet_send();
	packet_write_wait();

	/* Perform session preparation. */
	do_authenticated(pw);
}
