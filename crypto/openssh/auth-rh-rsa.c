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
RCSID("$OpenBSD: auth-rh-rsa.c,v 1.34 2002/03/25 09:25:06 markus Exp $");

#include "packet.h"
#include "uidswap.h"
#include "log.h"
#include "servconf.h"
#include "key.h"
#include "hostfile.h"
#include "pathnames.h"
#include "auth.h"
#include "canohost.h"

#include "monitor_wrap.h"

/* import */
extern ServerOptions options;

int
auth_rhosts_rsa_key_allowed(struct passwd *pw, char *cuser, char *chost,
    Key *client_host_key)
{
	HostStatus host_status;

	/* Check if we would accept it using rhosts authentication. */
	if (!auth_rhosts(pw, cuser))
		return 0;

	host_status = check_key_in_hostfiles(pw, client_host_key,
	    chost, _PATH_SSH_SYSTEM_HOSTFILE,
	    options.ignore_user_known_hosts ? NULL : _PATH_SSH_USER_HOSTFILE);

	return (host_status == HOST_OK);
}

/*
 * Tries to authenticate the user using the .rhosts file and the host using
 * its host key.  Returns true if authentication succeeds.
 */
int
auth_rhosts_rsa(struct passwd *pw, char *cuser, Key *client_host_key)
{
	char *chost;

	debug("Trying rhosts with RSA host authentication for client user %.100s",
	    cuser);

	if (pw == NULL || client_host_key == NULL ||
	    client_host_key->rsa == NULL)
		return 0;

	chost = (char *)get_canonical_hostname(options.verify_reverse_mapping);
	debug("Rhosts RSA authentication: canonical host %.900s", chost);

	if (!PRIVSEP(auth_rhosts_rsa_key_allowed(pw, cuser, chost, client_host_key))) {
		debug("Rhosts with RSA host authentication denied: unknown or invalid host key");
		packet_send_debug("Your host key cannot be verified: unknown or invalid host key.");
		return 0;
	}
	/* A matching host key was found and is known. */

	/* Perform the challenge-response dialog with the client for the host key. */
	if (!auth_rsa_challenge_dialog(client_host_key)) {
		log("Client on %.800s failed to respond correctly to host authentication.",
		    chost);
		return 0;
	}
	/*
	 * We have authenticated the user using .rhosts or /etc/hosts.equiv,
	 * and the host using RSA. We accept the authentication.
	 */

	verbose("Rhosts with RSA host authentication accepted for %.100s, %.100s on %.700s.",
	   pw->pw_name, cuser, chost);
	packet_send_debug("Rhosts with RSA host authentication accepted.");
	return 1;
}
