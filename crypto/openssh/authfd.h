/*	$OpenBSD: authfd.h,v 1.32 2003/01/23 13:50:27 markus Exp $	*/

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

/* smartcard */
#define SSH_AGENTC_ADD_SMARTCARD_KEY		20
#define SSH_AGENTC_REMOVE_SMARTCARD_KEY		21

/* lock/unlock the agent */
#define SSH_AGENTC_LOCK				22
#define SSH_AGENTC_UNLOCK			23

/* add key with constraints */
#define SSH_AGENTC_ADD_RSA_ID_CONSTRAINED	24
#define SSH2_AGENTC_ADD_ID_CONSTRAINED		25

#define	SSH_AGENT_CONSTRAIN_LIFETIME		1
#define	SSH_AGENT_CONSTRAIN_CONFIRM		2

/* extended failure messages */
#define SSH2_AGENT_FAILURE			30

/* additional error code for ssh.com's ssh-agent2 */
#define SSH_COM_AGENT2_FAILURE			102

#define	SSH_AGENT_OLD_SIGNATURE			0x01

typedef struct {
	int	fd;
	Buffer	identities;
	int	howmany;
}	AuthenticationConnection;

int	ssh_agent_present(void);
int	ssh_get_authentication_socket(void);
void	ssh_close_authentication_socket(int);

AuthenticationConnection *ssh_get_authentication_connection(void);
void	ssh_close_authentication_connection(AuthenticationConnection *);
int	 ssh_get_num_identities(AuthenticationConnection *, int);
Key	*ssh_get_first_identity(AuthenticationConnection *, char **, int);
Key	*ssh_get_next_identity(AuthenticationConnection *, char **, int);
int	 ssh_add_identity(AuthenticationConnection *, Key *, const char *);
int	 ssh_add_identity_constrained(AuthenticationConnection *, Key *,
    const char *, u_int, u_int);
int	 ssh_remove_identity(AuthenticationConnection *, Key *);
int	 ssh_remove_all_identities(AuthenticationConnection *, int);
int	 ssh_lock_agent(AuthenticationConnection *, int, const char *);
int	 ssh_update_card(AuthenticationConnection *, int, const char *, const char *);

int
ssh_decrypt_challenge(AuthenticationConnection *, Key *, BIGNUM *, u_char[16],
    u_int, u_char[16]);

int
ssh_agent_sign(AuthenticationConnection *, Key *, u_char **, u_int *, u_char *,
    u_int);

#endif				/* AUTHFD_H */
