/*
 * Author: Tatu Ylonen <ylo@cs.hut.fi>
 * Copyright (c) 1995 Tatu Ylonen <ylo@cs.hut.fi>, Espoo, Finland
 *                    All rights reserved
 * Functions to interface with the SSH_AUTHENTICATION_FD socket.
 *
 * As far as I am concerned, the code I have written for this software
 * can be used freely for any purpose.  Any derived versions of this
 * software must be clearly marked as such, and if the derived work is
 * incompatible with the protocol description in the RFC file, it must be
 * called by a name other than "ssh" or "Secure Shell".
 */

/* RCSID("$OpenBSD: authfd.h,v 1.13 2000/10/09 21:51:00 markus Exp $"); */

#ifndef AUTHFD_H
#define AUTHFD_H

#include "buffer.h"

/* Messages for the authentication agent connection. */
#define SSH_AGENTC_REQUEST_RSA_IDENTITIES	1
#define SSH_AGENT_RSA_IDENTITIES_ANSWER		2
#define SSH_AGENTC_RSA_CHALLENGE		3
#define SSH_AGENT_RSA_RESPONSE			4
#define SSH_AGENT_FAILURE			5
#define SSH_AGENT_SUCCESS			6
#define SSH_AGENTC_ADD_RSA_IDENTITY		7
#define SSH_AGENTC_REMOVE_RSA_IDENTITY		8
#define SSH_AGENTC_REMOVE_ALL_RSA_IDENTITIES	9

/* private OpenSSH extensions for SSH2 */
#define SSH2_AGENTC_REQUEST_IDENTITIES		11
#define SSH2_AGENT_IDENTITIES_ANSWER		12
#define SSH2_AGENTC_SIGN_REQUEST		13
#define SSH2_AGENT_SIGN_RESPONSE		14
#define SSH2_AGENTC_ADD_IDENTITY		17
#define SSH2_AGENTC_REMOVE_IDENTITY		18
#define SSH2_AGENTC_REMOVE_ALL_IDENTITIES	19

/* additional error code for ssh.com's ssh-agent2 */
#define SSH_COM_AGENT2_FAILURE                   102

#define	SSH_AGENT_OLD_SIGNATURE			0x01


typedef struct {
	int     fd;
	Buffer  identities;
	int     howmany;
}       AuthenticationConnection;

/* Returns the number of the authentication fd, or -1 if there is none. */
int     ssh_get_authentication_socket();

/*
 * This should be called for any descriptor returned by
 * ssh_get_authentication_socket().  Depending on the way the descriptor was
 * obtained, this may close the descriptor.
 */
void    ssh_close_authentication_socket(int authfd);

/*
 * Opens and connects a private socket for communication with the
 * authentication agent.  Returns NULL if an error occurred and the
 * connection could not be opened.  The connection should be closed by the
 * caller by calling ssh_close_authentication_connection().
 */
AuthenticationConnection *ssh_get_authentication_connection();

/*
 * Closes the connection to the authentication agent and frees any associated
 * memory.
 */
void    ssh_close_authentication_connection(AuthenticationConnection *auth);

/*
 * Returns the first authentication identity held by the agent or NULL if
 * no identies are available. Caller must free comment and key.
 * Note that you cannot mix calls with different versions.
 */
Key	*ssh_get_first_identity(AuthenticationConnection *auth, char **comment, int version);

/*
 * Returns the next authentication identity for the agent.  Other functions
 * can be called between this and ssh_get_first_identity or two calls of this
 * function.  This returns NULL if there are no more identities.  The caller
 * must free key and comment after a successful return.
 */
Key	*ssh_get_next_identity(AuthenticationConnection *auth, char **comment, int version);

/*
 * Requests the agent to decrypt the given challenge.  Returns true if the
 * agent claims it was able to decrypt it.
 */
int
ssh_decrypt_challenge(AuthenticationConnection *auth,
    Key *key, BIGNUM * challenge,
    unsigned char session_id[16],
    unsigned int response_type,
    unsigned char response[16]);

/* Requests the agent to sign data using key */
int
ssh_agent_sign(AuthenticationConnection *auth,
    Key *key,
    unsigned char **sigp, int *lenp,
    unsigned char *data, int datalen);

/*
 * Adds an identity to the authentication server.  This call is not meant to
 * be used by normal applications.  This returns true if the identity was
 * successfully added.
 */
int
ssh_add_identity(AuthenticationConnection *auth, Key *key,
    const char *comment);

/*
 * Removes the identity from the authentication server.  This call is not
 * meant to be used by normal applications.  This returns true if the
 * identity was successfully added.
 */
int     ssh_remove_identity(AuthenticationConnection *auth, Key *key);

/*
 * Removes all identities from the authentication agent.  This call is not
 * meant to be used by normal applications.  This returns true if the
 * operation was successful.
 */
int     ssh_remove_all_identities(AuthenticationConnection *auth, int version);

#endif				/* AUTHFD_H */
