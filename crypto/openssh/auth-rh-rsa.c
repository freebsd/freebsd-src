/*
 * Author: Tatu Ylonen <ylo@cs.hut.fi>
 * Copyright (c) 1995 Tatu Ylonen <ylo@cs.hut.fi>, Espoo, Finland
 *                    All rights reserved
 * Rhosts or /etc/hosts.equiv authentication combined with RSA host
 * authentication.
 *
 * As far as I am concerned, the code I have written for this software
 * can be used freely for any purpose.  Any derived versions of this
 * software must be clearly marked as such, and if the derived work is
 * incompatible with the protocol description in the RFC file, it must be
 * called by a name other than "ssh" or "Secure Shell".
 */

#include "includes.h"
RCSID("$OpenBSD: auth-rh-rsa.c,v 1.29 2002/03/04 12:43:06 markus Exp $");
RCSID("$FreeBSD$");

#include "packet.h"
#include "uidswap.h"
#include "log.h"
#include "servconf.h"
#include "key.h"
#include "hostfile.h"
#include "pathnames.h"
#include "auth.h"
#include "canohost.h"

/*
 * Tries to authenticate the user using the .rhosts file and the host using
 * its host key.  Returns true if authentication succeeds.
 */

int
auth_rhosts_rsa(struct passwd *pw, const char *client_user, Key *client_host_key)
{
	extern ServerOptions options;
	const char *canonical_hostname;
	HostStatus host_status;

	debug("Trying rhosts with RSA host authentication for client user %.100s", client_user);

	if (pw == NULL || client_host_key == NULL || client_host_key->rsa == NULL)
		return 0;

	/* Check if we would accept it using rhosts authentication. */
	if (!auth_rhosts(pw, client_user))
		return 0;

	canonical_hostname = get_canonical_hostname(
	    options.verify_reverse_mapping);

	debug("Rhosts RSA authentication: canonical host %.900s", canonical_hostname);

	host_status = check_key_in_hostfiles(pw, client_host_key,
	    canonical_hostname, _PATH_SSH_SYSTEM_HOSTFILE,
	    options.ignore_user_known_hosts ? NULL : _PATH_SSH_USER_HOSTFILE);

	if (host_status != HOST_OK) {
		debug("Rhosts with RSA host authentication denied: unknown or invalid host key");
		packet_send_debug("Your host key cannot be verified: unknown or invalid host key.");
		return 0;
	}
	/* A matching host key was found and is known. */

	/* Perform the challenge-response dialog with the client for the host key. */
	if (!auth_rsa_challenge_dialog(client_host_key->rsa)) {
		log("Client on %.800s failed to respond correctly to host authentication.",
		    canonical_hostname);
		return 0;
	}
	/*
	 * We have authenticated the user using .rhosts or /etc/hosts.equiv,
	 * and the host using RSA. We accept the authentication.
	 */

	verbose("Rhosts with RSA host authentication accepted for %.100s, %.100s on %.700s.",
	   pw->pw_name, client_user, canonical_hostname);
	packet_send_debug("Rhosts with RSA host authentication accepted.");
	return 1;
}
